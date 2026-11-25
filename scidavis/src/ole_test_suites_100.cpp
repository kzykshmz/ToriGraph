#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

// =============================================================================
// Suite 100: numerical analysis regressions found by the second data-driven round.
//   100a curvemath.xrd_analyze on Al patterns with counting noise (background 300-800):
//        the half-maximum search stopped at the first noise wiggle, the width fell back to
//        2 steps and the profile refinement could not leave 0.72-1.28 of it, so (220)-(222)
//        came out 0.02-0.05 deg wide and W-H gave 10-318 nm with no microstrain. Now every
//        width is within 15 % of the truth and W-H is near D = 40 nm, eps = 0.0015.
//   100b a noise-free pattern with ~40 reflections weaker than 3.5 % of (111) was taken as
//        noisy (their extrema counted as noise) and lost the weak peak at 49.8 deg; it is
//        noise-free again (no zigzags), and the result keeps that peak.
//   100c curvemath.lookup_xy(x=...) on a curve whose X goes out and back returns the value
//        of every branch (y_values, y null when not unique) with a notice; monotonic curves
//        are unchanged; rows in no order are still sorted, with a notice.
//   100d one-way ANOVA and Kruskal-Wallis keep a group of one value (A(5), B(5), C(1):
//        F = 93.34, p = 2.85e-6, df (2, 8); H = 6.164, p = 0.0459); all groups of one value
//        is an explicit error; Mann-Whitney with a group of one value is computed (p = 0.2416
//        with continuity correction); a test that cannot run says why.
//   100e curvemath.kissinger with spectrum columns (y_cols): the peak temperature of a noisy
//        spectrum is the vertex of a least-squares parabola, not the largest sample (2 % noise
//        at a 1 K step gave Ea = 91 kJ/mol for 100); noise-free spectra keep the largest
//        sample.
//   100f signal.smooth returns the X-spacing notice (unequal / non-monotonic X) in the result
//        (it only went to the results log), also for the 'gaussian' kernel.
//   100g undefined statistics are null: Weibull beta_se / eta_se without residual degrees of
//        freedom (n = 2, 3-parameter n = 3), and the correlation of a constant column.
//   100h XRD with counting noise and Cu Ka2 not stripped (the default): Ka2 was taken into the
//        single-profile least-squares window and the widths came out 0.75-1.8 times the Ka1
//        width. The doublet is now fitted: widths within 10 % of Ka1, W-H near the truth.
//   100i Welch / Games-Howell between two groups of one value: the pooled variance was counted
//        as two independent terms (df 2 (N - k) = 8, p 0.217); now df N - k = 4, p 0.2508.
//   100j Kissinger spectra with a narrow and a broad peak and no noise (2 K step, 50-60
//        points): the extrema were taken for noise and the moving average moved the peak from
//        430 K to the broad one at 470 K; now the largest sample (and with noise, near it).
//   100k lookup_xy on an almost monotonic X with noise (x = 0.01 i + N(0, 0.005), 1000 points)
//        was taken for branches (~2 % of x gave y null); now sorted and one value.
// Noise comes from a fixed splitmix64 + Box-Muller generator (identical on every platform).
// =============================================================================
namespace {

struct SplitMix100
{
    uint64_t s;
    explicit SplitMix100(uint64_t seed) : s(seed) {}
    uint64_t next()
    {
        s += 0x9E3779B97F4A7C15ULL;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uniform() { return double((next() >> 11) + 1) * (1.0 / 9007199254740992.0); }
    double gauss()
    {
        const double u1 = uniform();
        const double u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    }
};

const double kLam100 = 1.540598;
const double kA100 = 4.0495;
const int kHkl100[5][3] = { { 1, 1, 1 }, { 2, 0, 0 }, { 2, 2, 0 }, { 3, 1, 1 }, { 2, 2, 2 } };

// Al, Gaussian profiles of D = 40 nm, eps = 0.0015, peak height scale / (h^2 + k^2 + l^2) on a
// flat background, 30..85 deg; counting-like noise round(I + sqrt(I) z).
void alPattern100(double bg, double scale, double step, uint64_t seed, std::vector<double> &tt,
                  std::vector<double> &y, double truthCentre[5], double truthFwhm[5])
{
    tt.clear();
    for (int i = 0;; ++i) {
        const double v = 30.0 + step * i;
        if (v >= 85.0 - 1e-9)
            break;
        tt.push_back(v);
    }
    y.assign(tt.size(), bg);
    for (int r = 0; r < 5; ++r) {
        const int m = kHkl100[r][0] * kHkl100[r][0] + kHkl100[r][1] * kHkl100[r][1]
                + kHkl100[r][2] * kHkl100[r][2];
        const double d = kA100 / std::sqrt(double(m));
        const double th = std::asin(kLam100 / (2.0 * d));
        const double fw = (0.9 * kLam100 / (40.0 * 10.0 * std::cos(th)) + 4.0 * 0.0015 * std::tan(th))
                * 180.0 / M_PI;
        const double c = 2.0 * th * 180.0 / M_PI;
        truthCentre[r] = c;
        truthFwhm[r] = fw;
        const double sig = fw / 2.3548200450309493;
        for (size_t i = 0; i < tt.size(); ++i) {
            const double u = (tt[i] - c) / sig;
            y[i] += scale / m * std::exp(-0.5 * u * u);
        }
    }
    SplitMix100 rng(seed);
    for (double &v : y)
        v = std::max(0.0, std::round(v + std::sqrt(v) * rng.gauss()));
}

// Al with Cu Ka1 + Ka2 (Ka2 at lambda 1.544426, half the height, same width) on a flat
// background, Gaussian profiles of crystallite size dNm and microstrain eps, 30..85 deg, the
// same counting-like noise. truthFwhm is the Ka1 width.
void alPatternKa2_100(double bg, double scale, double step, uint64_t seed, double dNm, double eps,
                      std::vector<double> &tt, std::vector<double> &y, double truthCentre[5],
                      double truthFwhm[5])
{
    tt.clear();
    for (int i = 0;; ++i) {
        const double v = 30.0 + step * i;
        if (v >= 85.0 - 1e-9)
            break;
        tt.push_back(v);
    }
    y.assign(tt.size(), bg);
    for (int r = 0; r < 5; ++r) {
        const int m = kHkl100[r][0] * kHkl100[r][0] + kHkl100[r][1] * kHkl100[r][1]
                + kHkl100[r][2] * kHkl100[r][2];
        const double d = kA100 / std::sqrt(double(m));
        const double th = std::asin(kLam100 / (2.0 * d));
        const double th2 = std::asin(1.544426 / (2.0 * d));
        const double fw = (0.9 * kLam100 / (dNm * 10.0 * std::cos(th)) + 4.0 * eps * std::tan(th))
                * 180.0 / M_PI;
        const double c = 2.0 * th * 180.0 / M_PI;
        const double c2 = 2.0 * th2 * 180.0 / M_PI;
        truthCentre[r] = c;
        truthFwhm[r] = fw;
        const double sig = fw / 2.3548200450309493;
        for (size_t i = 0; i < tt.size(); ++i) {
            const double u = (tt[i] - c) / sig;
            const double u2 = (tt[i] - c2) / sig;
            y[i] += scale / m * (std::exp(-0.5 * u * u) + 0.5 * std::exp(-0.5 * u2 * u2));
        }
    }
    SplitMix100 rng(seed);
    for (double &v : y)
        v = std::max(0.0, std::round(v + std::sqrt(v) * rng.gauss()));
}

double pv100(double x, double c, double fwhm, double eta)
{
    const double u = (x - c) / (0.5 * fwhm);
    return eta / (1.0 + u * u) + (1.0 - eta) * std::exp(-std::log(2.0) * u * u);
}

// First-order desorption rate (Polanyi-Wigner, theta0 = 1) at the temperatures T for a heating
// rate beta (K/s), Ea = 100 kJ/mol, A = 1e10 /s, normalised to a maximum of 1 on a fine grid.
// *tPeak gets the temperature of the maximum of the fine grid.
std::vector<double> tds100(const std::vector<double> &T, double beta, double *tPeak)
{
    const double R = 8.314462618, Ea = 100e3, A = 1e10;
    const double t0 = T.front(), t1 = T.back();
    const int nf = 400001;
    const double h = (t1 - t0) / (nf - 1);
    std::vector<double> rate(static_cast<size_t>(nf), 0.0);
    double integ = 0.0, kPrev = 0.0, best = -1.0;
    for (int i = 0; i < nf; ++i) {
        const double t = t0 + h * i;
        const double k = A * std::exp(-Ea / (R * t)) / beta;
        if (i > 0)
            integ += 0.5 * (k + kPrev) * h;
        kPrev = k;
        rate[size_t(i)] = k * std::exp(-integ);
        if (rate[size_t(i)] > best) {
            best = rate[size_t(i)];
            *tPeak = t;
        }
    }
    std::vector<double> out;
    for (double t : T) {
        const double pos = (t - t0) / h;
        const int i0 = std::clamp(int(std::floor(pos)), 0, nf - 2);
        const double f = pos - i0;
        out.push_back(((1.0 - f) * rate[size_t(i0)] + f * rate[size_t(i0 + 1)]) / best);
    }
    return out;
}

} // namespace

int runSuite100NumericsRound2(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 100: numerics round 2 (XRD widths, lookup branches, "
                 "singleton groups, Kissinger spectra, smooth notice, undefined SE)...\n";

    struct NonInteractiveQuitGuard100
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard100()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard100()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard100;

    struct AppWinGuard100
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard100(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard100() { reset(); }
        AppWinGuard100(const AppWinGuard100 &) = delete;
        AppWinGuard100 &operator=(const AppWinGuard100 &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard100 w(new ApplicationWindow());
    w->applyUserSettings();
    w->confirmCloseTable = false;
    w->confirmClosePlot2D = false;
    w->confirmClosePlot3D = false;
    w->confirmCloseFolder = false;
    w->resize(1280, 800);
    w->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "100: pythonAutomationServer null");

    int rpcId = 10000;
    auto rpc = [&](const QString &method, const QJsonObject &params) -> QJsonObject {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = ++rpcId;
        req[QStringLiteral("method")] = method;
        req[QStringLiteral("params")] = params;
        const QJsonObject resp = srv->handleRpcRequest(req);
        QCoreApplication::processEvents();
        return resp;
    };
    auto errMsg = [](const QJsonObject &resp) {
        return resp.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
    };
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto num = [&](const QJsonValue &v) { return v.isDouble() ? v.toDouble() : nan; };

    // Table with the given columns (NaN = empty cell).
    int tableNo = 0;
    auto makeTable = [&](const QStringList &names,
                         const std::vector<std::vector<double>> &cols) -> QString {
        size_t rows = 0;
        for (const auto &c : cols)
            rows = std::max(rows, c.size());
        const QString tName = QStringLiteral("T100_%1").arg(++tableNo);
        Table *t = w->newTable(int(rows), int(cols.size()), tName, tName);
        if (!t)
            return QString();
        for (int c = 0; c < int(cols.size()); ++c) {
            t->setColName(c, names.value(c));
            for (int r = 0; r < int(cols[size_t(c)].size()); ++r) {
                const double v = cols[size_t(c)][size_t(r)];
                if (std::isfinite(v))
                    t->setCell(r, c, v);
            }
        }
        return t->name();
    };
    // Two-column table plotted as a line; returns the graph window name ("" on failure).
    auto plotXY = [&](const std::vector<double> &x, const std::vector<double> &y) -> QString {
        const QString tName = makeTable(QStringList() << "X" << "Y", { x, y });
        Table *t = w->table(tName);
        if (!t)
            return QString();
        MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
        QCoreApplication::processEvents();
        return (ml && ml->activeGraph() && ml->activeGraph()->curves() == 1) ? ml->name()
                                                                              : QString();
    };
    auto xrd = [&](const std::vector<double> &tt, const std::vector<double> &y) -> QJsonObject {
        const QString g = plotXY(tt, y);
        if (g.isEmpty())
            return QJsonObject();
        return rpc(QStringLiteral("curvemath.xrd_analyze"),
                   QJsonObject{ { "graph", g },
                                { "curve", 0 },
                                { "wavelength", kLam100 },
                                { "instr_fwhm", 0.0 },
                                { "crystal_system", "fcc" },
                                { "create_wh_plot", false },
                                { "add_top_d_axis", false } })
                .value(QStringLiteral("result"))
                .toObject();
    };

    // -------------------------------------------------------------------------
    // 100a: Al with counting noise, widths and W-H
    // -------------------------------------------------------------------------
    {
        // ref: FWHM of a Poisson-weighted least-squares Gauss + constant fit to each
        // reflection (+-2.5 FWHM) of the same data (scipy-free Levenberg-Marquardt, Python).
        struct XCase
        {
            double bg, scale, step;
            uint64_t seed;
            double ref[5];
        };
        const XCase xcases[] = {
            { 300, 3000, 0.02, 1, { 0.3310, 0.3582, 0.4582, 0.5340, 0.5378 } },
            { 300, 3000, 0.02, 2, { 0.3279, 0.3528, 0.4454, 0.5450, 0.5818 } },
            { 500, 2000, 0.02, 3, { 0.3249, 0.3548, 0.4462, 0.4905, 0.4875 } },
            { 500, 2000, 0.02, 4, { 0.3286, 0.3560, 0.4529, 0.4936, 0.5522 } },
            { 300, 1500, 0.01, 5, { 0.3286, 0.3527, 0.4619, 0.5350, 0.5845 } },
            { 300, 1500, 0.01, 6, { 0.3295, 0.3534, 0.4457, 0.5177, 0.5755 } },
            { 800, 3000, 0.02, 7, { 0.3311, 0.3497, 0.4776, 0.5315, 0.5794 } },
            { 800, 3000, 0.02, 8, { 0.3372, 0.3667, 0.4614, 0.4954, 0.5646 } },
        };
        double sumD = 0.0, sumEps = 0.0;
        for (const XCase &xc : xcases) {
            std::vector<double> tt, y;
            double cTrue[5], wTrue[5];
            alPattern100(xc.bg, xc.scale, xc.step, 1000 + xc.seed, tt, y, cTrue, wTrue);
            const QJsonObject res = xrd(tt, y);
            const QJsonArray peaks = res.value(QStringLiteral("peaks")).toArray();
            const double whD = num(res.value(QStringLiteral("wh_size_nm")));
            const double whEps = num(res.value(QStringLiteral("wh_microstrain")));
            std::cout << "[INFO] 100a bg " << xc.bg << " scale " << xc.scale << " step " << xc.step
                      << " seed " << xc.seed << ": W-H D=" << whD << " eps=" << whEps << " FWHM:";
            for (const QJsonValue &v : peaks)
                std::cout << " " << v.toObject().value("fwhm_deg").toDouble();
            std::cout << "\n";
            CHECK_COND(peaks.size() == 5, "100a: expected the 5 Al reflections, got " << peaks.size());
            for (int r = 0; r < 5; ++r) {
                const QJsonObject o = peaks.at(r).toObject();
                const double t = o.value(QStringLiteral("two_theta_deg")).toDouble();
                const double fw = o.value(QStringLiteral("fwhm_deg")).toDouble();
                CHECK_COND(std::fabs(t - cTrue[r]) < 0.03,
                           "100a: peak " << r << " at " << t << ", expected " << cTrue[r]);
                CHECK_COND(std::fabs(fw / xc.ref[r] - 1.0) < 0.10 && std::fabs(fw / wTrue[r] - 1.0) < 0.20,
                           "100a: bg " << xc.bg << " seed " << xc.seed << " peak " << r
                                       << " FWHM " << fw << " deg, least-squares reference "
                                       << xc.ref[r] << ", truth " << wTrue[r]
                                       << " (collapsed to a few steps before)");
            }
            CHECK_COND(std::isfinite(whD) && std::fabs(whD / 40.0 - 1.0) < 0.30,
                       "100a: W-H size " << whD << " nm, truth 40 nm");
            CHECK_COND(std::isfinite(whEps) && std::fabs(whEps / 0.0015 - 1.0) < 0.60,
                       "100a: W-H microstrain " << whEps << ", truth 0.0015 (null before)");
            sumD += whD;
            sumEps += whEps;
        }
        const double meanD = sumD / 8.0, meanEps = sumEps / 8.0;
        std::cout << "[INFO] 100a mean W-H D=" << meanD << " eps=" << meanEps << "\n";
        CHECK_COND(std::fabs(meanD / 40.0 - 1.0) < 0.08 && std::fabs(meanEps / 0.0015 - 1.0) < 0.12,
                   "100a: mean W-H over 8 patterns D=" << meanD << " eps=" << meanEps);
    }

    // -------------------------------------------------------------------------
    // 100b: noise-free pattern crowded with weak reflections (0.1 deg step)
    // -------------------------------------------------------------------------
    {
        // Al (111)..(222) at 100/47/22/24/7, 38 reflections of 1-3 % and 6 of 5-12 % (% of
        // 10000 counts), pseudo-Voigt eta 0.5, FWHM 0.15 deg, background 50.
        static const double kPk[][2] = {
            { 38.47, 100 },        { 44.74, 47 },         { 65.13, 22 },
            { 78.23, 24 },         { 82.44, 7 },          { 33.343209, 1.539684 },
            { 38.121628, 2.078438 }, { 57.927397, 2.366587 }, { 86.937653, 2.522648 },
            { 39.050360, 1.310612 }, { 49.885101, 2.732786 }, { 42.119049, 1.208377 },
            { 49.655313, 2.406984 }, { 34.204369, 2.617366 }, { 67.883061, 1.041436 },
            { 34.977681, 1.740048 }, { 87.555823, 1.946021 }, { 39.772577, 2.317253 },
            { 48.121084, 1.503831 }, { 34.003939, 2.737947 }, { 72.357556, 2.981427 },
            { 54.158458, 1.233794 }, { 61.080510, 2.933970 }, { 71.072529, 1.744555 },
            { 33.537163, 2.138548 }, { 46.105358, 1.804365 }, { 55.298095, 2.338109 },
            { 36.390673, 1.437751 }, { 35.779886, 2.558873 }, { 31.261229, 1.749406 },
            { 71.451248, 2.078749 }, { 55.694645, 2.950669 }, { 82.369478, 1.267244 },
            { 39.172002, 1.994254 }, { 53.676576, 2.862372 }, { 87.851894, 2.582029 },
            { 42.350514, 2.993616 }, { 49.774250, 2.403527 }, { 88.632255, 2.492976 },
            { 82.552954, 1.846380 }, { 82.309823, 1.550339 }, { 45.431592, 2.182707 },
            { 83.396567, 1.944495 }, { 49.423343, 11.856584 }, { 57.423423, 8.690152 },
            { 76.463731, 6.650709 }, { 87.041326, 7.856642 }, { 61.412333, 6.093850 },
            { 66.539753, 7.172536 },
        };
        std::vector<double> tt, y;
        for (int i = 0; i <= 600; ++i)
            tt.push_back(30.0 + 0.1 * i);
        y.assign(tt.size(), 50.0);
        for (const auto &p : kPk) {
            for (size_t i = 0; i < tt.size(); ++i)
                y[i] += 10000.0 * p[1] / 100.0 * pv100(tt[i], p[0], 0.15, 0.5);
        }
        const QJsonObject res = xrd(tt, y);
        const QJsonArray peaks = res.value(QStringLiteral("peaks")).toArray();
        std::cout << "[INFO] 100b crowded noise-free: " << peaks.size() << " peaks:";
        bool has498 = false;
        for (const QJsonValue &v : peaks) {
            const double t = v.toObject().value("two_theta_deg").toDouble();
            std::cout << " " << t;
            has498 = has498 || std::fabs(t - 49.8) < 0.08;
        }
        std::cout << "\n";
        CHECK_COND(has498 && peaks.size() == 12,
                   "100b: the noise-free crowded pattern must keep the 12 peaks including the "
                   "weak one at 49.8 deg (got "
                           << peaks.size() << ", 49.8 " << (has498 ? "found" : "missing") << ")");
    }

    // -------------------------------------------------------------------------
    // 100c: lookup_xy on curves whose X goes out and back
    // -------------------------------------------------------------------------
    {
        // Loading y = 2x (x = 0..10, 101 points), unloading y = 0.2 x^2 on the same grid.
        std::vector<double> x, y;
        for (int i = 0; i <= 100; ++i) {
            x.push_back(0.1 * i);
            y.push_back(0.2 * i);
        }
        for (int i = 99; i >= 0; --i) {
            x.push_back(0.1 * i);
            y.push_back(0.2 * (0.1 * i) * (0.1 * i));
        }
        const QString g = plotXY(x, y);
        CHECK_COND(!g.isEmpty(), "100c: plot setup failed");
        auto lookup = [&](const QString &graph, double xq) {
            return rpc(QStringLiteral("curvemath.lookup_xy"),
                       QJsonObject{ { "graph", graph }, { "curve", 0 }, { "x", xq } })
                    .value(QStringLiteral("result"))
                    .toObject();
        };
        const QJsonObject r5 = lookup(g, 5.0);
        const QJsonArray yv = r5.value(QStringLiteral("y_values")).toArray();
        std::cout << "[INFO] 100c loop x=5: y_values=" << yv.size() << " notice: "
                  << r5.value("notice").toString().toStdString() << "\n";
        CHECK_COND(r5.value("ok").toBool() && r5.value("y").isNull() && yv.size() == 2
                           && std::fabs(yv.at(0).toDouble() - 10.0) < 1e-9
                           && std::fabs(yv.at(1).toDouble() - 5.0) < 1e-9
                           && r5.value("notice").toString().contains(QLatin1String("turns back")),
                   "100c: loop at x = 5 must give both branches [10, 5] with y null and a notice "
                   "(it returned the average of the branches)");
        // The turning point is one value; x beyond the data is none.
        const QJsonObject r10 = lookup(g, 10.0);
        CHECK_COND(r10.value("ok").toBool()
                           && std::fabs(r10.value("y").toDouble(nan) - 20.0) < 1e-9
                           && r10.value("y_values").toArray().size() == 1,
                   "100c: the turning point x = 10 has one value (20)");
        CHECK_COND(!lookup(g, 11.0).value("ok").toBool(), "100c: x = 11 is outside the loop");

        // Return branch on its own grid (x = 9.95..0.05): x = 5.02 -> 10.04 and 0.2 * 5.02^2.
        std::vector<double> xo, yo;
        for (int i = 0; i <= 10; ++i) {
            xo.push_back(i);
            yo.push_back(2.0 * i);
        }
        for (int i = 0; i < 100; ++i) {
            const double xv = 9.95 - 0.1 * i;
            xo.push_back(xv);
            yo.push_back(0.2 * xv * xv);
        }
        const QString g2 = plotXY(xo, yo);
        const QJsonObject r502 = lookup(g2, 5.02);
        const QJsonArray yv2 = r502.value(QStringLiteral("y_values")).toArray();
        CHECK_COND(yv2.size() == 2 && std::fabs(yv2.at(0).toDouble() - 10.04) < 1e-9
                           && std::fabs(yv2.at(1).toDouble() - 0.2 * 5.02 * 5.02) < 2e-3,
                   "100c: offset loop at x = 5.02 must give [10.04, 5.04]");

        // Monotonic: unchanged (one value, no y_values, no notice).
        const QString g3 = plotXY({ 0, 1, 2, 3, 4 }, { 0, 1, 4, 9, 16 });
        const QJsonObject rm = lookup(g3, 2.5);
        CHECK_COND(rm.value("ok").toBool() && std::fabs(rm.value("y").toDouble(nan) - 6.5) < 1e-12
                           && !rm.contains("y_values") && !rm.contains("notice"),
                   "100c: a monotonic curve must keep its plain result");
        // Rows in no order (many changes of direction): sorted as before, with a notice.
        const QString g4 = plotXY({ 3, 0, 4, 1, 2, 6, 5 }, { 9, 0, 16, 1, 4, 36, 25 });
        const QJsonObject rs = lookup(g4, 2.5);
        CHECK_COND(rs.value("ok").toBool() && std::fabs(rs.value("y").toDouble(nan) - 6.5) < 1e-12
                           && rs.value("notice").toString().contains(QLatin1String("sorted")),
                   "100c: shuffled rows are interpolated in X order, with a notice");
    }

    // -------------------------------------------------------------------------
    // 100d: groups of one value in ANOVA / Kruskal-Wallis / Mann-Whitney
    // -------------------------------------------------------------------------
    {
        const QString t = makeTable(QStringList() << "A" << "B" << "C",
                                    { { 10.1, 9.8, 10.3, 10.0, 9.9 },
                                      { 10.4, 10.2, 10.6, 10.1, 10.5 },
                                      { 13.0, nan, nan, nan, nan } });
        for (const char *ph : { "holm", "tukey", "games_howell", "dunnett" }) {
            const QJsonObject resp = rpc(QStringLiteral("stats.anova_oneway"),
                                         QJsonObject{ { "table", t },
                                                      { "cols", QJsonArray{ "A", "B", "C" } },
                                                      { "posthoc", ph } });
            CHECK_COND(!resp.contains("error"), "100d: ANOVA (" << ph << ") failed: "
                                                                 << errMsg(resp).toStdString());
            const QJsonObject r = resp.value("result").toObject();
            const double F = num(r.value("f_statistic")), p = num(r.value("p_value"));
            std::cout << "[INFO] 100d ANOVA " << ph << ": F=" << F << " p=" << p << " df ("
                      << r.value("df_between").toInt() << ", " << r.value("df_within").toInt()
                      << ") notice: " << r.value("notice").toString().toStdString() << "\n";
            CHECK_COND(std::fabs(F - 93.3409090909) < 1e-6 && std::fabs(p / 2.8514014996e-6 - 1.0) < 1e-4
                               && r.value("df_between").toInt() == 2
                               && r.value("df_within").toInt() == 8,
                       "100d: ANOVA with C(1) must give F = 93.34, p = 2.85e-6, df (2, 8)");
            const QJsonArray comps = r.value("comparisons").toArray();
            CHECK_COND(comps.size() == (QLatin1String(ph) == QLatin1String("dunnett") ? 2 : 3),
                       "100d: " << ph << " comparisons " << comps.size());
            for (const QJsonValue &c : comps) {
                CHECK_COND(c.toObject().value("p_adj").isDouble(),
                           "100d: " << ph << " p_adj not finite for "
                                    << c.toObject().value("group_a").toString().toStdString()
                                    << " vs " << c.toObject().value("group_b").toString().toStdString());
            }
            CHECK_COND(r.value("notice").toString().contains(QLatin1String("C")),
                       "100d: the single-value group must be named in a notice");
            // The report table has the same omnibus test.
            const QJsonArray rep = r.value("report").toObject().value("data").toArray();
            CHECK_COND(!rep.isEmpty()
                               && std::fabs(rep.at(0).toObject().value("Statistic").toDouble() - F)
                                       < 1e-6 * F,
                       "100d: report table omnibus F differs from the result");
        }
        const QString tOnes = makeTable(QStringList() << "P" << "Q", { { 1.0 }, { 2.0 } });
        const QJsonObject eResp = rpc(QStringLiteral("stats.anova_oneway"),
                                      QJsonObject{ { "table", tOnes },
                                                   { "cols", QJsonArray{ "P", "Q" } } });
        CHECK_COND(eResp.contains("error") && errMsg(eResp).contains(QLatin1String("N - k = 0")),
                   "100d: all groups of one value must be an explicit error (got '"
                           << errMsg(eResp).toStdString() << "')");

        const QJsonObject kw = rpc(QStringLiteral("stats.nonparametric"),
                                   QJsonObject{ { "table", t },
                                                { "cols", QJsonArray{ "A", "B", "C" } },
                                                { "test", "kruskal_wallis" } })
                                       .value("result")
                                       .toObject();
        std::cout << "[INFO] 100d KW: H=" << num(kw.value("statistic"))
                  << " p=" << num(kw.value("p_value")) << "\n";
        CHECK_COND(kw.value("ok").toBool()
                           && std::fabs(num(kw.value("statistic")) - 6.164383561643839) < 1e-9
                           && std::fabs(num(kw.value("p_value")) - 0.045858634343033104) < 1e-9,
                   "100d: Kruskal-Wallis with C(1) must give H = 6.164, p = 0.0459");
        const QJsonObject mwr = rpc(QStringLiteral("stats.nonparametric"),
                                    QJsonObject{ { "table", t },
                                                 { "cols", QJsonArray{ "A", "C" } },
                                                 { "test", "mann_whitney" } })
                                        .value("result")
                                        .toObject();
        std::cout << "[INFO] 100d MW A vs C: ok=" << mwr.value("ok").toBool()
                  << " U=" << num(mwr.value("statistic")) << " p=" << num(mwr.value("p_value"))
                  << "\n";
        CHECK_COND(mwr.value("ok").toBool() && num(mwr.value("statistic")) == 0.0
                           && std::fabs(num(mwr.value("p_value")) - 0.24156658696897298) < 1e-9
                           && mwr.value("test").toString() == QLatin1String("Mann-Whitney U"),
                   "100d: Mann-Whitney A(5) vs C(1) must be computed (U = 0, p = 0.2416)");
        const QString tEmpty = makeTable(QStringList() << "E" << "F",
                                         { { 1.0, 2.0, 3.0 }, { nan, nan, nan } });
        const QJsonObject mwe = rpc(QStringLiteral("stats.nonparametric"),
                                    QJsonObject{ { "table", tEmpty },
                                                 { "cols", QJsonArray{ "E", "F" } },
                                                 { "test", "mann_whitney" } })
                                        .value("result")
                                        .toObject();
        CHECK_COND(!mwe.value("ok").toBool() && !mwe.value("message").toString().isEmpty(),
                   "100d: a test that cannot run must say why in 'message'");
    }

    // -------------------------------------------------------------------------
    // 100e: Kissinger from spectrum columns
    // -------------------------------------------------------------------------
    {
        const double rates[4] = { 2, 5, 10, 20 }; // K/min
        std::vector<double> T;
        for (int i = 0; i < 190; ++i)
            T.push_back(330.0 + i);
        double tTrue[4];
        std::vector<std::vector<double>> clean;
        for (int j = 0; j < 4; ++j)
            clean.push_back(tds100(T, rates[j] / 60.0, &tTrue[j]));
        // peak_temps_k in the order of 'heating_rates' (the result lists them by rate).
        auto peakOfRate = [](const QJsonObject &r, double rate) {
            const QJsonArray rr = r.value("heating_rates_k_min").toArray();
            const QJsonArray tp = r.value("peak_temps_k").toArray();
            for (int i = 0; i < rr.size() && i < tp.size(); ++i) {
                if (std::fabs(rr.at(i).toDouble() - rate) < 1e-9)
                    return tp.at(i).toDouble();
            }
            return std::numeric_limits<double>::quiet_NaN();
        };
        auto kissinger = [&](const std::vector<std::vector<double>> &spec) {
            const QString t = makeTable(QStringList() << "T" << "r2" << "r5" << "r10" << "r20",
                                        { T, spec[0], spec[1], spec[2], spec[3] });
            return rpc(QStringLiteral("curvemath.kissinger"),
                       QJsonObject{ { "table", t },
                                    { "heating_rates", QJsonArray{ 2, 5, 10, 20 } },
                                    { "temp_col", "T" },
                                    { "y_cols", QJsonArray{ "r2", "r5", "r10", "r20" } },
                                    { "temp_in_celsius", false },
                                    { "plot", false } })
                    .value(QStringLiteral("result"))
                    .toObject();
        };
        // Noise-free: the largest sample, as before.
        {
            const QJsonObject r = kissinger(clean);
            const QJsonArray tp = r.value("peak_temps_k").toArray();
            CHECK_COND(r.value("ok").toBool() && tp.size() == 4, "100e: noise-free Kissinger failed");
            for (int j = 0; j < 4; ++j) {
                const auto &s = clean[size_t(j)];
                const double tArg = T[size_t(std::max_element(s.begin(), s.end()) - s.begin())];
                CHECK_COND(std::fabs(peakOfRate(r, rates[j]) - tArg) < 1e-9,
                           "100e: noise-free spectrum " << j << " peak " << peakOfRate(r, rates[j])
                                                        << " K, the largest sample is at " << tArg);
            }
            std::cout << "[INFO] 100e noise-free Ea=" << num(r.value("ea_kj_mol")) << "\n";
        }
        for (uint64_t seed : { 23, 24, 25 }) {
            SplitMix100 rng(seed);
            std::vector<std::vector<double>> noisy = clean;
            for (auto &s : noisy) {
                for (double &v : s)
                    v += 0.02 * rng.gauss();
            }
            const QJsonObject r = kissinger(noisy);
            const QJsonArray tp = r.value("peak_temps_k").toArray();
            const double ea = num(r.value("ea_kj_mol"));
            std::cout << "[INFO] 100e 2 % noise seed " << seed << ": Ea=" << ea << " Tp:";
            for (const QJsonValue &v : tp)
                std::cout << " " << v.toDouble();
            std::cout << " (true " << tTrue[0] << " " << tTrue[1] << " " << tTrue[2] << " "
                      << tTrue[3] << ")\n";
            CHECK_COND(tp.size() == 4, "100e: noisy Kissinger failed");
            for (int j = 0; j < 4; ++j) {
                CHECK_COND(std::fabs(peakOfRate(r, rates[j]) - tTrue[j]) < 1.2,
                           "100e: seed " << seed << " peak " << j << " at " << peakOfRate(r, rates[j])
                                         << " K, true " << tTrue[j]);
            }
            CHECK_COND(std::fabs(ea / 100.0 - 1.0) < 0.05,
                       "100e: seed " << seed << " Ea " << ea << " kJ/mol, truth 100");
        }
    }

    // -------------------------------------------------------------------------
    // 100f: smoothing notices in the RPC result
    // -------------------------------------------------------------------------
    {
        std::vector<double> xu, xe, xs, y;
        for (int i = 0; i < 30; ++i) {
            xe.push_back(i);
            xu.push_back(i + 0.02 * i * i); // unequal steps
            y.push_back(std::sin(0.3 * i));
        }
        xs = xe;
        std::swap(xs[5], xs[20]); // not monotonic
        auto smooth = [&](const std::vector<double> &x, const char *method) {
            const QString g = plotXY(x, y);
            return rpc(QStringLiteral("signal.smooth"),
                       QJsonObject{ { "graph", g }, { "curve", 0 }, { "method", method }, { "window", 5 } })
                    .value(QStringLiteral("result"))
                    .toObject();
        };
        for (const char *m : { "savgol", "average", "gaussian" }) {
            const QJsonObject ru = smooth(xu, m);
            const QJsonObject re = smooth(xe, m);
            std::cout << "[INFO] 100f " << m << " unequal X notice: "
                      << ru.value("notice").toString().toStdString() << "\n";
            CHECK_COND(ru.value("ok").toBool() && !ru.value("notice").toString().isEmpty(),
                       "100f: " << m << " on unequally spaced X must return a notice");
            CHECK_COND(re.value("ok").toBool() && !re.contains("notice"),
                       "100f: " << m << " on equally spaced X must not carry a notice");
        }
        const QJsonObject rs = smooth(xs, "average");
        CHECK_COND(rs.value("ok").toBool() && !rs.value("notice").toString().isEmpty(),
                   "100f: average on non-monotonic X must return a notice");
        const QJsonObject rg = smooth(xs, "gaussian");
        CHECK_COND(rg.value("ok").toBool() && !rg.value("notice").toString().isEmpty(),
                   "100f: gaussian on non-monotonic X must return a notice");
    }

    // -------------------------------------------------------------------------
    // 100g: undefined standard errors and correlations are null
    // -------------------------------------------------------------------------
    {
        auto weibull = [&](const std::vector<double> &v, bool threeP) {
            const QString t = makeTable(QStringList() << "v", { v });
            return rpc(QStringLiteral("stats.weibull"),
                       QJsonObject{ { "table", t }, { "col", 0 }, { "plot", false },
                                    { "three_param", threeP } })
                    .value(QStringLiteral("result"))
                    .toObject();
        };
        const QJsonObject w2 = weibull({ 100.0, 200.0 }, false);
        CHECK_COND(w2.value("ok").toBool() && w2.value("beta_se").isNull()
                           && w2.value("eta_se").isNull() && w2.value("beta").isDouble(),
                   "100g: Weibull n = 2 must report beta_se / eta_se as null (were 0)");
        const QJsonObject w3 = weibull({ 100.0, 150.0, 210.0 }, true);
        CHECK_COND(w3.value("ok").toBool() && w3.value("beta_se").isNull()
                           && w3.value("eta_se").isNull(),
                   "100g: 3-parameter Weibull n = 3 must report beta_se / eta_se as null");
        const QJsonObject w5 = weibull({ 100.0, 150.0, 210.0, 180.0, 240.0 }, false);
        CHECK_COND(w5.value("beta_se").isDouble() && w5.value("beta_se").toDouble() > 0.0
                           && w5.value("eta_se").isDouble(),
                   "100g: Weibull n = 5 keeps finite standard errors");

        const QString tc = makeTable(QStringList() << "a" << "b" << "c",
                                     { { 1, 2, 3, 4 }, { 2, 4, 7, 7.5 }, { 5, 5, 5, 5 } });
        const QJsonObject cr = rpc(QStringLiteral("stats.correlation"),
                                   QJsonObject{ { "table", tc },
                                                { "cols", QJsonArray{ "a", "b", "c" } },
                                                { "method", "pearson" },
                                                { "plot_heatmap", false } })
                                       .value(QStringLiteral("result"))
                                       .toObject();
        const QJsonArray R = cr.value("r_matrix").toArray();
        const QJsonArray P = cr.value("p_matrix").toArray();
        CHECK_COND(R.size() == 3 && P.size() == 3, "100g: correlation matrix size");
        CHECK_COND(R.at(0).toArray().at(1).isDouble() && P.at(0).toArray().at(1).isDouble(),
                   "100g: r / p of a and b must stay numbers");
        for (int i = 0; i < 3; ++i) {
            CHECK_COND(R.at(i).toArray().at(2).isNull() && P.at(i).toArray().at(2).isNull()
                               && R.at(2).toArray().at(i).isNull(),
                       "100g: r / p with the constant column c must be null (were r = 0, p = 1)");
        }
    }

    // -------------------------------------------------------------------------
    // 100h: noisy XRD with Ka2 left in (strip_kalpha2 off)
    // -------------------------------------------------------------------------
    {
        // D = 150 nm, eps = 0.0005: Ka1 FWHM 0.096-0.17 deg, Ka1-Ka2 split 0.10-0.25 deg.
        double sumD = 0.0, sumEps = 0.0;
        const uint64_t seeds[] = { 31, 32, 33, 34 };
        for (uint64_t seed : seeds) {
            std::vector<double> tt, y;
            double cTrue[5], wTrue[5];
            alPatternKa2_100(400.0, 9000.0, 0.02, seed, 150.0, 0.0005, tt, y, cTrue, wTrue);
            const QJsonObject res = xrd(tt, y);
            const QJsonArray peaks = res.value(QStringLiteral("peaks")).toArray();
            const double whD = num(res.value(QStringLiteral("wh_size_nm")));
            const double whEps = num(res.value(QStringLiteral("wh_microstrain")));
            std::cout << "[INFO] 100h Ka2 seed " << seed << ": W-H D=" << whD << " eps=" << whEps
                      << " FWHM/true:";
            for (int r = 0; r < peaks.size() && r < 5; ++r)
                std::cout << " " << peaks.at(r).toObject().value("fwhm_deg").toDouble() / wTrue[r];
            std::cout << "\n";
            CHECK_COND(peaks.size() == 5, "100h: expected the 5 Al reflections, got " << peaks.size());
            for (int r = 0; r < 5 && r < peaks.size(); ++r) {
                const QJsonObject o = peaks.at(r).toObject();
                const double t = o.value(QStringLiteral("two_theta_deg")).toDouble();
                const double fw = o.value(QStringLiteral("fwhm_deg")).toDouble();
                CHECK_COND(std::fabs(t - cTrue[r]) < 0.03,
                           "100h: peak " << r << " at " << t << ", Ka1 at " << cTrue[r]);
                CHECK_COND(std::fabs(fw / wTrue[r] - 1.0) < 0.10,
                           "100h: seed " << seed << " peak " << r << " FWHM " << fw
                                         << " deg, Ka1 truth " << wTrue[r]
                                         << " (0.75-1.8 times before)");
            }
            CHECK_COND(std::isfinite(whD) && std::fabs(whD / 150.0 - 1.0) < 0.30,
                       "100h: W-H size " << whD << " nm, truth 150 nm");
            CHECK_COND(std::isfinite(whEps) && std::fabs(whEps / 0.0005 - 1.0) < 0.40,
                       "100h: W-H microstrain " << whEps << ", truth 0.0005");
            sumD += whD;
            sumEps += whEps;
        }
        std::cout << "[INFO] 100h mean W-H D=" << sumD / 4.0 << " eps=" << sumEps / 4.0 << "\n";
    }

    // -------------------------------------------------------------------------
    // 100i: Welch / Games-Howell between two groups of one value
    // -------------------------------------------------------------------------
    {
        // A = [10], B = [13], C = [1..5]: N - k = 4, MS within 2.5. A vs B: t = 3 / sqrt(5),
        // df 4, p = 0.250815 (df 8 gave 0.216547). A vs C keeps its Welch df 5.538.
        const QString t = makeTable(QStringList() << "A" << "B" << "C",
                                    { { 10.0, nan, nan, nan, nan },
                                      { 13.0, nan, nan, nan, nan },
                                      { 1.0, 2.0, 3.0, 4.0, 5.0 } });
        for (const char *ph : { "holm", "games_howell" }) {
            const QJsonObject resp = rpc(QStringLiteral("stats.anova_oneway"),
                                         QJsonObject{ { "table", t },
                                                      { "cols", QJsonArray{ "A", "B", "C" } },
                                                      { "posthoc", ph } });
            CHECK_COND(!resp.contains("error"), "100i: ANOVA (" << ph << ") failed: "
                                                                 << errMsg(resp).toStdString());
            const QJsonObject r = resp.value("result").toObject();
            bool seenAB = false, seenAC = false;
            for (const QJsonValue &cv : r.value("comparisons").toArray()) {
                const QJsonObject c = cv.toObject();
                const QString a = c.value("group_a").toString(), b = c.value("group_b").toString();
                const double df = num(c.value("df")), pRaw = num(c.value("p_raw"));
                std::cout << "[INFO] 100i " << ph << " " << a.toStdString() << " vs "
                          << b.toStdString() << ": df=" << df << " p_raw=" << pRaw
                          << " p_adj=" << num(c.value("p_adj")) << "\n";
                if (a == QLatin1String("A") && b == QLatin1String("B")) {
                    seenAB = true;
                    CHECK_COND(std::fabs(df - 4.0) < 1e-9 && std::fabs(pRaw - 0.250815360) < 1e-6
                                       && c.value("p_adj").isDouble(),
                               "100i: " << ph << " A(1) vs B(1) must use df N - k = 4, p 0.2508 "
                                        << "(got df " << df << ", p " << pRaw << ")");
                } else if (a == QLatin1String("A") && b == QLatin1String("C")) {
                    seenAC = true;
                    CHECK_COND(std::fabs(df - 9.0 / 1.625) < 1e-9
                                       && std::fabs(pRaw - 0.008021066) < 1e-6,
                               "100i: " << ph << " A(1) vs C(5) df " << df << " p " << pRaw
                                        << ", expected 5.538 / 0.00802");
                }
            }
            CHECK_COND(seenAB && seenAC, "100i: " << ph << " comparisons missing");
            // The report table (GraphStatsAndSurfaceFit) has the same degrees of freedom.
            bool repAB = false;
            for (const QJsonValue &rv : r.value("report").toObject().value("data").toArray()) {
                const QJsonObject row = rv.toObject();
                if (row.value("Comparison").toString() != QLatin1String("A vs B"))
                    continue;
                repAB = true;
                const double dfRep = num(row.value(QLatin1String(ph) == QLatin1String("holm")
                                                           ? "df1"
                                                           : "df2"));
                CHECK_COND(std::fabs(dfRep - 4.0) < 1e-9
                                   && std::fabs(num(row.value("p_value")) - 0.250815360) < 1e-6,
                           "100i: report " << ph << " A vs B df " << dfRep << " p "
                                           << num(row.value("p_value")) << ", expected 4 / 0.2508");
            }
            CHECK_COND(repAB, "100i: report row 'A vs B' missing (" << ph << ")");
        }
    }

    // -------------------------------------------------------------------------
    // 100j: Kissinger spectra with a narrow and a broad peak
    // -------------------------------------------------------------------------
    {
        auto gauss = [](double x, double c, double s) {
            const double u = (x - c) / s;
            return std::exp(-0.5 * u * u);
        };
        const double narrow[3] = { 430.0, 436.0, 442.0 }; // on the 2 K grid
        for (double tEnd : { 500.0, 520.0 }) {
            for (double noise : { 0.0, 0.02 }) {
                std::vector<double> T;
                for (double v = 400.0; v < tEnd - 1e-9; v += 2.0)
                    T.push_back(v);
                SplitMix100 rng(77);
                std::vector<std::vector<double>> spec(3);
                for (int j = 0; j < 3; ++j) {
                    for (double v : T) {
                        spec[size_t(j)].push_back(gauss(v, narrow[j], 2.55)
                                                  + 0.9 * gauss(v, narrow[j] + 40.0, 12.7)
                                                  + noise * rng.gauss());
                    }
                }
                const QString t = makeTable(QStringList() << "T" << "r5" << "r10" << "r20",
                                            { T, spec[0], spec[1], spec[2] });
                const QJsonObject r =
                        rpc(QStringLiteral("curvemath.kissinger"),
                            QJsonObject{ { "table", t },
                                         { "heating_rates", QJsonArray{ 5, 10, 20 } },
                                         { "temp_col", "T" },
                                         { "y_cols", QJsonArray{ "r5", "r10", "r20" } },
                                         { "temp_in_celsius", false },
                                         { "plot", false } })
                                .value(QStringLiteral("result"))
                                .toObject();
                const QJsonArray tp = r.value("peak_temps_k").toArray();
                std::cout << "[INFO] 100j " << T.size() << " points, noise " << noise << ": Tp";
                for (const QJsonValue &v : tp)
                    std::cout << " " << v.toDouble();
                std::cout << "\n";
                CHECK_COND(tp.size() == 3, "100j: Kissinger failed");
                // The result lists the peaks by heating rate (heating_rates_k_min).
                const QJsonArray rr = r.value("heating_rates_k_min").toArray();
                const double rateOf[3] = { 5.0, 10.0, 20.0 };
                for (int j = 0; j < 3; ++j) {
                    double tPk = nan;
                    for (int i = 0; i < rr.size() && i < tp.size(); ++i) {
                        if (std::fabs(rr.at(i).toDouble() - rateOf[j]) < 1e-9)
                            tPk = tp.at(i).toDouble();
                    }
                    const double tol = (noise > 0.0) ? 1.0 : 1e-9;
                    CHECK_COND(std::fabs(tPk - narrow[j]) < tol,
                               "100j: " << T.size() << " points, noise " << noise << ": peak "
                                        << tPk << " K, the narrow peak is at " << narrow[j]
                                        << " K (470 K before)");
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 100k: lookup_xy on an almost monotonic X with noise
    // -------------------------------------------------------------------------
    {
        SplitMix100 rng(91);
        std::vector<double> x, y;
        for (int i = 0; i < 1000; ++i) {
            x.push_back(0.01 * i + 0.005 * rng.gauss());
            y.push_back(2.0 * x.back() + 1.0);
        }
        const QString g = plotXY(x, y);
        CHECK_COND(!g.isEmpty(), "100k: plot setup failed");
        int nullY = 0;
        double worst = 0.0;
        QString notice;
        for (int q = 0; q <= 180; ++q) {
            const double xq = 0.5 + 0.05 * q + 0.0013;
            const QJsonObject r = rpc(QStringLiteral("curvemath.lookup_xy"),
                                      QJsonObject{ { "graph", g }, { "curve", 0 }, { "x", xq } })
                                          .value(QStringLiteral("result"))
                                          .toObject();
            if (!r.value("ok").toBool() || !r.value("y").isDouble()) {
                ++nullY;
                continue;
            }
            worst = std::max(worst, std::fabs(r.value("y").toDouble() - (2.0 * xq + 1.0)));
            notice = r.value("notice").toString();
        }
        std::cout << "[INFO] 100k noisy X: " << nullY << " of 181 without y, worst |dy| " << worst
                  << ", notice: " << notice.toStdString() << "\n";
        CHECK_COND(nullY == 0 && worst < 1e-9,
                   "100k: an almost monotonic X with noise must give one y everywhere ("
                           << nullY << " of 181 were null)");
        CHECK_COND(notice.contains(QLatin1String("one branch")),
                   "100k: the notice must say the curve was taken as one branch");
    }

    w.reset();
    std::cout << "[PASS] Suite 100 passed.\n";
    return 0;
}
