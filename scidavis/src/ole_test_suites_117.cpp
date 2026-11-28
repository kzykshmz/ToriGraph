// Suite 117: regression checks from the 2026-11-28 review / debug campaign.
//
// Scale-invariant statistics: absolute epsilons (1e-14 / 1e-15 / 1e-30 / 1e-12) used to decide
// "tie", "zero variance" or "flat trace", so data in small SI units (fA, fs, F) silently got
// wrong results (Mann-Whitney p = 1, Shapiro-Wilk W = 0, ANOVA F = 0, no Quick Peaks, ...).
// Every check runs the same analysis on data and on data * 1e-16 (or a pA / fA scale) and
// requires the same statistic / p-value. Also: TableStatistics SD / variance of a single value
// is an empty cell (not NaN), curvemath.integrate accepts 5e-16 X steps, graph.add_error_band
// clamps a huge alpha in double, and Gaussian2D accepts a huge sigma.
#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "TableStatistics.h"

#include <QJsonArray>
#include <QJsonObject>
#include <gsl/gsl_cdf.h>

#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace {

int g117Failures = 0;

#define CHECK117(cond, msg)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "[FAIL] " << msg << "\n";                                                 \
            ++g117Failures;                                                                        \
        }                                                                                          \
    } while (0)

constexpr double kTiny117 = 1e-16;

// Same value within a relative tolerance (both zero counts as the same; NaN never does).
bool same117(double a, double b, double relTol = 1e-8)
{
    if (!std::isfinite(a) || !std::isfinite(b))
        return false;
    return std::fabs(a - b) <= relTol * std::max(std::fabs(a), std::fabs(b));
}

// Expected normal order statistics: a sample that passes Shapiro-Wilk comfortably.
std::vector<double> normalScores117(int n)
{
    std::vector<double> z(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        z[static_cast<size_t>(i)] = gsl_cdf_ugaussian_Pinv((i + 0.5) / static_cast<double>(n));
    return z;
}

Table *makeTable117(ApplicationWindow *mw, const QString &name, const QStringList &colNames,
                    const std::vector<std::vector<double>> &cols, double scale)
{
    size_t rows = 0;
    for (const auto &c : cols)
        rows = std::max(rows, c.size());
    Table *t = mw->newTable(name, static_cast<int>(rows), colNames.size());
    if (!t)
        return nullptr;
    for (int c = 0; c < colNames.size(); ++c) {
        t->setColName(c, colNames[c]);
        const auto &v = cols[static_cast<size_t>(c)];
        for (size_t r = 0; r < v.size(); ++r)
            t->setCell(static_cast<int>(r), c, v[r] * scale);
    }
    return t;
}

// Same partition up to relabelling (k-means restarts may number equal clusters differently).
bool samePartition117(const QVector<int> &a, const QVector<int> &b)
{
    if (a.size() != b.size() || a.isEmpty())
        return false;
    for (int i = 0; i < a.size(); ++i)
        for (int j = i + 1; j < a.size(); ++j)
            if ((a[i] == a[j]) != (b[i] == b[j]))
                return false;
    return true;
}

std::vector<double> scaled117(const std::vector<double> &v, double s)
{
    std::vector<double> out(v);
    for (double &x : out)
        x *= s;
    return out;
}

struct NonInteractiveGuard117
{
    QVariant prevNonInteractive;
    NonInteractiveGuard117() : prevNonInteractive(qApp->property("torigraph_noninteractive"))
    {
        qApp->setProperty("torigraph_noninteractive", true);
    }
    ~NonInteractiveGuard117() { qApp->setProperty("torigraph_noninteractive", prevNonInteractive); }
};

QJsonObject rpc117(PythonAutomationServer &srv, const QString &method, const QJsonObject &params)
{
    static int id = 11700;
    QJsonObject req;
    req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    req[QStringLiteral("id")] = ++id;
    req[QStringLiteral("method")] = method;
    req[QStringLiteral("params")] = params;
    const QJsonObject resp = srv.handleRpcRequest(req);
    QCoreApplication::processEvents();
    if (resp.contains(QStringLiteral("error")))
        std::cout << "[INFO] 117: " << method.toStdString() << " error: "
                  << QJsonDocument(resp.value(QStringLiteral("error")).toObject())
                             .toJson(QJsonDocument::Compact)
                             .toStdString()
                  << "\n";
    return resp;
}

} // namespace

int runSuite117ScaleInvariantStatistics(ApplicationWindow *mw)
{
    std::cout << "[INFO] Suite 117: runSuite117ScaleInvariantStatistics\n";
    g117Failures = 0;
    if (!mw) {
        std::cerr << "[FAIL] Suite 117: no ApplicationWindow\n";
        return 1;
    }
    NonInteractiveGuard117 guard;
    using GS = GraphStatsAndSurfaceFit;
    const std::vector<double> z8 = normalScores117(8);
    const std::vector<double> z30 = normalScores117(30);

    // ---------------------------------------------------------------------------------------
    // 117a Rank tests (mid-rank ties relative to the data scale): Mann-Whitney, Kruskal-Wallis
    // ---------------------------------------------------------------------------------------
    {
        const std::vector<double> a{ 1, 2, 3, 4, 5 }, b{ 6, 7, 8, 9, 10 }, c{ 11, 12, 13, 14, 15 };
        double pMw[2] = { -1, -1 }, uMw[2] = { -1, -1 }, pKw[2] = { -1, -1 }, hKw[2] = { -1, -1 };
        const double scales[2] = { 1.0, 1e-15 };
        for (int s = 0; s < 2; ++s) {
            Table *t = makeTable117(mw, QStringLiteral("Rank117_%1").arg(s),
                                    QStringList() << "A" << "B" << "C", { a, b, c }, scales[s]);
            const auto mwRes = GS::runNonparametricComparison(mw, t, QStringList() << "A" << "B",
                                                              GS::TestMannWhitneyU);
            const auto kwRes = GS::runNonparametricComparison(
                    mw, t, QStringList() << "A" << "B" << "C", GS::TestKruskalWallisDunn);
            pMw[s] = mwRes.valid ? mwRes.pValue : -1.0;
            uMw[s] = mwRes.valid ? mwRes.statistic : -1.0;
            pKw[s] = kwRes.valid ? kwRes.pValue : -1.0;
            hKw[s] = kwRes.valid ? kwRes.statistic : -1.0;
        }
        std::cout << "[INFO] 117a Mann-Whitney p " << pMw[0] << " / (x1e-15) " << pMw[1]
                  << ", Kruskal-Wallis H " << hKw[0] << " / " << hKw[1] << "\n";
        CHECK117(pMw[0] > 0.0 && pMw[0] < 0.05 && same117(pMw[0], pMw[1]) && same117(uMw[0], uMw[1]),
                 "117a: Mann-Whitney must give the same p (~0.012) for fA-scale data, got "
                         << pMw[0] << " vs " << pMw[1]);
        CHECK117(pKw[0] > 0.0 && pKw[0] < 0.05 && same117(pKw[0], pKw[1]) && same117(hKw[0], hKw[1]),
                 "117a: Kruskal-Wallis must be scale invariant, got H " << hKw[0] << " vs " << hKw[1]);
    }

    // ---------------------------------------------------------------------------------------
    // 117b Wilcoxon signed-rank (zero differences relative to the data) + paired plot
    // ---------------------------------------------------------------------------------------
    {
        std::vector<double> before, after;
        const double d[10] = { 0.5, 1.2, -0.3, 2.1, 0.9, 1.7, -0.6, 1.1, 2.4, 0.8 };
        for (int i = 0; i < 10; ++i) {
            before.push_back(1.0 + i);
            after.push_back(1.0 + i + d[i]);
        }
        double pW[2] = { -1, -1 }, wW[2] = { -1, -1 }, pPaired[2] = { -1, -1 };
        const double scales[2] = { 1.0, kTiny117 };
        for (int s = 0; s < 2; ++s) {
            Table *t = makeTable117(mw, QStringLiteral("Wsr117_%1").arg(s),
                                    QStringList() << "Before" << "After", { before, after }, scales[s]);
            const auto r = GS::runNonparametricComparison(
                    mw, t, QStringList() << "Before" << "After", GS::TestWilcoxonSignedRank);
            pW[s] = r.valid ? r.pValue : -1.0;
            wW[s] = r.valid ? r.statistic : -1.0;
            GS::plotPairedComparison(mw, t, QStringLiteral("Before"), QStringLiteral("After"), true,
                                     &pPaired[s]);
        }
        std::cout << "[INFO] 117b Wilcoxon W " << wW[0] << " / " << wW[1] << ", p " << pW[0]
                  << " / " << pW[1] << ", paired plot p " << pPaired[0] << " / " << pPaired[1] << "\n";
        CHECK117(pW[0] > 0.0 && pW[0] < 0.05 && same117(pW[0], pW[1]) && same117(wW[0], wW[1]),
                 "117b: Wilcoxon signed-rank must be scale invariant (1e-16 diffs were dropped as zero)");
        CHECK117(same117(pPaired[0], pPaired[1]) && same117(pPaired[0], pW[0]),
                 "117b: plotPairedComparison(Wilcoxon) p must be scale invariant");
    }

    // ---------------------------------------------------------------------------------------
    // 117c Shapiro-Wilk / normality: small-scale normal data, and constant data
    // ---------------------------------------------------------------------------------------
    {
        std::vector<double> normal, constant;
        for (double z : z30) {
            normal.push_back(10.0 + 0.3 * z);
            constant.push_back(0.1);
        }
        GS::NormalityResult nr[2], cr[2];
        const double scales[2] = { 1.0, kTiny117 };
        for (int s = 0; s < 2; ++s) {
            Table *t = makeTable117(mw, QStringLiteral("Norm117_%1").arg(s),
                                    QStringList() << "N" << "Const", { normal, constant }, scales[s]);
            nr[s] = GS::runNormalityTest(nullptr, t, QStringLiteral("N"), nullptr);
            cr[s] = GS::runNormalityTest(nullptr, t, QStringLiteral("Const"), nullptr);
        }
        std::cout << "[INFO] 117c Shapiro-Wilk W " << nr[0].shapiroWilkW << " / " << nr[1].shapiroWilkW
                  << ", p " << nr[0].shapiroWilkP << " / " << nr[1].shapiroWilkP
                  << "; constant W " << cr[0].shapiroWilkW << " / " << cr[1].shapiroWilkW << "\n";
        CHECK117(nr[0].shapiroWilkW > 0.9 && nr[0].isNormalAt005 && nr[1].isNormalAt005
                         && same117(nr[0].shapiroWilkW, nr[1].shapiroWilkW, 1e-6)
                         && same117(nr[0].shapiroWilkP, nr[1].shapiroWilkP, 1e-6),
                 "117c: Shapiro-Wilk must not reject normal 1e-16-scale data (W " << nr[1].shapiroWilkW
                                                                               << ")");
        CHECK117(cr[0].shapiroWilkW == 0.0 && cr[1].shapiroWilkW == 0.0 && !cr[0].isNormalAt005
                         && !cr[1].isNormalAt005,
                 "117c: constant data (sd = rounding residue) must take the constant-data branch");
    }

    // ---------------------------------------------------------------------------------------
    // 117d One-way ANOVA, auto Welch / ANOVA switch, two-way ANOVA (zero tests vs rounding floor)
    // ---------------------------------------------------------------------------------------
    {
        std::vector<double> g1, g2, g3;
        for (size_t i = 0; i < z8.size(); ++i) {
            g1.push_back(1.0 + 0.1 * z8[i]);
            g2.push_back(2.0 + 0.1 * z8[(i + 3) % z8.size()]);
            g3.push_back(3.0 + 0.1 * z8[(i + 5) % z8.size()]);
        }
        double fAnova[2] = { -1, -1 }, pAnova[2] = { -1, -1 };
        double tWelch[2] = { 0, 0 }, pWelch[2] = { -1, -1 };
        QString welchName[2];
        const double scales[2] = { 1.0, kTiny117 };
        for (int s = 0; s < 2; ++s) {
            Table *t = makeTable117(mw, QStringLiteral("Anova117_%1").arg(s),
                                    QStringList() << "G1" << "G2" << "G3", { g1, g2, g3 }, scales[s]);
            Table *rep = GS::runGroupComparisonAndAnnotate(mw, t,
                                                           QStringList() << "G1" << "G2" << "G3");
            if (rep) {
                fAnova[s] = rep->cell(0, 1);
                pAnova[s] = rep->cell(0, 4);
            }
            const auto w = GS::runNonparametricComparison(mw, t, QStringList() << "G1" << "G2",
                                                          GS::TestAutoSmartSwitch);
            tWelch[s] = w.valid ? w.statistic : 0.0;
            pWelch[s] = w.valid ? w.pValue : -1.0;
            welchName[s] = w.chosenTestName;
        }
        std::cout << "[INFO] 117d one-way F " << fAnova[0] << " / " << fAnova[1] << ", p "
                  << pAnova[0] << " / " << pAnova[1] << "; auto 2-group '"
                  << welchName[0].toStdString() << "' t " << tWelch[0] << " / " << tWelch[1] << "\n";
        CHECK117(fAnova[0] > 100.0 && fAnova[0] < 1e11 && same117(fAnova[0], fAnova[1], 1e-6)
                         && same117(pAnova[0], pAnova[1], 1e-6),
                 "117d: one-way ANOVA F must be the same for 1e-16-scale groups (got "
                         << fAnova[1] << ")");
        CHECK117(welchName[0] == welchName[1] && std::fabs(tWelch[0]) > 5.0
                         && std::fabs(tWelch[0]) < 1e5 && same117(tWelch[0], tWelch[1], 1e-6)
                         && same117(pWelch[0], pWelch[1], 1e-6),
                 "117d: auto-switch two-group test must be scale invariant (t " << tWelch[0] << " vs "
                                                                               << tWelch[1] << ")");

        GS::TwoWayANOVAResult tw[2];
        for (int s = 0; s < 2; ++s) {
            Table *t = mw->newTable(24, 3, QStringLiteral("TwoWay117_%1").arg(s));
            t->setColName(0, "FA");
            t->setColumnType(0, SciDAVis::ColumnMode::Text);
            t->setColName(1, "FB");
            t->setColumnType(1, SciDAVis::ColumnMode::Text);
            t->setColName(2, "Y");
            int row = 0;
            for (int a = 0; a < 2; ++a) {
                for (int b = 0; b < 2; ++b) {
                    const double cellMean = 2.0 + 1.5 * a + 1.2 * b + 2.5 * a * b;
                    for (int rep = 0; rep < 6; ++rep) {
                        t->setText(row, 0, a == 0 ? "Low" : "High");
                        t->setText(row, 1, b == 0 ? "CatA" : "CatB");
                        t->setCell(row, 2, (cellMean + 0.3 * (rep - 2.5)) * scales[s]);
                        ++row;
                    }
                }
            }
            tw[s] = GS::runTwoWayANOVA(mw, t, "FA", "FB", "Y", false);
        }
        std::cout << "[INFO] 117d two-way F_A " << tw[0].fFactorA << " / " << tw[1].fFactorA
                  << ", F_AxB " << tw[0].fInteraction << " / " << tw[1].fInteraction << "\n";
        CHECK117(tw[0].valid && tw[1].valid && tw[0].fFactorA > 10.0 && tw[0].fFactorA < 1e11
                         && same117(tw[0].fFactorA, tw[1].fFactorA, 1e-6)
                         && same117(tw[0].fFactorB, tw[1].fFactorB, 1e-6)
                         && same117(tw[0].fInteraction, tw[1].fInteraction, 1e-6)
                         && same117(tw[0].pInteraction, tw[1].pInteraction, 1e-6),
                 "117d: two-way ANOVA must be scale invariant (F_A " << tw[1].fFactorA << ")");
    }

    // ---------------------------------------------------------------------------------------
    // 117e Passing-Bablok / Deming (identical / vertical pairs and CUSUM residuals relative)
    // ---------------------------------------------------------------------------------------
    {
        std::vector<double> x, y;
        for (int i = 0; i < 20; ++i) {
            const double xi = 1.0 + i + 0.07 * std::sin(1.7 * i);
            x.push_back(xi);
            y.push_back(1.05 * xi + 0.2 + 0.15 * std::sin(2.3 * i + 0.4));
        }
        GS::PassingBablokDemingResult pb[2];
        const double scales[2] = { 1.0, kTiny117 };
        for (int s = 0; s < 2; ++s) {
            Table *t = makeTable117(mw, QStringLiteral("Pb117_%1").arg(s), QStringList() << "X" << "Y",
                                    { x, y }, scales[s]);
            pb[s] = GS::runPassingBablokDeming(mw, t, QStringLiteral("X"), QStringLiteral("Y"), 1.0,
                                               false);
        }
        std::cout << "[INFO] 117e Passing-Bablok slope " << pb[0].pbSlope << " / " << pb[1].pbSlope
                  << ", CUSUM " << pb[0].cusumStat << " / " << pb[1].cusumStat << ", Deming "
                  << pb[0].demingSlope << " / " << pb[1].demingSlope << "\n";
        CHECK117(std::fabs(pb[0].pbSlope - 1.05) < 0.1 && same117(pb[0].pbSlope, pb[1].pbSlope, 1e-6)
                         && same117(pb[0].pbIntercept * kTiny117, pb[1].pbIntercept, 1e-6)
                         && same117(pb[0].cusumStat, pb[1].cusumStat, 1e-6)
                         && same117(pb[0].demingSlope, pb[1].demingSlope, 1e-6),
                 "117e: Passing-Bablok / Deming must be scale invariant (slope " << pb[1].pbSlope
                                                                               << ")");
    }

    // ---------------------------------------------------------------------------------------
    // 117f k-means standardization (a 1e-16-scale feature is standardized, not ignored),
    //      Pearson correlation heatmap
    // ---------------------------------------------------------------------------------------
    {
        std::vector<double> f1, f2;
        for (int i = 0; i < 24; ++i) {
            f1.push_back(std::sin(1.3 * i));              // no cluster structure
            f2.push_back(10.0 * (i % 3) + 0.4 * std::cos(2.1 * i)); // three tight clusters
        }
        GS::KMeansResult km[2];
        double rPearson[2] = { 0, 0 };
        const double f2Scale[2] = { 1.0, kTiny117 };
        // Sums of squares of 1e-18-scale columns are far below the old absolute 1e-30.
        const double corrScale[2] = { 1.0, 1e-18 };
        for (int s = 0; s < 2; ++s) {
            Table *t = makeTable117(mw, QStringLiteral("Km117_%1").arg(s),
                                    QStringList() << "F1" << "F2",
                                    { f1, scaled117(f2, f2Scale[s]) }, 1.0);
            km[s] = GS::runKMeansClustering(mw, t, QStringList() << "F1" << "F2", 3, false);
            Table *tc = makeTable117(mw, QStringLiteral("Corr117_%1").arg(s),
                                     QStringList() << "U" << "V",
                                     { f2, scaled117(f2, 2.0) }, corrScale[s]);
            Matrix *corr = nullptr;
            GS::plotCorrelationMatrixHeatmap(mw, tc, QStringList() << "U" << "V", false, &corr);
            rPearson[s] = corr ? corr->cell(0, 1) : 0.0;
        }
        std::cout << "[INFO] 117f k-means silhouette " << km[0].silhouetteScore << " / "
                  << km[1].silhouetteScore << ", Pearson r " << rPearson[0] << " / " << rPearson[1]
                  << "\n";
        CHECK117(km[0].valid && km[1].valid && samePartition117(km[0].assignments, km[1].assignments)
                         && same117(km[0].silhouetteScore, km[1].silhouetteScore, 1e-6),
                 "117f: k-means must standardize a 1e-16-scale feature like an O(1) one");
        CHECK117(same117(rPearson[0], 1.0, 1e-9) && same117(rPearson[1], 1.0, 1e-9),
                 "117f: Pearson r of 1e-18-scale columns must be 1, got " << rPearson[1]);
    }

    // ---------------------------------------------------------------------------------------
    // 117g TableStatistics: SD / variance of a single valid value are empty, not NaN
    // ---------------------------------------------------------------------------------------
    {
        Table *base = mw->newTable(QStringLiteral("StatOne117"), 1, 2);
        base->setCell(0, 0, 1.0);
        base->setCell(0, 1, 4.25);
        TableStatistics *ts =
                mw->newTableStatistics(base, TableStatistics::StatColumn, QList<int>() << 1);
        CHECK117(ts != nullptr && ts->numRows() == 1, "117g: TableStatistics not created");
        if (ts) {
            ts->update(base, base->colName(1));
            const bool sdInvalid = ts->column(3)->isInvalid(0);
            const bool varInvalid = ts->column(4)->isInvalid(0);
            std::cout << "[INFO] 117g n=1: mean " << ts->cell(0, 2) << ", SD invalid " << sdInvalid
                      << ", Var invalid " << varInvalid << "\n";
            CHECK117(sdInvalid && varInvalid && ts->cell(0, 2) == 4.25,
                     "117g: SD / variance of one value must be an empty cell, not NaN");
        }
    }

    // ---------------------------------------------------------------------------------------
    // 117h JSON-RPC: curvemath.integrate on 5e-16 X steps; graph.add_error_band alpha clamp
    // ---------------------------------------------------------------------------------------
    {
        PythonAutomationServer srv(mw);
        std::vector<double> xs, ys, es;
        for (int i = 0; i <= 20; ++i) {
            xs.push_back(5e-16 * i);
            ys.push_back(2.0);
            es.push_back(0.1);
        }
        Table *t = makeTable117(mw, QStringLiteral("Integ117"), QStringList() << "X" << "Y" << "E",
                                { xs, ys, es }, 1.0);
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Line);
        CHECK117(ml != nullptr, "117h: plot not created");
        if (ml) {
            const QJsonObject resp = rpc117(
                    srv, QStringLiteral("curvemath.integrate"),
                    QJsonObject{ { "graph", ml->name() }, { "curve", 0 }, { "plot_curve", false } });
            const double area = resp.value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("area"))
                                        .toDouble(-1.0);
            std::cout << "[INFO] 117h integrate over 1e-14 span: area " << area << "\n";
            CHECK117(!resp.contains(QStringLiteral("error")) && same117(area, 2e-14, 1e-9),
                     "117h: curvemath.integrate must accept 5e-16 X steps (area " << area << ")");
        }

        auto bandAlphaFor = [&](const QJsonValue &alpha, bool &okOut) -> int {
            MultiLayer *mlB = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Line);
            okOut = false;
            if (!mlB)
                return -1;
            const QJsonObject resp = rpc117(srv, QStringLiteral("graph.add_error_band"),
                                            QJsonObject{ { "graph", mlB->name() },
                                                         { "curve", 0 },
                                                         { "err", "E" },
                                                         { "alpha", alpha } });
            okOut = !resp.contains(QStringLiteral("error"));
            Graph *g = mlB->activeGraph();
            for (int i = 0; g && i < g->curves(); ++i)
                if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(i)))
                    if (er->isBand())
                        return er->bandAlpha();
            return -1;
        };
        bool okHuge = false, okNeg = false, okFrac = false;
        const int aHuge = bandAlphaFor(1e10, okHuge);
        const int aNeg = bandAlphaFor(-3.0, okNeg);
        const int aFrac = bandAlphaFor(0.5, okFrac);
        std::cout << "[INFO] 117h add_error_band alpha 1e10 -> " << aHuge << ", -3 -> " << aNeg
                  << ", 0.5 -> " << aFrac << "\n";
        CHECK117(okHuge && aHuge == 255 && okNeg && aNeg == 0 && okFrac && aFrac == 128,
                 "117h: add_error_band alpha must clamp in double (1e10 -> 255, -3 -> 0, 0.5 -> 128)");
    }

    // ---------------------------------------------------------------------------------------
    // 117i Gadgets / filters: Quick Peaks on a pA trace, LOWESS robustness on fA data,
    //      Gaussian2D with a huge sigma
    // ---------------------------------------------------------------------------------------
    {
        std::vector<double> xs, ys;
        for (int i = 0; i < 200; ++i) {
            const double x = i;
            xs.push_back(x);
            ys.push_back(0.8 * std::exp(-std::pow((x - 60.0) / 5.0, 2.0))
                         + 0.6 * std::exp(-std::pow((x - 140.0) / 5.0, 2.0)));
        }
        double count[2] = { -1, -1 }, center[2] = { -1, -1 };
        const double scales[2] = { 1.0, 1e-12 };
        for (int s = 0; s < 2; ++s) {
            Table *t = makeTable117(mw, QStringLiteral("Peaks117_%1").arg(s),
                                    QStringList() << "X" << "Y", { xs, scaled117(ys, scales[s]) }, 1.0);
            MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Line);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            RoiGadgetTool *roi =
                    g ? RoiGadgetTool::attachGadget(mw, g, RoiGadgetTool::QuickPeaks) : nullptr;
            if (roi) {
                roi->setTargetCurve(t->name() + "_Y");
                roi->setRoiRect(QRectF(QPointF(-5.0, -0.1 * scales[s]), QPointF(205.0, 1.0 * scales[s])));
                roi->recalculate();
                count[s] = roi->primaryMetric();
                center[s] = roi->secondaryMetric();
                RoiGadgetTool::clearGadget(g);
            }
        }
        std::cout << "[INFO] 117i Quick Peaks count " << count[0] << " / (pA) " << count[1]
                  << ", main peak x " << center[0] << " / " << center[1] << "\n";
        CHECK117(count[0] == 2.0 && count[1] == 2.0 && center[0] == 60.0 && center[1] == 60.0,
                 "117i: Quick Peaks must find the same 2 peaks on a pA-scale trace");

        // LOWESS: one outlier must be down-weighted by the robustness iterations at any scale.
        std::vector<double> ly;
        for (int i = 0; i < 60; ++i)
            ly.push_back(std::sin(i / 6.0) + 0.02 * std::cos(3.7 * i) + (i == 30 ? 5.0 : 0.0));
        std::vector<double> smooth[2];
        const double lScales[2] = { 1.0, kTiny117 };
        for (int s = 0; s < 2; ++s) {
            std::vector<double> lx;
            for (int i = 0; i < 60; ++i)
                lx.push_back(i);
            Table *t = makeTable117(mw, QStringLiteral("Lowess117_%1").arg(s),
                                    QStringList() << "X" << "Y", { lx, scaled117(ly, lScales[s]) }, 1.0);
            MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Line);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            if (!g)
                continue;
            const int before = g->curves();
            SmoothFilter sf(mw, g, t->name() + "_Y", SmoothFilter::LOWESS);
            sf.setLowessSpan(0.3);
            sf.run();
            if (g->curves() == before + 1) {
                QwtPlotCurve *c = g->curve(g->curves() - 1);
                for (int i = 0; i < static_cast<int>(c->dataSize()); ++i)
                    smooth[s].push_back(c->y(i) / lScales[s]);
            }
        }
        double maxDiff =
                (smooth[0].size() > 30 && smooth[0].size() == smooth[1].size()) ? 0.0 : 1e9;
        for (size_t i = 0; maxDiff < 1e9 && i < smooth[0].size(); ++i)
            maxDiff = std::max(maxDiff, std::fabs(smooth[0][i] - smooth[1][i]));
        std::cout << "[INFO] 117i LOWESS max |y(1) - y(1e-16)/1e-16| = " << maxDiff
                  << ", y(30) = " << (smooth[0].size() > 30 ? smooth[0][30] : -1.0) << "\n";
        CHECK117(maxDiff < 1e-6 && smooth[0].size() > 30 && std::fabs(smooth[0][30]) < 1.5,
                 "117i: LOWESS robustness iterations must also run on 1e-16-scale data");

        Matrix *m = mw->newMatrix(5, 5);
        for (int r = 0; r < 5; ++r)
            for (int c = 0; c < 5; ++c)
                m->setCell(r, c, (r == 2 && c == 2) ? 10.0 : 1.0);
        Matrix *mg = AdvancedSignalProc::filterMatrix2D(mw, m, AdvancedSignalProc::FilterGaussian2D,
                                                        1e300, 1, false, false);
        CHECK117(mg != nullptr && std::isfinite(mg->cell(2, 2)),
                 "117i: Gaussian2D with a huge sigma must clamp the radius before converting to int");
    }

    if (g117Failures == 0)
        std::cout << "[PASS] Suite 117\n";
    else
        std::cerr << "[FAIL] Suite 117: " << g117Failures << " check(s) failed\n";
    return g117Failures == 0 ? 0 : 1;
}
