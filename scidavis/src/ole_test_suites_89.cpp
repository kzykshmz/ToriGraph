#include "ole_test_common.h"
#include "RangeSelectorTool.h"
#include "TranslateCurveTool.h"
#include "VectorCurve.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>

#include <cmath>
#include <memory>
#include <vector>

// =============================================================================
// Suite 89: curve data extraction and point <-> table row mapping.
//   89a error bars follow the master's rows: an invalid Y cell does not shift the errors, and
//       reloading only the error column keeps the master's stack offset / 0-1 normalisation
//   89b DataCurve::setRowRange / setFullRange also move the error bars' own row range
//   89c RangeSelectorTool: clicking the largest-x point puts the other marker on the smallest x
//   89d DataPickerTool Move writes original (un-offset / un-normalised) values to the table
//   89e DataPickerTool Move on horizontal bars writes x to the Y column and y to the X column
//   89f box / histogram points map to no table row (-1); Move refuses them; vector curves map
//       their points to the rows they came from
//   89g Graph::updateCurvesData does not skip a curve when the previous one is removed
//   89h error bars survive master reloads that leave no finite error (row range, stack offset)
//   89i TranslateCurveTool writes each point back to its own row, in original coordinates;
//       box plots are refused
//   89j RangeSelectorTool status shows the point number for box plots (no table row)
//   89k baseline subtraction / Subtract Straight Line do not write histogram bins to the table
//   89l York (Deming) fit leaves out points whose associated error cell is empty
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g89Failures = 0;

#define CHECK89(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g89Failures;                                                                      \
        }                                                                                       \
    } while (0)

bool near89(double a, double b, double tol = 1e-9)
{
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol * qMax(1.0, std::fabs(b));
}

// Table with the given columns (column 0 is the X column); NaN entries are left invalid.
Table *suite89Table(ApplicationWindow *mw, const QString &name,
                    const std::vector<std::vector<double>> &cols)
{
    const int rows = cols.empty() ? 0 : int(cols[0].size());
    Table *t = mw->newTable(name, rows, int(cols.size()));
    if (!t)
        return nullptr;
    for (int c = 0; c < int(cols.size()); ++c) {
        for (int r = 0; r < rows; ++r) {
            const double v = cols[size_t(c)][size_t(r)];
            if (std::isfinite(v))
                t->column(c)->setValueAt(r, v);
            else
                t->column(c)->setInvalid(r, true);
        }
    }
    return t;
}

Graph *suite89Plot(ApplicationWindow *mw, Table *t, const QStringList &yCols, int style,
                   MultiLayer **mlOut = nullptr)
{
    if (!t)
        return nullptr;
    MultiLayer *ml = mw->multilayerPlot(t, yCols, style);
    if (mlOut)
        *mlOut = ml;
    return ml ? ml->activeGraph() : nullptr;
}

QwtErrorPlotCurve *suite89ErrorBars(DataCurve *master)
{
    if (!master || master->errorBarsList().isEmpty())
        return nullptr;
    return dynamic_cast<QwtErrorPlotCurve *>(master->errorBarsList().first());
}

void suite89Close(MultiLayer *ml)
{
    if (!ml)
        return;
    ml->askOnCloseEvent(false);
    ml->close();
}

double suite89Cell(Table *t, int col, int row)
{
    return (t && t->column(col) && !t->column(col)->isInvalid(row)) ? t->column(col)->valueAt(row)
                                                                      : std::nan("");
}

// Silent York fit without a fit curve on the graph.
class Suite89LinearFit : public LinearFit
{
public:
    Suite89LinearFit(ApplicationWindow *app, Graph *g) : LinearFit(app, g)
    {
        d_silent = true;
        d_suppress_curve_output = true;
    }
};

} // namespace

int runSuite89CurveDataMapping(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 89: curve data extraction / point-row mapping (error "
                 "bars, row range, range selector, data picker, box/histogram/vector rows, "
                 "updateCurvesData)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 89");
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    g89Failures = 0;

    // ------------------------------------------------ 89a: error values follow the rows
    {
        Table *t = suite89Table(mw, "S89Err", { { 1, 2, 3, 4, 5 },
                                                { 10, 20, 30, 40, 50 },
                                                { 1, 2, 3, 4, 5 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::LineSymbols, &ml);
        CHECK_COND(g != nullptr, "89a: graph");
        CHECK_COND(g->addErrorBars(t->colName(1), t, t->colName(2), QwtErrorPlotCurve::Vertical),
                   "89a: addErrorBars");
        DataCurve *m = dynamic_cast<DataCurve *>(g->curve(0));
        QwtErrorPlotCurve *eb = suite89ErrorBars(m);
        CHECK_COND(m && eb, "89a: master / error bar curve");

        // Y3 becomes empty after plotting: the master drops row 2 and is reloaded alone.
        t->blockSignals(true);
        t->column(1)->setInvalid(2, true);
        t->blockSignals(false);
        g->updateCurvesData(t, t->colName(1));
        std::cout << "[INFO] 89a master points " << m->dataSize() << ", error points "
                  << eb->dataSize() << ", error of point 2 (row 4) = " << eb->errorValue(2)
                  << "\n";
        CHECK89(m->dataSize() == 4 && eb->dataSize() == 4,
                "89a: master/error point counts " << m->dataSize() << "/" << eb->dataSize());
        CHECK89(near89(eb->errorValue(2), 4.0) && near89(eb->errorValue(3), 5.0),
                "89a: point 2 (row 4) must carry err=4, got " << eb->errorValue(2));
        CHECK89(eb->tableRow(2) == 3, "89a: error bar point 2 must map to row 3, got "
                                              << eb->tableRow(2));

        // Stack offset, then only the error column changes: bars stay on the shifted points.
        m->setStackOffset(0.0, 100.0);
        t->blockSignals(true);
        t->column(2)->setValueAt(0, 1.5);
        t->blockSignals(false);
        g->updateCurvesData(t, t->colName(2));
        std::cout << "[INFO] 89a offset: master y0=" << m->y(0) << ", error bar y0=" << eb->y(0)
                  << ", err0=" << eb->errorValue(0) << "\n";
        CHECK89(eb->dataSize() == m->dataSize() && near89(eb->y(0), m->y(0))
                        && near89(eb->y(0), 110.0) && near89(eb->errorValue(0), 1.5),
                "89a: error-column reload lost the stack offset: bar y0=" << eb->y(0)
                        << " master y0=" << m->y(0) << " err0=" << eb->errorValue(0));

        // 0-1 normalisation: positions follow the master, errors scale by 1/span (span 40).
        m->setStackOffset(0.0, 0.0);
        m->setStackNormalize01(true);
        std::cout << "[INFO] 89a normalised: master y1=" << m->y(1) << ", bar y1=" << eb->y(1)
                  << ", err1=" << eb->errorValue(1) << "\n";
        CHECK89(near89(eb->y(1), m->y(1)) && near89(m->y(1), 0.25),
                "89a: normalised bar y1=" << eb->y(1) << " master y1=" << m->y(1));
        CHECK89(near89(eb->errorValue(1), 2.0 / 40.0),
                "89a: normalised error must be 2/40, got " << eb->errorValue(1));
        t->blockSignals(true);
        t->column(2)->setValueAt(1, 4.0);
        t->blockSignals(false);
        g->updateCurvesData(t, t->colName(2));
        CHECK89(near89(eb->y(1), 0.25) && near89(eb->errorValue(1), 4.0 / 40.0),
                "89a: normalised error-column reload: y1=" << eb->y(1)
                        << " err1=" << eb->errorValue(1));
        m->setStackNormalize01(false);
        CHECK89(near89(eb->y(1), 20.0) && near89(eb->errorValue(1), 4.0),
                "89a: un-normalised bar y1=" << eb->y(1) << " err1=" << eb->errorValue(1));
        suite89Close(ml);
    }

    // ------------------------------------------------ 89b: row range reaches the error bars
    {
        Table *t = suite89Table(mw, "S89Range", { { 1, 2, 3, 4, 5 },
                                                  { 10, 20, 30, 40, 50 },
                                                  { 1, 2, 3, 4, 5 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::LineSymbols, &ml);
        CHECK_COND(g != nullptr, "89b: graph");
        CHECK_COND(g->addErrorBars(t->colName(1), t, t->colName(2), QwtErrorPlotCurve::Vertical),
                   "89b: addErrorBars");
        DataCurve *m = dynamic_cast<DataCurve *>(g->curve(0));
        QwtErrorPlotCurve *eb = suite89ErrorBars(m);
        CHECK_COND(m && eb, "89b: master / error bar curve");
        m->setRowRange(1, 3);
        std::cout << "[INFO] 89b rows 2..4: master points " << m->dataSize() << ", error points "
                  << eb->dataSize() << ", error range " << eb->startRow() << ".." << eb->endRow()
                  << "\n";
        CHECK89(m->dataSize() == 3 && eb->dataSize() == 3,
                "89b: after setRowRange(1,3) master/error points " << m->dataSize() << "/"
                                                                   << eb->dataSize());
        CHECK89(eb->startRow() == 1 && eb->endRow() == 3,
                "89b: error bar row range " << eb->startRow() << ".." << eb->endRow());
        CHECK89(near89(eb->errorValue(0), 2.0), "89b: first error " << eb->errorValue(0));
        m->setFullRange();
        CHECK89(m->dataSize() == 5 && eb->dataSize() == 5 && eb->startRow() == 0
                        && eb->endRow() == 4,
                "89b: after setFullRange master/error points " << m->dataSize() << "/"
                        << eb->dataSize() << ", error range " << eb->startRow() << ".."
                        << eb->endRow());
        suite89Close(ml);
    }

    // ------------------------------------------------ 89c: range selector opposite end
    {
        Table *t = suite89Table(mw, "S89Range2", { { 3, 1, 5, 2, 4 },
                                                   { 30, 10, 50, 20, 40 },
                                                   { 1, 1, 1, 1, 1 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1) << t->colName(2),
                               Graph::LineSymbols, &ml);
        CHECK_COND(g != nullptr && g->curves() == 2, "89c: graph with two curves");
        QwtPlotCurve *c0 = g->curve(0);
        std::unique_ptr<RangeSelectorTool> rs(new RangeSelectorTool(g));
        rs->setSelectedCurve(nullptr);
        rs->selectCurvePoint(c0, 2); // x = 5, the largest x
        std::cout << "[INFO] 89c click on x=5: active point " << rs->activePoint()
                  << ", other point " << rs->inactivePoint() << " (x="
                  << c0->x(rs->inactivePoint()) << ")\n";
        CHECK89(rs->activePoint() == 2 && rs->inactivePoint() == 1,
                "89c: clicking the largest x must put the other marker on the smallest x "
                "(point 1), got " << rs->inactivePoint());
        CHECK89(near89(rs->minXValue(), 1.0) && near89(rs->maxXValue(), 5.0),
                "89c: selected range " << rs->minXValue() << ".." << rs->maxXValue());
        rs->setSelectedCurve(nullptr);
        rs->selectCurvePoint(c0, 1); // x = 1, the smallest x
        CHECK89(rs->inactivePoint() == 2,
                "89c: clicking the smallest x must select the largest x, got "
                        << rs->inactivePoint());
        rs.reset();
        suite89Close(ml);
    }

    // ------------------------------------------------ 89d: Move on offset / normalised curves
    {
        Table *t = suite89Table(mw, "S89MoveOff", { { 1, 2, 3, 4, 5 }, { 10, 20, 30, 40, 50 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::LineSymbols, &ml);
        CHECK_COND(g != nullptr, "89d: graph");
        DataCurve *m = dynamic_cast<DataCurve *>(g->curve(0));
        CHECK_COND(m != nullptr, "89d: curve");
        m->setStackOffset(2.0, 100.0);
        {
            std::unique_ptr<DataPickerTool> dp(new DataPickerTool(g, mw, DataPickerTool::Move));
            dp->selectPoint(m, 1); // shown at (4, 120)
            CHECK89(dp->selectedCurve() == m && dp->selectedPoint() == 1, "89d: selection");
            const bool ok = dp->movePointTo(4.0, 150.0);
            std::cout << "[INFO] 89d offset move to (4,150): table row 2 = ("
                      << suite89Cell(t, 0, 1) << ", " << suite89Cell(t, 1, 1) << ")\n";
            CHECK89(ok && near89(suite89Cell(t, 0, 1), 2.0) && near89(suite89Cell(t, 1, 1), 50.0),
                    "89d: offset curve move must write (2, 50), got (" << suite89Cell(t, 0, 1)
                            << ", " << suite89Cell(t, 1, 1) << ")");
        }
        m = dynamic_cast<DataCurve *>(g->curve(0));
        CHECK_COND(m != nullptr, "89d: curve after move");
        m->setStackOffset(0.0, 0.0);
        m->setStackNormalize01(true);
        m->setStackOffset(0.0, 1.0);
        {
            // rows: (1,10) (2,50) (3,30) (4,40) (5,50): span 40, row 3 shown at 1 + 20/40 = 1.5
            std::unique_ptr<DataPickerTool> dp(new DataPickerTool(g, mw, DataPickerTool::Move));
            dp->selectPoint(m, 2);
            CHECK89(near89(m->y(2), 1.5), "89d: normalised y2 = " << m->y(2));
            const bool ok = dp->movePointTo(3.0, 1.25);
            std::cout << "[INFO] 89d normalised move to (3,1.25): table row 3 = ("
                      << suite89Cell(t, 0, 2) << ", " << suite89Cell(t, 1, 2) << ")\n";
            CHECK89(ok && near89(suite89Cell(t, 0, 2), 3.0) && near89(suite89Cell(t, 1, 2), 20.0),
                    "89d: normalised curve move must write (3, 20), got ("
                            << suite89Cell(t, 0, 2) << ", " << suite89Cell(t, 1, 2) << ")");
        }
        suite89Close(ml);

        // All Y equal: the normalisation cannot be undone, the move is refused.
        Table *tf = suite89Table(mw, "S89MoveFlat", { { 1, 2, 3 }, { 7, 7, 7 } });
        MultiLayer *mlf = nullptr;
        Graph *gf = suite89Plot(mw, tf, QStringList() << tf->colName(1), Graph::LineSymbols, &mlf);
        CHECK_COND(gf != nullptr, "89d: flat graph");
        DataCurve *mf = dynamic_cast<DataCurve *>(gf->curve(0));
        CHECK_COND(mf != nullptr, "89d: flat curve");
        mf->setStackNormalize01(true);
        {
            std::unique_ptr<DataPickerTool> dp(new DataPickerTool(gf, mw, DataPickerTool::Move));
            dp->selectPoint(mf, 1);
            const bool ok = dp->movePointTo(2.0, 0.5);
            CHECK89(!ok && near89(suite89Cell(tf, 1, 1), 7.0) && near89(suite89Cell(tf, 0, 1), 2.0),
                    "89d: flat normalised curve move must be refused, table row 2 = ("
                            << suite89Cell(tf, 0, 1) << ", " << suite89Cell(tf, 1, 1) << ")");
        }
        suite89Close(mlf);
    }

    // ------------------------------------------------ 89e: Move on horizontal bars
    {
        Table *t = suite89Table(mw, "S89HBar", { { 1, 2, 3, 4, 5 }, { 10, 20, 30, 40, 50 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::HorizontalBars, &ml);
        CHECK_COND(g != nullptr, "89e: graph");
        DataCurve *m = dynamic_cast<DataCurve *>(g->curve(0));
        CHECK_COND(m != nullptr && m->type() == Graph::HorizontalBars, "89e: horizontal bars");
        std::cout << "[INFO] 89e bar 2 shown at (" << m->x(2) << ", " << m->y(2) << ")\n";
        CHECK89(near89(m->x(2), 30.0) && near89(m->y(2), 3.0),
                "89e: horizontal bar 2 at (" << m->x(2) << ", " << m->y(2) << ")");
        {
            std::unique_ptr<DataPickerTool> dp(new DataPickerTool(g, mw, DataPickerTool::Move));
            dp->selectPoint(m, 2);
            const bool ok = dp->movePointTo(35.0, 3.0);
            std::cout << "[INFO] 89e move to (35,3): table row 3 = (" << suite89Cell(t, 0, 2)
                      << ", " << suite89Cell(t, 1, 2) << ")\n";
            CHECK89(ok && near89(suite89Cell(t, 0, 2), 3.0) && near89(suite89Cell(t, 1, 2), 35.0),
                    "89e: horizontal bars move must write X=3, Y=35, got ("
                            << suite89Cell(t, 0, 2) << ", " << suite89Cell(t, 1, 2) << ")");
        }
        suite89Close(ml);
    }

    // ------------------------------------------------ 89f: box / histogram / vector rows
    {
        Table *t = suite89Table(mw, "S89Box", { { 1, 2, 3, 4, 5 }, { 50, 10, 40, 20, 30 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::Box, &ml);
        CHECK_COND(g != nullptr, "89f: box graph");
        BoxCurve *box = nullptr;
        for (int i = 0; i < g->curves() && !box; ++i)
            box = dynamic_cast<BoxCurve *>(g->curve(i));
        CHECK_COND(box != nullptr && box->dataSize() == 5, "89f: box curve");
        std::cout << "[INFO] 89f box point 0 (value " << box->y(0) << ") -> row "
                  << box->tableRow(0) << "\n";
        CHECK89(box->tableRow(0) == -1 && box->tableRow(4) == -1,
                "89f: box points must map to no row, got " << box->tableRow(0));
        {
            std::unique_ptr<DataPickerTool> dp(new DataPickerTool(g, mw, DataPickerTool::Move));
            dp->selectPoint(box, 0);
            const bool ok = dp->movePointTo(box->x(0), 999.0);
            CHECK89(!ok && near89(suite89Cell(t, 1, 0), 50.0) && near89(suite89Cell(t, 0, 0), 1.0),
                    "89f: moving a box point must not touch the table, row 1 = ("
                            << suite89Cell(t, 0, 0) << ", " << suite89Cell(t, 1, 0) << ")");
        }
        suite89Close(ml);

        MultiLayer *mlh = nullptr;
        Table *th = suite89Table(mw, "S89Hist", { { 1, 2, 3, 4, 5, 6 }, { 1, 2, 2, 3, 3, 3 } });
        Graph *gh = suite89Plot(mw, th, QStringList() << th->colName(1), Graph::Histogram, &mlh);
        CHECK_COND(gh != nullptr, "89f: histogram graph");
        QwtHistogram *hist = nullptr;
        for (int i = 0; i < gh->curves() && !hist; ++i)
            hist = dynamic_cast<QwtHistogram *>(gh->curve(i));
        CHECK_COND(hist != nullptr && hist->dataSize() > 0, "89f: histogram curve");
        CHECK89(hist->tableRow(0) == -1,
                "89f: histogram bins must map to no row, got " << hist->tableRow(0));
        suite89Close(mlh);

        // Vector: row 2 has an empty end X, so points 0,1,2,3 come from rows 0,2,3,4.
        Table *tv = suite89Table(mw, "S89Vec", { { 1, 2, 3, 4, 5 },
                                                 { 1, 1, 1, 1, 1 },
                                                 { 2, std::nan(""), 4, 5, 6 },
                                                 { 2, 2, 2, 2, 2 } });
        MultiLayer *mlv = mw->newGraph("S89VecGraph");
        Graph *gv = mlv ? (mlv->activeGraph() ? mlv->activeGraph() : mlv->addLayer()) : nullptr;
        CHECK_COND(gv != nullptr && tv != nullptr, "89f: vector graph");
        QStringList vcols;
        for (int c = 0; c < 4; ++c)
            vcols << tv->colName(c);
        gv->plotVectorCurve(tv, vcols, Graph::VectXYXY, 0, 4);
        VectorCurve *vc = nullptr;
        for (int i = 0; i < gv->curves() && !vc; ++i)
            vc = dynamic_cast<VectorCurve *>(gv->curve(i));
        CHECK_COND(vc != nullptr, "89f: vector curve");
        std::cout << "[INFO] 89f vector points " << vc->dataSize() << ", point 1 -> row "
                  << vc->tableRow(1) << "\n";
        CHECK89(vc->dataSize() == 4 && vc->tableRow(0) == 0 && vc->tableRow(1) == 2
                        && vc->tableRow(3) == 4,
                "89f: vector point 1 must map to row 2, got " << vc->tableRow(1));
        suite89Close(mlv);
    }

    // ------------------------------------------------ 89g: updateCurvesData after a removal
    {
        // Y1 only has a value in row 1; clearing X1 empties that curve (removed) while the
        // second curve on the same X column must still be reloaded.
        const double nan = std::nan("");
        Table *t = suite89Table(mw, "S89Upd", { { 1, 2, 3, 4, 5 },
                                                { 10, nan, nan, nan, nan },
                                                { 5, 6, 7, 8, 9 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1) << t->colName(2),
                               Graph::LineSymbols, &ml);
        CHECK_COND(g != nullptr && g->curves() == 2, "89g: graph with two curves");
        t->blockSignals(true);
        t->column(0)->setInvalid(0, true);
        t->blockSignals(false);
        g->updateCurvesData(t, t->colName(0));
        const QwtPlotCurve *left = (g->curves() == 1) ? g->curve(0) : nullptr;
        std::cout << "[INFO] 89g after clearing X1: " << g->curves() << " curve(s), remaining "
                  << (left ? left->dataSize() : -1) << " point(s)\n";
        CHECK89(g->curves() == 1 && left && left->title().text() == t->colName(2)
                        && left->dataSize() == 4,
                "89g: second curve on the same X column was not reloaded ("
                        << g->curves() << " curves, " << (left ? left->dataSize() : -1)
                        << " points)");
        suite89Close(ml);

        Table *t2 = suite89Table(mw, "S89Upd2", { { 1, 2, 3 }, { 1, 2, 3 }, { 4, 5, 6 } });
        MultiLayer *ml2 = nullptr;
        Graph *g2 = suite89Plot(mw, t2, QStringList() << t2->colName(1) << t2->colName(2),
                                Graph::LineSymbols, &ml2);
        CHECK_COND(g2 != nullptr && g2->curves() == 2, "89g: second graph with two curves");
        t2->blockSignals(true);
        for (int r = 0; r < 3; ++r)
            t2->column(0)->setInvalid(r, true);
        t2->blockSignals(false);
        g2->updateCurvesData(t2, t2->colName(0));
        std::cout << "[INFO] 89g after clearing the whole X column: " << g2->curves()
                  << " curve(s) left\n";
        CHECK89(g2->curves() == 0,
                "89g: both curves must be removed when the X column is cleared, "
                        << g2->curves() << " left");
        suite89Close(ml2);
    }

    // ------------------------------------------------ 89h: error bars survive master reloads
    {
        // Errors only in rows 6..10: showing rows 1..5 leaves no finite error on the master's
        // points, which must not delete the error bars.
        const double nan = std::nan("");
        Table *t = suite89Table(mw, "S89ErrKeep", { { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
                                                    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
                                                    { nan, nan, nan, nan, nan, 6, 7, 8, 9, 10 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::LineSymbols, &ml);
        CHECK_COND(g != nullptr, "89h: graph");
        CHECK_COND(g->addErrorBars(t->colName(1), t, t->colName(2), QwtErrorPlotCurve::Vertical),
                   "89h: addErrorBars");
        DataCurve *m = dynamic_cast<DataCurve *>(g->curve(0));
        CHECK_COND(m && suite89ErrorBars(m), "89h: master / error bar curve");
        m->setRowRange(0, 4);
        QwtErrorPlotCurve *eb = suite89ErrorBars(m);
        std::cout << "[INFO] 89h rows 1..5: " << g->curves() << " curve(s), error bars "
                  << (eb ? "kept" : "removed") << ", error points " << (eb ? eb->dataSize() : -1)
                  << "\n";
        CHECK89(eb && g->curves() == 2 && eb->dataSize() == 5,
                "89h: error bars must stay when rows 1..5 have no error value ("
                        << g->curves() << " curves)");
        m->setFullRange();
        eb = suite89ErrorBars(m);
        std::cout << "[INFO] 89h rows 1..10: error points " << (eb ? eb->dataSize() : -1)
                  << ", error of point 7 = " << (eb ? eb->errorValue(7) : nan) << "\n";
        CHECK89(eb && eb->dataSize() == 10 && !std::isfinite(eb->errorValue(0))
                        && near89(eb->errorValue(7), 8.0) && near89(eb->y(7), 8.0),
                "89h: back to rows 1..10 the errors of rows 6..10 must show again");
        suite89Close(ml);

        // Error bars on an all-empty error column, then a stack offset on the master.
        Table *t2 = suite89Table(mw, "S89ErrEmpty", { { 1, 2, 3, 4, 5 },
                                                      { 10, 20, 30, 40, 50 },
                                                      { nan, nan, nan, nan, nan } });
        MultiLayer *ml2 = nullptr;
        Graph *g2 = suite89Plot(mw, t2, QStringList() << t2->colName(1), Graph::LineSymbols, &ml2);
        CHECK_COND(g2 != nullptr, "89h: second graph");
        CHECK_COND(g2->addErrorBars(t2->colName(1), t2, t2->colName(2),
                                    QwtErrorPlotCurve::Vertical),
                   "89h: addErrorBars on an empty column");
        DataCurve *m2 = dynamic_cast<DataCurve *>(g2->curve(0));
        CHECK_COND(m2 && suite89ErrorBars(m2), "89h: master / empty error bar curve");
        m2->setStackOffset(0.0, 100.0);
        QwtErrorPlotCurve *eb2 = suite89ErrorBars(m2);
        CHECK89(eb2 && g2->curves() == 2,
                "89h: offset on a curve with empty error bars removed them (" << g2->curves()
                                                                               << " curves)");
        t2->blockSignals(true);
        t2->column(2)->setValueAt(0, 3.0);
        t2->blockSignals(false);
        g2->updateCurvesData(t2, t2->colName(2));
        eb2 = suite89ErrorBars(m2);
        std::cout << "[INFO] 89h empty column filled: bar y0=" << (eb2 ? eb2->y(0) : nan)
                  << ", err0=" << (eb2 ? eb2->errorValue(0) : nan) << "\n";
        CHECK89(eb2 && near89(eb2->errorValue(0), 3.0) && near89(eb2->y(0), 110.0),
                "89h: error entered later must show on the shifted point");
        suite89Close(ml2);
    }

    // ------------------------------------------------ 89i: TranslateCurveTool rows and offsets
    {
        const double nan = std::nan("");
        Table *t = suite89Table(mw, "S89Trans", { { 1, 2, 3, 4, 5 }, { 10, 20, nan, 40, 50 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::LineSymbols, &ml);
        CHECK_COND(g != nullptr, "89i: graph");
        DataCurve *m = dynamic_cast<DataCurve *>(g->curve(0));
        CHECK_COND(m != nullptr && m->dataSize() == 4, "89i: curve with 4 points");
        {
            std::unique_ptr<TranslateCurveTool> tool(
                    new TranslateCurveTool(g, mw, TranslateCurveTool::Vertical));
            const bool ok = tool->translateCurve(m, 5.0);
            std::cout << "[INFO] 89i vertical +5: Y = " << suite89Cell(t, 1, 0) << ", "
                      << suite89Cell(t, 1, 1) << ", " << suite89Cell(t, 1, 2) << ", "
                      << suite89Cell(t, 1, 3) << ", " << suite89Cell(t, 1, 4) << "\n";
            CHECK89(ok && near89(suite89Cell(t, 1, 0), 15.0) && near89(suite89Cell(t, 1, 1), 25.0)
                            && !std::isfinite(suite89Cell(t, 1, 2))
                            && near89(suite89Cell(t, 1, 3), 45.0)
                            && near89(suite89Cell(t, 1, 4), 55.0),
                    "89i: translating a curve with an empty Y3 must shift rows 1,2,4,5 by 5");
        }
        m = dynamic_cast<DataCurve *>(g->curve(0));
        CHECK_COND(m != nullptr, "89i: curve after translation");
        m->setStackOffset(2.0, 100.0);
        {
            std::unique_ptr<TranslateCurveTool> tool(
                    new TranslateCurveTool(g, mw, TranslateCurveTool::Vertical));
            const bool ok = tool->translateCurve(m, 5.0);
            std::cout << "[INFO] 89i offset curve vertical +5: Y1 = " << suite89Cell(t, 1, 0)
                      << ", Y5 = " << suite89Cell(t, 1, 4) << "\n";
            CHECK89(ok && near89(suite89Cell(t, 1, 0), 20.0) && near89(suite89Cell(t, 1, 3), 50.0)
                            && near89(suite89Cell(t, 1, 4), 60.0)
                            && !std::isfinite(suite89Cell(t, 1, 2)),
                    "89i: offset curve must get original values + 5, got Y1="
                            << suite89Cell(t, 1, 0) << " Y5=" << suite89Cell(t, 1, 4));
        }
        m = dynamic_cast<DataCurve *>(g->curve(0));
        CHECK_COND(m != nullptr, "89i: curve after second translation");
        {
            std::unique_ptr<TranslateCurveTool> tool(
                    new TranslateCurveTool(g, mw, TranslateCurveTool::Horizontal));
            const bool ok = tool->translateCurve(m, 1.0);
            std::cout << "[INFO] 89i offset curve horizontal +1: X = " << suite89Cell(t, 0, 0)
                      << ", " << suite89Cell(t, 0, 1) << ", " << suite89Cell(t, 0, 2) << ", "
                      << suite89Cell(t, 0, 3) << ", " << suite89Cell(t, 0, 4) << "\n";
            CHECK89(ok && near89(suite89Cell(t, 0, 0), 2.0) && near89(suite89Cell(t, 0, 1), 3.0)
                            && near89(suite89Cell(t, 0, 2), 3.0)
                            && near89(suite89Cell(t, 0, 3), 5.0)
                            && near89(suite89Cell(t, 0, 4), 6.0),
                    "89i: horizontal translation must shift X of rows 1,2,4,5 by 1 (row 3 kept)");
        }
        suite89Close(ml);

        Table *tb = suite89Table(mw, "S89TransBox", { { 1, 2, 3, 4, 5 }, { 50, 10, 40, 20, 30 } });
        MultiLayer *mlb = nullptr;
        Graph *gb = suite89Plot(mw, tb, QStringList() << tb->colName(1), Graph::Box, &mlb);
        CHECK_COND(gb != nullptr, "89i: box graph");
        BoxCurve *box = nullptr;
        for (int i = 0; i < gb->curves() && !box; ++i)
            box = dynamic_cast<BoxCurve *>(gb->curve(i));
        CHECK_COND(box != nullptr, "89i: box curve");
        {
            std::unique_ptr<TranslateCurveTool> tool(
                    new TranslateCurveTool(gb, mw, TranslateCurveTool::Vertical));
            QString status;
            QObject::connect(tool.get(), &TranslateCurveTool::statusText,
                             [&status](const QString &s) { status = s; });
            const bool ok = tool->translateCurve(box, 7.0);
            bool unchanged = true;
            const double orig[] = { 50, 10, 40, 20, 30 };
            for (int r = 0; r < 5; ++r)
                unchanged = unchanged && near89(suite89Cell(tb, 1, r), orig[r])
                        && near89(suite89Cell(tb, 0, r), double(r + 1));
            std::cout << "[INFO] 89i box translation: ok=" << ok << ", status \""
                      << status.toStdString() << "\"\n";
            CHECK89(!ok && unchanged && !status.isEmpty(),
                    "89i: translating a box plot must be refused without touching the table");
        }
        suite89Close(mlb);
    }

    // ------------------------------------------------ 89j: range selector status on box plots
    {
        Table *t = suite89Table(mw, "S89RangeBox", { { 1, 2, 3, 4, 5 }, { 50, 10, 40, 20, 30 } });
        MultiLayer *ml = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::Box, &ml);
        CHECK_COND(g != nullptr, "89j: box graph");
        BoxCurve *box = nullptr;
        for (int i = 0; i < g->curves() && !box; ++i)
            box = dynamic_cast<BoxCurve *>(g->curve(i));
        CHECK_COND(box != nullptr && box->dataSize() == 5, "89j: box curve");
        std::unique_ptr<RangeSelectorTool> rs(new RangeSelectorTool(g));
        QString status;
        QObject::connect(rs.get(), &RangeSelectorTool::statusText,
                         [&status](const QString &s) { status = s; });
        rs->setSelectedCurve(nullptr);
        rs->selectCurvePoint(box, 2);
        const QString expected = box->title().text() + QStringLiteral("[3]");
        std::cout << "[INFO] 89j box status: \"" << status.toStdString() << "\"\n";
        CHECK89(status.contains(expected),
                "89j: status must name point 3 of the box plot, got \"" << status.toStdString()
                                                                      << "\"");
        rs.reset();
        suite89Close(ml);
    }

    // ------------------------------------------------ 89k: histogram bins are not table rows
    {
        const std::vector<double> ys = { 1, 2, 2, 3, 3, 3 };
        Table *th = suite89Table(mw, "S89HistW", { { 1, 2, 3, 4, 5, 6 }, ys });
        MultiLayer *mlh = nullptr;
        Graph *gh = suite89Plot(mw, th, QStringList() << th->colName(1), Graph::Histogram, &mlh);
        CHECK_COND(gh != nullptr, "89k: histogram graph");
        QwtHistogram *hist = nullptr;
        for (int i = 0; i < gh->curves() && !hist; ++i)
            hist = dynamic_cast<QwtHistogram *>(gh->curve(i));
        CHECK_COND(hist != nullptr && hist->dataSize() > 0, "89k: histogram curve");
        const QString histTitle = hist->title().text();
        auto tableUnchanged = [&]() {
            if (th->numCols() != 2)
                return false;
            for (int r = 0; r < int(ys.size()); ++r) {
                if (!near89(suite89Cell(th, 1, r), ys[size_t(r)])
                    || !near89(suite89Cell(th, 0, r), double(r + 1)))
                    return false;
            }
            return true;
        };

        QwtPlotCurve *res = CurveMathTools::subtractStraightLine(
                mw, gh, histTitle, QPointF(0.0, 0.5), QPointF(10.0, 0.5), true);
        std::cout << "[INFO] 89k Subtract Straight Line in place on a histogram: "
                  << (res ? "done" : "refused") << "\n";
        CHECK89(res == nullptr && tableUnchanged(),
                "89k: Subtract Straight Line (in place) must not write histogram bins");

        // Peak Analyzer "Subtract Baseline Now" with "overwrite" (needs the interactive dialog).
        {
            MultiPeakFit probeFit(mw, gh, MultiPeakFit::Gauss, 1);
            probeFit.setSilent(true);
            CHECK89(probeFit.setDataFromCurve(histTitle),
                    "89k: the baseline subtraction must reach the histogram's data");
        }
        qApp->setProperty("torigraph_noninteractive", false);
        mw->activateSubWindow(mlh);
        QApplication::processEvents();
        if (mw->statusBar())
            mw->statusBar()->clearMessage();
        bool askedOverwrite = false;
        QTimer boxProbe;
        QObject::connect(&boxProbe, &QTimer::timeout, [&]() {
            if (QMessageBox *mb = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                askedOverwrite = true;
                if (QAbstractButton *yes = mb->button(QMessageBox::Yes))
                    yes->click();
                else
                    mb->reject();
            }
        });
        boxProbe.start(20);
        mw->showPeakAnalyzerDialog(0);
        QDialog *peakDlg = nullptr;
        for (QDialog *d : mw->findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (d && d->isVisible())
                peakDlg = d;
        }
        bool clicked = false;
        if (peakDlg) {
            for (QComboBox *cb : peakDlg->findChildren<QComboBox *>()) {
                const int idx = cb->findText(histTitle);
                if (idx >= 0) {
                    cb->setCurrentIndex(idx);
                    break;
                }
            }
            const QString overwriteText =
                    ApplicationWindow::tr("Overwrite original data column (destructive)");
            for (QCheckBox *cbx : peakDlg->findChildren<QCheckBox *>()) {
                if (cbx->text() == overwriteText)
                    cbx->setChecked(true);
            }
            const QString subtractText = ApplicationWindow::tr("Subtract Baseline Now");
            for (QPushButton *pb : peakDlg->findChildren<QPushButton *>()) {
                if (pb->text() == subtractText) {
                    pb->click();
                    clicked = true;
                    break;
                }
            }
        }
        QApplication::processEvents();
        boxProbe.stop();
        const QString msg = mw->statusBar() ? mw->statusBar()->currentMessage() : QString();
        std::cout << "[INFO] 89k Peak Analyzer subtract baseline (overwrite) on a histogram: "
                  << "dialog " << (peakDlg ? "open" : "missing") << ", clicked " << clicked
                  << ", asked " << askedOverwrite << ", status \"" << msg.toStdString() << "\"\n";
        CHECK89(peakDlg && clicked, "89k: Peak Analyzer dialog / Subtract Baseline Now button");
        CHECK89(!askedOverwrite && tableUnchanged() && msg.contains(histTitle),
                "89k: baseline subtraction must refuse to write histogram bins to the table");
        if (peakDlg)
            peakDlg->close();
        QApplication::processEvents();
        qApp->setProperty("torigraph_noninteractive", true);
        suite89Close(mlh);
    }

    // ------------------------------------------------ 89l: York fit and empty error cells
    {
        // y = 2x + 1 except row 4, an outlier whose error cell is empty: it must be left out
        // instead of getting a tiny sigma that pins the line to it.
        const double nan = std::nan("");
        Table *t = suite89Table(mw, "S89York", { { 1, 2, 3, 4, 5, 6 },
                                                 { 3.1, 4.9, 7.2, 30.0, 10.8, 13.1 },
                                                 { 0.2, 0.3, 0.2, nan, 0.4, 0.3 } });
        Table *tr = suite89Table(mw, "S89YorkRef", { { 1, 2, 3, 5, 6 },
                                                     { 3.1, 4.9, 7.2, 10.8, 13.1 },
                                                     { 0.2, 0.3, 0.2, 0.4, 0.3 } });
        MultiLayer *ml = nullptr;
        MultiLayer *mlr = nullptr;
        Graph *g = suite89Plot(mw, t, QStringList() << t->colName(1), Graph::LineSymbols, &ml);
        Graph *gr = suite89Plot(mw, tr, QStringList() << tr->colName(1), Graph::LineSymbols, &mlr);
        CHECK_COND(g != nullptr && gr != nullptr, "89l: graphs");
        CHECK_COND(g->addErrorBars(t->colName(1), t, t->colName(2), QwtErrorPlotCurve::Vertical),
                   "89l: addErrorBars");
        CHECK_COND(gr->addErrorBars(tr->colName(1), tr, tr->colName(2),
                                    QwtErrorPlotCurve::Vertical),
                   "89l: reference addErrorBars");
        Suite89LinearFit fit(mw, g);
        Suite89LinearFit ref(mw, gr);
        const bool okData = fit.setDataFromCurve(t->colName(1), g)
                && ref.setDataFromCurve(tr->colName(1), gr);
        CHECK_COND(okData, "89l: fit data");
        const bool ok = fit.fitWithXYErrors(QString(), QString());
        const bool okRef = ref.fitWithXYErrors(QString(), QString());
        std::cout << "[INFO] 89l York with empty error cell: slope " << fit.yorkSlope()
                  << ", intercept " << fit.yorkIntercept() << "; without the row: slope "
                  << ref.yorkSlope() << ", intercept " << ref.yorkIntercept() << "\n";
        CHECK89(ok && okRef && near89(fit.yorkSlope(), ref.yorkSlope(), 1e-9)
                        && near89(fit.yorkIntercept(), ref.yorkIntercept(), 1e-9),
                "89l: the point with an empty error must be left out of the York fit (slope "
                        << fit.yorkSlope() << " vs " << ref.yorkSlope() << ")");
        CHECK89(near89(fit.yorkSlope(), 2.0, 0.05),
                "89l: York slope must stay near 2, got " << fit.yorkSlope());
        suite89Close(ml);
        suite89Close(mlr);
    }

    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    if (g89Failures > 0) {
        std::cerr << "[FAIL] Suite 89: " << g89Failures << " check(s) failed.\n";
        return 1;
    }
    std::cout << "[PASS] Suite 89 passed.\n";
    return 0;
}
