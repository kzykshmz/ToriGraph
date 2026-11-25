#include "ole_test_common.h"
#include "Graph_Internal.h"
#include "ScaleDraw.h"

#include <QLineEdit>
#include <QListWidget>

#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>

#include <cmath>
#include <memory>
#include <typeinfo>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Suite 86: Axis scale / tick / label correctness.
//   86a Graph::updateScale  - autoscale off keeps a log axis at decade majors (no linear step)
//   86b Graph::setScale     - the tick-count guard counts decades on a log axis (step 1 kept)
//   86c log autoscale       - 0 / negative data no longer drag the lower bound to 1e-100
//   86d ScaleDraw::label    - Automatic labels of neighbouring majors stay distinct
//   86e formula-linked axis - 1e7/x over a parent range starting at 0 keeps true positions
//   86f axis break          - a tiny before/after step falls back to automatic (tick cap)
//   86g AxesDialog          - Time (ms) / Date (days from origin) start/end conversion
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g86Failures = 0;

#define CHECK86(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g86Failures;                                                                      \
        }                                                                                       \
    } while (0)

std::string suite86Str(const QwtValueList &v)
{
    std::ostringstream os;
    os << "[";
    for (int i = 0; i < v.size(); ++i)
        os << (i ? ", " : "") << v[i];
    os << "]";
    return os.str();
}

QwtValueList suite86Majors(Graph *g, int axis)
{
    const QwtScaleDiv *sd = g->plotWidget()->axisScaleDiv(axis);
    return sd ? sd->ticks(QwtScaleDiv::MajorTick) : QwtValueList();
}

// True when the majors are exactly lo, 10*lo, 100*lo, ... hi (decade steps).
bool suite86IsDecadeSequence(const QwtValueList &m, double lo, double hi)
{
    if (m.size() < 2)
        return false;
    QwtValueList s = m;
    std::sort(s.begin(), s.end());
    if (std::fabs(s.first() - lo) > 1e-9 * lo || std::fabs(s.last() - hi) > 1e-9 * hi)
        return false;
    for (int i = 0; i + 1 < s.size(); ++i) {
        if (!(s[i] > 0.0) || std::fabs(s[i + 1] / s[i] - 10.0) > 1e-9)
            return false;
    }
    return true;
}

Table *suite86Table(ApplicationWindow *mw, const QString &name, const std::vector<double> &x,
                    const std::vector<double> &y)
{
    Table *t = mw->newTable(name, int(x.size()), 2);
    if (!t)
        return nullptr;
    for (int r = 0; r < int(x.size()); ++r) {
        t->column(0)->setValueAt(r, x[size_t(r)]);
        t->column(1)->setValueAt(r, y[size_t(r)]);
    }
    return t;
}

Graph *suite86Graph(ApplicationWindow *mw, const QString &name, const std::vector<double> &x,
                    const std::vector<double> &y, MultiLayer **mlOut = nullptr)
{
    Table *t = suite86Table(mw, name, x, y);
    if (!t)
        return nullptr;
    MultiLayer *ml = mw->multilayerPlot(t, QStringList() << t->colName(1), Graph::LineSymbols);
    if (mlOut)
        *mlOut = ml;
    return ml ? ml->activeGraph() : nullptr;
}

QString suite86Label(Graph *g, int axis, double v)
{
    const ScaleDraw *sd = dynamic_cast<const ScaleDraw *>(g->plotWidget()->axisScaleDraw(axis));
    return sd ? sd->label(v).text() : QStringLiteral("<no ScaleDraw>");
}

struct Suite86AxesDialogProbe : public AxesDialog
{
    void showScaleOf(int qwtAxis)
    {
        setCurrentQwtAxis(qwtAxis);
        updateScale();
    }
    QString startText() const { return boxStart ? boxStart->text() : QString(); }
    QString endText() const { return boxEnd ? boxEnd->text() : QString(); }
    bool applyRange(int qwtAxis, const QString &start, const QString &end)
    {
        showScaleOf(qwtAxis);
        generalDialog.setCurrentWidget(scalesPage);
        boxStart->setText(start);
        boxEnd->setText(end);
        return updatePlot();
    }
};

} // namespace

int runSuite86AxisScaleCorrectness(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 86: axis scale / tick / label correctness (log step, "
                 "log autoscale, label digits, linked 1/x axis, break step cap, date/time "
                 "scale dialog)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 86");
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    g86Failures = 0;

    // ------------------------------------------------ 86a: updateScale with autoscale off
    {
        Graph *g = suite86Graph(mw, "S86LogFreeze", { 1.0, 10.0, 100.0, 1000.0, 10000.0 },
                                { 1.0, 2.0, 3.0, 4.0, 5.0 });
        CHECK_COND(g != nullptr, "86a: graph");
        g->setScale(QwtPlot::xBottom, 1.0, 1e4, 0.0, 5, 8, 1, false);
        const QwtValueList before = suite86Majors(g, QwtPlot::xBottom);
        CHECK86(suite86IsDecadeSequence(before, 1.0, 1e4),
                "86a: setScale(1..1e4, log) majors should be decades, got " << suite86Str(before));
        g->enableAutoscaling(false);
        g->updateScale();
        const QwtValueList after = suite86Majors(g, QwtPlot::xBottom);
        std::cout << "[INFO] 86a log majors after updateScale (autoscale off) = "
                  << suite86Str(after) << "\n";
        CHECK86(suite86IsDecadeSequence(after, 1.0, 1e4),
                "86a: updateScale() with autoscale off must keep decade majors on a log axis, got "
                        << suite86Str(after));
        const QwtScaleDiv *sd = g->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        CHECK86(sd && std::fabs(qMin(sd->lowerBound(), sd->upperBound()) - 1.0) < 1e-9
                        && std::fabs(qMax(sd->lowerBound(), sd->upperBound()) - 1e4) < 1e-6,
                "86a: updateScale() changed the log range");

        // Linear axis: the step passed back is still the major spacing (unchanged behaviour).
        g->setScale(QwtPlot::yLeft, 0.0, 10.0, 2.5, 5, 1, 0, false);
        g->updateScale();
        const QwtValueList lin = suite86Majors(g, QwtPlot::yLeft);
        CHECK86(lin.size() == 5 && std::fabs(lin[1] - lin[0] - 2.5) < 1e-12,
                "86a: linear yLeft majors should stay 0,2.5,..,10, got " << suite86Str(lin));
        g->enableAutoscaling(true);
    }

    // ------------------------------------------------ 86b: setScale tick guard on a log axis
    {
        Graph *g = suite86Graph(mw, "S86LogStep", { 1.0, 10.0, 100.0 }, { 1.0, 2.0, 3.0 });
        CHECK_COND(g != nullptr, "86b: graph");
        g->setScale(QwtPlot::xBottom, 1.0, 1e4, 1.0, 5, 8, 1, false);
        const QwtValueList m = suite86Majors(g, QwtPlot::xBottom);
        std::cout << "[INFO] 86b setScale(1..1e4, step=1, log): axisStep=" << g->axisStep(QwtPlot::xBottom)
                  << " majors=" << suite86Str(m) << "\n";
        CHECK86(std::fabs(g->axisStep(QwtPlot::xBottom) - 1.0) < 1e-12,
                "86b: a 1-decade step on 1..1e4 (4 ticks) must be kept, axisStep="
                        << g->axisStep(QwtPlot::xBottom));
        CHECK86(suite86IsDecadeSequence(m, 1.0, 1e4),
                "86b: majors should be 1,10,..,1e4, got " << suite86Str(m));
        // The guard still rejects an absurd decade step count.
        g->setScale(QwtPlot::xBottom, 1.0, 1e4, 1e-4, 5, 8, 1, false);
        CHECK86(g->axisStep(QwtPlot::xBottom) == 0.0,
                "86b: 40000 log steps must fall back to automatic, axisStep="
                        << g->axisStep(QwtPlot::xBottom));
        // Linear guard unchanged.
        g->setScale(QwtPlot::yLeft, 0.0, 1000.0, 0.01, 5, 1, 0, false);
        CHECK86(g->axisStep(QwtPlot::yLeft) == 0.0,
                "86b: 100000 linear steps must fall back to automatic");
    }

    // ------------------------------------------------ 86c: log autoscale with 0 in the data
    {
        Graph *g = suite86Graph(mw, "S86LogAuto", { 1.0, 2.0, 3.0, 4.0 }, { 0.0, 1.0, 10.0, 1000.0 });
        CHECK_COND(g != nullptr, "86c: graph");
        g->setScale(QwtPlot::yLeft, 1.0, 1000.0, 0.0, 5, 8, 1, false);
        g->setAutoScale();
        const QwtScaleDiv *sd = g->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
        const double lo = sd ? qMin(sd->lowerBound(), sd->upperBound()) : 0.0;
        const double hi = sd ? qMax(sd->lowerBound(), sd->upperBound()) : 0.0;
        std::cout << "[INFO] 86c log autoscale y={0,1,10,1000}: " << lo << " .. " << hi << "\n";
        CHECK86(lo >= 0.09 && lo <= 1.0 + 1e-9,
                "86c: log autoscale lower bound must come from the positive data (~1), got " << lo);
        CHECK86(hi >= 1000.0 * (1.0 - 1e-9) && hi <= 1e4 * (1.0 + 1e-9),
                "86c: log autoscale upper bound should cover 1000, got " << hi);
        const QwtValueList m = suite86Majors(g, QwtPlot::yLeft);
        CHECK86(m.size() >= 2 && m.size() <= 12,
                "86c: log autoscale major count should be a few decades, got " << m.size());

        // Single-axis autoscale goes through the same path.
        g->setScale(QwtPlot::yLeft, 1.0, 1000.0, 0.0, 5, 8, 1, false);
        g->setAutoScaleAxis(QwtPlot::yLeft);
        sd = g->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
        const double lo2 = sd ? qMin(sd->lowerBound(), sd->upperBound()) : 0.0;
        CHECK86(lo2 >= 0.09 && lo2 <= 1.0 + 1e-9,
                "86c: setAutoScaleAxis(log) lower bound must be ~1, got " << lo2);

        // All-positive data keep Qwt's own result.
        Graph *gp = suite86Graph(mw, "S86LogAutoPos", { 1.0, 2.0, 3.0 }, { 2.0, 20.0, 200.0 });
        CHECK_COND(gp != nullptr, "86c: graph (positive)");
        gp->setScale(QwtPlot::yLeft, 1.0, 1000.0, 0.0, 5, 8, 1, false);
        gp->setAutoScale();
        sd = gp->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
        const double lo3 = sd ? qMin(sd->lowerBound(), sd->upperBound()) : 0.0;
        const double hi3 = sd ? qMax(sd->lowerBound(), sd->upperBound()) : 0.0;
        CHECK86(lo3 > 0.5 && lo3 <= 2.0 && hi3 >= 200.0 && hi3 <= 1000.0 + 1e-6,
                "86c: positive log autoscale range changed: " << lo3 << " .. " << hi3);
    }

    // ------------------------------------------------ 86d: Automatic label digits
    {
        Graph *g = suite86Graph(mw, "S86Labels", { 0.0, 1.0 }, { 0.0, 1.0 });
        CHECK_COND(g != nullptr, "86d: graph");
        const int ax = QwtPlot::yLeft;
        auto setLin = [&](double lo, double hi, double step) {
            g->setScale(ax, lo, hi, step, 5, 1, 0, false);
            g->setLabelsNumericFormat(ax, Plot::Automatic, 4);
        };

        setLin(101300.0, 101340.0, 20.0);
        {
            const QString a = suite86Label(g, ax, 101300.0);
            const QString b = suite86Label(g, ax, 101320.0);
            const QString c = suite86Label(g, ax, 101340.0);
            std::cout << "[INFO] 86d labels 101300/101320/101340 = " << a.toStdString() << " | "
                      << b.toStdString() << " | " << c.toStdString() << "\n";
            CHECK86(a != b && b != c && a != c,
                    "86d: labels of 101300/101320/101340 must differ, got " << a.toStdString()
                            << " | " << b.toStdString() << " | " << c.toStdString());
        }

        setLin(1.0, 1.0001, 0.00005);
        {
            const QwtValueList m = suite86Majors(g, ax);
            CHECK86(m.size() == 3, "86d: expected 3 majors on 1..1.0001, got " << suite86Str(m));
            QStringList labels;
            for (double v : m)
                labels << suite86Label(g, ax, v);
            std::cout << "[INFO] 86d labels 1/1.00005/1.0001 = "
                      << labels.join(QStringLiteral(" | ")).toStdString() << "\n";
            bool distinct = labels.size() >= 2;
            for (int i = 0; i + 1 < labels.size(); ++i)
                distinct = distinct && labels[i] != labels[i + 1];
            CHECK86(distinct, "86d: labels of 1.00000/1.00005/1.00010 must differ, got "
                                      << labels.join(QStringLiteral(" | ")).toStdString());
        }

        // Labels that were already distinct keep their old text.
        setLin(0.0, 100.0, 20.0);
        CHECK86(suite86Label(g, ax, 0.0) == QLatin1String("0")
                        && suite86Label(g, ax, 20.0) == QLatin1String("20")
                        && suite86Label(g, ax, 100.0) == QLatin1String("100"),
                "86d: 0..100 labels changed: " << suite86Label(g, ax, 20.0).toStdString());
        setLin(200000.0, 1000000.0, 200000.0);
        CHECK86(suite86Label(g, ax, 200000.0) == QLatin1String("2e+05")
                        && suite86Label(g, ax, 400000.0) == QLatin1String("4e+05")
                        && suite86Label(g, ax, 1000000.0) == QLatin1String("1e+06"),
                "86d: 2e5..1e6 labels changed: " << suite86Label(g, ax, 400000.0).toStdString());
        setLin(100000.0, 100400.0, 100.0);
        CHECK86(suite86Label(g, ax, 100000.0) == QLatin1String("1e+05")
                        && suite86Label(g, ax, 100100.0) == QLatin1String("1.001e+05")
                        && suite86Label(g, ax, 100200.0) == QLatin1String("1.002e+05"),
                "86d: 1e5..1.004e5 labels changed: "
                        << suite86Label(g, ax, 100100.0).toStdString());
    }

    // ------------------------------------------------ 86e: formula-linked 1e7/x over 0..4000
    {
        MultiLayer *ml = nullptr;
        Graph *parent = suite86Graph(mw, "S86Linked", { 100.0, 4000.0 }, { 1.0, 2.0 }, &ml);
        CHECK_COND(parent != nullptr && ml != nullptr, "86e: parent graph");
        parent->setScale(QwtPlot::xBottom, 0.0, 4000.0, 0.0, 5, 1, 0, false);
        Graph *child = ml->addLinkedLayer(parent, true, false, QStringLiteral("1e7/x"));
        CHECK_COND(child != nullptr && child->linkedParentLayer() == parent, "86e: child layer");

        const QwtScaleEngine *se = child->plotWidget()->axisScaleEngine(QwtPlot::xTop);
        const QwtScaleDiv *sd = child->plotWidget()->axisScaleDiv(QwtPlot::xTop);
        std::unique_ptr<QwtScaleTransformation> tf(se ? se->transformation() : nullptr);
        std::cout << "[INFO] 86e child xTop engine=" << (se ? typeid(*se).name() : "null")
                  << " div=" << (sd ? sd->lowerBound() : 0.0) << ".." << (sd ? sd->upperBound() : 0.0)
                  << " linkMode=" << child->xLinkMode() << "\n";
        CHECK86(tf && tf->type() == QwtScaleTransformation::Other && sd,
                "86e: child xTop should use the formula-linked (non-linear) transformation");
        if (tf && tf->type() == QwtScaleTransformation::Other && sd) {
            const double s1 = sd->lowerBound();
            const double s2 = sd->upperBound();
            // The end with the larger u (=1e7/x) sits at the parent's lower end x = 0.
            const double fromParentLo = (s1 > s2) ? 0.0 : 1.0;
            auto parentFraction = [&](double u) {
                const double px = tf->xForm(u, s1, s2, 0.0, 1000.0) / 1000.0;
                return std::fabs(px - fromParentLo);
            };
            const double f10k = parentFraction(1e4);   // x = 1000 -> 25 %
            const double f5k = parentFraction(5e3);    // x = 2000 -> 50 %
            const double f2500 = parentFraction(2500); // x = 4000 -> 100 %
            std::cout << "[INFO] 86e 1e7/x positions: u=1e4 -> " << f10k << ", u=5e3 -> " << f5k
                      << ", u=2500 -> " << f2500 << " (div " << s1 << " .. " << s2 << ")\n";
            CHECK86(std::fabs(f10k - 0.25) < 2e-3,
                    "86e: tick u=1e4 (x=1000 of 0..4000) must sit at 25 %, got " << f10k);
            CHECK86(std::fabs(f5k - 0.50) < 2e-3,
                    "86e: tick u=5e3 (x=2000 of 0..4000) must sit at 50 %, got " << f5k);
            CHECK86(std::fabs(f2500 - 1.0) < 1e-6,
                    "86e: u=2500 (x=4000) must sit at the parent's upper end, got " << f2500);
            // invXForm stays the inverse of xForm.
            const double px = tf->xForm(1e4, s1, s2, 0.0, 1000.0);
            const double back = tf->invXForm(px, 0.0, 1000.0, s1, s2);
            CHECK86(std::fabs(back - 1e4) < 1e-6 * 1e4,
                    "86e: invXForm(xForm(1e4)) = " << back);
        }
    }

    // ------------------------------------------------ 86f: break step cap
    {
        bool engineOk = true;
        {
            Graph::AxisBreakSpec spec;
            spec.enabled = true;
            spec.breakFrom = 100.0;
            spec.breakTo = 900.0;
            spec.stepBefore = 0.001;
            GraphInternal::PiecewiseBreakScaleEngine eng(spec);
            const QwtScaleDiv div = eng.divideScale(0.0, 1000.0, 5, 4, 0.0);
            const int n = div.ticks(QwtScaleDiv::MajorTick).size();
            std::cout << "[INFO] 86f break 0..1000 (100..900, stepBefore 0.001): " << n
                      << " majors\n";
            CHECK86(n >= 2 && n <= 4002,
                    "86f: break step 0.001 over 0..100 must fall back to automatic, majors=" << n);
            engineOk = engineOk && n <= 4002;

            Graph::AxisBreakSpec spec2 = spec;
            spec2.stepBefore = 0.0;
            spec2.stepAfter = 0.0001;
            GraphInternal::PiecewiseBreakScaleEngine eng2(spec2);
            const int n2 = eng2.divideScale(0.0, 1000.0, 5, 4, 0.0)
                                   .ticks(QwtScaleDiv::MajorTick)
                                   .size();
            CHECK86(n2 >= 2 && n2 <= 4002, "86f: stepAfter 0.0001 must be capped, majors=" << n2);
            engineOk = engineOk && n2 <= 4002;

            Graph::AxisBreakSpec logSpec;
            logSpec.enabled = true;
            logSpec.breakFrom = 100.0;
            logSpec.breakTo = 1000.0;
            logSpec.log10Before = true;
            logSpec.log10After = true;
            logSpec.stepBefore = 1e-4; // 2 decades / 1e-4 = 20000 ticks
            logSpec.stepAfter = 1.0;   // 3 decades / 1 = 3 ticks: kept
            GraphInternal::PiecewiseBreakScaleEngine eng3(logSpec);
            const QwtScaleDiv div3 = eng3.divideScale(1.0, 1e6, 5, 9, 0.0);
            const QwtValueList m3 = div3.ticks(QwtScaleDiv::MajorTick);
            std::cout << "[INFO] 86f log break 1..1e6 (100..1000): " << m3.size() << " majors\n";
            CHECK86(m3.size() >= 2 && m3.size() <= 4002,
                    "86f: log stepBefore 1e-4 decades must be capped, majors=" << m3.size());
            bool has1e5 = false;
            for (double v : m3)
                has1e5 = has1e5 || std::fabs(v - 1e5) < 1e-3;
            CHECK86(has1e5, "86f: log stepAfter = 1 decade must be kept (1e5 tick expected), got "
                                    << (m3.size() < 40 ? suite86Str(m3) : std::string("...")));
            engineOk = engineOk && m3.size() <= 4002;
        }
        // Through the Graph only when the engine is bounded (an unbounded tick list would hang
        // the label layout instead of failing).
        if (engineOk) {
            Graph *g = suite86Graph(mw, "S86Break", { 0.0, 1000.0 }, { 0.0, 1.0 });
            CHECK_COND(g != nullptr, "86f: graph");
            g->setScale(QwtPlot::xBottom, 0.0, 1000.0, 0.0, 5, 4, 0, false);
            Graph::AxisBreakSpec spec;
            spec.enabled = true;
            spec.breakFrom = 100.0;
            spec.breakTo = 900.0;
            spec.stepBefore = 0.001;
            g->setAxisBreak(QwtPlot::xBottom, spec);
            const int n = suite86Majors(g, QwtPlot::xBottom).size();
            std::cout << "[INFO] 86f graph break majors = " << n << "\n";
            CHECK86(n >= 2 && n <= 4002, "86f: graph break majors not capped: " << n);
        }
    }

    // ------------------------------------------------ 86g: AxesDialog Time / Date conversion
    {
        Graph *g = suite86Graph(mw, "S86DateTime", { 0.0, 3600000.0 }, { 0.0, 10.0 });
        CHECK_COND(g != nullptr, "86g: graph");
        g->setLabelsDateTimeFormat(QwtPlot::xBottom, Graph::AxisType::Time,
                                   QStringLiteral("00:00:00;hh:mm:ss"));
        g->setScale(QwtPlot::xBottom, 0.0, 3600000.0, 0.0, 5, 1, 0, false);
        g->setLabelsDateTimeFormat(QwtPlot::yLeft, Graph::AxisType::Date,
                                   QStringLiteral("2020-01-01;yyyy-MM-dd"));
        g->setScale(QwtPlot::yLeft, 0.0, 10.0, 0.0, 5, 1, 0, false);

        {
            Suite86AxesDialogProbe dlg;
            dlg.setGraph(g);
            dlg.showScaleOf(QwtPlot::xBottom);
            std::cout << "[INFO] 86g Time axis 0..3600000 ms shown as " << dlg.startText().toStdString()
                      << " .. " << dlg.endText().toStdString() << "\n";
            CHECK86(dlg.startText() == QLatin1String("1970-01-01 00:00:00")
                            && dlg.endText() == QLatin1String("1970-01-01 01:00:00"),
                    "86g: Time axis (ms after 00:00:00) shown as " << dlg.startText().toStdString()
                            << " .. " << dlg.endText().toStdString());
            dlg.showScaleOf(QwtPlot::yLeft);
            std::cout << "[INFO] 86g Date axis 0..10 days shown as " << dlg.startText().toStdString()
                      << " .. " << dlg.endText().toStdString() << "\n";
            CHECK86(dlg.startText() == QLatin1String("2020-01-01 00:00:00")
                            && dlg.endText() == QLatin1String("2020-01-11 00:00:00"),
                    "86g: Date axis (days after 2020-01-01) shown as "
                            << dlg.startText().toStdString() << " .. "
                            << dlg.endText().toStdString());
        }
        {
            Suite86AxesDialogProbe dlg;
            dlg.setGraph(g);
            const bool ok = dlg.applyRange(QwtPlot::xBottom, QStringLiteral("1970-01-01 00:10:00"),
                                           QStringLiteral("1970-01-01 00:50:00"));
            const QwtScaleDiv *sd = g->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
            const double lo = sd ? qMin(sd->lowerBound(), sd->upperBound()) : -1.0;
            const double hi = sd ? qMax(sd->lowerBound(), sd->upperBound()) : -1.0;
            std::cout << "[INFO] 86g Time apply 00:10..00:50 -> " << lo << " .. " << hi << " ms\n";
            CHECK86(ok && std::fabs(lo - 600000.0) < 1e-3 && std::fabs(hi - 3000000.0) < 1e-3,
                    "86g: Time axis 00:10..00:50 must become 600000..3000000 ms, got " << lo
                            << " .. " << hi);
        }
        {
            Suite86AxesDialogProbe dlg;
            dlg.setGraph(g);
            const bool ok = dlg.applyRange(QwtPlot::yLeft, QStringLiteral("2020-01-03 00:00:00"),
                                           QStringLiteral("2020-01-08 12:00:00"));
            const QwtScaleDiv *sd = g->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
            const double lo = sd ? qMin(sd->lowerBound(), sd->upperBound()) : -1.0;
            const double hi = sd ? qMax(sd->lowerBound(), sd->upperBound()) : -1.0;
            std::cout << "[INFO] 86g Date apply 01-03..01-08 12:00 -> " << lo << " .. " << hi
                      << " days\n";
            CHECK86(ok && std::fabs(lo - 2.0) < 1e-9 && std::fabs(hi - 7.5) < 1e-9,
                    "86g: Date axis 2020-01-03..2020-01-08 12:00 must become 2..7.5 days, got "
                            << lo << " .. " << hi);
        }
    }

    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    if (g86Failures > 0) {
        std::cerr << "[FAIL] Suite 86: " << g86Failures << " check(s) failed.\n";
        return 1;
    }
    std::cout << "[PASS] Suite 86 passed.\n";
    return 0;
}
