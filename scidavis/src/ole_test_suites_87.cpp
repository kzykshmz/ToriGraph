#include "ole_test_common.h"

#include "Graph3D.h"
#include "Plot3DDialog.h"

#include <QDoubleSpinBox>
#include <QElapsedTimer>

#include <cmath>
#include <limits>

// =============================================================================
// Suite 87: 3D plot of matrices - orientation, resampling grid, dialog scaling and
//           degenerate data. Every check runs on the real Graph3D / qwtplot3d data:
//   87a  a 3x10 matrix lies with its columns along x (qwt3d vertex x = 0..9 across the
//        columns) and the z of every vertex is cell(row, col); also after updateMatrixData
//   87b  updateScalesFromMatrix samples the matrix on the grid it draws: with the range
//        0..10.5 the vertex at x=10.5 is outside the matrix and no longer shows column 10
//   87c  Plot3DDialog::changeZoom sends the X scale as a factor (1.0 stays 1.0, 250 % is 2.5)
//   87d  small scale factors (1e-5 shown as 0.00 %) survive OK unchanged
//   87e  1xN / Nx1 matrices and a table with one valid point return in time and leave no
//        NaN vertex in the plot
//   87f  plots emptied (NOPLOT) by degenerate data draw again once there are two points:
//        "Change matrix" after a 1-row matrix, a ribbon of an empty table that gets data, a
//        1xN matrix that grows to 3xN (and is tied to the matrix meanwhile), a ribbon with an
//        X range holding one point that is widened or gets a second point
//   87g  a plot emptied (NOPLOT) by degenerate data saves the style the user chose: the Style
//        record of saveToString names it (not the floor style), and a new Graph3D restored from
//        it draws with that style once the data is back (ribbon with an X range: wireframe and
//        points; 1-row matrix: wireframe). A broken record ("nofloor" as the plot style, written
//        by the old code) restores the default filled mesh
// The checks report every failure (they do not stop at the first one); 87e/87f come last because
// the old code could loop forever there.
// =============================================================================

namespace {

#define CHECK87(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl; \
            ++failures; \
        } \
    } while (0)

struct NonInteractiveGuard87
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard87()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard87()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard87
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard87(ApplicationWindow *w = nullptr) : win(w)
    {
        if (win) {
            win->setHeadlessMode(true);
            win->applyUserSettings();
            win->confirmCloseTable = false;
            win->confirmCloseMatrix = false;
            win->confirmClosePlot2D = false;
            win->confirmClosePlot3D = false;
            win->confirmCloseFolder = false;
            win->confirmCloseNotes = false;
        }
    }
    ~AppWinGuard87()
    {
        if (win) {
            win->savedProject();
            win->setOleEmbeddedMode(true);
            delete win;
            win = nullptr;
        }
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    AppWinGuard87(const AppWinGuard87 &) = delete;
    AppWinGuard87 &operator=(const AppWinGuard87 &) = delete;
    ApplicationWindow *operator->() const { return win; }
};

// qwtplot3d keeps the data it draws behind the protected Plot3D::actualData_p (the grid of
// loadFromData(double **...)); a pointer to member formed through a derived class reads it
// without changing the library.
struct SurfacePeek87 : Qwt3D::SurfacePlot
{
    static const Qwt3D::GridData *grid(const Qwt3D::SurfacePlot *p)
    {
        if (!p)
            return nullptr;
        const Qwt3D::Data *d = p->*(&SurfacePeek87::actualData_p);
        return dynamic_cast<const Qwt3D::GridData *>(d);
    }
};

int gridColumns87(const Qwt3D::GridData *g)
{
    return g ? int(g->vertices.size()) : 0;
}

int gridRows87(const Qwt3D::GridData *g)
{
    return (g && !g->vertices.empty()) ? int(g->vertices[0].size()) : 0;
}

// True when every vertex of the grid has finite x, y and z.
bool gridFinite87(const Qwt3D::GridData *g)
{
    if (!g)
        return true;
    for (const auto &col : g->vertices)
        for (const double *v : col)
            if (v && (!std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2])))
                return false;
    return true;
}

bool near87(double a, double b, double tol = 1e-9)
{
    return std::fabs(a - b) <= tol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

// Checks vertex (i, j) of a 3x10 / 3xN matrix plot: x = column i, y = row j, z = cell(j, i).
bool orientationOk87(const Qwt3D::GridData *g, Matrix *m, QString *why)
{
    const int nc = m->numCols(), nr = m->numRows();
    if (gridColumns87(g) != nc || gridRows87(g) != nr) {
        *why = QStringLiteral("grid is %1 columns x %2 rows, expected %3 x %4")
                       .arg(gridColumns87(g))
                       .arg(gridRows87(g))
                       .arg(nc)
                       .arg(nr);
        return false;
    }
    for (int i = 0; i < nc; ++i) {
        for (int j = 0; j < nr; ++j) {
            const double *v = g->vertices[i][j];
            if (!near87(v[0], double(i)) || !near87(v[1], double(j))
                || !near87(v[2], m->cell(j, i))) {
                *why = QStringLiteral("vertex[%1][%2] = (%3, %4, %5), expected (%1, %2, %6)")
                               .arg(i)
                               .arg(j)
                               .arg(v[0])
                               .arg(v[1])
                               .arg(v[2])
                               .arg(m->cell(j, i));
                return false;
            }
        }
    }
    return true;
}

QDoubleSpinBox *spinWithValue87(QWidget *parent, double value)
{
    QDoubleSpinBox *hit = nullptr;
    for (QDoubleSpinBox *b : parent->findChildren<QDoubleSpinBox *>()) {
        if (std::fabs(b->value() - value) < 1e-9) {
            if (hit)
                return nullptr; // ambiguous
            hit = b;
        }
    }
    return hit;
}

} // namespace

int runSuite87Graph3DMatrixOrientation(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 87: 3D Matrix Plot Orientation, Grid and Scaling...\n";
    NonInteractiveGuard87 niGuard;
    AppWinGuard87 app(new ApplicationWindow());
    int failures = 0;

    // ---------------------------------------------------------------------------------------
    // 87a: rows x columns orientation (addMatrixData and updateMatrixData)
    // ---------------------------------------------------------------------------------------
    Matrix *m = app->newMatrix(QStringLiteral("M87a"), 3, 10);
    if (!m) {
        std::cerr << "[FAIL] 87a: matrix creation failed" << std::endl;
        return 1;
    }
    m->setCoordinates(0.0, 9.0, 0.0, 2.0);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 10; ++c)
            m->setCell(r, c, 100.0 * r + c);

    Graph3D *g = new Graph3D(QString());
    g->addMatrixData(m);
    {
        QString why;
        CHECK87(g->hasData(), "87a: addMatrixData loaded no data");
        CHECK87(orientationOk87(SurfacePeek87::grid(g->sp), m, &why),
                "87a: addMatrixData put the 3x10 matrix in the wrong orientation: "
                        << why.toStdString());
    }
    m->setCell(2, 7, -55.0);
    g->updateMatrixData(m);
    {
        QString why;
        CHECK87(orientationOk87(SurfacePeek87::grid(g->sp), m, &why),
                "87a: updateMatrixData put the 3x10 matrix in the wrong orientation: "
                        << why.toStdString());
    }

    // ---------------------------------------------------------------------------------------
    // 87b: updateScalesFromMatrix samples on the drawn grid (11 columns x=0..10, range 0..10.5)
    // ---------------------------------------------------------------------------------------
    {
        Matrix *mb = app->newMatrix(QStringLiteral("M87b"), 3, 11);
        CHECK87(mb != nullptr, "87b: matrix creation failed");
        if (mb) {
            mb->setCoordinates(0.0, 10.0, 0.0, 2.0);
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 11; ++c)
                    mb->setCell(r, c, 100.0 * r + c + 1.0);
            Graph3D *gb = new Graph3D(QString());
            gb->addMatrixData(mb);
            gb->updateScalesFromMatrix(0.0, 10.5, 0.0, 2.0, -1000.0, 1000.0);
            const Qwt3D::GridData *grid = SurfacePeek87::grid(gb->sp);
            const int nc = gridColumns87(grid), nr = gridRows87(grid);
            CHECK87(nc == 11 && nr == 3,
                    "87b: resampled grid is " << nc << " x " << nr << ", expected 11 x 3");
            int bad = 0;
            QString firstBad;
            for (int i = 0; i < nc; ++i) {
                for (int j = 0; j < nr; ++j) {
                    const double *v = grid->vertices[i][j];
                    const double x = v[0], y = v[1];
                    const bool inside = x >= -1e-9 && x <= 10.0 + 1e-9 && y >= -1e-9
                            && y <= 2.0 + 1e-9;
                    const double expected =
                            inside ? mb->cell(qRound(y), qRound(x)) : 0.0;
                    if (!near87(v[2], expected)) {
                        if (bad++ == 0)
                            firstBad = QStringLiteral("vertex (%1, %2) has z=%3, expected %4")
                                               .arg(x)
                                               .arg(y)
                                               .arg(v[2])
                                               .arg(expected);
                    }
                }
            }
            CHECK87(bad == 0,
                    "87b: " << bad << " resampled vertices do not show the matrix cell under "
                               "them; first: "
                            << firstBad.toStdString());
            if (nc == 11 && nr == 3)
                CHECK87(near87(grid->vertices[10][0][0], 10.5),
                        "87b: last grid column is not drawn at x=10.5");
            delete gb;
        }
    }

    // ---------------------------------------------------------------------------------------
    // 87c: Plot3DDialog::changeZoom scales X like Y and Z
    // ---------------------------------------------------------------------------------------
    {
        g->updateScaling(1.0, 1.5, 2.0);
        g->updateZoom(1.0);
        Plot3DDialog *pd = new Plot3DDialog();
        QObject::connect(pd, SIGNAL(updateScaling(double, double, double)), g,
                         SLOT(updateScaling(double, double, double)));
        pd->setZoom(0.5);
        pd->setScaling(1.0, 1.5, 2.0);
        pd->showGeneralTab();
        pd->changeZoom(50.0);
        CHECK87(near87(g->xScale(), 1.0) && near87(g->yScale(), 1.5) && near87(g->zScale(), 2.0),
                "87c: changeZoom with unchanged boxes changed the scale to ("
                        << g->xScale() << ", " << g->yScale() << ", " << g->zScale()
                        << "), expected (1, 1.5, 2)");
        QDoubleSpinBox *xBox = spinWithValue87(pd, 100.0);
        CHECK87(xBox != nullptr, "87c: X Zoom (%) box not found");
        if (xBox) {
            xBox->setValue(250.0); // valueChanged -> changeZoom
            CHECK87(near87(g->xScale(), 2.5) && near87(g->yScale(), 1.5)
                            && near87(g->zScale(), 2.0),
                    "87c: X Zoom 250 % gave the scale (" << g->xScale() << ", " << g->yScale()
                                                         << ", " << g->zScale()
                                                         << "), expected (2.5, 1.5, 2)");
        }
        delete pd;
    }

    // ---------------------------------------------------------------------------------------
    // 87d: small scale factors are not rounded to 0 by OK
    // ---------------------------------------------------------------------------------------
    {
        const double xs = 1e-5, ys = 2.345678e-3, zs = 0.5;
        g->updateScaling(xs, ys, zs);
        Plot3DDialog *pd = new Plot3DDialog();
        QObject::connect(pd, SIGNAL(updateScaling(double, double, double)), g,
                         SLOT(updateScaling(double, double, double)));
        pd->setZoom(g->zoom());
        pd->setScaling(g->xScale(), g->yScale(), g->zScale());
        pd->showGeneralTab();
        pd->updatePlot(); // what OK / Apply do on the General tab
        CHECK87(near87(g->xScale(), xs) && near87(g->yScale(), ys) && near87(g->zScale(), zs),
                "87d: OK changed untouched scale factors to ("
                        << g->xScale() << ", " << g->yScale() << ", " << g->zScale()
                        << "), expected (" << xs << ", " << ys << ", " << zs << ")");
        // Editing one box still applies the edited value and keeps the others.
        QDoubleSpinBox *zBox = spinWithValue87(pd, 50.0);
        CHECK87(zBox != nullptr, "87d: Z Zoom (%) box not found");
        if (zBox) {
            zBox->setValue(80.0);
            pd->updatePlot();
            CHECK87(near87(g->xScale(), xs) && near87(g->yScale(), ys)
                            && near87(g->zScale(), 0.8),
                    "87d: after editing Z only the scale is ("
                            << g->xScale() << ", " << g->yScale() << ", " << g->zScale()
                            << "), expected (" << xs << ", " << ys << ", 0.8)");
        }
        delete pd;
    }
    delete g;
    g = nullptr;

    // ---------------------------------------------------------------------------------------
    // 87e: degenerate data (fewer than 2 points along an axis)
    // ---------------------------------------------------------------------------------------
    {
        QElapsedTimer timer;
        const qint64 limitMs = 10000;

        const struct
        {
            int rows, cols;
        } shapes[] = { { 1, 6 }, { 6, 1 }, { 1, 1 } };
        for (const auto &s : shapes) {
            Matrix *md = app->newMatrix(QStringLiteral("M87e_%1x%2").arg(s.rows).arg(s.cols),
                                        s.rows, s.cols);
            CHECK87(md != nullptr, "87e: matrix creation failed");
            if (!md)
                continue;
            md->setCoordinates(0.0, 5.0, 0.0, 5.0);
            for (int r = 0; r < s.rows; ++r)
                for (int c = 0; c < s.cols; ++c)
                    md->setCell(r, c, 1.0 + r + c);
            Graph3D *gd = new Graph3D(QString());
            timer.start();
            gd->addMatrixData(md);
            gd->updateMatrixData(md);
            gd->updateScalesFromMatrix(0.0, 5.0, 0.0, 5.0, -10.0, 10.0);
            CHECK87(timer.elapsed() < limitMs,
                    "87e: " << s.rows << "x" << s.cols << " matrix took " << timer.elapsed()
                            << " ms");
            CHECK87(gridFinite87(SurfacePeek87::grid(gd->sp)),
                    "87e: " << s.rows << "x" << s.cols << " matrix left NaN/inf vertices");
            delete gd;
        }

        // A plotted 3x4 matrix that shrinks to one row.
        Matrix *ms = app->newMatrix(QStringLiteral("M87e_shrink"), 3, 4);
        CHECK87(ms != nullptr, "87e: matrix creation failed");
        if (ms) {
            ms->setCoordinates(0.0, 3.0, 0.0, 2.0);
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 4; ++c)
                    ms->setCell(r, c, 1.0 + r * c);
            Graph3D *gs = new Graph3D(QString());
            gs->addMatrixData(ms);
            ms->setDimensions(1, 4);
            timer.start();
            gs->updateMatrixData(ms);
            gs->updateScalesFromMatrix(0.0, 3.0, 0.0, 2.0, -10.0, 10.0);
            CHECK87(timer.elapsed() < limitMs,
                    "87e: shrunk 1x4 matrix took " << timer.elapsed() << " ms");
            CHECK87(gridFinite87(SurfacePeek87::grid(gs->sp)),
                    "87e: shrunk 1x4 matrix left NaN/inf vertices");
            delete gs;
        }

        // A table with a single valid (x, y) point.
        Table *t = app->newTable(QStringLiteral("T87e"), 1, 2);
        CHECK87(t != nullptr, "87e: table creation failed");
        if (t) {
            t->setCell(0, 0, 1.0);
            t->setCell(0, 1, 2.0);
            Graph3D *gt = new Graph3D(QString());
            timer.start();
            gt->addData(t, 0, 1);
            CHECK87(timer.elapsed() < limitMs,
                    "87e: one-point table took " << timer.elapsed() << " ms");
            CHECK87(gridFinite87(SurfacePeek87::grid(gt->sp)),
                    "87e: one-point table left NaN/inf vertices");
            delete gt;
        }
    }

    // ---------------------------------------------------------------------------------------
    // 87f: a plot emptied by degenerate data (NOPLOT) draws again once the data has two points
    // ---------------------------------------------------------------------------------------
    {
        // f1: matrix shrunk to one row (NOPLOT), then "Change matrix" to a normal matrix.
        Matrix *ma = app->newMatrix(QStringLiteral("M87f_a"), 3, 4);
        Matrix *mb = app->newMatrix(QStringLiteral("M87f_b"), 3, 5);
        CHECK87(ma && mb, "87f1: matrix creation failed");
        if (ma && mb) {
            ma->setCoordinates(0.0, 3.0, 0.0, 2.0);
            mb->setCoordinates(0.0, 4.0, 0.0, 2.0);
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 4; ++c)
                    ma->setCell(r, c, 1.0 + r + c);
                for (int c = 0; c < 5; ++c)
                    mb->setCell(r, c, 10.0 * r + c);
            }
            Graph3D *gf = new Graph3D(QString());
            gf->addMatrixData(ma);
            gf->customPlotStyle(Qwt3D::FILLED); // as plot3DMatrix() does
            ma->setDimensions(1, 4);
            gf->updateMatrixData(ma);
            CHECK87(gf->plotStyle() == Qwt3D::NOPLOT,
                    "87f1: matrix shrunk to 1x4 is still drawn (style " << int(gf->plotStyle())
                                                                         << ")");
            gf->changeMatrix(mb);
            QString why;
            CHECK87(gf->plotStyle() == Qwt3D::FILLED,
                    "87f1: after changeMatrix to a 3x5 matrix the style is "
                            << int(gf->plotStyle()) << ", expected FILLED (" << int(Qwt3D::FILLED)
                            << ")");
            CHECK87(gf->matrix() == mb, "87f1: changeMatrix did not switch the matrix");
            CHECK87(orientationOk87(SurfacePeek87::grid(gf->sp), mb, &why),
                    "87f1: changeMatrix grid: " << why.toStdString());
            delete gf;
        }

        // f2: ribbon made from an empty table draws once the table gets data.
        Table *te = app->newTable(QStringLiteral("T87f_empty"), 3, 2);
        CHECK87(te != nullptr, "87f2: table creation failed");
        if (te) {
            // A new table may start with 0.0 in every cell: make the cells really empty.
            for (int c = 0; c < 2; ++c)
                for (int r = 0; r < 3; ++r)
                    te->column(c)->setInvalid(r, true);
            Graph3D *gr = new Graph3D(QString());
            gr->addData(te, te->colName(0), te->colName(1));
            CHECK87(gr->plotStyle() == Qwt3D::NOPLOT,
                    "87f2: ribbon of an empty table is not empty (style " << int(gr->plotStyle())
                                                                          << ")");
            for (int r = 0; r < 3; ++r) {
                te->setCell(r, 0, double(r));
                te->setCell(r, 1, 2.0 * r + 1.0);
            }
            gr->updateData(te);
            const Qwt3D::GridData *grid = SurfacePeek87::grid(gr->sp);
            CHECK87(gr->plotStyle() == Qwt3D::FILLEDMESH,
                    "87f2: ribbon is not drawn after the table got 3 points (style "
                            << int(gr->plotStyle()) << ")");
            CHECK87(gridColumns87(grid) == 3 && gridFinite87(grid),
                    "87f2: ribbon grid has " << gridColumns87(grid)
                                             << " columns (expected 3) or NaN vertices");
            delete gr;
        }

        // f3: 1xN matrix: the plot is associated with it and draws once it grows to 3xN.
        Matrix *m1 = app->newMatrix(QStringLiteral("M87f_1xN"), 1, 5);
        CHECK87(m1 != nullptr, "87f3: matrix creation failed");
        if (m1) {
            m1->setCoordinates(0.0, 4.0, 0.0, 2.0);
            for (int c = 0; c < 5; ++c)
                m1->setCell(0, c, 1.0 + c);
            Graph3D *g1 = new Graph3D(QString());
            g1->addMatrixData(m1);
            CHECK87(g1->matrix() == m1, "87f3: the 3D plot of a 1x5 matrix is not tied to it");
            CHECK87(g1->formula() == QStringLiteral("matrix<M87f_1xN>"),
                    "87f3: plot association is '" << g1->formula().toStdString()
                                                  << "', expected 'matrix<M87f_1xN>'");
            CHECK87(g1->plotStyle() == Qwt3D::NOPLOT,
                    "87f3: 1x5 matrix plot is not empty (style " << int(g1->plotStyle()) << ")");
            CHECK87(gridFinite87(SurfacePeek87::grid(g1->sp)),
                    "87f3: 1x5 matrix plot has NaN vertices");
            m1->setDimensions(3, 5);
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 5; ++c)
                    m1->setCell(r, c, 10.0 * r + c);
            // ApplicationWindow::updateMatrixPlots() calls this for plots with matrix() == m.
            g1->updateMatrixData(m1);
            QString why;
            CHECK87(g1->plotStyle() != Qwt3D::NOPLOT,
                    "87f3: plot of the matrix grown to 3x5 is still empty");
            CHECK87(orientationOk87(SurfacePeek87::grid(g1->sp), m1, &why),
                    "87f3: grown 3x5 matrix grid: " << why.toStdString());
            delete g1;
        }

        // f4: ribbon with X range: one point in range -> empty without NaN; a wider range or a
        //     second point in range draws again.
        Table *tr = app->newTable(QStringLiteral("T87f_range"), 6, 2);
        CHECK87(tr != nullptr, "87f4: table creation failed");
        if (tr) {
            for (int r = 0; r < 4; ++r) {
                tr->setCell(r, 0, double(r));
                tr->setCell(r, 1, 1.0 + r);
            }
            for (int c = 0; c < 2; ++c) // rows 4 and 5 stay empty for now
                for (int r = 4; r < 6; ++r)
                    tr->column(c)->setInvalid(r, true);
            Graph3D *gq = new Graph3D(QString());
            gq->addData(tr, tr->colName(0), tr->colName(1), -0.5, 0.5, 0.0, 1.0, 0.0, 10.0);
            CHECK87(gq->plotStyle() == Qwt3D::NOPLOT,
                    "87f4: one point in the X range is drawn (style " << int(gq->plotStyle())
                                                                       << ")");
            CHECK87(gridFinite87(SurfacePeek87::grid(gq->sp)),
                    "87f4: one point in the X range left NaN vertices");

            gq->updateScales(-0.5, 3.5, 0.0, 1.0, 0.0, 10.0);
            const Qwt3D::GridData *grid = SurfacePeek87::grid(gq->sp);
            CHECK87(gq->plotStyle() == Qwt3D::FILLEDMESH,
                    "87f4: wider X range (4 points) is not drawn (style " << int(gq->plotStyle())
                                                                           << ")");
            CHECK87(gridColumns87(grid) == 4 && gridFinite87(grid),
                    "87f4: wider X range grid has " << gridColumns87(grid)
                                                    << " columns (expected 4) or NaN vertices");

            gq->updateScales(-0.5, 0.5, 0.0, 1.0, 0.0, 10.0);
            CHECK87(gq->plotStyle() == Qwt3D::NOPLOT,
                    "87f4: X range narrowed to one point is still drawn");
            CHECK87(gridFinite87(SurfacePeek87::grid(gq->sp)),
                    "87f4: X range narrowed to one point left NaN vertices");

            tr->setCell(4, 0, 0.25);
            tr->setCell(4, 1, 7.0);
            gq->updateScales(-0.5, 0.5, 0.0, 1.0, 0.0, 10.0);
            grid = SurfacePeek87::grid(gq->sp);
            CHECK87(gq->plotStyle() == Qwt3D::FILLEDMESH,
                    "87f4: second point in the X range is not drawn (style "
                            << int(gq->plotStyle()) << ")");
            CHECK87(gridColumns87(grid) == 2 && gridFinite87(grid),
                    "87f4: two-point grid has " << gridColumns87(grid)
                                                << " columns (expected 2) or NaN vertices");
            delete gq;
        }
    }

    // ---------------------------------------------------------------------------------------
    // 87g: saving a plot emptied by degenerate data keeps the plot style the user chose
    // ---------------------------------------------------------------------------------------
    {
        // The "Style" record of saveToString(), split as openSurfacePlot() does.
        auto styleRecord = [](Graph3D *g) -> QStringList {
            const QStringList lines = g->saveToString(QString()).split(QLatin1Char('\n'));
            for (const QString &line : lines)
                if (line.startsWith(QLatin1String("Style\t")))
                    return line.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
            return QStringList();
        };

        Table *tg = app->newTable(QStringLiteral("T87g_range"), 4, 2);
        CHECK87(tg != nullptr, "87g: table creation failed");
        if (tg) {
            for (int r = 0; r < 4; ++r) {
                tg->setCell(r, 0, double(r));
                tg->setCell(r, 1, 1.0 + r);
            }
            struct Variant
            {
                const char *label;
                int style; // passed to customPlotStyle()
                const char *savedName;
                Qwt3D::PLOTSTYLE drawn;
                Graph3D::PointStyle point;
            };
            const Variant variants[] = {
                { "wireframe", int(Qwt3D::WIREFRAME), "wireframe", Qwt3D::WIREFRAME,
                  Graph3D::None },
                { "points", int(Qwt3D::POINTS), "points", Qwt3D::USER, Graph3D::Dots },
            };
            for (const Variant &v : variants) {
                Graph3D *g = new Graph3D(QString());
                g->addData(tg, tg->colName(0), tg->colName(1), -0.5, 3.5, 0.0, 1.0, 0.0, 10.0);
                g->customPlotStyle(v.style);
                CHECK87(g->plotStyle() == v.drawn,
                        "87g " << v.label << ": style after customPlotStyle is "
                               << int(g->plotStyle()));
                g->updateScales(-0.5, 0.5, 0.0, 1.0, 0.0, 10.0); // one point left -> NOPLOT
                CHECK87(g->plotStyle() == Qwt3D::NOPLOT,
                        "87g " << v.label << ": X range with one point is still drawn");
                const QStringList rec = styleRecord(g);
                std::cout << "[INFO] 87g " << v.label << " (NOPLOT) Style record: "
                          << rec.join(QLatin1Char(' ')).toStdString() << "\n";
                CHECK87(rec.value(3) == QLatin1String(v.savedName),
                        "87g " << v.label << ": saved plot style is '"
                               << rec.value(3).toStdString() << "', expected '" << v.savedName
                               << "'");

                // Restore as openSurfacePlot() does: data first (still one point), then Style.
                Graph3D *gn = new Graph3D(QString());
                gn->addData(tg, tg->colName(0), tg->colName(1), -0.5, 0.5, 0.0, 1.0, 0.0, 10.0);
                CHECK87(gn->plotStyle() == Qwt3D::NOPLOT,
                        "87g " << v.label << ": restored plot with one point is drawn");
                if (rec.size() >= 4)
                    gn->setStyle(rec);
                gn->updateScales(-0.5, 3.5, 0.0, 1.0, 0.0, 10.0);
                const Qwt3D::GridData *grid = SurfacePeek87::grid(gn->sp);
                CHECK87(gn->plotStyle() == v.drawn && gn->pointType() == v.point,
                        "87g " << v.label << ": restored plot draws with style "
                               << int(gn->plotStyle()) << " / point " << int(gn->pointType())
                               << ", expected " << int(v.drawn) << " / " << int(v.point));
                CHECK87(gridColumns87(grid) == 4 && gridFinite87(grid),
                        "87g " << v.label << ": restored grid has " << gridColumns87(grid)
                               << " columns (expected 4) or NaN vertices");
                delete gn;
                delete g;
            }

            // A Style record written by the old code ("nofloor" in the plot style field)
            // restores qwtplot3d's default filled mesh, for an empty and a drawn plot.
            const QStringList broken = { QStringLiteral("Style"), QStringLiteral("frame"),
                                         QStringLiteral("nofloor"), QStringLiteral("nofloor") };
            Graph3D *gb = new Graph3D(QString());
            gb->addData(tg, tg->colName(0), tg->colName(1), -0.5, 0.5, 0.0, 1.0, 0.0, 10.0);
            gb->setStyle(broken);
            gb->updateScales(-0.5, 3.5, 0.0, 1.0, 0.0, 10.0);
            CHECK87(gb->plotStyle() == Qwt3D::FILLEDMESH,
                    "87g broken record (empty plot): style " << int(gb->plotStyle())
                                                             << ", expected FILLEDMESH");
            delete gb;
            Graph3D *gd = new Graph3D(QString());
            gd->addData(tg, tg->colName(0), tg->colName(1), -0.5, 3.5, 0.0, 1.0, 0.0, 10.0);
            gd->setStyle(broken);
            CHECK87(gd->plotStyle() == Qwt3D::FILLEDMESH,
                    "87g broken record (drawn plot): style " << int(gd->plotStyle())
                                                             << ", expected FILLEDMESH");
            delete gd;
        }

        // 1-row matrix: the wireframe chosen on the 3x5 matrix survives save / restore.
        Matrix *mg = app->newMatrix(QStringLiteral("M87g"), 3, 5);
        CHECK87(mg != nullptr, "87g matrix: creation failed");
        if (mg) {
            mg->setCoordinates(0.0, 4.0, 0.0, 2.0);
            auto fill = [mg]() {
                for (int r = 0; r < mg->numRows(); ++r)
                    for (int c = 0; c < mg->numCols(); ++c)
                        mg->setCell(r, c, 10.0 * r + c);
            };
            fill();
            Graph3D *g = new Graph3D(QString());
            g->addMatrixData(mg);
            g->customPlotStyle(Qwt3D::WIREFRAME);
            mg->setDimensions(1, 5);
            g->updateMatrixData(mg);
            CHECK87(g->plotStyle() == Qwt3D::NOPLOT, "87g matrix: 1x5 matrix plot is drawn");
            const QStringList rec = styleRecord(g);
            std::cout << "[INFO] 87g matrix (NOPLOT) Style record: "
                      << rec.join(QLatin1Char(' ')).toStdString() << "\n";
            CHECK87(rec.value(3) == QLatin1String("wireframe"),
                    "87g matrix: saved plot style is '" << rec.value(3).toStdString()
                                                        << "', expected 'wireframe'");

            Graph3D *gn = new Graph3D(QString());
            gn->addMatrixData(mg);
            CHECK87(gn->plotStyle() == Qwt3D::NOPLOT,
                    "87g matrix: restored 1x5 matrix plot is drawn");
            if (rec.size() >= 4)
                gn->setStyle(rec);
            mg->setDimensions(3, 5);
            fill();
            gn->updateMatrixData(mg);
            QString why;
            CHECK87(gn->plotStyle() == Qwt3D::WIREFRAME,
                    "87g matrix: restored plot of the 3x5 matrix draws with style "
                            << int(gn->plotStyle()) << ", expected WIREFRAME");
            CHECK87(orientationOk87(SurfacePeek87::grid(gn->sp), mg, &why),
                    "87g matrix: restored grid: " << why.toStdString());
            delete gn;
            delete g;
        }
    }

    if (failures != 0) {
        std::cerr << "[FAIL] Suite 87: " << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "[PASS] Suite 87: 3D matrix orientation, resampling grid, dialog scaling and "
                 "degenerate data\n";
    return 0;
}

#undef CHECK87
