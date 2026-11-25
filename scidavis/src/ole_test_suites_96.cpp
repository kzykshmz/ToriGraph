#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <cstdint>
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
//       ascending, values match cos x; a descending copy gives the same table.
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

        // Runs curvemath.differentiate and returns the result table's (X, dY/dX) columns.
        auto differentiate = [&](const QString &tName, const std::vector<double> &x,
                                 const std::vector<double> &y, std::vector<double> &outX,
                                 std::vector<double> &outY) -> bool {
            const QString gName = plotXY(tName, x, y);
            if (gName.isEmpty())
                return false;
            const QJsonObject resp = rpc(QStringLiteral("curvemath.differentiate"),
                                         QJsonObject{ { "graph", gName }, { "curve", 0 } });
            if (resp.contains(QStringLiteral("error")))
                return false;
            const QJsonObject res = resp.value(QStringLiteral("result")).toObject();
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
        CHECK_COND(differentiate(QStringLiteral("T96DShuf"), xs, ys, sX, sY),
                   "96d: differentiate on the shuffled sin table failed");
        CHECK_COND(differentiate(QStringLiteral("T96DDesc"), xd, yd, dX, dY),
                   "96d: differentiate on the descending sin table failed");
        CHECK_COND(differentiate(QStringLiteral("T96DAsc"), xa, ya, aX, aY),
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
        std::cout << "[INFO] 96d shuffled sin: max |d/dx - cos| = " << maxErr << "\n";
        CHECK_COND(ascending, "96d: derivative of shuffled data must be listed in ascending X");
        for (int i = 0; i < n; ++i) {
            CHECK_COND(std::fabs(sX[size_t(i)] - aX[size_t(i)]) < 1e-12
                               && std::fabs(sY[size_t(i)] - aY[size_t(i)]) < 1e-12
                               && std::fabs(dX[size_t(i)] - aX[size_t(i)]) < 1e-12
                               && std::fabs(dY[size_t(i)] - aY[size_t(i)]) < 1e-12,
                       "96d: shuffled / descending / ascending inputs must give the same "
                       "derivative table (row "
                               << i << ")");
        }

        // Duplicate X that only meet after sorting must still be refused.
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
        const bool dupOk = !dupResp.contains(QStringLiteral("error"))
                && dupResp.value(QStringLiteral("result")).toObject().value("ok").toBool();
        CHECK_COND(!dupOk && w96->windowsList().size() == nWin,
                   "96d: duplicate X (non-adjacent in the rows) must be refused, not differentiated");
    }

    w96.reset();
    std::cout << "[PASS] Suite 96 passed.\n";
    return 0;
}
