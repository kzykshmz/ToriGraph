#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <cstdint>
#include <sstream>
#include <utility>
#include <vector>

// =============================================================================
// Suite 96: XRD peak search on noisy patterns and differentiation of unsorted X.
//   96a curvemath.xrd_analyze, noise-free FCC Al pattern (Cu Ka1, D = 40 nm, eps = 0.0015,
//       FWHM from Scherrer + 4 eps tan(theta)): exactly the five reflections, W-H returns
//       D = 40.00 nm and eps = 0.00150.
//   96b the same pattern with Gaussian noise (sigma = 1 count on a 20-count background).
//       The old prominence (lowest point anywhere within +-1.4 deg) let a noise wiggle on the
//       flank of (222) or (311) through as a peak of its own; it took the (222) index and the
//       W-H result fell to ~17 nm with no microstrain. Now: five peaks, each within 0.02 deg
//       of a reflection, all indexed, D within 15 % and eps within 20 % of the truth.
//   96c the same with counting-like noise (background 200, peaks 830-3330 counts,
//       sigma = sqrt(I)), 0.02 deg step. Seed 29 also checks the apex: the 3-point apex of
//       (222) sat 0.08 deg off centre, the profile pinned there was 24 % too wide and W-H
//       gave 64 nm; the centre is now refined with the profile.
//   96d curvemath.differentiate on sin(x) with shuffled rows: the derivative is taken in X
//       order (it was taken in row order and came out up to 1.2 away from cos x). Output X is
//       ascending, values match cos x, and a notice says the rows were sorted. A descending
//       copy keeps its row order (the same slopes, mirrored), with no notice. Duplicate X
//       that meet after sorting are refused, with the reason in the RPC result.
//   96e a scan that goes out and back (x = 0..10..0, loading/unloading): each branch is
//       differentiated in row order (sorting by X put equal X side by side and refused, or
//       interleaved a slightly shifted return branch into nonsense slopes); the turning point
//       takes the incoming branch's one-sided slope and the result carries a notice.
//   96f a weak reflection (B = 10-20 % of A, 0.5-0.6 deg away, pseudo-Voigt eta 0.5,
//       FWHM 0.3 deg) is found next to the strong one, noise-free and with sigma = 1 noise.
//       The topographic prominence (valley depth) had to reach 3 % of the tallest peak.
//       (At 0.4 deg B is a shoulder without a maximum of its own, which no search reports.)
//   96g Al with counting noise on a 400-count background, net (111) 1000 counts: the 7 % (222)
//       reflection is found (5 sigma of a single sample needed more than 100 counts).
//   96h coarse (0.1 deg step) noise-free patterns, one crowded with three FCC phases: the
//       result equals the one before the noise tests (bf5db55d^). The second-difference
//       noise estimate is not 0 there (the peaks are only a few samples wide), which dropped
//       two weak reflections and moved the centres.
// The noise comes from a fixed splitmix64 + Box-Muller generator, so the data are identical
// on every platform. The seeds below are ones on which the old peak search failed.
// =============================================================================
namespace {

struct SplitMix64
{
    uint64_t s;
    explicit SplitMix64(uint64_t seed) : s(seed) {}
    uint64_t next()
    {
        s += 0x9E3779B97F4A7C15ULL;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // Uniform on (0, 1].
    double uniform() { return double((next() >> 11) + 1) * (1.0 / 9007199254740992.0); }
    double gauss()
    {
        const double u1 = uniform();
        const double u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    }
};

struct XrdPattern
{
    std::vector<double> tt;
    std::vector<double> y;
};

const double kLambda96 = 1.540598;
const double kA96 = 4.0495;
const double kK96 = 0.9;
const double kD96Nm = 40.0;
const double kEps96 = 0.0015;
const int kHkl96[5][3] = { { 1, 1, 1 }, { 2, 0, 0 }, { 2, 2, 0 }, { 3, 1, 1 }, { 2, 2, 2 } };

double true2Theta96(int r)
{
    const int m = kHkl96[r][0] * kHkl96[r][0] + kHkl96[r][1] * kHkl96[r][1]
            + kHkl96[r][2] * kHkl96[r][2];
    const double d = kA96 / std::sqrt(double(m));
    return 2.0 * std::asin(kLambda96 / (2.0 * d)) * 180.0 / M_PI;
}

// kind 0: noise free, 1: Gaussian sigma = 1, 2: counting-like sigma = sqrt(I) (rounded, >= 0)
XrdPattern makeXrd96(int n, double step, double bg, double amp, int kind, uint64_t seed)
{
    XrdPattern p;
    p.tt.resize(size_t(n));
    p.y.assign(size_t(n), bg);
    for (int i = 0; i < n; ++i)
        p.tt[size_t(i)] = 30.0 + step * i;
    for (int r = 0; r < 5; ++r) {
        const int m = kHkl96[r][0] * kHkl96[r][0] + kHkl96[r][1] * kHkl96[r][1]
                + kHkl96[r][2] * kHkl96[r][2];
        const double d = kA96 / std::sqrt(double(m));
        const double th = std::asin(kLambda96 / (2.0 * d));
        const double fwhm = (kK96 * kLambda96 / (kD96Nm * 10.0 * std::cos(th))
                             + 4.0 * kEps96 * std::tan(th))
                * 180.0 / M_PI;
        const double c = 2.0 * th * 180.0 / M_PI;
        const double sig = fwhm / 2.3548200450309493;
        for (int i = 0; i < n; ++i) {
            const double u = (p.tt[size_t(i)] - c) / sig;
            p.y[size_t(i)] += amp / m * std::exp(-0.5 * u * u);
        }
    }
    if (kind != 0) {
        SplitMix64 rng(seed);
        for (int i = 0; i < n; ++i) {
            const double z = rng.gauss();
            double &v = p.y[size_t(i)];
            if (kind == 1)
                v += z;
            else
                v = std::max(0.0, std::round(v + std::sqrt(v) * z));
        }
    }
    return p;
}

double pv96(double x, double c, double fwhm, double amp, double eta)
{
    const double u = (x - c) / (0.5 * fwhm);
    return amp * (eta / (1.0 + u * u) + (1.0 - eta) * std::exp(-std::log(2.0) * u * u));
}

double tt96(double a, int m)
{
    const double d = a / std::sqrt(double(m));
    return 2.0 * std::asin(kLambda96 / (2.0 * d)) * 180.0 / M_PI;
}

// FWHM (deg) of a reflection at 2theta = cDeg for D = 40 nm, eps = 0.0015 (Scherrer + strain).
double fwhm96(double cDeg)
{
    const double th = cDeg / 2.0 * M_PI / 180.0;
    return (kK96 * kLambda96 / (kD96Nm * 10.0 * std::cos(th)) + 4.0 * kEps96 * std::tan(th))
            * 180.0 / M_PI;
}

// Pseudo-Voigt (eta 0.5) FCC pattern(s): for each phase (lattice a, scale) the five
// reflections of kHkl96 with powder intensities 100 / 47 / 22 / 24 / 7 times the scale.
std::vector<double> fccPv96(const std::vector<double> &tt, double bg,
                            const std::vector<std::pair<double, double>> &phases)
{
    static const double kRel[5] = { 100.0, 47.0, 22.0, 24.0, 7.0 };
    std::vector<double> y(tt.size(), bg);
    for (const auto &ph : phases) {
        for (int r = 0; r < 5; ++r) {
            const int m = kHkl96[r][0] * kHkl96[r][0] + kHkl96[r][1] * kHkl96[r][1]
                    + kHkl96[r][2] * kHkl96[r][2];
            const double c = tt96(ph.first, m);
            const double w = fwhm96(c);
            for (size_t i = 0; i < tt.size(); ++i)
                y[i] += pv96(tt[i], c, w, ph.second * kRel[r], 0.5);
        }
    }
    return y;
}

} // namespace

int runSuite96XrdPeaksAndUnsortedDiff(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 96: XRD peaks on noisy patterns & unsorted-X derivative...\n";

    struct NonInteractiveQuitGuard96
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard96()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard96()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard96;

    struct AppWinGuard96
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard96(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard96() { reset(); }
        AppWinGuard96(const AppWinGuard96 &) = delete;
        AppWinGuard96 &operator=(const AppWinGuard96 &) = delete;
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

    AppWinGuard96 w96(new ApplicationWindow());
    w96->applyUserSettings();
    w96->confirmCloseTable = false;
    w96->confirmClosePlot2D = false;
    w96->confirmClosePlot3D = false;
    w96->confirmCloseFolder = false;
    w96->resize(1280, 800);
    w96->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w96->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "96: pythonAutomationServer null");

    int rpcId = 9600;
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

    // Two-column table plotted as a line; returns the graph window name ("" on failure).
    auto plotXY = [&](const QString &tName, const std::vector<double> &x,
                      const std::vector<double> &y) -> QString {
        Table *t = w96->newTable(int(x.size()), 2, tName, tName);
        if (!t)
            return QString();
        for (int r = 0; r < int(x.size()); ++r) {
            t->setCell(r, 0, x[size_t(r)]);
            t->setCell(r, 1, y[size_t(r)]);
        }
        MultiLayer *ml = w96->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
        QCoreApplication::processEvents();
        return (ml && ml->activeGraph() && ml->activeGraph()->curves() == 1) ? ml->name()
                                                                              : QString();
    };

    // -------------------------------------------------------------------------
    // 96a-c: XRD peak search and Williamson-Hall
    // -------------------------------------------------------------------------
    struct XrdCase
    {
        const char *tag;
        int n;
        double step, bg, amp;
        int kind;
        uint64_t seed;
    };
    const XrdCase cases[] = {
        { "96a clean", 5500, 0.01, 20.0, 1000.0, 0, 0 },
        { "96b gauss seed 3", 5500, 0.01, 20.0, 1000.0, 1, 3 },
        { "96b gauss seed 4", 5500, 0.01, 20.0, 1000.0, 1, 4 },
        { "96b gauss seed 10", 5500, 0.01, 20.0, 1000.0, 1, 10 },
        { "96c counts seed 28", 2750, 0.02, 200.0, 10000.0, 2, 28 },
        { "96c counts seed 29", 2750, 0.02, 200.0, 10000.0, 2, 29 },
        { "96c counts seed 40", 2750, 0.02, 200.0, 10000.0, 2, 40 },
    };
    const char *expectHkl[5] = { "(111)", "(200)", "(220)", "(311)", "(222)" };
    int caseNo = 0;
    for (const XrdCase &xc : cases) {
        ++caseNo;
        const XrdPattern p = makeXrd96(xc.n, xc.step, xc.bg, xc.amp, xc.kind, xc.seed);
        const QString gName = plotXY(QStringLiteral("T96X%1").arg(caseNo), p.tt, p.y);
        CHECK_COND(!gName.isEmpty(), xc.tag << ": table/plot setup failed");
        const QJsonObject resp = rpc(QStringLiteral("curvemath.xrd_analyze"),
                                     QJsonObject{ { "graph", gName },
                                                  { "curve", 0 },
                                                  { "wavelength", kLambda96 },
                                                  { "instr_fwhm", 0.0 },
                                                  { "crystal_system", "fcc" },
                                                  { "create_wh_plot", false },
                                                  { "add_top_d_axis", false } });
        CHECK_COND(!resp.contains(QStringLiteral("error")),
                   xc.tag << ": xrd_analyze returned an error: "
                          << resp.value("error").toObject().value("message").toString().toStdString());
        const QJsonObject res = resp.value(QStringLiteral("result")).toObject();
        const QJsonArray peaks = res.value(QStringLiteral("peaks")).toArray();
        const double whD = res.value(QStringLiteral("wh_size_nm")).toDouble(std::nan(""));
        const double whEps = res.value(QStringLiteral("wh_microstrain")).toDouble(std::nan(""));
        std::cout << "[INFO] " << xc.tag << ": n_peaks=" << peaks.size() << " WH D=" << whD
                  << " nm eps=" << whEps << " peaks:";
        for (const QJsonValue &v : peaks) {
            const QJsonObject o = v.toObject();
            std::cout << " " << o.value("two_theta_deg").toDouble() << o.value("hkl").toString().toStdString();
        }
        std::cout << "\n";
        CHECK_COND(peaks.size() == 5,
                   xc.tag << ": expected exactly the 5 FCC reflections, got " << peaks.size()
                          << " (flank noise reported as a peak?)");
        for (int r = 0; r < 5; ++r) {
            const QJsonObject o = peaks.at(r).toObject();
            const double tt = o.value(QStringLiteral("two_theta_deg")).toDouble();
            const QString hkl = o.value(QStringLiteral("hkl")).toString();
            CHECK_COND(std::fabs(tt - true2Theta96(r)) < 0.02,
                       xc.tag << ": peak " << r << " at " << tt << " deg, expected "
                              << true2Theta96(r));
            CHECK_COND(hkl == QLatin1String(expectHkl[r]),
                       xc.tag << ": peak at " << tt << " indexed as '" << hkl.toStdString()
                              << "', expected " << expectHkl[r]);
        }
        if (xc.kind == 0) {
            CHECK_COND(std::fabs(whD - kD96Nm) < 0.05 && std::fabs(whEps - kEps96) < 5e-6,
                       xc.tag << ": noise-free W-H must return D = 40.00 nm, eps = 0.00150 (got "
                              << whD << ", " << whEps << ")");
        } else {
            CHECK_COND(std::isfinite(whD) && std::fabs(whD - kD96Nm) < 0.15 * kD96Nm,
                       xc.tag << ": W-H size " << whD << " nm too far from 40 nm");
            CHECK_COND(std::isfinite(whEps) && std::fabs(whEps - kEps96) < 0.20 * kEps96,
                       xc.tag << ": W-H microstrain " << whEps << " too far from 0.0015");
        }
    }

    // -------------------------------------------------------------------------
    // 96d: differentiate with shuffled / descending X
    // -------------------------------------------------------------------------
    {
        const int n = 41;
        std::vector<int> perm(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
            perm[size_t(i)] = i;
        SplitMix64 rng(20);
        for (int i = n - 1; i > 0; --i) {
            const int j = int(rng.next() % uint64_t(i + 1));
            std::swap(perm[size_t(i)], perm[size_t(j)]);
        }
        std::vector<double> xs, ys, xd, yd, xa, ya;
        for (int i = 0; i < n; ++i) {
            const double x = 2.0 * M_PI * perm[size_t(i)] / (n - 1);
            xs.push_back(x);
            ys.push_back(std::sin(x));
            const double xdsc = 2.0 * M_PI * (n - 1 - i) / (n - 1);
            xd.push_back(xdsc);
            yd.push_back(std::sin(xdsc));
            const double xasc = 2.0 * M_PI * i / (n - 1);
            xa.push_back(xasc);
            ya.push_back(std::sin(xasc));
        }

        // Runs curvemath.differentiate and returns the result table's (X, dY/dX) columns and
        // the notice (empty if none).
        auto differentiate = [&](const QString &tName, const std::vector<double> &x,
                                 const std::vector<double> &y, std::vector<double> &outX,
                                 std::vector<double> &outY, QString *notice = nullptr) -> bool {
            const QString gName = plotXY(tName, x, y);
            if (gName.isEmpty())
                return false;
            const QJsonObject resp = rpc(QStringLiteral("curvemath.differentiate"),
                                         QJsonObject{ { "graph", gName }, { "curve", 0 } });
            if (resp.contains(QStringLiteral("error")))
                return false;
            const QJsonObject res = resp.value(QStringLiteral("result")).toObject();
            if (notice)
                *notice = res.value(QStringLiteral("notice")).toString();
            Table *t = w96->table(res.value(QStringLiteral("table")).toString());
            if (!res.value(QStringLiteral("ok")).toBool() || !t || t->numCols() < 2)
                return false;
            outX.clear();
            outY.clear();
            for (int r = 0; r < t->numRows(); ++r) {
                if (t->column(0)->isInvalid(r) || t->column(1)->isInvalid(r))
                    continue;
                outX.push_back(t->cell(r, 0));
                outY.push_back(t->cell(r, 1));
            }
            return true;
        };

        std::vector<double> sX, sY, dX, dY, aX, aY;
        QString sNotice, dNotice, aNotice;
        CHECK_COND(differentiate(QStringLiteral("T96DShuf"), xs, ys, sX, sY, &sNotice),
                   "96d: differentiate on the shuffled sin table failed");
        CHECK_COND(differentiate(QStringLiteral("T96DDesc"), xd, yd, dX, dY, &dNotice),
                   "96d: differentiate on the descending sin table failed");
        CHECK_COND(differentiate(QStringLiteral("T96DAsc"), xa, ya, aX, aY, &aNotice),
                   "96d: differentiate on the ascending sin table failed");
        CHECK_COND(sX.size() == size_t(n) && dX.size() == size_t(n) && aX.size() == size_t(n),
                   "96d: derivative must keep all " << n << " points (got " << sX.size() << ", "
                                                    << dX.size() << ", " << aX.size() << ")");
        double maxErr = 0.0;
        bool ascending = true;
        for (int i = 0; i < n; ++i) {
            if (i > 0 && !(sX[size_t(i)] > sX[size_t(i - 1)]))
                ascending = false;
            // End points use one-sided differences (error ~ h/2 = 0.08); interior central
            // means (error ~ h^2/6 = 0.004).
            const double tol = (i == 0 || i == n - 1) ? 0.1 : 0.01;
            const double err = std::fabs(sY[size_t(i)] - std::cos(sX[size_t(i)]));
            maxErr = std::max(maxErr, err);
            CHECK_COND(err < tol,
                       "96d: shuffled-X derivative at x = " << sX[size_t(i)] << " is "
                               << sY[size_t(i)] << ", cos x = " << std::cos(sX[size_t(i)]));
        }
        std::cout << "[INFO] 96d shuffled sin: max |d/dx - cos| = " << maxErr
                  << ", notice: " << sNotice.toStdString() << "\n";
        CHECK_COND(ascending, "96d: derivative of shuffled data must be listed in ascending X");
        // The notice is translated with the UI; its numbers are not.
        CHECK_COND(sNotice.contains(QLatin1String("41")) && sNotice.contains(QLatin1String("30")),
                   "96d: shuffled rows must carry a 'sorted by X' notice naming 30 changes of "
                   "direction in 41 points (got '"
                           << sNotice.toStdString() << "')");
        CHECK_COND(dNotice.isEmpty() && aNotice.isEmpty(),
                   "96d: monotonic X must not carry a notice");
        // Shuffled == ascending; descending keeps its row order, i.e. the ascending table read
        // bottom-up (the one-sided end slopes swap ends, the central means are the same).
        for (int i = 0; i < n; ++i) {
            const size_t j = size_t(n - 1 - i);
            CHECK_COND(std::fabs(sX[size_t(i)] - aX[size_t(i)]) < 1e-12
                               && std::fabs(sY[size_t(i)] - aY[size_t(i)]) < 1e-12
                               && std::fabs(dX[size_t(i)] - aX[j]) < 1e-12
                               && std::fabs(dY[size_t(i)] - aY[j]) < 1e-12,
                       "96d: shuffled / ascending inputs must give the same derivative table, "
                       "descending the same one in its own row order (row "
                               << i << ")");
        }

        // Duplicate X that only meet after sorting must still be refused, and say why.
        Table *tDup = w96->newTable(6, 2, QStringLiteral("T96DDup"), QStringLiteral("T96DDup"));
        CHECK_COND(tDup != nullptr, "96d: dup table setup failed");
        const double dupX[6] = { 3, 1, 2, 1, 4, 0 };
        for (int r = 0; r < 6; ++r) {
            tDup->setCell(r, 0, dupX[r]);
            tDup->setCell(r, 1, dupX[r] * dupX[r] + r);
        }
        MultiLayer *mlDup =
                w96->multilayerPlot(tDup, QStringList() << tDup->colName(1), Graph::Line);
        CHECK_COND(mlDup != nullptr, "96d: dup plot setup failed");
        const int nWin = w96->windowsList().size();
        const QJsonObject dupResp = rpc(QStringLiteral("curvemath.differentiate"),
                                        QJsonObject{ { "graph", mlDup->name() }, { "curve", 0 } });
        const QJsonObject dupRes = dupResp.value(QStringLiteral("result")).toObject();
        const bool dupOk =
                !dupResp.contains(QStringLiteral("error")) && dupRes.value("ok").toBool();
        CHECK_COND(!dupOk && w96->windowsList().size() == nWin,
                   "96d: duplicate X (non-adjacent in the rows) must be refused, not differentiated");
        const QString dupMsg = dupRes.value(QStringLiteral("message")).toString();
        CHECK_COND(dupMsg.contains(QLatin1String("x = 1")),
                   "96d: a refused derivative must say why in 'message' (got '"
                           << dupMsg.toStdString() << "')");

        // ---------------------------------------------------------------------
        // 96e: out-and-back scan. Out: y = x^2 at x = 0..10; back: y = 10 x (a straight line
        // from the turning point (10, 100) to the origin), on the same X grid and on one
        // shifted by 0.013. Expected per row: forward slope 1 at x = 0, 2x up the out branch
        // (the central mean is exact for a parabola), 19 (= 100 - 81) at the turning point
        // (incoming branch), 10 all the way back.
        // ---------------------------------------------------------------------
        for (int variant = 0; variant < 2; ++variant) {
            const double shift = (variant == 0) ? 0.0 : 0.013;
            std::vector<double> xo, yo, expect;
            for (int k = 0; k <= 10; ++k) {
                xo.push_back(k);
                yo.push_back(double(k) * k);
                expect.push_back(k == 0 ? 1.0 : (k == 10 ? 19.0 : 2.0 * k));
            }
            for (int k = 9; k >= 0; --k) {
                const double x = k + shift;
                xo.push_back(x);
                yo.push_back(10.0 * x);
                expect.push_back(10.0);
            }
            const char *tag = (variant == 0) ? "96e same grid" : "96e shifted return";
            std::vector<double> eX, eY;
            QString eNotice;
            CHECK_COND(differentiate(QStringLiteral("T96EBack%1").arg(variant), xo, yo, eX, eY,
                                     &eNotice),
                       tag << ": the out-and-back scan must be differentiated (sorted by X it "
                              "was refused for equal X side by side)");
            CHECK_COND(eX.size() == xo.size(), tag << ": derivative must keep all " << xo.size()
                                                   << " rows (got " << eX.size() << ")");
            double maxErrE = 0.0;
            for (size_t r = 0; r < xo.size(); ++r) {
                CHECK_COND(eX[r] == xo[r], tag << ": row " << r << " X " << eX[r]
                                               << " must stay in row order (" << xo[r] << ")");
                maxErrE = std::max(maxErrE, std::fabs(eY[r] - expect[r]));
                CHECK_COND(std::fabs(eY[r] - expect[r]) < 1e-9,
                           tag << ": row " << r << " (x = " << xo[r] << ") d/dx = " << eY[r]
                               << ", expected " << expect[r]);
            }
            std::cout << "[INFO] " << tag << ": max |d/dx - expected| = " << maxErrE
                      << ", notice: " << eNotice.toStdString() << "\n";
            CHECK_COND(eNotice.contains(QLatin1String(" 1 "))
                               && eNotice.contains(QLatin1String("x = 10")),
                       tag << ": the turning point must be noted (got '" << eNotice.toStdString()
                           << "')");
        }

        // Equal X next to each other in the rows: still refused, with the reason.
        {
            const std::vector<double> xq{ 0, 1, 2, 2, 3, 4 };
            const std::vector<double> yq{ 0, 1, 4, 4.1, 9, 16 };
            const QString gName = plotXY(QStringLiteral("T96EDup"), xq, yq);
            CHECK_COND(!gName.isEmpty(), "96e: dup plot setup failed");
            const QJsonObject res = rpc(QStringLiteral("curvemath.differentiate"),
                                        QJsonObject{ { "graph", gName }, { "curve", 0 } })
                                            .value(QStringLiteral("result"))
                                            .toObject();
            const QString msg = res.value(QStringLiteral("message")).toString();
            CHECK_COND(!res.value(QStringLiteral("ok")).toBool()
                               && msg.contains(QLatin1String("x = 2")),
                       "96e: adjacent equal X must be refused with a message (got '"
                               << msg.toStdString() << "')");
        }
    }

    // Runs curvemath.xrd_analyze on (tt, y) and returns the result object.
    int xrdTableNo = 0;
    auto xrd = [&](const std::vector<double> &tt, const std::vector<double> &y,
                   const char *crystal) -> QJsonObject {
        const QString gName = plotXY(QStringLiteral("T96XR%1").arg(++xrdTableNo), tt, y);
        if (gName.isEmpty())
            return QJsonObject();
        const QJsonObject resp = rpc(QStringLiteral("curvemath.xrd_analyze"),
                                     QJsonObject{ { "graph", gName },
                                                  { "curve", 0 },
                                                  { "wavelength", kLambda96 },
                                                  { "instr_fwhm", 0.0 },
                                                  { "crystal_system", crystal },
                                                  { "create_wh_plot", false },
                                                  { "add_top_d_axis", false } });
        return resp.value(QStringLiteral("result")).toObject();
    };
    auto peakList = [](const QJsonObject &res) {
        std::ostringstream os;
        const QJsonArray arr = res.value(QLatin1String("peaks")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            os << " " << o.value("two_theta_deg").toDouble()
               << o.value("hkl").toString().toStdString() << "/w"
               << o.value("fwhm_deg").toDouble();
        }
        return os.str();
    };

    // -------------------------------------------------------------------------
    // 96f: weak reflection beside a strong one (pseudo-Voigt eta 0.5, FWHM 0.3 deg)
    // -------------------------------------------------------------------------
    {
        std::vector<double> tt;
        for (int i = 0; i < 500; ++i)
            tt.push_back(35.0 + 0.02 * i);
        struct WeakCase
        {
            double a, b, sep, bg, noise;
            uint64_t seed;
        };
        const WeakCase wcases[] = {
            { 100, 10, 0.5, 5, 0, 0 },    { 100, 15, 0.5, 5, 0, 0 },    { 100, 20, 0.5, 5, 0, 0 },
            { 100, 10, 0.6, 5, 0, 0 },    { 100, 15, 0.6, 5, 0, 0 },    { 100, 20, 0.6, 5, 0, 0 },
            { 1000, 150, 0.5, 20, 1, 1 }, { 1000, 150, 0.5, 20, 1, 2 }, { 1000, 150, 0.5, 20, 1, 3 },
        };
        for (const WeakCase &wc : wcases) {
            std::vector<double> y(tt.size());
            SplitMix64 rng(wc.seed);
            for (size_t i = 0; i < tt.size(); ++i) {
                y[i] = wc.bg + pv96(tt[i], 40.0, 0.3, wc.a, 0.5)
                        + pv96(tt[i], 40.0 + wc.sep, 0.3, wc.b, 0.5);
                if (wc.noise > 0.0)
                    y[i] += wc.noise * rng.gauss();
            }
            const QJsonObject res = xrd(tt, y, "none");
            const QJsonArray peaks = res.value(QStringLiteral("peaks")).toArray();
            std::cout << "[INFO] 96f A=" << wc.a << " B=" << wc.b << " sep=" << wc.sep
                      << " noise=" << wc.noise << " seed=" << wc.seed << ":" << peakList(res)
                      << "\n";
            CHECK_COND(peaks.size() == 2,
                       "96f: A=" << wc.a << " B=" << wc.b << " sep=" << wc.sep << " noise="
                                 << wc.noise << ": expected the strong and the weak peak, got "
                                 << peaks.size());
            const double ttB = peaks.at(1).toObject().value("two_theta_deg").toDouble();
            // The apex of B sits a little towards A on A's flank (0.03 deg at 0.5 deg).
            CHECK_COND(std::fabs(ttB - (40.0 + wc.sep)) < 0.04,
                       "96f: weak peak reported at " << ttB << " deg, it is at "
                                                     << 40.0 + wc.sep);
        }
    }

    // -------------------------------------------------------------------------
    // 96g: Al, counting noise on a 400-count background, (111) net 1000 counts
    // -------------------------------------------------------------------------
    {
        std::vector<double> tt;
        for (int i = 0; i < 2750; ++i)
            tt.push_back(30.0 + 0.02 * i);
        const std::vector<double> clean = fccPv96(tt, 400.0, { { kA96, 10.0 } });
        for (uint64_t seed : { 1, 2, 3, 4, 5, 6 }) {
            std::vector<double> y = clean;
            SplitMix64 rng(500 + seed);
            for (double &v : y)
                v = std::max(0.0, std::round(v + std::sqrt(v) * rng.gauss()));
            const QJsonObject res = xrd(tt, y, "fcc");
            const QJsonArray peaks = res.value(QStringLiteral("peaks")).toArray();
            std::cout << "[INFO] 96g seed " << seed << ":" << peakList(res) << "\n";
            CHECK_COND(peaks.size() == 5,
                       "96g seed " << seed << ": expected the 5 Al reflections, got "
                                   << peaks.size());
            for (int r = 0; r < 5; ++r) {
                const double t = peaks.at(r).toObject().value("two_theta_deg").toDouble();
                // (222) is 0.57 deg wide and 70 counts on sigma 20: its apex wanders.
                CHECK_COND(std::fabs(t - true2Theta96(r)) < 0.2,
                           "96g seed " << seed << ": peak " << r << " at " << t
                                       << " deg, expected " << true2Theta96(r));
            }
        }
    }

    // -------------------------------------------------------------------------
    // 96h: coarse (0.1 deg) noise-free patterns give the result of bf5db55d^
    // -------------------------------------------------------------------------
    {
        std::vector<double> tt;
        for (int i = 0; i <= 550; ++i)
            tt.push_back(30.0 + 0.1 * i);
        struct Expect
        {
            double tt, fwhm;
            const char *hkl;
        };
        // Al + Ag + Cu (Ag's reflections sit 0.3-0.9 deg below Al's). Values of bf5db55d^.
        const Expect crowded[] = {
            { 38.4637, 0.6956, "(111)" }, { 43.3166, 0.3551, "" },
            { 44.7104, 0.4854, "(200)" }, { 50.4492, 0.3812, "" },
            { 64.4541, 0.6165, "(220)" }, { 65.0945, 0.6053, "" },
            { 74.1253, 0.5050, "" },      { 77.4071, 0.7078, "(311)" },
            { 78.2276, 0.6542, "" },      { 81.5481, 0.7360, "(222)" },
            { 82.4351, 0.6513, "" },
        };
        const Expect alOnly[] = {
            { 38.4749, 0.3309, "(111)" }, { 44.7208, 0.3554, "(200)" },
            { 65.0987, 0.4543, "(220)" }, { 78.2311, 0.5337, "(311)" },
            { 82.4382, 0.5610, "(222)" },
        };
        struct CoarseCase
        {
            const char *tag;
            std::vector<std::pair<double, double>> phases;
            const Expect *expect;
            int nExpect;
            double whD, whEps; // whEps NaN: no microstrain reported
        };
        const CoarseCase ccases[] = {
            { "96h crowded", { { kA96, 10.0 }, { 4.0862, 6.0 }, { 3.615, 4.0 } }, crowded, 11,
              13.6692, std::nan("") },
            { "96h Al", { { kA96, 10.0 } }, alOnly, 5, 39.4277, 0.00146605 },
        };
        for (const CoarseCase &cc : ccases) {
            const QJsonObject res = xrd(tt, fccPv96(tt, 20.0, cc.phases), "fcc");
            const QJsonArray peaks = res.value(QStringLiteral("peaks")).toArray();
            const double whD = res.value(QStringLiteral("wh_size_nm")).toDouble(std::nan(""));
            const double whEps =
                    res.value(QStringLiteral("wh_microstrain")).toDouble(std::nan(""));
            std::cout << "[INFO] " << cc.tag << ": W-H D=" << whD << " eps=" << whEps
                      << " peaks:" << peakList(res) << "\n";
            CHECK_COND(peaks.size() == cc.nExpect,
                       cc.tag << ": expected " << cc.nExpect << " peaks as before, got "
                              << peaks.size());
            for (int r = 0; r < cc.nExpect; ++r) {
                const QJsonObject o = peaks.at(r).toObject();
                const double t = o.value(QStringLiteral("two_theta_deg")).toDouble();
                const double w = o.value(QStringLiteral("fwhm_deg")).toDouble();
                const QString hkl = o.value(QStringLiteral("hkl")).toString();
                CHECK_COND(std::fabs(t - cc.expect[r].tt) < 5e-4
                                   && std::fabs(w - cc.expect[r].fwhm) < 5e-4
                                   && hkl == QLatin1String(cc.expect[r].hkl),
                           cc.tag << ": peak " << r << " is " << t << " deg / FWHM " << w
                                  << " / '" << hkl.toStdString() << "', before it was "
                                  << cc.expect[r].tt << " / " << cc.expect[r].fwhm << " / '"
                                  << cc.expect[r].hkl << "'");
            }
            CHECK_COND(std::fabs(whD - cc.whD) < 1e-3,
                       cc.tag << ": W-H size " << whD << " nm, before " << cc.whD);
            CHECK_COND(std::isnan(cc.whEps) ? std::isnan(whEps)
                                            : std::fabs(whEps - cc.whEps) < 1e-7,
                       cc.tag << ": W-H microstrain " << whEps << ", before " << cc.whEps);
        }
    }

    w96.reset();
    std::cout << "[PASS] Suite 96 passed.\n";
    return 0;
}
