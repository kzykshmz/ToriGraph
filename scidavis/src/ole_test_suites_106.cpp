#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "NonLinearFit.h"
#include "Interpolation.h"
#include "GraphStatsAndSurfaceFit.h"
#include "OriginGadgets.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// =============================================================================
// Suite 106: numerical analysis regressions (fit bounds, undefined statistics, interpolation
// range, silently ignored fit parameters), all through JSON-RPC.
//   106a analysis.fit_nonlinear "y0+A*exp(-x/t)" with bounds t in [3, 5] whose optimum lies
//        below 3: the solution is t = 3 (the same as a fit with t fixed at 3), but with t0 = 1,
//        3, 4.9 and 6 the fit ended "no further improvement" (not converged), because the
//        gradient test did not know that the parameter sat on its bound. It is converged now
//        (Karush-Kuhn-Tucker: the descent direction points out of the box), and the result
//        says in "notes" that t is at its bound. The same for the upper bound of [0.1, 1] and a
//        one-sided bound [3, null]; the open side was 1e30 and the start point became 1e26 (a
//        "converged" t = 1e26). A bound that does not bind gives no note.
//   106b r2 / adj_r2 that are not defined are null: n = p = 3 gave adj_r2 = 1 (min(1, NaN)),
//        a diverging fit (all params null) gave r2 = adj_r2 = 0 and an empty
//        convergence_message; it now says why the fit failed.
//   106c curvemath.interpolate with x_min / x_max took only the points inside the range for the
//        coefficients and shrank the grid to the first / last of them ([0.5, 5.5] gave
//        [1, 4.5], a spline different from the spline through all points; [2, 2.9] gave a
//        bare ok: false). The coefficients now come from every point, the grid is the range
//        (cut to the data with a warning), and a range outside the data is an error saying so.
//   106d fit_nonlinear: unknown names in fixed / init_params / bounds ("Y0", "tau", "T") are an
//        invalid-params error listing the parameter names; x_min alone or x_max alone in
//        fit_nonlinear, fit_linear, curvemath.integrate and curvemath.differentiate was ignored
//        (the whole curve was used); the missing side is now the end of the data.
//   106e the global (multi-curve) fit: a shared t on its bound [3, 5] / [3, inf) / [0.1, 1]
//        ended "no further improvement" like 106a; it is converged now, equals the global fit
//        with t fixed at the bound and notes it. analysis.fit_global reports converged from the
//        solver state, convergence_message and notes.
//   106f analysis.fit_nonlinear bounds with lower > upper were swapped silently; they are an
//        invalid-params error naming the parameter.
//   106g a curve with a repeated X (x = 0, 0, 1, ..., 10) or an X out of order outside the
//        range [2, 8] failed in Analysis > Interpolate (every point was taken, the repeated X
//        is an error); the interpolant is built from the points in the range and one point on
//        each side then (GUI and curvemath.interpolate alike). A repeated X in that window,
//        or without a range, stays an error.
//   106h a*x+b with the default initial values on data of size 1e-20 / 1e-22 / 1e-30 stopped
//        "converged" away from the solution (r2 0.629 / -54 / -6e7): the absolute part of the
//        step test passed for any step of such small parameters. The single, global and
//        implicit ODR fits now give the linear least squares line there and at 1e+20. A
//        negative r2 (a fit further from the data than their mean) is noted.
//   106i the surface fit (Custom2D a*x+b*y+c from a = b = c = 1) stopped "converged" on the
//        absolute step test 1e-8 at data of size 1e-22 / 1e-30, and r2 was undefined for
//        tss < 1e-30; it gives the least squares plane and its r2 now (and at 1e+20).
//   106j the ROI quick fit gadget (Gauss): the same step test, and r2 = 1 for rss <= 1e-24;
//        at size 1e-22 and 1e+20 it equals the fit at size 1.
//   106k Curve_R2 / Global_R2 of the global fit were blank for tss < 1e-30 (data of size
//        1e-22); constant data stay undefined unless the model reproduces them (R2 = 1).
// =============================================================================
namespace {

struct SplitMix106
{
    uint64_t s;
    explicit SplitMix106(uint64_t seed) : s(seed) {}
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

// Natural cubic spline through (x, y) (the same as GSL's cspline), evaluated at xe.
double naturalSpline106(const std::vector<double> &x, const std::vector<double> &y, double xe)
{
    const size_t n = x.size();
    std::vector<double> m(n, 0.0); // second derivatives
    if (n > 2) {
        std::vector<double> a(n, 0.0), b(n, 0.0), c(n, 0.0), d(n, 0.0);
        for (size_t i = 1; i + 1 < n; ++i) {
            const double h0 = x[i] - x[i - 1];
            const double h1 = x[i + 1] - x[i];
            a[i] = h0;
            b[i] = 2.0 * (h0 + h1);
            c[i] = h1;
            d[i] = 6.0 * ((y[i + 1] - y[i]) / h1 - (y[i] - y[i - 1]) / h0);
        }
        for (size_t i = 2; i + 1 < n; ++i) {
            const double w = a[i] / b[i - 1];
            b[i] -= w * c[i - 1];
            d[i] -= w * d[i - 1];
        }
        for (size_t i = n - 2; i >= 1; --i) {
            m[i] = (d[i] - c[i] * m[i + 1]) / b[i];
            if (i == 1)
                break;
        }
    }
    size_t k = 0;
    while (k + 2 < n && xe > x[k + 1])
        ++k;
    const double h = x[k + 1] - x[k];
    const double t = (xe - x[k]) / h;
    const double u = 1.0 - t;
    return u * y[k] + t * y[k + 1]
            + ((u * u * u - u) * m[k] + (t * t * t - t) * m[k + 1]) * h * h / 6.0;
}

double linearInterp106(const std::vector<double> &x, const std::vector<double> &y, double xe)
{
    size_t k = 0;
    while (k + 2 < x.size() && xe > x[k + 1])
        ++k;
    const double t = (xe - x[k]) / (x[k + 1] - x[k]);
    return y[k] + t * (y[k + 1] - y[k]);
}

} // namespace

int runSuite106NumericsRound3(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 106: numerics round 3 (fit bounds KKT, undefined r2, "
                 "interpolation range, ignored fit parameters)...\n";

    struct NonInteractiveQuitGuard106
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard106()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard106()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard106;

    struct AppWinGuard106
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard106(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard106() { reset(); }
        AppWinGuard106(const AppWinGuard106 &) = delete;
        AppWinGuard106 &operator=(const AppWinGuard106 &) = delete;
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

    AppWinGuard106 w(new ApplicationWindow());
    w->applyUserSettings();
    w->confirmCloseTable = false;
    w->confirmClosePlot2D = false;
    w->confirmClosePlot3D = false;
    w->confirmCloseFolder = false;
    w->resize(1280, 800);
    w->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "106: pythonAutomationServer null");

    // Every sub-case is checked; the suite fails at the end when any check failed.
    int failures = 0;
#define EXPECT106(cond, msg)                                                                     \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;          \
            ++failures;                                                                          \
        }                                                                                        \
    } while (0)

    int rpcId = 60000;
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
    auto result = [](const QJsonObject &resp) {
        return resp.value(QStringLiteral("result")).toObject();
    };
    auto errMsg = [](const QJsonObject &resp) {
        return resp.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
    };
    auto errCode = [](const QJsonObject &resp) {
        return resp.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt();
    };
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto num = [&](const QJsonValue &v) { return v.isDouble() ? v.toDouble() : nan; };
    auto close = [](double a, double b, double rel) {
        return std::isfinite(a) && std::isfinite(b)
                && std::fabs(a - b) <= rel * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    };

    int tableNo = 0;
    // Two-column table plotted as a line; returns the graph window name ("" on failure).
    auto plotXY = [&](const std::vector<double> &x, const std::vector<double> &y) -> QString {
        const QString tName = QStringLiteral("T106_%1").arg(++tableNo);
        Table *t = w->newTable(int(x.size()), 2, tName, tName);
        if (!t)
            return QString();
        t->setColName(0, QStringLiteral("X"));
        t->setColName(1, QStringLiteral("Y"));
        for (int r = 0; r < int(x.size()); ++r) {
            t->setCell(r, 0, x[size_t(r)]);
            t->setCell(r, 1, y[size_t(r)]);
        }
        MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
        QCoreApplication::processEvents();
        return (ml && ml->activeGraph() && ml->activeGraph()->curves() == 1) ? ml->name()
                                                                              : QString();
    };

    // y = 2 + 5 exp(-x / 1.7) + N(0, 0.05), x = 0..10 step 0.25 (41 points)
    std::vector<double> ex, ey;
    {
        SplitMix106 rng(15);
        for (int i = 0; i <= 40; ++i) {
            const double xv = 0.25 * i;
            ex.push_back(xv);
            ey.push_back(2.0 + 5.0 * std::exp(-xv / 1.7) + 0.05 * rng.gauss());
        }
    }
    const QString gExp = plotXY(ex, ey);
    CHECK_COND(!gExp.isEmpty(), "106: exponential test graph not created");
    auto expFit = [&](QJsonObject extra) -> QJsonObject {
        QJsonObject p{ { "graph", gExp },
                       { "curve", 0 },
                       { "formula", "y0+A*exp(-x/t)" },
                       { "param_names", QJsonArray{ "y0", "A", "t" } },
                       { "plot_curve", false } };
        for (auto it = extra.begin(); it != extra.end(); ++it)
            p[it.key()] = it.value();
        return rpc(QStringLiteral("analysis.fit_nonlinear"), p);
    };

    // -------------------------------------------------------------------------
    // 106a: a solution on a bound is converged, whatever the initial value
    // -------------------------------------------------------------------------
    {
        const QJsonObject ref = result(expFit(QJsonObject{
                { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "t", 3 } } },
                { "fixed", QJsonArray{ "t" } } }));
        const double refY0 = num(ref.value("params").toObject().value("y0"));
        const double refA = num(ref.value("params").toObject().value("A"));
        EXPECT106(ref.value("converged").toBool() && std::isfinite(refY0) && std::isfinite(refA),
                  "106a: the reference fit with t fixed at 3 must converge");
        for (double t0 : { 1.0, 3.0, 4.0, 4.9, 6.0 }) {
            const QJsonObject r = result(expFit(QJsonObject{
                    { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "t", t0 } } },
                    { "bounds", QJsonObject{ { "t", QJsonArray{ 3, 5 } } } } }));
            const QJsonObject pr = r.value("params").toObject();
            const double t = num(pr.value("t"));
            const QString notes = r.value("notes").toVariant().toStringList().join(" | ");
            std::cout << "[INFO] 106a t0=" << t0 << ": converged " << r.value("converged").toBool()
                      << ", t " << t << ", y0 " << num(pr.value("y0")) << " (ref " << refY0
                      << "), A " << num(pr.value("A")) << " (ref " << refA << "), message '"
                      << r.value("convergence_message").toString().toStdString() << "', notes '"
                      << notes.toStdString() << "'\n";
            EXPECT106(r.value("ok").toBool() && r.value("converged").toBool(),
                      "106a: t0=" << t0 << ": a solution on the bound t = 3 must be converged");
            EXPECT106(close(t, 3.0, 1e-9), "106a: t0=" << t0 << ": t must be the bound 3, got " << t);
            EXPECT106(close(num(pr.value("y0")), refY0, 1e-5) && close(num(pr.value("A")), refA, 1e-5),
                      "106a: t0=" << t0 << ": y0 / A must equal the fit with t fixed at 3");
            // "t = 3 is at a bound of its allowed range." (translated in a Japanese UI)
            EXPECT106(notes.contains(QLatin1String("t = 3")),
                      "106a: t0=" << t0 << ": notes must say that t is at its bound");
        }
        // The upper bound of [0.1, 1] and a one-sided lower bound [3, open) bind as well.
        struct BCase
        {
            double lo, hi, tFix, t0;
        };
        const BCase bcases[] = { { 0.1, 1.0, 1.0, 0.5 }, { 0.1, 1.0, 1.0, 2.0 },
                                 { 3.0, nan, 3.0, 1.0 }, { 3.0, nan, 3.0, 8.0 } };
        for (const BCase &b : bcases) {
            const QJsonObject rf = result(expFit(QJsonObject{
                    { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "t", b.tFix } } },
                    { "fixed", QJsonArray{ "t" } } }));
            const QJsonArray range{ b.lo, std::isfinite(b.hi) ? QJsonValue(b.hi) : QJsonValue() };
            const QJsonObject r = result(expFit(QJsonObject{
                    { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "t", b.t0 } } },
                    { "bounds", QJsonObject{ { "t", range } } } }));
            const QJsonObject pr = r.value("params").toObject();
            const QString notes = r.value("notes").toVariant().toStringList().join(" | ");
            std::cout << "[INFO] 106a bounds [" << b.lo << ", " << b.hi << "] t0=" << b.t0
                      << ": converged " << r.value("converged").toBool() << ", t "
                      << num(pr.value("t")) << ", notes '" << notes.toStdString() << "'\n";
            EXPECT106(r.value("converged").toBool() && close(num(pr.value("t")), b.tFix, 1e-9)
                              && close(num(pr.value("y0")),
                                       num(rf.value("params").toObject().value("y0")), 1e-5)
                              && close(num(pr.value("A")),
                                       num(rf.value("params").toObject().value("A")), 1e-5),
                      "106a: bounds [" << b.lo << ", " << b.hi << "] t0=" << b.t0
                                       << ": must converge to the fit with t fixed at " << b.tFix);
            EXPECT106(notes.contains(QStringLiteral("t = %1").arg(b.tFix)),
                      "106a: bounds [" << b.lo << ", " << b.hi << "] t0=" << b.t0
                                       << ": notes must say that t is at its bound");
        }
        // A bound that does not bind (true t ~ 1.7): no note, converged.
        const QJsonObject r = result(expFit(QJsonObject{
                { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "t", 1 } } },
                { "bounds", QJsonObject{ { "t", QJsonArray{ 0.5, 3 } } } } }));
        const double t = num(r.value("params").toObject().value("t"));
        EXPECT106(r.value("converged").toBool() && t > 1.5 && t < 1.9,
                  "106a: an inactive bound must converge inside the box (t " << t << ")");
        EXPECT106(r.value("notes").toArray().isEmpty(),
                  "106a: an inactive bound must not be reported as reached");
    }

    // -------------------------------------------------------------------------
    // 106b: undefined r2 / adj_r2 are null, a failed fit says why
    // -------------------------------------------------------------------------
    {
        const QString g3 = plotXY({ 0.0, 1.0, 2.0 }, { 7.0, 4.0, 3.0 });
        const QJsonObject r = result(rpc(QStringLiteral("analysis.fit_nonlinear"),
                                         QJsonObject{ { "graph", g3 },
                                                      { "curve", 0 },
                                                      { "formula", "y0+A*exp(-x/t)" },
                                                      { "param_names", QJsonArray{ "y0", "A", "t" } },
                                                      { "init_params", QJsonObject{ { "y0", 2 }, { "A", 5 }, { "t", 1 } } },
                                                      { "plot_curve", false } }));
        std::cout << "[INFO] 106b n = p = 3: chi2_dof null " << r.value("chi2_dof").isNull()
                  << ", r2 " << num(r.value("r2")) << " (null " << r.value("r2").isNull()
                  << "), adj_r2 null " << r.value("adj_r2").isNull() << " (value "
                  << num(r.value("adj_r2")) << ")\n";
        EXPECT106(r.value("chi2_dof").isNull(), "106b: n = p: chi2_dof must be null");
        EXPECT106(r.value("adj_r2").isNull(), "106b: n = p: adj_r2 is not defined and must be null, got "
                                                      << num(r.value("adj_r2")));

        const QJsonObject d = result(expFit(QJsonObject{
                { "init_params", QJsonObject{ { "y0", 0 }, { "A", 1e6 }, { "t", -0.01 } } } }));
        bool allNull = true;
        const QJsonObject dp = d.value("params").toObject();
        for (auto it = dp.begin(); it != dp.end(); ++it)
            allNull = allNull && it.value().isNull();
        std::cout << "[INFO] 106b diverging fit: params all null " << allNull << ", r2 null "
                  << d.value("r2").isNull() << ", adj_r2 null " << d.value("adj_r2").isNull()
                  << ", message '" << d.value("convergence_message").toString().toStdString()
                  << "'\n";
        EXPECT106(!d.value("converged").toBool(), "106b: the diverging fit must not be converged");
        if (allNull) {
            EXPECT106(d.value("r2").isNull() && d.value("adj_r2").isNull(),
                      "106b: a failed fit must give r2 = adj_r2 = null");
        }
        EXPECT106(!d.value("convergence_message").toString().trimmed().isEmpty(),
                  "106b: a failed fit must say why in convergence_message");
    }

    // -------------------------------------------------------------------------
    // 106c: interpolation range
    // -------------------------------------------------------------------------
    {
        const std::vector<double> ix{ 0.0, 1.0, 2.5, 3.0, 4.5, 6.0 };
        std::vector<double> iy;
        for (double v : ix)
            iy.push_back(std::sin(v));
        const QString gi = plotXY(ix, iy);
        CHECK_COND(!gi.isEmpty(), "106c: interpolation test graph not created");
        MultiLayer *mli = qobject_cast<MultiLayer *>(w->window(gi));
        CHECK_COND(mli && mli->activeGraph(), "106c: interpolation graph not found");
        auto outCurve = [&](const QJsonObject &r, std::vector<double> &X, std::vector<double> &Y) {
            X.clear();
            Y.clear();
            Graph *g = mli->activeGraph();
            const int idx = g->curveIndex(r.value("output_curve").toString());
            QwtPlotCurve *c = (idx >= 0) ? g->curve(idx) : nullptr;
            if (!c)
                return false;
            for (int i = 0; i < int(c->dataSize()); ++i) {
                X.push_back(c->x(i));
                Y.push_back(c->y(i));
            }
            return true;
        };
        auto interp = [&](const QString &m, QJsonValue lo, QJsonValue hi, int pts) {
            QJsonObject p{ { "graph", gi }, { "curve", 0 }, { "method", m }, { "points", pts } };
            if (!lo.isUndefined())
                p["x_min"] = lo;
            if (!hi.isUndefined())
                p["x_max"] = hi;
            return rpc(QStringLiteral("curvemath.interpolate"), p);
        };

        struct ICase
        {
            const char *method;
            double lo, hi;
            int pts;
            double gridLo, gridHi;
            bool warned;
        };
        const ICase cases[] = { { "linear", 0.5, 5.5, 6, 0.5, 5.5, false },
                                { "spline", 0.5, 5.5, 11, 0.5, 5.5, false },
                                { "linear", 2.0, 2.9, 4, 2.0, 2.9, false },
                                { "spline", 2.2, 3.6, 8, 2.2, 3.6, false },
                                { "linear", -1.0, 3.0, 5, 0.0, 3.0, true } };
        for (const ICase &c : cases) {
            const QJsonObject resp = interp(QLatin1String(c.method), c.lo, c.hi, c.pts);
            const QJsonObject r = result(resp);
            std::vector<double> X, Y;
            const bool got = r.value("ok").toBool() && outCurve(r, X, Y);
            double worst = got ? 0.0 : nan;
            for (size_t i = 0; got && i < X.size(); ++i) {
                const double ref = (QLatin1String(c.method) == QLatin1String("linear"))
                        ? linearInterp106(ix, iy, X[i])
                        : naturalSpline106(ix, iy, X[i]);
                worst = std::max(worst, std::fabs(Y[i] - ref));
            }
            const QString warn = r.value("warnings").toVariant().toStringList().join(" | ");
            std::cout << "[INFO] 106c " << c.method << " [" << c.lo << ", " << c.hi << "]: ok "
                      << r.value("ok").toBool() << ", " << X.size() << " points "
                      << (X.empty() ? nan : X.front()) << " .. " << (X.empty() ? nan : X.back())
                      << ", worst |y - ref| " << worst << ", warnings '" << warn.toStdString()
                      << "', error '" << errMsg(resp).toStdString() << "'\n";
            EXPECT106(got && int(X.size()) == c.pts,
                      "106c: " << c.method << " [" << c.lo << ", " << c.hi << "] must give "
                               << c.pts << " points");
            EXPECT106(got && !X.empty() && close(X.front(), c.gridLo, 1e-12)
                              && close(X.back(), c.gridHi, 1e-12),
                      "106c: " << c.method << " [" << c.lo << ", " << c.hi
                               << "]: the grid must span the range (within the data)");
            EXPECT106(got && worst < 1e-9,
                      "106c: " << c.method << " [" << c.lo << ", " << c.hi
                               << "]: values must come from the interpolant through every point ("
                               << worst << ")");
            EXPECT106(warn.isEmpty() != c.warned,
                      "106c: " << c.method << " [" << c.lo << ", " << c.hi << "]: warning "
                               << (c.warned ? "expected" : "unexpected"));
        }
        const QJsonObject out = interp(QStringLiteral("linear"), 7.0, 9.0, 5);
        std::cout << "[INFO] 106c range outside the data: error '" << errMsg(out).toStdString()
                  << "'\n";
        EXPECT106(errCode(out) == -32602 && errMsg(out).contains(QLatin1String("range")),
                  "106c: a range outside the data must be an invalid-params error saying why");
    }

    // -------------------------------------------------------------------------
    // 106d: unknown parameter names, one-sided X range
    // -------------------------------------------------------------------------
    {
        const QJsonObject init{ { "y0", 1 }, { "A", 4 }, { "t", 1 } };
        struct UCase
        {
            const char *label;
            QJsonObject extra;
        };
        const UCase ucases[] = {
            { "fixed Y0", QJsonObject{ { "init_params", init }, { "fixed", QJsonArray{ "Y0" } } } },
            { "init_params tau",
              QJsonObject{ { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "tau", 1 } } } } },
            { "bounds T",
              QJsonObject{ { "init_params", init }, { "bounds", QJsonObject{ { "T", QJsonArray{ 0, 1 } } } } } },
        };
        for (const UCase &u : ucases) {
            const QJsonObject resp = expFit(u.extra);
            std::cout << "[INFO] 106d " << u.label << ": error " << errCode(resp) << " '"
                      << errMsg(resp).toStdString() << "'\n";
            EXPECT106(errCode(resp) == -32602 && errMsg(resp).contains(QLatin1String("y0"))
                              && errMsg(resp).contains(QLatin1String("A"))
                              && errMsg(resp).contains(QLatin1String("t")),
                      "106d: " << u.label << ": an unknown name must be an invalid-params error "
                                              "listing the parameter names");
        }
        // Known names still work.
        const QJsonObject okFit = result(expFit(QJsonObject{
                { "init_params", init }, { "fixed", QJsonArray{ "y0" } },
                { "bounds", QJsonObject{ { "t", QJsonArray{ 0.1, 10 } } } } }));
        EXPECT106(okFit.value("converged").toBool(), "106d: known names must still fit");

        // fit_nonlinear: x_min alone == [x_min, last x]; x_max alone == [first x, x_max]
        auto paramsOf = [&](const QJsonObject &r) {
            const QJsonObject p = r.value("params").toObject();
            return std::vector<double>{ num(p.value("y0")), num(p.value("A")), num(p.value("t")) };
        };
        auto sameParams = [&](const std::vector<double> &a, const std::vector<double> &b) {
            for (size_t i = 0; i < a.size(); ++i)
                if (!close(a[i], b[i], 1e-9))
                    return false;
            return true;
        };
        const auto full = paramsOf(result(expFit(QJsonObject{ { "init_params", init } })));
        const auto lo1 = paramsOf(result(expFit(QJsonObject{ { "init_params", init }, { "x_min", 2.0 } })));
        const auto lo2 = paramsOf(result(
                expFit(QJsonObject{ { "init_params", init }, { "x_min", 2.0 }, { "x_max", 10.0 } })));
        const auto hi1 = paramsOf(result(expFit(QJsonObject{ { "init_params", init }, { "x_max", 6.0 } })));
        const auto hi2 = paramsOf(result(
                expFit(QJsonObject{ { "init_params", init }, { "x_min", 0.0 }, { "x_max", 6.0 } })));
        const auto hiN = paramsOf(result(expFit(QJsonObject{
                { "init_params", init }, { "x_min", QJsonValue::Null }, { "x_max", 6.0 } })));
        std::cout << "[INFO] 106d fit_nonlinear t: all " << full[2] << ", x_min=2 " << lo1[2]
                  << " (explicit " << lo2[2] << "), x_max=6 " << hi1[2] << " (explicit " << hi2[2]
                  << ", x_min null " << hiN[2] << ")\n";
        EXPECT106(sameParams(lo1, lo2) && !sameParams(lo1, full),
                  "106d: fit_nonlinear x_min alone must fit [x_min, last x]");
        EXPECT106(sameParams(hi1, hi2) && sameParams(hiN, hi2),
                  "106d: fit_nonlinear x_max alone (or x_min null) must fit [first x, x_max]");

        auto linN = [&](QJsonObject extra) {
            QJsonObject p{ { "graph", gExp }, { "curve", 0 }, { "plot_curve", false } };
            for (auto it = extra.begin(); it != extra.end(); ++it)
                p[it.key()] = it.value();
            const QJsonObject r = result(rpc(QStringLiteral("analysis.fit_linear"), p));
            return std::make_pair(r.value("n").toInt(-1), num(r.value("slope")));
        };
        const auto l1 = linN(QJsonObject{ { "x_min", 5.0 } });
        const auto l2 = linN(QJsonObject{ { "x_max", 5.0 } });
        std::cout << "[INFO] 106d fit_linear n: x_min=5 " << l1.first << ", x_max=5 " << l2.first
                  << " (of 41)\n";
        EXPECT106(l1.first == 21 && l2.first == 21,
                  "106d: fit_linear with one side of the range must use 21 points (got "
                          << l1.first << ", " << l2.first << ")");

        auto area = [&](QJsonObject extra) {
            QJsonObject p{ { "graph", gExp }, { "curve", 0 } };
            for (auto it = extra.begin(); it != extra.end(); ++it)
                p[it.key()] = it.value();
            return num(result(rpc(QStringLiteral("curvemath.integrate"), p)).value("area"));
        };
        const double aHi1 = area(QJsonObject{ { "x_max", 4.0 } });
        const double aHi2 = area(QJsonObject{ { "x_min", 0.0 }, { "x_max", 4.0 } });
        const double aLo1 = area(QJsonObject{ { "x_min", 4.0 } });
        const double aLo2 = area(QJsonObject{ { "x_min", 4.0 }, { "x_max", 10.0 } });
        std::cout << "[INFO] 106d integrate: x_max=4 " << aHi1 << " (explicit " << aHi2
                  << "), x_min=4 " << aLo1 << " (explicit " << aLo2 << ")\n";
        EXPECT106(close(aHi1, aHi2, 1e-12) && close(aLo1, aLo2, 1e-12),
                  "106d: integrate with one side of the range must use the end of the data");

        auto derivRows = [&](QJsonObject extra) {
            QJsonObject p{ { "graph", gExp }, { "curve", 0 } };
            for (auto it = extra.begin(); it != extra.end(); ++it)
                p[it.key()] = it.value();
            const QJsonObject r = result(rpc(QStringLiteral("curvemath.differentiate"), p));
            Table *t = w->table(r.value("table").toString());
            if (!t || !t->column(0))
                return std::make_pair(-1, nan);
            int n = 0;
            double first = nan;
            for (int i = 0; i < t->column(0)->rowCount(); ++i) {
                const double v = t->column(0)->valueAt(i);
                if (std::isfinite(v) && !t->column(0)->isInvalid(i)) {
                    if (n == 0)
                        first = v;
                    ++n;
                }
            }
            return std::make_pair(n, first);
        };
        const auto d1 = derivRows(QJsonObject{ { "x_min", 5.0 } });
        const auto d2 = derivRows(QJsonObject{ { "x_min", 5.0 }, { "x_max", 10.0 } });
        std::cout << "[INFO] 106d differentiate x_min=5: " << d1.first << " rows from " << d1.second
                  << " (explicit " << d2.first << " from " << d2.second << ")\n";
        EXPECT106(d1.first > 0 && d1.first == d2.first && close(d1.second, d2.second, 1e-12),
                  "106d: differentiate with x_min alone must use [x_min, last x]");
    }

    // -------------------------------------------------------------------------
    // 106e: the global (multi-curve) fit judges a parameter on its bound the same way
    // -------------------------------------------------------------------------
    {
        SplitMix106 rng(27);
        std::vector<double> gx, gy1, gy2;
        for (int i = 0; i <= 40; ++i) {
            const double xv = 0.25 * i;
            gx.push_back(xv);
            gy1.push_back(2.0 + 5.0 * std::exp(-xv / 1.7) + 0.05 * rng.gauss());
            gy2.push_back(1.0 + 3.0 * std::exp(-xv / 1.7) + 0.05 * rng.gauss());
        }
        const QString tName = QStringLiteral("T106_G");
        Table *t = w->newTable(int(gx.size()), 3, tName, tName);
        CHECK_COND(t != nullptr, "106e: table not created");
        t->setColName(0, QStringLiteral("X"));
        t->setColName(1, QStringLiteral("Y1"));
        t->setColName(2, QStringLiteral("Y2"));
        for (int r = 0; r < int(gx.size()); ++r) {
            t->setCell(r, 0, gx[size_t(r)]);
            t->setCell(r, 1, gy1[size_t(r)]);
            t->setCell(r, 2, gy2[size_t(r)]);
        }
        MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1) << t->colName(2),
                                           Graph::Line);
        QCoreApplication::processEvents();
        Graph *g = ml ? ml->activeGraph() : nullptr;
        CHECK_COND(g && g->curves() == 2, "106e: two-curve graph not created");
        const QStringList titles{ g->curve(0)->title().text(), g->curve(1)->title().text() };
        const QVector<bool> shared{ false, false, true };

        // y0, A per curve, t shared: bounded to [lo, hi] (hi may be infinite) or fixed at t0.
        auto globalFit = [&](double t0, double lo, double hi, bool fixT, std::vector<double> &res,
                             QStringList &notes) {
            NonLinearFit nlf(w, g);
            nlf.setSilent(true);
            nlf.setParametersList(QStringList{ "y0", "A", "t" });
            nlf.setFormula(QStringLiteral("y0+A*exp(-x/t)"));
            nlf.setInitialGuess(0, 1.0);
            nlf.setInitialGuess(1, 4.0);
            nlf.setInitialGuess(2, t0);
            if (fixT)
                nlf.setParameterFixed(2, true);
            else
                nlf.setParameterBounds(2, lo, hi);
            const bool ok = nlf.fitGlobal(titles, shared, nullptr);
            res = nlf.results();
            notes = nlf.fitNotes();
            return ok && nlf.converged();
        };
        const double inf = std::numeric_limits<double>::infinity();
        struct GCase
        {
            double lo, hi, t0, tBound;
        };
        const GCase gcases[] = { { 3.0, 5.0, 1.0, 3.0 }, { 3.0, 5.0, 3.0, 3.0 },
                                 { 3.0, 5.0, 4.9, 3.0 }, { 3.0, 5.0, 6.0, 3.0 },
                                 { 3.0, inf, 1.0, 3.0 }, { 0.1, 1.0, 2.0, 1.0 } };
        for (const GCase &c : gcases) {
            std::vector<double> ref, res;
            QStringList refNotes, notes;
            const bool refConv = globalFit(c.tBound, 0.0, 0.0, true, ref, refNotes);
            const bool conv = globalFit(c.t0, c.lo, c.hi, false, res, notes);
            const bool same = refConv && res.size() == 3 && ref.size() == 3
                    && close(res[2], c.tBound, 1e-9) && close(res[0], ref[0], 1e-4)
                    && close(res[1], ref[1], 1e-4);
            std::cout << "[INFO] 106e global bounds [" << c.lo << ", " << c.hi << "] t0=" << c.t0
                      << ": converged " << conv << ", y0 " << (res.size() == 3 ? res[0] : nan)
                      << " A " << (res.size() == 3 ? res[1] : nan) << " t "
                      << (res.size() == 3 ? res[2] : nan) << " (t fixed: y0 "
                      << (ref.size() == 3 ? ref[0] : nan) << " A " << (ref.size() == 3 ? ref[1] : nan)
                      << "), notes '" << notes.join(" | ").toStdString() << "'\n";
            EXPECT106(conv, "106e: global bounds [" << c.lo << ", " << c.hi << "] t0=" << c.t0
                                                    << ": a solution on the bound must be converged");
            EXPECT106(same, "106e: global bounds [" << c.lo << ", " << c.hi << "] t0=" << c.t0
                                                    << ": must equal the fit with t fixed at the bound");
            EXPECT106(notes.join(" ").contains(QStringLiteral("t = %1").arg(c.tBound)),
                      "106e: global bounds [" << c.lo << ", " << c.hi << "] t0=" << c.t0
                                              << ": notes must say that t is at its bound");
        }

        // analysis.fit_global on noise-free decays (the SDK e2e case): converged, no notes.
        std::vector<double> ex2, ya, yb;
        for (int xv = 0; xv <= 90; xv += 3) {
            ex2.push_back(xv);
            ya.push_back(5.0 + 80.0 * std::exp(-xv / 18.0));
            yb.push_back(12.0 + 140.0 * std::exp(-xv / 18.0));
        }
        Table *t2 = w->newTable(int(ex2.size()), 3, QStringLiteral("T106_G2"),
                                QStringLiteral("T106_G2"));
        CHECK_COND(t2 != nullptr, "106e: table 2 not created");
        for (int r = 0; r < int(ex2.size()); ++r) {
            t2->setCell(r, 0, ex2[size_t(r)]);
            t2->setCell(r, 1, ya[size_t(r)]);
            t2->setCell(r, 2, yb[size_t(r)]);
        }
        MultiLayer *ml2 = w->multilayerPlot(t2, QStringList() << t2->colName(1) << t2->colName(2),
                                            Graph::Line);
        QCoreApplication::processEvents();
        CHECK_COND(ml2 != nullptr, "106e: graph 2 not created");
        const QJsonObject gr = result(rpc(QStringLiteral("analysis.fit_global"),
                                          QJsonObject{ { "graph", ml2->name() },
                                                       { "model", "ExpDec1" },
                                                       { "shared", QJsonArray{ "t1" } } }));
        std::cout << "[INFO] 106e fit_global ExpDec1: ok " << gr.value("ok").toBool()
                  << ", converged " << gr.value("converged").toBool() << ", message '"
                  << gr.value("convergence_message").toString().toStdString() << "'\n";
        EXPECT106(gr.value("ok").toBool() && gr.value("converged").toBool()
                          && gr.value("notes").toArray().isEmpty(),
                  "106e: fit_global on noise-free decays must converge without notes");
    }

    // -------------------------------------------------------------------------
    // 106f: reversed bounds are an error naming the parameter
    // -------------------------------------------------------------------------
    {
        const QJsonObject resp = expFit(QJsonObject{
                { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "t", 1 } } },
                { "bounds", QJsonObject{ { "t", QJsonArray{ 3, 1 } } } } });
        std::cout << "[INFO] 106f bounds t [3, 1]: error " << errCode(resp) << " '"
                  << errMsg(resp).toStdString() << "'\n";
        EXPECT106(errCode(resp) == -32602 && errMsg(resp).contains(QLatin1String("'t'")),
                  "106f: bounds with lower > upper must be an invalid-params error naming 't'");
        const QJsonObject okResp = expFit(QJsonObject{
                { "init_params", QJsonObject{ { "y0", 1 }, { "A", 4 }, { "t", 1 } } },
                { "bounds", QJsonObject{ { "t", QJsonArray{ 1, 1 } } } } });
        EXPECT106(!okResp.contains(QStringLiteral("error")),
                  "106f: equal bounds (t held at 1) must still be accepted");
    }

    // -------------------------------------------------------------------------
    // 106g: a repeated X (or X out of order) outside the range: the interpolant is built from the
    // points in the range and one point on each side (GUI Interpolation and RPC alike)
    // -------------------------------------------------------------------------
    {
        struct GCase
        {
            const char *label;
            std::vector<double> x;
            double lo, hi;
            int method; // Interpolation::Linear / Cubic
            bool ok;
            std::vector<double> wx; // window the interpolant must go through (ok cases)
        };
        std::vector<double> dupOut{ 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        std::vector<double> dupIn{ 0, 1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 10 };
        std::vector<double> unordered{ 3, 0, 1, 2, 4, 5, 6, 7, 8, 9, 10 };
        const std::vector<double> win19{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        const std::vector<double> win410{ 4, 5, 6, 7, 8, 9, 10 };
        const GCase gcases[] = {
            { "dup x=0, linear [2, 8]", dupOut, 2.0, 8.0, Interpolation::Linear, true, win19 },
            { "dup x=0, cubic [2, 8]", dupOut, 2.0, 8.0, Interpolation::Cubic, true, win19 },
            { "dup x=5 inside, linear [2, 8]", dupIn, 2.0, 8.0, Interpolation::Linear, false, {} },
            { "dup x=0, whole range", dupOut, 0.0, 10.0, Interpolation::Linear, false, {} },
            { "x=3 first, cubic [5, 9]", unordered, 5.0, 9.0, Interpolation::Cubic, true, win410 },
        };
        int gNo = 0;
        for (const GCase &c : gcases) {
            std::vector<double> y;
            for (size_t i = 0; i < c.x.size(); ++i)
                y.push_back(std::sin(0.7 * c.x[i]) + 0.1 * double(i)); // repeated X: other Y
            const QString gname = plotXY(c.x, y);
            MultiLayer *gml = qobject_cast<MultiLayer *>(w->window(gname));
            Graph *g = gml ? gml->activeGraph() : nullptr;
            EXPECT106(g != nullptr, "106g: " << c.label << ": graph not created");
            if (!g)
                continue;
            ++gNo;
            const QString title = g->curve(0)->title().text();
            std::vector<double> wy;
            for (double xv : c.wx) {
                for (size_t i = 0; i < c.x.size(); ++i)
                    if (c.x[i] == xv) {
                        wy.push_back(y[i]);
                        break;
                    }
            }
            auto refAt = [&](double xe) {
                return (c.method == Interpolation::Linear) ? linearInterp106(c.wx, wy, xe)
                                                           : naturalSpline106(c.wx, wy, xe);
            };
            auto lastCurveWorst = [&](int pts) {
                QwtPlotCurve *oc = g->curve(g->curves() - 1);
                if (!oc || int(oc->dataSize()) != pts)
                    return nan;
                double worst = 0.0;
                for (int i = 0; i < pts; ++i)
                    worst = std::max(worst, std::fabs(oc->y(i) - refAt(oc->x(i))));
                return worst;
            };
            // GUI: Analysis > Interpolate
            {
                const int before = g->curves();
                Interpolation gi(w, g, title, c.lo, c.hi, c.method);
                gi.setOutputPoints(13);
                const bool ran = !gi.error() && gi.run() && g->curves() > before;
                const double worst = ran ? lastCurveWorst(13) : nan;
                std::cout << "[INFO] 106g GUI " << c.label << ": ran " << ran << ", "
                          << gi.dataSize() << " points used, worst " << worst << "\n";
                if (c.ok)
                    EXPECT106(ran && int(gi.dataSize()) == int(c.wx.size()) && worst < 1e-9,
                              "106g: GUI " << c.label << " must interpolate through the range "
                                           << "and one point on each side");
                else
                    EXPECT106(!ran, "106g: GUI " << c.label << " must stay an error");
            }
            // RPC: curvemath.interpolate
            {
                const int before = g->curves();
                const QJsonObject resp = rpc(
                        QStringLiteral("curvemath.interpolate"),
                        QJsonObject{ { "graph", gname },
                                     { "curve", 0 },
                                     { "method",
                                       c.method == Interpolation::Linear ? "linear" : "cubic" },
                                     { "points", 13 },
                                     { "x_min", c.lo },
                                     { "x_max", c.hi } });
                const bool ran = result(resp).value("ok").toBool() && g->curves() > before;
                const double worst = ran ? lastCurveWorst(13) : nan;
                std::cout << "[INFO] 106g RPC " << c.label << ": ok " << ran << ", worst "
                          << worst << ", error '" << errMsg(resp).toStdString() << "'\n";
                if (c.ok)
                    EXPECT106(ran && worst < 1e-9,
                              "106g: RPC " << c.label << " must interpolate as the GUI does");
                else
                    EXPECT106(errCode(resp) == -32602,
                              "106g: RPC " << c.label << " must stay an invalid-params error");
            }
        }
        EXPECT106(gNo == int(sizeof(gcases) / sizeof(gcases[0])), "106g: graphs missing");
    }

    // -------------------------------------------------------------------------
    // 106h: the convergence test does not depend on the size of the parameters. With the
    // default initial values, a*x+b on data of size 1e-20 gave r2 0.629 (0.638 is right), 1e-22
    // gave a ten times too large with r2 -54 and 1e-30 r2 -6e7, all "converged": the absolute
    // part (1e-6) of the step test passed for any step once the parameters were below 1e-20.
    // The single, global and implicit ODR fits must give the linear least squares solution at
    // 1e-20, 1e-22, 1e-30 and 1e+20 as well.
    // -------------------------------------------------------------------------
    {
        std::vector<double> hx, hBase;
        {
            SplitMix106 rng(5);
            for (int i = 0; i < 60; ++i) {
                const double xv = 10.0 * i / 59.0;
                hx.push_back(xv);
                hBase.push_back(3.0 * std::exp(-std::pow((xv - 4.2) / 1.3, 2)) + 0.5 * xv
                                + 0.05 * rng.gauss());
            }
        }
        // linear least squares (what numpy.polyfit(x, y, 1) gives) and its r2
        auto lineFit = [](const std::vector<double> &x, const std::vector<double> &y, double &a,
                          double &b, double &r2) {
            const double n = double(x.size());
            double mx = 0.0, my = 0.0;
            for (size_t i = 0; i < x.size(); ++i) {
                mx += x[i];
                my += y[i];
            }
            mx /= n;
            my /= n;
            double sxx = 0.0, sxy = 0.0, syy = 0.0;
            for (size_t i = 0; i < x.size(); ++i) {
                sxx += (x[i] - mx) * (x[i] - mx);
                sxy += (x[i] - mx) * (y[i] - my);
                syy += (y[i] - my) * (y[i] - my);
            }
            a = sxy / sxx;
            b = my - a * mx;
            double ss = 0.0;
            for (size_t i = 0; i < x.size(); ++i)
                ss += (y[i] - a * x[i] - b) * (y[i] - a * x[i] - b);
            r2 = 1.0 - ss / syy;
        };
        auto relClose = [](double v, double ref, double rel) {
            return std::isfinite(v) && std::fabs(v - ref) <= rel * std::fabs(ref);
        };
        for (double sc : { 1e-20, 1e-22, 1e-30, 1e+20 }) {
            std::vector<double> hy, hy2;
            for (size_t i = 0; i < hBase.size(); ++i) {
                hy.push_back(sc * hBase[i]);
                hy2.push_back(sc * (0.7 * hBase[i] + 0.3));
            }
            double aRef, bRef, r2Ref;
            lineFit(hx, hy, aRef, bRef, r2Ref);

            // analysis.fit_nonlinear with a formula and the default initial values
            const QString gname = plotXY(hx, hy);
            EXPECT106(!gname.isEmpty(), "106h: scale " << sc << ": graph not created");
            const QJsonObject r =
                    result(rpc(QStringLiteral("analysis.fit_nonlinear"),
                               QJsonObject{ { "graph", gname },
                                            { "curve", 0 },
                                            { "formula", "a*x+b" },
                                            { "param_names", QJsonArray{ "a", "b" } },
                                            { "plot_curve", false } }));
            const double a = num(r.value("params").toObject().value("a"));
            const double b = num(r.value("params").toObject().value("b"));
            const double r2 = num(r.value("r2"));
            std::cout << "[INFO] 106h scale " << sc << ": converged "
                      << r.value("converged").toBool() << ", a " << a << " (ref " << aRef
                      << "), b " << b << " (ref " << bRef << "), r2 " << r2 << " (ref " << r2Ref
                      << "), notes '"
                      << r.value("notes").toVariant().toStringList().join(" | ").toStdString()
                      << "'\n";
            EXPECT106(r.value("converged").toBool() && relClose(a, aRef, 1e-6)
                              && relClose(b, bRef, 1e-6) && std::fabs(r2 - r2Ref) < 1e-6,
                      "106h: scale " << sc
                                     << ": a*x+b must give the linear least squares solution");

            // the global fit (a and b per curve) runs through its own solver loop
            {
                const QString tName = QStringLiteral("T106_H%1").arg(++tableNo);
                Table *t = w->newTable(int(hx.size()), 3, tName, tName);
                EXPECT106(t != nullptr, "106h: global table not created");
                if (!t)
                    continue;
                for (int i = 0; i < int(hx.size()); ++i) {
                    t->setCell(i, 0, hx[size_t(i)]);
                    t->setCell(i, 1, hy[size_t(i)]);
                    t->setCell(i, 2, hy2[size_t(i)]);
                }
                MultiLayer *gml = w->multilayerPlot(
                        t, QStringList() << t->colName(1) << t->colName(2), Graph::Line);
                QCoreApplication::processEvents();
                Graph *gg = gml ? gml->activeGraph() : nullptr;
                EXPECT106(gg && gg->curves() == 2, "106h: global graph not created");
                if (!gg || gg->curves() != 2)
                    continue;
                NonLinearFit nlf(w, gg);
                nlf.setSilent(true);
                nlf.setParametersList(QStringList{ "a", "b" });
                nlf.setFormula(QStringLiteral("a*x+b"));
                const bool ok = nlf.fitGlobal(
                        QStringList{ gg->curve(0)->title().text(), gg->curve(1)->title().text() },
                        QVector<bool>{ false, false }, nullptr);
                const std::vector<double> res = nlf.results(); // curve 0
                std::cout << "[INFO] 106h global scale " << sc << ": converged "
                          << (ok && nlf.converged()) << ", curve 0 a "
                          << (res.size() == 2 ? res[0] : nan) << " b "
                          << (res.size() == 2 ? res[1] : nan) << "\n";
                EXPECT106(ok && nlf.converged() && res.size() == 2 && relClose(res[0], aRef, 1e-6)
                                  && relClose(res[1], bRef, 1e-6),
                          "106h: global scale " << sc
                                                << ": curve 0 must get its least squares line");
            }

            // the implicit ODR fit y - (a*x + b) = 0: for |a| << 1 the orthogonal distance is the
            // vertical one, so the least squares line again
            if (sc < 1.0) {
                const QJsonObject ro =
                        result(rpc(QStringLiteral("analysis.fit_implicit_odr"),
                                   QJsonObject{ { "graph", gname },
                                                { "curve", 0 },
                                                { "formula", "y - (a*x + b)" },
                                                { "param_names", QJsonArray{ "a", "b" } },
                                                { "plot_curve", false } }));
                const double ao = num(ro.value("params").toObject().value("a"));
                const double bo = num(ro.value("params").toObject().value("b"));
                std::cout << "[INFO] 106h ODR scale " << sc << ": converged "
                          << ro.value("converged").toBool() << ", a " << ao << ", b " << bo
                          << "\n";
                EXPECT106(ro.value("converged").toBool() && relClose(ao, aRef, 1e-6)
                                  && relClose(bo, bRef, 1e-6),
                          "106h: ODR scale " << sc << ": must give the least squares line");
            }
        }

        // A fit further from the data than their mean (r2 < 0) says so in the notes: a*x
        // (through the origin) on data around 10 with a falling slope.
        {
            std::vector<double> ny;
            for (double xv : hx)
                ny.push_back(10.0 - 0.3 * xv);
            const QString gname = plotXY(hx, ny);
            const QJsonObject r =
                    result(rpc(QStringLiteral("analysis.fit_nonlinear"),
                               QJsonObject{ { "graph", gname },
                                            { "curve", 0 },
                                            { "formula", "a*x" },
                                            { "param_names", QJsonArray{ "a" } },
                                            { "plot_curve", false } }));
            const QString notes = r.value("notes").toVariant().toStringList().join(" | ");
            std::cout << "[INFO] 106h a*x on 10-0.3x: r2 " << num(r.value("r2")) << ", notes '"
                      << notes.toStdString() << "'\n";
            EXPECT106(num(r.value("r2")) < 0.0 && notes.contains(QLatin1String("R^2")),
                      "106h: a negative r2 must be explained in the notes");
        }

        // 106i: the surface fit (analysis.fit_surface_3d, Analysis > Surface Fit) stopped on the
        // same absolute step test (1e-8). Custom "a*x+b*y+c" starts at a = b = c = 1; on data of
        // size 1e-22 / 1e-30 it must reach the linear least squares plane, at 1e+20 as well, and
        // r2 (0 < tss < 1e-30 was undefined) must be that of the plane.
        {
            std::vector<double> sx, sy, sBase;
            SplitMix106 rng(31);
            for (int i = 0; i < 9; ++i) {
                for (int j = 0; j < 9; ++j) {
                    const double xv = 0.5 * i, yv = 0.4 * j;
                    sx.push_back(xv);
                    sy.push_back(yv);
                    sBase.push_back(1.5 * xv - 0.8 * yv + 2.0 + 0.3 * std::sin(1.7 * xv * yv)
                                    + 0.05 * rng.gauss());
                }
            }
            for (double sc : { 1e-22, 1e-30, 1e+20 }) {
                std::vector<double> sz;
                for (double v : sBase)
                    sz.push_back(sc * v);
                // normal equations of z = a x + b y + c
                double A[3][4] = {};
                for (size_t i = 0; i < sz.size(); ++i) {
                    const double row[3] = { sx[i], sy[i], 1.0 };
                    for (int r = 0; r < 3; ++r) {
                        for (int c = 0; c < 3; ++c)
                            A[r][c] += row[r] * row[c];
                        A[r][3] += row[r] * sz[i];
                    }
                }
                for (int k = 0; k < 3; ++k)
                    for (int r = k + 1; r < 3; ++r) {
                        const double f = A[r][k] / A[k][k];
                        for (int c = k; c < 4; ++c)
                            A[r][c] -= f * A[k][c];
                    }
                double ref[3];
                for (int k = 2; k >= 0; --k) {
                    double s = A[k][3];
                    for (int c = k + 1; c < 3; ++c)
                        s -= A[k][c] * ref[c];
                    ref[k] = s / A[k][k];
                }
                double mz = 0.0;
                for (double v : sz)
                    mz += v;
                mz /= double(sz.size());
                double rss = 0.0, tss = 0.0;
                for (size_t i = 0; i < sz.size(); ++i) {
                    const double e = sz[i] - (ref[0] * sx[i] + ref[1] * sy[i] + ref[2]);
                    rss += e * e;
                    tss += (sz[i] - mz) * (sz[i] - mz);
                }
                const double r2Ref = 1.0 - rss / tss;

                const QString tName = QStringLiteral("T106_S%1").arg(++tableNo);
                Table *t = w->newTable(int(sz.size()), 3, tName, tName);
                EXPECT106(t != nullptr, "106i: table not created");
                if (!t)
                    continue;
                t->setColName(0, QStringLiteral("X"));
                t->setColName(1, QStringLiteral("Y"));
                t->setColName(2, QStringLiteral("Z"));
                for (int i = 0; i < int(sz.size()); ++i) {
                    t->setCell(i, 0, sx[size_t(i)]);
                    t->setCell(i, 1, sy[size_t(i)]);
                    t->setCell(i, 2, sz[size_t(i)]);
                }
                const GraphStatsAndSurfaceFit::SurfaceFitResult sf =
                        GraphStatsAndSurfaceFit::fitSurfaceXYZ(
                                w, t, QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
                                GraphStatsAndSurfaceFit::Custom2D, QStringLiteral("a*x+b*y+c"), 8);
                auto pv = [&](const char *nm) {
                    const int k = sf.paramNames.indexOf(QLatin1String(nm));
                    return (k >= 0 && k < sf.params.size()) ? sf.params[k] : nan;
                };
                std::cout << "[INFO] 106i surface scale " << sc << ": converged " << sf.converged
                          << " (" << sf.convergenceNote.toStdString() << "), a " << pv("a")
                          << " b " << pv("b") << " c " << pv("c") << " (ref " << ref[0] << " "
                          << ref[1] << " " << ref[2] << "), r2 " << sf.rSquare << " (ref "
                          << r2Ref << ")\n";
                EXPECT106(sf.converged && relClose(pv("a"), ref[0], 1e-6)
                                  && relClose(pv("b"), ref[1], 1e-6)
                                  && relClose(pv("c"), ref[2], 1e-6)
                                  && std::fabs(sf.rSquare - r2Ref) < 1e-6,
                          "106i: surface scale " << sc
                                                 << ": must give the least squares plane and r2");
            }
        }

        // 106j: the ROI quick fit gadget (Gauss) stopped on the absolute step test 1e-8 too, and
        // its r2 was 1 whenever rss <= 1e-24. On data of size 1e-22 and 1e+20 it must give the
        // centre, FWHM and r2 of the same data at size 1, and the amplitude scaled.
        {
            std::vector<double> gy0;
            SplitMix106 rng(41);
            for (double xv : hx)
                gy0.push_back(0.2 + 3.0 * std::exp(-2.0 * std::pow((xv - 4.2) / 1.1, 2))
                              + 0.03 * rng.gauss());
            struct QF
            {
                bool conv = false;
                double r2 = nan, xc = nan, fwhm = nan, amp = nan;
            };
            auto quickGauss = [&](double sc) {
                QF q;
                std::vector<double> yy;
                for (double v : gy0)
                    yy.push_back(sc * v);
                const QString gname = plotXY(hx, yy);
                MultiLayer *gml = qobject_cast<MultiLayer *>(w->window(gname));
                Graph *g = gml ? gml->activeGraph() : nullptr;
                if (!g)
                    return q;
                RoiGadgetTool *roi = RoiGadgetTool::attachGadget(w, g, RoiGadgetTool::QuickFit);
                if (!roi)
                    return q;
                roi->setTargetCurve(g->curve(0)->title().text());
                roi->setQuickFitModel(RoiGadgetTool::FitGauss);
                roi->setRoiRect(QRectF(-1.0, -sc, 12.0, 10.0 * sc));
                Table *ot = roi->createOutputTable();
                if (ot) {
                    for (int r = 0; r < ot->numRows(); ++r) {
                        const QString m = ot->text(r, 0);
                        const double v = ot->cell(r, 1);
                        if (m == QLatin1String("R2"))
                            q.r2 = v;
                        else if (m == QLatin1String("xc"))
                            q.xc = v;
                        else if (m == QLatin1String("FWHM"))
                            q.fwhm = v;
                        else if (m == QLatin1String("Amplitude"))
                            q.amp = v;
                        else if (m == QLatin1String("Converged"))
                            q.conv = (v == 1.0);
                    }
                }
                RoiGadgetTool::clearGadget(g);
                return q;
            };
            const QF ref = quickGauss(1.0);
            EXPECT106(ref.conv && ref.r2 > 0.99, "106j: quick Gauss at size 1 must converge");
            for (double sc : { 1e-22, 1e+20 }) {
                const QF q = quickGauss(sc);
                std::cout << "[INFO] 106j quick Gauss scale " << sc << ": converged " << q.conv
                          << ", r2 " << q.r2 << " (size 1: " << ref.r2 << "), xc " << q.xc
                          << " (" << ref.xc << "), FWHM " << q.fwhm << " (" << ref.fwhm
                          << "), amplitude " << q.amp << " (" << ref.amp * sc << ")\n";
                EXPECT106(q.conv && std::fabs(q.r2 - ref.r2) < 1e-6
                                  && relClose(q.xc, ref.xc, 1e-6)
                                  && relClose(q.fwhm, ref.fwhm, 1e-5)
                                  && relClose(q.amp, ref.amp * sc, 1e-5),
                          "106j: quick Gauss scale " << sc << ": must equal the fit at size 1");
            }
        }

        // 106k: Curve_R2 / Global_R2 of the global fit summary were undefined (blank) when
        // tss < 1e-30, i.e. for every curve of size 1e-22. Constant data (tss exactly 0) stay
        // undefined unless the model reproduces them.
        {
            struct KCase
            {
                double sc;
                bool constant;
                const char *formula;
            };
            const KCase kcases[] = { { 1e-22, false, "a*x+b" },
                                     { 1.0, true, "a*x+b" },
                                     { 1.0, true, "a*exp(x/20)" },
                                     { 1e-22, true, "a*exp(x/20)" } };
            for (const KCase &kc : kcases) {
                std::vector<double> y1, y2;
                for (size_t i = 0; i < hBase.size(); ++i) {
                    y1.push_back(kc.sc * (kc.constant ? 3.0 : hBase[i]));
                    y2.push_back(kc.sc * (kc.constant ? 3.0 : 0.7 * hBase[i] + 0.3));
                }
                const QString tName = QStringLiteral("T106_K%1").arg(++tableNo);
                Table *t = w->newTable(int(hx.size()), 3, tName, tName);
                EXPECT106(t != nullptr, "106k: table not created");
                if (!t)
                    continue;
                for (int i = 0; i < int(hx.size()); ++i) {
                    t->setCell(i, 0, hx[size_t(i)]);
                    t->setCell(i, 1, y1[size_t(i)]);
                    t->setCell(i, 2, y2[size_t(i)]);
                }
                MultiLayer *gml = w->multilayerPlot(
                        t, QStringList() << t->colName(1) << t->colName(2), Graph::Line);
                QCoreApplication::processEvents();
                Graph *gg = gml ? gml->activeGraph() : nullptr;
                EXPECT106(gg && gg->curves() == 2, "106k: graph not created");
                if (!gg || gg->curves() != 2)
                    continue;
                const QString f = QString::fromLatin1(kc.formula);
                const QStringList pn = f.contains(QLatin1String("b")) ? QStringList{ "a", "b" }
                                                                      : QStringList{ "a" };
                NonLinearFit nlf(w, gg);
                nlf.setSilent(true);
                nlf.setParametersList(pn);
                nlf.setFormula(f);
                Table *sum = nullptr;
                const bool ok = nlf.fitGlobal(
                        QStringList{ gg->curve(0)->title().text(), gg->curve(1)->title().text() },
                        QVector<bool>(pn.size(), false), &sum);
                std::vector<double> curveR2;
                double globalR2 = nan;
                if (sum) {
                    for (int r = 0; r < sum->numRows(); r += pn.size()) {
                        curveR2.push_back(sum->column(5)->isInvalid(r) ? nan : sum->cell(r, 5));
                        globalR2 = sum->column(6)->isInvalid(r) ? nan : sum->cell(r, 6);
                    }
                }
                std::cout << "[INFO] 106k global " << kc.formula << " scale " << kc.sc
                          << (kc.constant ? " constant" : "") << ": ok " << ok << ", Curve_R2";
                for (double v : curveR2)
                    std::cout << " " << v;
                std::cout << ", Global_R2 " << globalR2 << "\n";
                EXPECT106(ok && curveR2.size() == 2, "106k: summary table missing");
                if (curveR2.size() != 2)
                    continue;
                if (!kc.constant) {
                    double a1, b1, r21, a2, b2, r22;
                    lineFit(hx, y1, a1, b1, r21);
                    lineFit(hx, y2, a2, b2, r22);
                    EXPECT106(std::fabs(curveR2[0] - r21) < 1e-6 && std::fabs(curveR2[1] - r22) < 1e-6
                                      && globalR2 > std::min(r21, r22) - 1e-6
                                      && globalR2 < std::max(r21, r22) + 1e-6,
                              "106k: Curve_R2 / Global_R2 at scale " << kc.sc
                                                                     << " must be those of the lines");
                } else if (pn.size() == 2) {
                    EXPECT106(curveR2[0] == 1.0 && curveR2[1] == 1.0 && globalR2 == 1.0,
                              "106k: constant data reproduced by the model: R2 = 1");
                } else {
                    EXPECT106(std::isnan(curveR2[0]) && std::isnan(curveR2[1])
                                      && std::isnan(globalR2),
                              "106k: constant data not reproduced by the model: R2 undefined");
                }
            }
        }
    }
#undef EXPECT106

    w.reset();
    if (failures > 0) {
        std::cerr << "[FAIL] Suite 106: " << failures << " check(s) failed.\n";
        return 1;
    }
    std::cout << "[PASS] Suite 106 passed.\n";
    return 0;
}
