#include "ole_test_common.h"
#include "Convolution.h"
#include "Correlation.h"
#include "FFT.h"
#include "FFTDialog.h"

#include <QLineEdit>
#include <QSet>

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Suite 81: Numerical correctness of the FFT-based transforms.
//   81a Convolution::convlv  - wrap-around storage of the response (m = 5, 9, ...)
//   81b Convolution::convlv  - halfcomplex bins (k = 0 and k = n/2 are the real bins)
//   81c Deconvolution        - same bins in the division branch
//   81d Correlation::output  - X0*Y0 (not X0*X0) and the Nyquist bin
//   81e Correlation          - zero padding to >= 2*rows (no circular wrap into lags)
//   81f SmoothFilter (FFT)   - X in descending order must still low-pass
//   81g FFT / FFTDialog      - default sampling interval = (xmax - xmin)/(n - 1), |dt| used
//   81h Deconvolution        - kernels with a zero at the Nyquist bin ([1,2,1], [1,4,6,4,1])
//                              must not divide by zero (finite output, peak preserved)
//   81i FFT / FFTDialog      - table with empty rows below the data: grid of the finite rows
// All checks compare against values computed directly from the definitions.
// Every check is evaluated (failures are counted) so a single run reports all regressions.
// =============================================================================
namespace {

int g81Failures = 0;

#define CHECK81(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g81Failures;                                                                      \
        }                                                                                       \
    } while (0)

// Table with nRows rows; column c gets cols[c] in its first rows, remaining rows stay empty.
Table *suite81Table(ApplicationWindow *mw, const QString &name, const QStringList &header,
                    int nRows, const std::vector<std::vector<double>> &cols)
{
    Table *t = mw->newTable(name, nRows, int(header.size()));
    if (!t)
        return nullptr;
    t->setHeader(header);
    for (int c = 0; c < int(cols.size()); ++c) {
        Column *col = t->column(c);
        col->setColumnMode(SciDAVis::ColumnMode::Numeric);
        const int filled = int(cols[size_t(c)].size());
        for (int r = 0; r < filled; ++r)
            col->setValueAt(r, cols[size_t(c)][size_t(r)]);
        for (int r = filled; r < nRows; ++r)
            col->setInvalid(r, true);
    }
    return t;
}

std::vector<double> suite81Column(Table *t, int col, int n)
{
    std::vector<double> v;
    Column *c = (t && col >= 0 && col < t->numCols()) ? t->column(col) : nullptr;
    if (!c)
        return v;
    for (int i = 0; i < n; ++i)
        v.push_back(c->valueAt(i));
    return v;
}

double suite81MaxAbsDiff(const std::vector<double> &a, const std::vector<double> &b)
{
    if (a.size() != b.size())
        return 1e300;
    double m = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}

std::string suite81Str(const std::vector<double> &v)
{
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < v.size(); ++i)
        os << (i ? ", " : "") << v[i];
    os << "]";
    return os.str();
}

// Circular convolution with the response centred at index m/2 (lag k <-> dres[m/2 + k]).
std::vector<double> suite81CircularConv(const std::vector<double> &sig, int n,
                                        const std::vector<double> &resp)
{
    const int m = int(resp.size());
    const int m2 = m / 2;
    std::vector<double> s(size_t(n), 0.0), out(size_t(n), 0.0);
    for (size_t i = 0; i < sig.size(); ++i)
        s[i] = sig[i];
    for (int j = 0; j < n; ++j)
        for (int k = -m2; k <= m2; ++k)
            out[size_t(j)] += s[size_t(((j - k) % n + n) % n)] * resp[size_t(m2 + k)];
    return out;
}

QSet<QString> suite81WindowNames(ApplicationWindow *mw)
{
    QSet<QString> names;
    for (MyWidget *w : mw->windowsList())
        if (w)
            names.insert(w->name());
    return names;
}

Table *suite81NewTable(ApplicationWindow *mw, const QSet<QString> &before)
{
    for (MyWidget *w : mw->windowsList()) {
        Table *t = qobject_cast<Table *>(w);
        if (t && !before.contains(t->name()))
            return t;
    }
    return nullptr;
}

} // namespace

int runSuite81NumericalTransformCorrectness(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 81: FFT convolution / correlation / smoothing / "
                 "sampling-interval correctness...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 81");
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    g81Failures = 0;

    // ------------------------------------------------ 81a: convolution, m = 5 (m2 even)
    {
        std::vector<double> sig(12, 0.0);
        sig[3] = 1.0;
        const std::vector<double> resp = { 0.0, 0.0, 0.0, 0.0, 1.0 }; // lag +2
        Table *t = suite81Table(mw, "S81ConvM5", QStringList() << "Sig" << "Resp", 12,
                                { sig, resp });
        CHECK_COND(t != nullptr, "81a: table");
        const int colsBefore = t->numCols();
        Convolution cv(mw, t, t->name() + "_Sig", t->name() + "_Resp");
        CHECK81(!cv.error(), "81a: Convolution init error");
        CHECK81(cv.run(), "81a: Convolution run failed");
        const std::vector<double> got = suite81Column(t, colsBefore + 1, 16);
        std::vector<double> expect(16, 0.0);
        expect[5] = 1.0; // delta at 3 shifted by +2
        const double err = suite81MaxAbsDiff(got, expect);
        std::cout << "[INFO] 81a conv(delta@3, [0,0,0,0,1]) = " << suite81Str(got) << "\n";
        CHECK81(err < 1e-9, "81a: convolution with m=5 must shift the delta by 2 (max err " << err
                                                                                         << ")");

        // General asymmetric kernel against the direct circular sum.
        const std::vector<double> sig2 = { 0.3, -1.2, 2.5, 0.7, -0.4, 1.9, 3.1, -2.2, 0.8, 1.1, -0.6, 0.2 };
        const std::vector<double> resp2 = { 1.0, 2.0, 3.0, 4.0, 5.0 };
        Table *t2 = suite81Table(mw, "S81ConvGen", QStringList() << "Sig" << "Resp", 12,
                                 { sig2, resp2 });
        CHECK_COND(t2 != nullptr, "81a: table 2");
        const int cols2 = t2->numCols();
        Convolution cv2(mw, t2, t2->name() + "_Sig", t2->name() + "_Resp");
        CHECK81(cv2.run(), "81a: general convolution run failed");
        const std::vector<double> got2 = suite81Column(t2, cols2 + 1, 16);
        const std::vector<double> exp2 = suite81CircularConv(sig2, 16, resp2);
        const double err2 = suite81MaxAbsDiff(got2, exp2);
        CHECK81(err2 < 1e-9, "81a: general m=5 convolution differs from the direct sum (max err "
                                     << err2 << ")");
    }

    // ------------------------------------------------ 81b: convolution, m = 3 (bins only)
    {
        std::vector<double> sig(12, 0.0);
        sig[3] = 1.0;
        const std::vector<double> resp = { 0.0, 0.0, 1.0 }; // lag +1
        Table *t = suite81Table(mw, "S81ConvM3", QStringList() << "Sig" << "Resp", 12,
                                { sig, resp });
        CHECK_COND(t != nullptr, "81b: table");
        const int colsBefore = t->numCols();
        Convolution cv(mw, t, t->name() + "_Sig", t->name() + "_Resp");
        CHECK81(cv.run(), "81b: Convolution run failed");
        const std::vector<double> got = suite81Column(t, colsBefore + 1, 16);
        std::vector<double> expect(16, 0.0);
        expect[4] = 1.0;
        const double err = suite81MaxAbsDiff(got, expect);
        std::cout << "[INFO] 81b conv(delta@3, [0,0,1]) = " << suite81Str(got) << "\n";
        CHECK81(err < 1e-9, "81b: convolution with m=3 must shift the delta by 1 (max err " << err
                                                                                         << ")");
    }

    // ------------------------------------------------ 81c: deconvolution, m = 3
    {
        std::vector<double> sig(12, 0.0);
        sig[5] = 1.0;
        const std::vector<double> resp = { 0.0, 0.0, 1.0 };
        Table *t = suite81Table(mw, "S81Deconv", QStringList() << "Sig" << "Resp", 12,
                                { sig, resp });
        CHECK_COND(t != nullptr, "81c: table");
        const int colsBefore = t->numCols();
        Deconvolution dcv(mw, t, t->name() + "_Sig", t->name() + "_Resp");
        CHECK81(dcv.run(), "81c: Deconvolution run failed");
        const std::vector<double> got = suite81Column(t, colsBefore + 1, 16);
        std::vector<double> expect(16, 0.0);
        expect[4] = 1.0;
        const double err = suite81MaxAbsDiff(got, expect);
        std::cout << "[INFO] 81c deconv(delta@5, [0,0,1]) = " << suite81Str(got) << "\n";
        CHECK81(err < 1e-9, "81c: deconvolution must undo a shift of 1 (max err " << err << ")");
    }

    // Output of Correlation: rows values for lags -rows/2 .. rows-1-rows/2.
    auto directCorr = [](const std::vector<double> &x, const std::vector<double> &y) {
        const int rows = int(x.size());
        const int half = rows / 2;
        std::vector<double> r(size_t(rows), 0.0);
        for (int i = 0; i < rows; ++i) {
            const int lag = i - half;
            for (int j = 0; j < rows; ++j)
                if (j + lag >= 0 && j + lag < rows)
                    r[size_t(i)] += x[size_t(j)] * y[size_t(j + lag)];
        }
        return r;
    };

    // ------------------------------------------------ 81d: cross-correlation bins
    {
        std::vector<double> x(16, 0.0), y(16, 0.0);
        x[0] = 1.0;
        y[0] = 2.0;
        Table *t = suite81Table(mw, "S81XCorrDelta", QStringList() << "X1" << "Y1", 16, { x, y });
        CHECK_COND(t != nullptr, "81d: table");
        const int colsBefore = t->numCols();
        Correlation cor(mw, t, t->name() + "_X1", t->name() + "_Y1");
        CHECK81(cor.run(), "81d: Correlation run failed");
        const std::vector<double> lags = suite81Column(t, colsBefore, 16);
        const std::vector<double> got = suite81Column(t, colsBefore + 1, 16);
        std::cout << "[INFO] 81d xcorr(delta, 2 delta) = " << suite81Str(got) << "\n";
        CHECK81(lags.size() == 16 && lags[8] == 0.0, "81d: lag axis must put lag 0 at row 8");
        const double err = suite81MaxAbsDiff(got, directCorr(x, y));
        CHECK81(err < 1e-9 && std::fabs(got.size() > 8 ? got[8] - 2.0 : 1.0) < 1e-9,
                "81d: cross-correlation of delta and 2*delta must be 2 at lag 0 only (max err "
                        << err << ")");

        const std::vector<double> a = { 0.5, 1.5, -0.7, 2.0, 0.1, -1.3, 0.9, 0.4,
                                        -0.2, 1.1, 0.6, -0.8, 1.7, 0.3, -0.5, 0.25 };
        const std::vector<double> b = { 1.2, -0.4, 0.8, 0.3, -1.1, 2.2, 0.05, -0.6,
                                        0.9, 0.0, -0.3, 1.4, -0.9, 0.7, 0.2, -1.0 };
        Table *t2 = suite81Table(mw, "S81XCorrGen", QStringList() << "A" << "B", 16, { a, b });
        CHECK_COND(t2 != nullptr, "81d: table 2");
        const int cols2 = t2->numCols();
        Correlation cor2(mw, t2, t2->name() + "_A", t2->name() + "_B");
        CHECK81(cor2.run(), "81d: general Correlation run failed");
        const std::vector<double> got2 = suite81Column(t2, cols2 + 1, 16);
        const double err2 = suite81MaxAbsDiff(got2, directCorr(a, b));
        CHECK81(err2 < 1e-9, "81d: general cross-correlation differs from the direct sum (max err "
                                     << err2 << ")");
    }

    // ------------------------------------------------ 81e: autocorrelation of ones
    {
        const std::vector<double> ones(16, 1.0);
        Table *t = suite81Table(mw, "S81ACorrOnes", QStringList() << "O", 16, { ones });
        CHECK_COND(t != nullptr, "81e: table");
        const int colsBefore = t->numCols();
        Correlation cor(mw, t, t->name() + "_O", t->name() + "_O");
        CHECK81(cor.run(), "81e: autocorrelation run failed");
        const std::vector<double> got = suite81Column(t, colsBefore + 1, 16);
        std::vector<double> expect(16);
        for (int i = 0; i < 16; ++i)
            expect[size_t(i)] = 16.0 - std::fabs(double(i - 8));
        const double err = suite81MaxAbsDiff(got, expect);
        std::cout << "[INFO] 81e autocorr(ones x16) = " << suite81Str(got) << "\n";
        CHECK81(err < 1e-9, "81e: autocorrelation of 16 ones must be 16-|lag| (max err " << err
                                                                                         << ")");
    }

    // ------------------------------------------------ 81f: FFT smoothing, X descending
    {
        std::vector<double> x, y;
        for (int i = 0; i < 32; ++i) {
            x.push_back(31.0 - i);
            y.push_back(5.0);
        }
        Table *t = suite81Table(mw, "S81SmoothDesc", QStringList() << "X" << "Y", 32, { x, y });
        CHECK_COND(t != nullptr, "81f: table");
        t->setColPlotDesignation(0, SciDAVis::X);
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Line);
        Graph *g = ml ? ml->activeGraph() : nullptr;
        CHECK_COND(g != nullptr, "81f: plot");
        SmoothFilter sf(mw, g, t->name() + "_Y", SmoothFilter::FFT);
        sf.setSilent(true);
        CHECK81(sf.run(), "81f: FFT smoothing run failed");
        QwtPlotCurve *c = g->curve(g->curves() - 1);
        CHECK81(c && c->dataSize() == 32, "81f: smoothed curve size");
        double maxErr = 0.0;
        if (c)
            for (int i = 0; i < int(c->dataSize()); ++i)
                maxErr = std::max(maxErr, std::fabs(c->y(i) - 5.0));
        std::cout << "[INFO] 81f FFT smoothing of constant 5 with descending X: max |y-5| = "
                  << maxErr << (c ? " (y0 = " + std::to_string(c->y(0)) + ")" : std::string())
                  << "\n";
        CHECK81(maxErr < 1e-9, "81f: FFT low-pass of a constant with descending X must stay "
                               "constant (max err "
                                       << maxErr << ")");
    }

    // ------------------------------------------------ 81g: FFT sampling interval
    {
        // (1) descending, uniform X: dt = -0.5 between neighbours, |dt| = 0.5
        std::vector<double> x, y;
        for (int i = 0; i < 16; ++i) {
            x.push_back(10.0 - 0.5 * i);
            y.push_back(std::cos(2.0 * M_PI * i / 4.0));
        }
        Table *t = suite81Table(mw, "S81FftDesc", QStringList() << "X" << "Y", 16, { x, y });
        CHECK_COND(t != nullptr, "81g: table");
        t->setColPlotDesignation(0, SciDAVis::X);
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Line);
        Graph *g = ml ? ml->activeGraph() : nullptr;
        CHECK_COND(g != nullptr, "81g: plot");

        {
            std::unique_ptr<FFTDialog> dlg(new FFTDialog(FFTDialog::onGraph, mw));
            dlg->setGraph(g);
            const double dflt = dlg->boxSampling->text().toDouble();
            std::cout << "[INFO] 81g FFTDialog default sampling (descending X) = "
                      << dlg->boxSampling->text().toStdString() << "\n";
            CHECK81(std::fabs(dflt - 0.5) < 1e-12,
                    "81g: FFTDialog default sampling for descending X must be +0.5, got "
                            << dlg->boxSampling->text().toStdString());
        }
        {
            std::unique_ptr<FFTDialog> dlg(new FFTDialog(FFTDialog::onTable, mw));
            dlg->setTable(t);
            const double dflt = dlg->boxSampling->text().toDouble();
            CHECK81(std::fabs(dflt - 0.5) < 1e-12,
                    "81g: FFTDialog (table) default sampling for descending X must be +0.5, got "
                            << dlg->boxSampling->text().toStdString());
        }

        // A negative interval handed to FFT (old dialog default) must be used as |dt|.
        const QSet<QString> before = suite81WindowNames(mw);
        FFT fft(mw, g, t->name() + "_Y");
        fft.setSampling(-0.5);
        fft.setSilent(true);
        CHECK81(fft.run(), "81g: FFT run failed");
        Table *ft = suite81NewTable(mw, before);
        CHECK81(ft != nullptr, "81g: FFT output table not found");
        if (ft) {
            const std::vector<double> f = suite81Column(ft, 0, 3);
            const double df = f.size() == 3 ? f[2] - f[1] : 0.0;
            std::cout << "[INFO] 81g FFT frequency step with sampling -0.5 = " << df
                      << " (expected 1/(16*0.5) = 0.125)\n";
            CHECK81(std::fabs(df - 0.125) < 1e-12,
                    "81g: FFT frequency step must be 1/(n*|dt|) = 0.125, got " << df);
        }

        // (2) non-uniform ascending X: default must be the resampled grid step.
        std::vector<double> xn, yn;
        for (int i = 0; i < 16; ++i) {
            xn.push_back(i + 0.3 * std::sin(1.3 * i));
            yn.push_back(std::sin(0.7 * i));
        }
        Table *tn = suite81Table(mw, "S81FftNonUni", QStringList() << "X" << "Y", 16, { xn, yn });
        CHECK_COND(tn != nullptr, "81g: table 2");
        tn->setColPlotDesignation(0, SciDAVis::X);
        MultiLayer *mln = mw->multilayerPlot(tn, QStringList() << (tn->name() + "_Y"), Graph::Line);
        Graph *gn = mln ? mln->activeGraph() : nullptr;
        CHECK_COND(gn != nullptr, "81g: plot 2");
        const double expectDt = (xn.back() - xn.front()) / 15.0;
        std::unique_ptr<FFTDialog> dlg(new FFTDialog(FFTDialog::onGraph, mw));
        dlg->setGraph(gn);
        const double dflt = dlg->boxSampling->text().toDouble();
        std::cout << "[INFO] 81g FFTDialog default sampling (non-uniform X) = " << dflt
                  << " (expected " << expectDt << ")\n";
        CHECK81(std::fabs(dflt - expectDt) < 1e-9 * expectDt,
                "81g: FFTDialog default sampling for non-uniform X must be (xmax-xmin)/(n-1) = "
                        << expectDt << ", got " << dflt);

        const QSet<QString> before2 = suite81WindowNames(mw);
        FFT fftn(mw, gn, tn->name() + "_Y");
        fftn.setSampling(dflt); // what FFTDialog::accept() hands over
        fftn.setSilent(true);
        CHECK81(fftn.run(), "81g: non-uniform FFT run failed");
        Table *ftn = suite81NewTable(mw, before2);
        if (ftn) {
            const std::vector<double> f = suite81Column(ftn, 0, 3);
            const double df = f.size() == 3 ? f[2] - f[1] : 0.0;
            CHECK81(std::fabs(df - 1.0 / (16.0 * expectDt)) < 1e-9,
                    "81g: non-uniform FFT frequency step must be 1/(n*dt_grid), got " << df);
        } else {
            CHECK81(false, "81g: non-uniform FFT output table not found");
        }
    }

    // ------------------------------------------------ 81h: deconvolution, Nyquist zero
    // [1,2,1] and [1,4,6,4,1] have H(n/2) = 0 exactly. conv followed by deconv with the same
    // kernel must give back the delta except for its (unrecoverable) Nyquist component:
    // delta_p[j] - (-1)^(j-p)/n, with no inf/NaN.
    {
        const std::vector<std::vector<double>> kernels = { { 1.0, 2.0, 1.0 },
                                                           { 1.0, 4.0, 6.0, 4.0, 1.0 } };
        const int p = 5;
        for (size_t kk = 0; kk < kernels.size(); ++kk) {
            const std::vector<double> &resp = kernels[kk];
            const std::string tag = "81h[m=" + std::to_string(resp.size()) + "]";
            std::vector<double> sig(12, 0.0);
            sig[size_t(p)] = 1.0;
            Table *t = suite81Table(mw, QString("S81DeconvNyq%1").arg(int(kk)),
                                    QStringList() << "Sig" << "Resp", 12, { sig, resp });
            CHECK_COND(t != nullptr, "81h: table");
            const int colsBefore = t->numCols();
            Convolution cv(mw, t, t->name() + "_Sig", t->name() + "_Resp");
            CHECK81(cv.run(), tag << ": Convolution run failed");
            const std::vector<double> conv = suite81Column(t, colsBefore + 1, 16);
            const std::vector<double> convExp = suite81CircularConv(sig, 16, resp);
            CHECK81(suite81MaxAbsDiff(conv, convExp) < 1e-9,
                    tag << ": convolution differs from the direct sum");

            // The convolved signal fits in the first 12 rows (support p-m2 .. p+m2).
            std::vector<double> conv12(conv.begin(), conv.begin() + std::min<size_t>(12, conv.size()));
            Table *t2 = suite81Table(mw, QString("S81DeconvNyqB%1").arg(int(kk)),
                                     QStringList() << "Sig" << "Resp", 12, { conv12, resp });
            CHECK_COND(t2 != nullptr, "81h: table 2");
            const int cols2 = t2->numCols();
            Deconvolution dcv(mw, t2, t2->name() + "_Sig", t2->name() + "_Resp");
            CHECK81(!dcv.error(), tag << ": Deconvolution init error");
            CHECK81(dcv.run(), tag << ": Deconvolution run failed");
            const std::vector<double> got = suite81Column(t2, cols2 + 1, 16);
            std::cout << "[INFO] " << tag << " deconv(conv(delta@5)) = " << suite81Str(got) << "\n";

            bool allFinite = got.size() == 16;
            size_t argmax = 0;
            for (size_t i = 0; i < got.size(); ++i) {
                if (!std::isfinite(got[i]))
                    allFinite = false;
                if (std::fabs(got[i]) > std::fabs(got[argmax]))
                    argmax = i;
            }
            CHECK81(allFinite, tag << ": deconvolution output must be finite, got " << suite81Str(got));
            CHECK81(argmax == size_t(p),
                    tag << ": deconvolution peak must stay at row " << p << ", got " << argmax);
            std::vector<double> expect(16);
            for (int j = 0; j < 16; ++j)
                expect[size_t(j)] = (j == p ? 1.0 : 0.0) - (((j - p) % 2 == 0) ? 1.0 : -1.0) / 16.0;
            const double err = suite81MaxAbsDiff(got, expect);
            CHECK81(err < 1e-9, tag << ": deconvolution must recover the delta minus its Nyquist "
                                       "component (max err "
                                    << err << ")");
        }
    }

    // ------------------------------------------------ 81i: FFT, empty rows below the data
    // 30-row table with X/Y in the first 10 rows only: FFT works on the 10 finite rows and its
    // grid step equals FFTDialog's default (xmax - xmin)/(10 - 1), so df = 1/(10*dt).
    {
        std::vector<double> x, y;
        for (int i = 0; i < 10; ++i) {
            x.push_back(0.2 * i);
            y.push_back(std::cos(2.0 * M_PI * i / 5.0));
        }
        const double dt = 0.2;
        Table *t = suite81Table(mw, "S81FftEmptyRows", QStringList() << "X" << "Y", 30, { x, y });
        CHECK_COND(t != nullptr, "81i: table");
        t->setColPlotDesignation(0, SciDAVis::X);
        t->setColPlotDesignation(1, SciDAVis::Y);

        double dflt = 0.0;
        {
            std::unique_ptr<FFTDialog> dlg(new FFTDialog(FFTDialog::onTable, mw));
            dlg->setTable(t);
            dflt = dlg->boxSampling->text().toDouble();
            std::cout << "[INFO] 81i FFTDialog default sampling (10 of 30 rows) = "
                      << dlg->boxSampling->text().toStdString() << "\n";
            CHECK81(std::fabs(dflt - dt) < 1e-12,
                    "81i: FFTDialog default sampling must be 0.2, got "
                            << dlg->boxSampling->text().toStdString());
        }

        // FFT's own grid step (no setSampling): df = 1/(m*dt_grid) must be 1/(10*dflt).
        auto runFft = [&](bool overrideSampling, double sampling, int &rowsOut) {
            const QSet<QString> before = suite81WindowNames(mw);
            FFT fft(mw, t, t->name() + "_Y");
            if (overrideSampling)
                fft.setSampling(sampling);
            fft.setSilent(true);
            CHECK81(fft.run(), "81i: FFT run failed");
            Table *ft = suite81NewTable(mw, before);
            rowsOut = ft ? ft->numRows() : -1;
            if (!ft)
                return std::numeric_limits<double>::quiet_NaN();
            const std::vector<double> f = suite81Column(ft, 0, 3);
            return f.size() == 3 ? f[2] - f[1] : std::numeric_limits<double>::quiet_NaN();
        };
        int rowsOwn = -1, rowsDlg = -1;
        const double dfOwn = runFft(false, 0.0, rowsOwn);
        const double dfDlg = runFft(true, dflt, rowsDlg);
        const double dfExp = 1.0 / (10.0 * dflt);
        std::cout << "[INFO] 81i FFT df (own grid) = " << dfOwn << ", df (dialog dt) = " << dfDlg
                  << ", expected 1/(10*dt) = " << dfExp << ", rows = " << rowsOwn << "/"
                  << rowsDlg << "\n";
        CHECK81(std::fabs(dfOwn - dfExp) < 1e-12,
                "81i: FFT grid step must equal the dialog default (df " << dfOwn << " vs "
                                                                        << dfExp << ")");
        CHECK81(std::fabs(dfDlg - dfExp) < 1e-12,
                "81i: FFT with the dialog default must give df = 1/(10*dt), got " << dfDlg);
        CHECK81(rowsOwn == 10 && rowsDlg == 10,
                "81i: FFT output must have one row per finite input row (10), got "
                        << rowsOwn << "/" << rowsDlg);
    }

    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    if (g81Failures > 0) {
        std::cerr << "[FAIL] Suite 81: " << g81Failures << " check(s) failed.\n";
        return 1;
    }
    std::cout << "[PASS] Suite 81 passed.\n";
    return 0;
}
