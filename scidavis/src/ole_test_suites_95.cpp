#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QJsonArray>
#include <QJsonObject>

#include <random>

// Suite 95: data-driven regressions for nonlinear fitting and peak analysis.
//   95a analysis.analyze_peaks honours 'centers' (and the GUI path: seedPeaksFromCenters + fit)
//       instead of replacing them with an automatic search inside fit()
//   95b parameters far below 1e-8 (GaussAmp with x in metres, ExpDec1 with x in seconds) move,
//       converge and get the same relative standard errors as the same data in nm / ns
//   95c peak models fitted to a dip start from A < 0 and converge to the dip (GaussAmp, Gauss,
//       Lorentz); an upward peak is unchanged
//   95d automatic peak search keeps a neighbour resolved by a valley (1.7 FWHM away) and no
//       fitted centre leaves the X range of the data (surplus peak_count)

int runSuite95FitDataDriven(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 95: data-driven fit & peak-analysis regressions...\n";

    struct NonInteractiveQuitGuard95
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard95()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard95()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard95;

    struct AppWinGuard95
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard95(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard95() { reset(); }
        AppWinGuard95(const AppWinGuard95 &) = delete;
        AppWinGuard95 &operator=(const AppWinGuard95 &) = delete;
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
        ApplicationWindow *get() const { return win; }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard95 w95(new ApplicationWindow());
    w95->applyUserSettings();
    w95->confirmCloseTable = false;
    w95->confirmClosePlot2D = false;
    w95->confirmClosePlot3D = false;
    w95->confirmCloseFolder = false;
    w95->resize(1280, 800);
    w95->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w95->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "95: pythonAutomationServer null");

    int rpcId = 9500;
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
    auto resultOf = [](const QJsonObject &resp) -> QJsonObject {
        return resp.value(QStringLiteral("result")).toObject();
    };
    auto errText = [](const QJsonObject &resp) -> std::string {
        return resp.contains(QStringLiteral("error"))
                ? resp.value(QStringLiteral("error"))
                          .toObject()
                          .value(QStringLiteral("message"))
                          .toString()
                          .toStdString()
                : std::string();
    };

    // Two-column table (x, y) named tName.
    int tableSerial = 0;
    auto makeTable = [&](const std::vector<double> &x, const std::vector<double> &y) -> Table * {
        const QString name = QStringLiteral("T95_%1").arg(++tableSerial);
        Table *t = w95->newTable(int(x.size()), 2, name, name);
        if (!t)
            return nullptr;
        for (size_t i = 0; i < x.size(); ++i) {
            t->setCell(int(i), 0, x[i]);
            t->setCell(int(i), 1, y[i]);
        }
        return t;
    };
    auto plotTable = [&](Table *t, QString &mlName, QString &curveTitle) -> Graph * {
        MultiLayer *ml = w95->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
        Graph *g = ml ? ml->activeGraph() : nullptr;
        if (!g || g->curves() < 1)
            return nullptr;
        mlName = ml->name();
        curveTitle = g->curve(0)->title().text();
        QCoreApplication::processEvents();
        return g;
    };
    auto fitNonlinear = [&](Table *t, const QString &model,
                            const QJsonObject &init = QJsonObject()) -> QJsonObject {
        QJsonObject p{ { "model", model },
                       { "table", t->name() },
                       { "x_col", 0 },
                       { "y_col", 1 },
                       { "plot_curve", false } };
        if (!init.isEmpty())
            p[QStringLiteral("init_params")] = init;
        return rpc(QStringLiteral("analysis.fit_nonlinear"), p);
    };
    auto peakCenters = [](const QJsonObject &res) -> std::vector<double> {
        std::vector<double> c;
        const QJsonArray peaks = res.value(QLatin1String("peaks")).toArray();
        for (const QJsonValue &pv : peaks)
            c.push_back(pv.toObject().value(QLatin1String("center")).toDouble(
                    std::numeric_limits<double>::quiet_NaN()));
        return c;
    };
    auto hasCenterNear = [](const std::vector<double> &c, double target, double tol) {
        for (double v : c) {
            if (std::isfinite(v) && std::abs(v - target) <= tol)
                return true;
        }
        return false;
    };
    // Gauss in Origin's area form: A*sqrt(2/pi)/w*exp(-2((x-xc)/w)^2)
    auto gaussArea = [](double x, double A, double xc, double w) {
        return A * std::sqrt(2.0 / M_PI) / w * std::exp(-2.0 * ((x - xc) / w) * ((x - xc) / w));
    };
    auto linspace = [](double a, double b, int n) {
        std::vector<double> v(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
            v[size_t(i)] = a + (b - a) * double(i) / double(n - 1);
        return v;
    };

    // -------------------------------------------------------------------------
    // 95a: 'centers' given to analyze_peaks are kept (three peaks at 3, 10, 16)
    // -------------------------------------------------------------------------
    {
        std::mt19937 rng(24);
        std::normal_distribution<double> noise(0.0, 0.05);
        const std::vector<double> x = linspace(0.0, 20.0, 401);
        std::vector<double> y(x.size());
        for (size_t i = 0; i < x.size(); ++i)
            y[i] = 1.0 + gaussArea(x[i], 40, 3, 1.0) + gaussArea(x[i], 20, 10, 1.0)
                    + gaussArea(x[i], 8, 16, 1.0) + noise(rng);
        Table *t = makeTable(x, y);
        CHECK_COND(t != nullptr, "95a: table setup failed");
        QString mlName, curveTitle;
        Graph *g = plotTable(t, mlName, curveTitle);
        CHECK_COND(g != nullptr, "95a: plot setup failed");

        auto analyze = [&](const QJsonObject &extra) -> QJsonObject {
            QJsonObject p{ { "graph", mlName },
                           { "curve", curveTitle },
                           { "profile", "Gauss" },
                           { "baseline", "constant" },
                           { "plot_components", false } };
            for (auto it = extra.begin(); it != extra.end(); ++it)
                p[it.key()] = it.value();
            return rpc(QStringLiteral("analysis.analyze_peaks"), p);
        };

        const QJsonObject r2c = analyze(QJsonObject{ { "centers", QJsonArray{ 10.0, 16.0 } } });
        CHECK_COND(!r2c.contains(QStringLiteral("error")),
                   "95a: analyze_peaks(centers=[10,16]) failed: " << errText(r2c));
        const std::vector<double> c2 = peakCenters(resultOf(r2c));
        CHECK_COND(c2.size() == 2 && hasCenterNear(c2, 10.0, 0.05) && hasCenterNear(c2, 16.0, 0.05),
                   "95a: centers=[10,16] gave centres "
                           << (c2.size() > 0 ? c2[0] : -1) << ", " << (c2.size() > 1 ? c2[1] : -1)
                           << " instead of ~10 and ~16 (auto search replaced them)");

        const QJsonObject r1c = analyze(QJsonObject{ { "centers", QJsonArray{ 16.0 } } });
        CHECK_COND(!r1c.contains(QStringLiteral("error")),
                   "95a: analyze_peaks(centers=[16]) failed: " << errText(r1c));
        const std::vector<double> c1 = peakCenters(resultOf(r1c));
        CHECK_COND(c1.size() == 1 && hasCenterNear(c1, 16.0, 0.05),
                   "95a: centers=[16] gave centre " << (c1.empty() ? -1 : c1[0])
                                                     << " instead of ~16");

        // Control: peak_count=2 without centres still picks the two tallest peaks (3 and 10).
        const QJsonObject rpc2 = analyze(QJsonObject{ { "peak_count", 2 } });
        const std::vector<double> cp = peakCenters(resultOf(rpc2));
        CHECK_COND(cp.size() == 2 && hasCenterNear(cp, 3.0, 0.05) && hasCenterNear(cp, 10.0, 0.05),
                   "95a: peak_count=2 control did not find the peaks at 3 and 10");

        // GUI path (MultiPeakFitTool::finalizePeakSelection): seed from picked centres, then fit().
        MultiPeakFit mpf(w95.get(), g, MultiPeakFit::Gauss, 1);
        CHECK_COND(mpf.setDataFromCurve(curveTitle), "95a: setDataFromCurve failed");
        mpf.setSilent(true);
        mpf.seedPeaksFromCenters(std::vector<double>{ 16.0 });
        mpf.fit();
        CHECK_COND(mpf.results().size() >= 4 && std::abs(mpf.results()[1] - 16.0) < 0.05,
                   "95a: GUI path seed(16)+fit() moved the peak to "
                           << (mpf.results().size() >= 2 ? mpf.results()[1] : -1));
    }

    // -------------------------------------------------------------------------
    // 95b: parameters below 1e-8 (x in metres / seconds) move and get correct errors
    // -------------------------------------------------------------------------
    {
        std::mt19937 rng(163);
        std::normal_distribution<double> noise(0.0, 1e-3);
        const std::vector<double> xn = linspace(1.0, 3.0, 100); // nm
        std::vector<double> xm(xn.size()), y(xn.size());
        for (size_t i = 0; i < xn.size(); ++i) {
            xm[i] = xn[i] * 1e-9;
            y[i] = std::exp(-(xn[i] - 2.0) * (xn[i] - 2.0) / (2.0 * 0.2 * 0.2)) + noise(rng);
        }
        Table *tm = makeTable(xm, y);
        Table *tn = makeTable(xn, y);
        CHECK_COND(tm && tn, "95b: table setup failed");

        const QJsonObject rn = resultOf(fitNonlinear(tn, QStringLiteral("GaussAmp")));
        CHECK_COND(rn.value("converged").toBool(), "95b: GaussAmp in nm did not converge");
        const double xcN = rn.value("params").toObject().value("xc").toDouble();
        const double wN = rn.value("params").toObject().value("w").toDouble();
        const double seXcN = rn.value("errors").toObject().value("xc").toDouble();
        CHECK_COND(std::abs(xcN - 2.0) < 0.01 && seXcN > 0.0, "95b: nm reference fit is off");

        const QJsonObject inits[2] = {
            QJsonObject(),
            QJsonObject{ { "y0", 0.0 }, { "A", 1.0 }, { "xc", 2.05e-9 }, { "w", 0.22e-9 } }
        };
        for (int k = 0; k < 2; ++k) {
            const QJsonObject resp = fitNonlinear(tm, QStringLiteral("GaussAmp"), inits[k]);
            CHECK_COND(!resp.contains(QStringLiteral("error")),
                       "95b: GaussAmp in metres failed: " << errText(resp));
            const QJsonObject rm = resultOf(resp);
            const double xcM = rm.value("params").toObject().value("xc").toDouble();
            const double wM = rm.value("params").toObject().value("w").toDouble();
            const double seXcM = rm.value("errors").toObject().value("xc").toDouble();
            CHECK_COND(rm.value("converged").toBool() && rm.value("r2").toDouble() > 0.9999,
                       "95b[" << k << "]: GaussAmp in metres not converged (r2="
                              << rm.value("r2").toDouble() << ")");
            CHECK_COND(std::abs(xcM / 1e-9 - xcN) < 1e-5 && std::abs(wM / 1e-9 - wN) < 1e-5,
                       "95b[" << k << "]: xc=" << xcM << " w=" << wM
                              << " (m) do not match the nm fit xc=" << xcN << " w=" << wN);
            CHECK_COND(std::isfinite(seXcM) && std::abs(seXcM / 1e-9 / seXcN - 1.0) < 0.02,
                       "95b[" << k << "]: SE(xc)=" << seXcM << " m vs " << seXcN
                              << " nm (should scale by 1e-9)");
        }

        // ExpDec1 of a 2 ns decay with x in seconds vs in ns.
        std::mt19937 rng2(161);
        std::normal_distribution<double> noise2(0.0, 0.05);
        const std::vector<double> tNs = linspace(0.0, 10.0, 200);
        std::vector<double> tS(tNs.size()), yd(tNs.size());
        for (size_t i = 0; i < tNs.size(); ++i) {
            tS[i] = tNs[i] * 1e-9;
            yd[i] = 1.0 + 10.0 * std::exp(-tNs[i] / 2.0) + noise2(rng2);
        }
        Table *tdn = makeTable(tNs, yd);
        Table *tds = makeTable(tS, yd);
        CHECK_COND(tdn && tds, "95b: decay table setup failed");
        const QJsonObject dn = resultOf(fitNonlinear(tdn, QStringLiteral("ExpDec1")));
        const QJsonObject ds = resultOf(fitNonlinear(tds, QStringLiteral("ExpDec1")));
        const double tauN = dn.value("params").toObject().value("t").toDouble();
        const double tauS = ds.value("params").toObject().value("t").toDouble();
        const double seN = dn.value("errors").toObject().value("t").toDouble();
        const double seS = ds.value("errors").toObject().value("t").toDouble();
        CHECK_COND(dn.value("converged").toBool() && ds.value("converged").toBool(),
                   "95b: ExpDec1 did not converge (ns: " << dn.value("converged").toBool()
                                                         << ", s: "
                                                         << ds.value("converged").toBool() << ")");
        CHECK_COND(std::abs(tauS / 1e-9 / tauN - 1.0) < 1e-5,
                   "95b: ExpDec1 t=" << tauS << " s vs " << tauN << " ns");
        CHECK_COND(seN > 0.0 && std::abs(seS / 1e-9 / seN - 1.0) < 0.02,
                   "95b: ExpDec1 SE(t)=" << seS << " s vs " << seN << " ns (should scale by 1e-9)");
    }

    // -------------------------------------------------------------------------
    // 95c: dips (negative peaks) with built-in peak models
    // -------------------------------------------------------------------------
    {
        std::mt19937 rng(17);
        std::normal_distribution<double> noise(0.0, 0.05);
        const std::vector<double> x = linspace(0.0, 10.0, 100);
        std::vector<double> y(x.size()), yUp(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            y[i] = 5.0 - 3.0 * std::exp(-(x[i] - 4.0) * (x[i] - 4.0) / (2.0 * 0.5 * 0.5))
                    + noise(rng);
            yUp[i] = 10.0 - y[i];
        }
        Table *t = makeTable(x, y);
        Table *tUp = makeTable(x, yUp);
        CHECK_COND(t && tUp, "95c: table setup failed");

        const QJsonObject ga = resultOf(fitNonlinear(t, QStringLiteral("GaussAmp")));
        const QJsonObject gp = ga.value("params").toObject();
        CHECK_COND(ga.value("converged").toBool() && ga.value("r2").toDouble() > 0.99,
                   "95c: GaussAmp on a dip: r2=" << ga.value("r2").toDouble());
        CHECK_COND(std::abs(gp.value("A").toDouble() + 3.0) < 0.1
                           && std::abs(gp.value("xc").toDouble() - 4.0) < 0.05
                           && std::abs(std::abs(gp.value("w").toDouble()) - 0.5) < 0.05
                           && std::abs(gp.value("y0").toDouble() - 5.0) < 0.1,
                   "95c: GaussAmp on a dip gave A=" << gp.value("A").toDouble() << " xc="
                                                    << gp.value("xc").toDouble() << " w="
                                                    << gp.value("w").toDouble() << " y0="
                                                    << gp.value("y0").toDouble());

        for (const char *mdl : { "Gauss", "Lorentz" }) {
            const QJsonObject r = resultOf(fitNonlinear(t, QString::fromLatin1(mdl)));
            const QJsonObject p = r.value("params").toObject();
            CHECK_COND(r.value("converged").toBool() && r.value("r2").toDouble() > 0.97
                               && p.value("A").toDouble() < 0.0
                               && std::abs(p.value("xc").toDouble() - 4.0) < 0.05,
                       "95c: " << mdl << " on a dip: r2=" << r.value("r2").toDouble() << " A="
                               << p.value("A").toDouble() << " xc=" << p.value("xc").toDouble());
        }

        // Control: the same data turned upside down is still fitted as an upward peak.
        const QJsonObject up = resultOf(fitNonlinear(tUp, QStringLiteral("GaussAmp")));
        const QJsonObject upP = up.value("params").toObject();
        CHECK_COND(up.value("converged").toBool() && std::abs(upP.value("A").toDouble() - 3.0) < 0.1
                           && std::abs(upP.value("xc").toDouble() - 4.0) < 0.05,
                   "95c: upward control GaussAmp gave A=" << upP.value("A").toDouble());
    }

    // -------------------------------------------------------------------------
    // 95d: three peaks 25.0 / 26.2 / 33.0 (26.2 is 1.7 FWHM from 25.0) on a sloping baseline
    // -------------------------------------------------------------------------
    {
        std::mt19937 rng(8);
        std::normal_distribution<double> noise(0.0, 0.3);
        const std::vector<double> x = linspace(20.0, 40.0, 800);
        std::vector<double> y(x.size());
        for (size_t i = 0; i < x.size(); ++i)
            y[i] = gaussArea(x[i], 100, 25.0, 0.6) + gaussArea(x[i], 50, 26.2, 0.6)
                    + gaussArea(x[i], 80, 33.0, 1.0) + 5.0 + 0.1 * (x[i] - 20.0) + noise(rng);
        Table *t = makeTable(x, y);
        CHECK_COND(t != nullptr, "95d: table setup failed");
        QString mlName, curveTitle;
        Graph *g = plotTable(t, mlName, curveTitle);
        CHECK_COND(g != nullptr, "95d: plot setup failed");

        auto analyze = [&](const QString &baseline, const QJsonObject &extra) -> QJsonObject {
            QJsonObject p{ { "graph", mlName },
                           { "curve", curveTitle },
                           { "profile", "Gauss" },
                           { "baseline", baseline },
                           { "plot_components", false } };
            for (auto it = extra.begin(); it != extra.end(); ++it)
                p[it.key()] = it.value();
            return resultOf(rpc(QStringLiteral("analysis.analyze_peaks"), p));
        };

        // peak_count=3, automatic count, and peak_count=3 with the second-derivative search (its
        // detections are now used, so the 3 kept must be the 3 peaks, not noise or duplicates).
        const QJsonObject cases[3] = {
            QJsonObject{ { "peak_count", 3 } }, QJsonObject(),
            QJsonObject{ { "peak_count", 3 }, { "use_second_derivative", true } }
        };
        const char *caseNames[3] = { "peak_count=3", "auto", "2nd-derivative, peak_count=3" };
        for (int k = 0; k < 3; ++k) {
            const QJsonObject r = analyze(QStringLiteral("linear"), cases[k]);
            const std::vector<double> c = peakCenters(r);
            CHECK_COND(r.value("r2").toDouble() > 0.99 && c.size() == 3
                               && hasCenterNear(c, 25.0, 0.02) && hasCenterNear(c, 26.2, 0.02)
                               && hasCenterNear(c, 33.0, 0.02),
                       "95d[" << caseNames[k] << "]: r2=" << r.value("r2").toDouble()
                              << " peaks=" << c.size() << " (the neighbour at 26.2 was dropped?)");
        }

        // Surplus peak count: every fitted centre stays inside the data range [20, 40].
        for (const QString &base : { QStringLiteral("none"), QStringLiteral("linear") }) {
            const QJsonObject r = analyze(base, QJsonObject{ { "peak_count", 4 } });
            const std::vector<double> c = peakCenters(r);
            CHECK_COND(c.size() == 4, "95d: peak_count=4 (" << base.toStdString() << ") returned "
                                                            << c.size() << " peaks");
            for (double v : c) {
                CHECK_COND(std::isfinite(v) && v >= 20.0 && v <= 40.0,
                           "95d: peak_count=4 (" << base.toStdString() << ") centre " << v
                                                 << " is outside the data range [20, 40]");
            }
            // The surplus peak may split one of the real ones; the fit itself must stay good.
            CHECK_COND(r.value("r2").toDouble() > 0.99 && hasCenterNear(c, 26.2, 0.05)
                               && hasCenterNear(c, 33.0, 0.05),
                       "95d: peak_count=4 (" << base.toStdString() << "): centres " << c[0]
                                             << ", " << c[1] << ", " << c[2] << ", " << c[3]
                                             << " r2=" << r.value("r2").toDouble());
        }
    }

    std::cout << "[PASS] Suite 95: data-driven fit & peak-analysis regressions passed.\n";
    return 0;
}
