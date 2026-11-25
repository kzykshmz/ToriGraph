#include "ole_test_common.h"

#include <cmath>
#include <vector>

// =============================================================================
// Suite 91: Graph::copy keeps the stack offset / 0-1 normalisation exactly once.
//   Graph::copy (used by Duplicate window, layer copy/paste and the clipboard layer) used to
//   copy the source curve's *displayed* points (offset / normalisation already applied) and
//   then apply the offset and normalisation again on the copy, so a duplicated graph showed
//   its curves shifted twice and normalised error bars lost their 1/span scaling.
//   91a Duplicate window (ApplicationWindow::clone): every point and error value of the copy
//       equals the source's; the copy maps its points back to the table values
//   91b the same through a plain Graph::copy into a new layer (layer paste path)
//   91c switching the copy's offset / normalisation off returns to the table values
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g91Failures = 0;

#define CHECK91(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g91Failures;                                                                      \
        }                                                                                       \
    } while (0)

bool near91(double a, double b, double tol = 1e-9)
{
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol * qMax(1.0, std::fabs(b));
}

const std::vector<double> kX91 = { 1, 2, 3, 4, 5 };
const std::vector<double> kY1 = { 10, 20, 30, 40, 50 };
const std::vector<double> kY2 = { 5, 7, 1, 9, 3 };
const std::vector<double> kE1 = { 1, 2, 3, 4, 5 };
const std::vector<double> kE2 = { 0.5, 1, 1.5, 2, 2.5 };

// Data curves (not error bars) of a graph, in curve order.
QList<DataCurve *> suite91Masters(Graph *g)
{
    QList<DataCurve *> out;
    if (!g)
        return out;
    for (int i = 0; i < g->curves(); ++i) {
        DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(i));
        if (dc && dc->type() != Graph::ErrorBars)
            out << dc;
    }
    return out;
}

QwtErrorPlotCurve *suite91ErrorBars(DataCurve *master)
{
    if (!master || master->errorBarsList().isEmpty())
        return nullptr;
    return dynamic_cast<QwtErrorPlotCurve *>(master->errorBarsList().first());
}

// Compares the copy's curves (points, stack state, error bars) with the source's.
void suite91Compare(const char *tag, Graph *src, Graph *dst)
{
    const QList<DataCurve *> sm = suite91Masters(src);
    const QList<DataCurve *> dm = suite91Masters(dst);
    CHECK91(sm.size() == 2 && dm.size() == 2,
            tag << ": data curve counts src=" << sm.size() << " dst=" << dm.size());
    if (sm.size() != dm.size())
        return;
    for (int c = 0; c < sm.size(); ++c) {
        DataCurve *s = sm[c];
        DataCurve *d = dm[c];
        CHECK91(d->stackOffsetX() == s->stackOffsetX() && d->stackOffsetY() == s->stackOffsetY()
                        && d->stackNormalize01() == s->stackNormalize01(),
                tag << ": curve " << c << " stack state dst=(" << d->stackOffsetX() << ","
                    << d->stackOffsetY() << "," << d->stackNormalize01() << ") src=("
                    << s->stackOffsetX() << "," << s->stackOffsetY() << ","
                    << s->stackNormalize01() << ")");
        CHECK91(d->dataSize() == s->dataSize(), tag << ": curve " << c << " point counts dst="
                                                     << d->dataSize() << " src=" << s->dataSize());
        const int n = qMin(d->dataSize(), s->dataSize());
        for (int i = 0; i < n; ++i) {
            CHECK91(near91(d->x(i), s->x(i)) && near91(d->y(i), s->y(i)),
                    tag << ": curve " << c << " point " << i << " dst=(" << d->x(i) << ","
                        << d->y(i) << ") src=(" << s->x(i) << "," << s->y(i) << ")");
            double rx = std::nan(""), ry = std::nan("");
            const bool ok = d->displayToRaw(d->x(i), d->y(i), &rx, &ry);
            const std::vector<double> &ys = (c == 0) ? kY1 : kY2;
            CHECK91(ok && near91(rx, kX91[size_t(i)]) && near91(ry, ys[size_t(i)]),
                    tag << ": curve " << c << " point " << i << " maps back to (" << rx << ","
                        << ry << "), table (" << kX91[size_t(i)] << "," << ys[size_t(i)] << ")");
            CHECK91(d->tableRow(i) == s->tableRow(i),
                    tag << ": curve " << c << " point " << i << " row dst=" << d->tableRow(i)
                        << " src=" << s->tableRow(i));
        }

        QwtErrorPlotCurve *se = suite91ErrorBars(s);
        QwtErrorPlotCurve *de = suite91ErrorBars(d);
        CHECK91(se && de, tag << ": curve " << c << " error bars src=" << (se != nullptr)
                              << " dst=" << (de != nullptr));
        if (!se || !de)
            continue;
        CHECK91(de->dataSize() == se->dataSize(),
                tag << ": curve " << c << " error points dst=" << de->dataSize()
                    << " src=" << se->dataSize());
        const int ne = qMin(de->dataSize(), se->dataSize());
        for (int i = 0; i < ne; ++i) {
            CHECK91(near91(de->x(i), se->x(i)) && near91(de->y(i), se->y(i))
                            && near91(de->errorValue(i), se->errorValue(i)),
                    tag << ": curve " << c << " error bar " << i << " dst=(" << de->x(i) << ","
                        << de->y(i) << ",err " << de->errorValue(i) << ") src=(" << se->x(i)
                        << "," << se->y(i) << ",err " << se->errorValue(i) << ")");
        }
    }
}

void suite91Close(MultiLayer *ml)
{
    if (!ml)
        return;
    ml->askOnCloseEvent(false);
    ml->close();
}

} // namespace

int runSuite91GraphCopyStackTransforms(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 91: Graph::copy keeps stack offset / 0-1 normalisation "
                 "and error bars (Duplicate window, layer copy)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 91");
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    g91Failures = 0;

    Table *t = mw->newTable("S91Stack", int(kX91.size()), 5);
    CHECK_COND(t != nullptr, "91: table");
    for (int r = 0; r < int(kX91.size()); ++r) {
        t->column(0)->setValueAt(r, kX91[size_t(r)]);
        t->column(1)->setValueAt(r, kY1[size_t(r)]);
        t->column(2)->setValueAt(r, kY2[size_t(r)]);
        t->column(3)->setValueAt(r, kE1[size_t(r)]);
        t->column(4)->setValueAt(r, kE2[size_t(r)]);
    }
    MultiLayer *ml = mw->multilayerPlot(t, QStringList() << t->colName(1) << t->colName(2),
                                        Graph::LineSymbols);
    Graph *g = ml ? ml->activeGraph() : nullptr;
    CHECK_COND(g != nullptr, "91: source graph");
    CHECK_COND(g->addErrorBars(t->colName(1), t, t->colName(3), QwtErrorPlotCurve::Vertical),
               "91: error bars on curve 1");
    CHECK_COND(g->addErrorBars(t->colName(2), t, t->colName(4), QwtErrorPlotCurve::Vertical),
               "91: error bars on curve 2");
    const QList<DataCurve *> sm = suite91Masters(g);
    CHECK_COND(sm.size() == 2, "91: two source data curves");
    // Curve 1: X offset 2, Y offset 10. Curve 2: 0-1 normalised (span 8) plus X 0.5 / Y 1.
    sm[0]->setStackOffset(2.0, 10.0);
    sm[1]->setStackNormalize01(true);
    sm[1]->setStackOffset(0.5, 1.0);
    CHECK_COND(near91(sm[0]->x(0), 3.0) && near91(sm[0]->y(0), 20.0), "91: source offset");
    CHECK_COND(near91(sm[1]->y(3), 2.0) && near91(sm[1]->y(2), 1.0), "91: source normalised");
    QwtErrorPlotCurve *srcErr2 = suite91ErrorBars(sm[1]);
    CHECK_COND(srcErr2 && near91(srcErr2->errorValue(0), 0.5 / 8.0),
               "91: source normalised error scaled by 1/span");

    // ------------------------------------------------ 91a: Duplicate window
    MultiLayer *dup = dynamic_cast<MultiLayer *>(mw->clone(ml));
    Graph *gd = dup ? dup->activeGraph() : nullptr;
    CHECK91(gd != nullptr, "91a: duplicated graph");
    if (gd) {
        const QList<DataCurve *> dm = suite91Masters(gd);
        if (dm.size() == 2) {
            std::cout << "[INFO] 91a duplicate: curve1 p0=(" << dm[0]->x(0) << "," << dm[0]->y(0)
                      << ") src (" << sm[0]->x(0) << "," << sm[0]->y(0) << "); curve2 p3=("
                      << dm[1]->x(3) << "," << dm[1]->y(3) << ") src (" << sm[1]->x(3) << ","
                      << sm[1]->y(3) << ")\n";
        }
        suite91Compare("91a", g, gd);
    }

    // ------------------------------------------------ 91b: Graph::copy into a new layer
    Graph *gl = ml->addLayer();
    CHECK91(gl != nullptr, "91b: new layer");
    if (gl) {
        gl->copy(mw, g);
        suite91Compare("91b", g, gl);
    }

    // ------------------------------------------------ 91c: transforms off on the copy
    if (gd) {
        const QList<DataCurve *> dm = suite91Masters(gd);
        if (dm.size() == 2) {
            dm[0]->setStackOffset(0.0, 0.0);
            dm[1]->setStackOffset(0.0, 0.0);
            dm[1]->setStackNormalize01(false);
            for (int c = 0; c < 2; ++c) {
                const std::vector<double> &ys = (c == 0) ? kY1 : kY2;
                const std::vector<double> &es = (c == 0) ? kE1 : kE2;
                CHECK91(dm[c]->dataSize() == int(kX91.size()),
                        "91c: curve " << c << " points " << dm[c]->dataSize());
                QwtErrorPlotCurve *de = suite91ErrorBars(dm[c]);
                for (int i = 0; i < qMin(dm[c]->dataSize(), int(kX91.size())); ++i) {
                    CHECK91(near91(dm[c]->x(i), kX91[size_t(i)]) && near91(dm[c]->y(i), ys[size_t(i)]),
                            "91c: curve " << c << " point " << i << " = (" << dm[c]->x(i) << ","
                                          << dm[c]->y(i) << "), table (" << kX91[size_t(i)]
                                          << "," << ys[size_t(i)] << ")");
                    if (de && i < de->dataSize()) {
                        CHECK91(near91(de->y(i), ys[size_t(i)])
                                        && near91(de->errorValue(i), es[size_t(i)]),
                                "91c: curve " << c << " error bar " << i << " = (y " << de->y(i)
                                              << ", err " << de->errorValue(i) << "), table (y "
                                              << ys[size_t(i)] << ", err " << es[size_t(i)]
                                              << ")");
                    }
                }
            }
        }
    }

    suite91Close(dup);
    suite91Close(ml);
    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);

    if (g91Failures != 0) {
        std::cerr << "[FAIL] Suite 91: " << g91Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 91: Graph::copy keeps stack offset / 0-1 normalisation once and "
                 "error bars match (Duplicate window, layer copy).\n";
    return 0;
}
