/***************************************************************************
    File                 : Graph3D.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : 3D graph widget

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/
#ifdef _MSC_VER
#define NOMINMAX
#endif
#include "Graph3D.h"
#include "ApplicationWindow.h"
#ifdef Q_OS_WIN
#include "OleServer.h"
#include "EmfPaintDevice.h"
#endif
#include "Bar.h"
#include "Cone3D.h"
#include "MyParser.h"
#include "ColorButton.h"
#include "core/column/Column.h"

#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QFile>
#include <QFileDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QPixmap>
#include <QBitmap>
#include <QDateTime>
#include <QCursor>
#include <QImageWriter>
#include <QMouseEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QResizeEvent>
#include <QTextStream>
#include <QWheelEvent>

#include <qwt3d_io_gl2ps.h>
#include <qwt3d_coordsys.h>

#include <gsl/gsl_vector.h>
#include <cmath>
#include <fstream>
#include <stdexcept>
using namespace std;

namespace {

bool isGraph3DNonInteractive(const QObject *context = nullptr)
{
    if (qApp && qApp->property("torigraph_noninteractive").toBool())
        return true;
    return ApplicationWindow::isNonInteractiveContext(context);
}

QFont normalizePlot3DFont(const QFont &font, int fallbackPt = 12)
{
    QFont fnt = font;
    if (fnt.family().trimmed().isEmpty())
        fnt.setFamily(QStringLiteral("Arial"));
    fnt.setBold(false);
    fnt.setWeight(QFont::Normal);
    if (fnt.pointSize() <= 0 && fnt.pointSizeF() <= 0.0 && fnt.pixelSize() <= 0)
        fnt.setPointSize(fallbackPt);
    return fnt;
}

int resolveTableColIndex(Table *table, const QString &spec)
{
    if (!table || spec.isEmpty())
        return -1;

    const int nCols = table->numCols();
    const QString tablePrefix = table->name() + QLatin1Char('_');

    if (spec.startsWith(tablePrefix)) {
        const QString stripped = spec.mid(tablePrefix.length());
        for (int i = 0; i < nCols; ++i) {
            Column *c = table->column(i);
            if (c && c->name() == stripped)
                return i;
        }
    }

    for (int i = 0; i < nCols; ++i) {
        Column *c = table->column(i);
        if (c && c->name() == spec)
            return i;
    }

    if (spec.startsWith(tablePrefix)) {
        const int idx = table->colIndex(spec.mid(tablePrefix.length()));
        if (idx >= 0 && idx < nCols && table->column(idx) != nullptr)
            return idx;
    }

    const int idx = table->colIndex(spec);
    if (idx >= 0 && idx < nCols && table->column(idx) != nullptr)
        return idx;

    return -1;
}

bool parsePlotAssociationColumns(Table *table, const QString &assoc, int &xCol, int &yCol,
                                 int &zCol, bool &isXYZ)
{
    xCol = -1;
    yCol = -1;
    zCol = -1;
    isXYZ = false;
    if (!table || assoc.isEmpty())
        return false;

    const int xTagPos = assoc.indexOf(QLatin1String("(X),"));
    if (xTagPos <= 0)
        return false;

    const QString xSpec = assoc.left(xTagPos);
    const QString restAfterX = assoc.mid(xTagPos + 4);

    if (restAfterX.endsWith(QLatin1String("(Z)"))) {
        isXYZ = true;
        const int yTagPos = restAfterX.indexOf(QLatin1String("(Y),"));
        if (yTagPos <= 0)
            return false;
        const QString ySpec = restAfterX.left(yTagPos);
        const QString zSpec =
                restAfterX.mid(yTagPos + 4, restAfterX.length() - (yTagPos + 4) - 3);
        xCol = resolveTableColIndex(table, xSpec);
        yCol = resolveTableColIndex(table, ySpec);
        zCol = resolveTableColIndex(table, zSpec);
        return (xCol >= 0 && yCol >= 0 && zCol >= 0);
    }

    if (restAfterX.endsWith(QLatin1String("(Y)"))) {
        isXYZ = false;
        const QString ySpec = restAfterX.left(restAfterX.length() - 3);
        xCol = resolveTableColIndex(table, xSpec);
        yCol = resolveTableColIndex(table, ySpec);
        return (xCol >= 0 && yCol >= 0);
    }

    return false;
}

} // namespace

UserFunction::UserFunction(const QString &s, SurfacePlot &pw) : Function(pw)
{
    formula = s;
}

double UserFunction::operator()(double x, double y)
{
    if (formula.isEmpty())
        return 0.0;

    MyParser parser;
    double result = 0.0;
    try {
        parser.DefineVar(_T("x"), &x);
        parser.DefineVar(_T("y"), &y);

        parser.SetExpr(formula);
        result = parser.Eval();
    } catch (mu::ParserError &e) {
        if (!isGraph3DNonInteractive(nullptr)) {
            QMessageBox::critical(0, QObject::tr("Input function error"),
                                  QStringFromString(e.GetMsg()));
        }
    }
    return result;
}

UserFunction::~UserFunction() { }

Graph3D::Graph3D(const QString &label, QWidget *parent, const char *name, Qt::WindowFlags f)
    : MyWidget(label, parent, name, f)
{
    initPlot();
}

void Graph3D::initPlot()
{
    worksheet = nullptr;
    d_matrix = nullptr;
    plotAssociation = QString();

    QDateTime dt = QDateTime::currentDateTime();
    setBirthDate(QLocale().toString(dt));

    color_map = QString();
    animation_redraw_wait = 50;
    d_timer = new QTimer(this);
    connect(d_timer, SIGNAL(timeout()), this, SLOT(rotate()));
    ignoreFonts = false;

    QWidget *d_main_widget = new QWidget();
    sp = new SurfacePlot(d_main_widget);
    sp->resize(500, 400);
    sp->installEventFilter(this);
    sp->setRotation(30, 0, 15);
    sp->setScale(1, 1, 1);
    sp->setShift(0.15, 0, 0);
    sp->setZoom(0.9);
    sp->setOrtho(false);

    smoothMesh = true;
    sp->setSmoothMesh(smoothMesh);

    d_autoscale = true;

    title = QString();
    sp->setTitle(title);

    titleCol = QColor(Qt::black);
    sp->setTitleColor(Qt2GL(titleCol));

    titleFnt = QFont("Arial", 14, QFont::Normal, false);
    titleFnt.setBold(false);

    sp->setTitleFont(titleFnt.family(), titleFnt.pointSize(), titleFnt.weight(), titleFnt.italic());

    axesCol = QColor(Qt::black);
    labelsCol = QColor(Qt::black);
    numCol = QColor(Qt::black);
    meshCol = QColor(Qt::black);
    gridCol = QColor(Qt::black);
    bgCol = QColor(255, 255, 255);
    fromColor = QColor(Qt::red);
    toColor = QColor(Qt::blue);

    col_ = 0;

    legendOn = false;
    legendMajorTicks = 5;
    sp->showColorLegend(legendOn);
    sp->legend()->setAutoScale(true);
    sp->legend()->setMajors(legendMajorTicks);
    sp->legend()->setNumberFont(QFont("Arial", 10, QFont::Normal, false));
    sp->legend()->setTitleFont("Arial", 10, QFont::Normal);

    labelsDist = 18;
    d_base_zoom = 1.0;

    scaleType = QVector<int>(3);
    for (int j = 0; j < 3; j++)
        scaleType[j] = 0;

    pointStyle = None;
    func = 0;
    alpha = 1.0;
    barsRad = 0.007;
    pointSize = 5;
    smooth = false;
    crossHairRad = 0.03, crossHairLineWidth = 2;
    crossHairSmooth = true, crossHairBoxed = false;
    conesQuality = 32;
    conesRad = 0.5;

    style_ = NOPLOT;
    this->setWidget(d_main_widget);
    initCoord();

    connect(sp, SIGNAL(rotationChanged(double, double, double)), this,
            SLOT(rotationChanged(double, double, double)));
    connect(sp, SIGNAL(zoomChanged(double)), this, SLOT(zoomChanged(double)));
    connect(sp, SIGNAL(scaleChanged(double, double, double)), this,
            SLOT(scaleChanged(double, double, double)));
    connect(sp, SIGNAL(shiftChanged(double, double, double)), this,
            SLOT(shiftChanged(double, double, double)));
}

void Graph3D::initCoord()
{
    sp->makeCurrent();
    for (unsigned i = 0; i != sp->coordinates()->axes.size(); ++i) {
        sp->coordinates()->axes[i].setMajors(5);
        sp->coordinates()->axes[i].setMinors(5);
    }

    QString s = tr("X axis");
    sp->coordinates()->axes[X1].setLabelString(s);
    sp->coordinates()->axes[X2].setLabelString(s);
    sp->coordinates()->axes[X3].setLabelString(s);
    sp->coordinates()->axes[X4].setLabelString(s);
    labels << s;

    s = tr("Y axis");
    sp->coordinates()->axes[Y1].setLabelString(s);
    sp->coordinates()->axes[Y2].setLabelString(s);
    sp->coordinates()->axes[Y3].setLabelString(s);
    sp->coordinates()->axes[Y4].setLabelString(s);
    labels << s;

    s = tr("Z axis");
    sp->coordinates()->axes[Z1].setLabelString(s);
    sp->coordinates()->axes[Z2].setLabelString(s);
    sp->coordinates()->axes[Z3].setLabelString(s);
    sp->coordinates()->axes[Z4].setLabelString(s);
    labels << s;

    sp->setCoordinateStyle(BOX);
    sp->coordinates()->setNumberFont(QFont("Arial", 11, QFont::Normal, false));
    sp->coordinates()->setLabelFont(QFont("Arial", 12, QFont::Normal, false));
    sp->coordinates()->setAutoScale(true);
    sp->coordinates()->adjustNumbers(6);
    sp->coordinates()->adjustLabels(labelsDist);
}

void Graph3D::addFunction(const QString &s, double xl, double xr, double yl, double yr, double zl,
                          double zr)
{
    sp->makeCurrent();
    if (this->width() > 100 && this->height() > 100)
        sp->resize(this->size());

    func = new UserFunction(s, *sp);

    func->setMesh(41, 31);
    func->setDomain(xl, xr, yl, yr);
    func->setMinZ(zl);
    func->setMaxZ(zr);
    func->create();

    sp->legend()->setLimits(zl, zr);

    if (sp->plotStyle() == NOPLOT) {
        sp->setPlotStyle(FILLED);
        style_ = FILLED;
        pointStyle = None;
    }
    sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
    findBestLayout();
}

void Graph3D::insertFunction(const QString &s, double xl, double xr, double yl, double yr,
                             double zl, double zr)
{
    addFunction(s, xl, xr, yl, yr, zl, zr);
    update();
}

void Graph3D::addData(Table *table, int xcol, int ycol)
{
    if (!table)
        return;
    Column *xColPtr = table->column(xcol);
    Column *yColPtr = table->column(ycol);
    if (!xColPtr || !yColPtr)
        return;

    worksheet = table;
    int r = table->numRows();
    int i, xmesh = 0, ymesh = 2;
    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !xColPtr->isMasked(i)
            && !yColPtr->isMasked(i) && std::isfinite(table->cell(i, xcol))
            && std::isfinite(table->cell(i, ycol))) {
            xmesh++;
        }
    }

    // qwtplot3d spaces the grid by (max - min) / (points - 1): with fewer than two points the
    // vertices become NaN (and the axis autoscaler may never return). Same as updateDataXY().
    // The ribbon is drawn as a filled mesh once the table has two points (resetNonEmptyStyle()).
    if (xmesh < 2) {
        if (style_ == NOPLOT)
            style_ = FILLEDMESH;
        sp->setPlotStyle(NOPLOT);
        update();
        return;
    }

    double **data = Matrix::allocateMatrixData(xmesh, ymesh);
    gsl_vector *x = gsl_vector_calloc(xmesh);
    gsl_vector *y = gsl_vector_calloc(xmesh);

    for (int j = 0; j < ymesh; j++) {
        int k = 0;
        for (i = 0; i < r && k < xmesh; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !xColPtr->isMasked(i)
                && !yColPtr->isMasked(i)) {
                const double xv = table->cell(i, xcol);
                const double yv = table->cell(i, ycol);
                if (!std::isfinite(xv) || !std::isfinite(yv))
                    continue;
                gsl_vector_set(x, k, xv);
                gsl_vector_set(y, k, yv);
                data[k][j] = yv;
                k++;
            }
        }
    }

    const double minx = gsl_vector_min(x);
    const double maxx = qMax(minx + 1e-6, gsl_vector_max(x));
    const double miny = gsl_vector_min(y);
    const double maxy = gsl_vector_max(y);
    const double maxz = qMax(1e-6, 0.6 * std::fabs(maxy - miny));
    sp->makeCurrent();
    sp->legend()->setLimits(miny, qMax(miny + 1e-9, maxy));
    sp->loadFromData(data, xmesh, ymesh, minx, maxx, 0.0, maxz);
    resetNonEmptyStyle(); // a NOPLOT left by an earlier degenerate table draws again

    if (d_autoscale)
        findBestLayout();

    gsl_vector_free(x);
    gsl_vector_free(y);
    Matrix::freeMatrixData(data, xmesh);
}

void Graph3D::addData(Table *table, const QString &colName)
{
    if (!table)
        return;
    int ycol = resolveTableColIndex(table, colName);
    if (ycol < 0)
        return;
    int xcol = table->colX(ycol);
    if (xcol < 0 || !table->column(xcol) || !table->column(ycol))
        return;

    plotAssociation = table->colName(xcol) + "(X)," + table->colName(ycol) + "(Y)";

    addData(table, xcol, ycol);
}

void Graph3D::addData(Table *table, const QString &xColName, const QString &yColName)
{
    if (!table)
        return;
    int xcol = resolveTableColIndex(table, xColName);
    int ycol = resolveTableColIndex(table, yColName);
    if (xcol < 0 || ycol < 0 || !table->column(xcol) || !table->column(ycol))
        return;

    plotAssociation = table->colName(xcol) + "(X)," + table->colName(ycol) + "(Y)";

    addData(table, xcol, ycol);
}

void Graph3D::changeMatrix(Matrix *m)
{
    if (!m || d_matrix == m)
        return;

    addMatrixData(m);
}

void Graph3D::addMatrixData(Matrix *m)
{
    if (!m || d_matrix == m)
        return;

    int cols = m->numCols();
    int rows = m->numRows();

    bool first_time = false;
    if (!d_matrix)
        first_time = true;

    // Associate the plot with the matrix even when it cannot be drawn yet, so that it follows
    // the matrix (updateMatrixData()) and the association is saved with the project.
    d_matrix = m;
    plotAssociation = "matrix<" + QString(m->name()) + ">";

    // qwtplot3d needs two grid lines along each axis (it spaces them by range / (n - 1)).
    // Leave the plot empty until the matrix has them (same as updateMatrixData()).
    if (cols < 2 || rows < 2) {
        sp->makeCurrent();
        sp->setPlotStyle(NOPLOT);
        update();
        return;
    }

    // loadFromData() takes data[x index][y index]: x runs along the matrix columns.
    double **data_matrix = Matrix::allocateMatrixData(cols, rows);
    for (int c = 0; c < cols; c++) {
        for (int r = 0; r < rows; r++)
            data_matrix[c][r] = m->cell(r, c);
    }

    sp->makeCurrent();
    resetNonEmptyStyle(); // e.g. "Change matrix" away from a matrix that had shrunk to one row
    sp->loadFromData(data_matrix, cols, rows, m->xStart(), m->xEnd(), m->yStart(), m->yEnd());

    double start, end;
    sp->coordinates()->axes[Z1].limits(start, end);
    sp->legend()->setLimits(start, end);
    sp->legend()->setMajors(legendMajorTicks);

    Matrix::freeMatrixData(data_matrix, cols);

    if (d_autoscale || first_time)
        findBestLayout();
    update();
}

void Graph3D::addMatrixData(Matrix *m, double xl, double xr, double yl, double yr, double zl,
                            double zr)
{
    if (!m)
        return;
    d_matrix = m;
    plotAssociation = "matrix<" + QString(m->name()) + ">";

    updateScalesFromMatrix(xl, xr, yl, yr, zl, zr);
    // Used when a project is reopened or a plot is cloned: findBestLayout() is not called, so
    // take the base zoom from the loaded data, as the plot did when it was created (the saved
    // zoom is relative to it).
    updateBaseZoom();
}

void Graph3D::addData(Table *table, const QString &xColName, const QString &yColName, double xl,
                      double xr, double yl, double yr, double zl, double zr)
{
    if (!table)
        return;
    int xcol = resolveTableColIndex(table, xColName);
    int ycol = resolveTableColIndex(table, yColName);
    Column *xColPtr = table->column(xcol);
    Column *yColPtr = table->column(ycol);
    if (!xColPtr || !yColPtr)
        return;

    worksheet = table;
    int r = table->numRows();
    plotAssociation = table->colName(xcol) + "(X)," + table->colName(ycol) + "(Y)";

    int i, j, xmesh = 0, ymesh = 2;
    double xv, yv;

    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i)) {
            xv = table->cell(i, xcol);
            if (xv >= xl && xv <= xr)
                xmesh++;
        }
    }

    // Fewer than two points in the X range cannot be drawn as a ribbon (qwtplot3d spaces the
    // grid by range / (points - 1), so one point gives NaN vertices): keep the axes but leave
    // the plot empty until updateScales() / updateData() finds two points again.
    if (xmesh < 2) {
        if (style_ == NOPLOT)
            style_ = FILLEDMESH;
        sp->makeCurrent();
        sp->setPlotStyle(NOPLOT);
        sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
        sp->legend()->setLimits(zl, zr);
        sp->legend()->setMajors(legendMajorTicks);
        return;
    }

    double **data = Matrix::allocateMatrixData(xmesh, ymesh);
    for (j = 0; j < ymesh; j++) {
        int k = 0;
        for (i = 0; i < r; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i)) {
                xv = table->cell(i, xcol);
                if (xv >= xl && xv <= xr) {
                    yv = table->cell(i, ycol);
                    if (yv > zr)
                        data[k][j] = zr;
                    else if (yv < zl)
                        data[k][j] = zl;
                    else
                        data[k][j] = yv;
                    k++;
                }
            }
        }
    }
    sp->makeCurrent();
    resetNonEmptyStyle();
    sp->loadFromData(data, xmesh, ymesh, xl, xr, yl, yr);
    sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
    sp->legend()->setLimits(zl, zr);
    sp->legend()->setMajors(legendMajorTicks);
    updateBaseZoom(); // reopen / clone: the saved zoom is relative to this base

    Matrix::freeMatrixData(data, xmesh);
}

void Graph3D::insertNewData(Table *table, const QString &colName)
{
    if (!table)
        return;
    int zCol = resolveTableColIndex(table, colName);
    if (zCol < 0)
        return;
    int yCol = table->colY(zCol);
    int xCol = table->colX(zCol);
    if (xCol < 0 || yCol < 0 || !table->column(xCol) || !table->column(yCol)
        || !table->column(zCol))
        return;

    addData(table, xCol, yCol, zCol, Trajectory);
    update();
}

void Graph3D::changeDataColumn(Table *table, const QString &colName)
{
    if (!table)
        return;
    int zCol = resolveTableColIndex(table, colName);
    if (zCol < 0)
        return;
    int yCol = table->colY(zCol);
    int xCol = table->colX(zCol);
    if (xCol < 0 || yCol < 0 || !table->column(xCol) || !table->column(yCol)
        || !table->column(zCol))
        return;

    worksheet = table;
    plotAssociation = table->colName(xCol) + "(X)," + table->colName(yCol) + "(Y),"
            + table->colName(zCol) + "(Z)";

    updateDataXYZ(table, xCol, yCol, zCol);
    if (d_autoscale)
        findBestLayout();
}

void Graph3D::addData(Table *table, int xCol, int yCol, int zCol, int type)
{
    if (!table)
        return;
    Column *xColPtr = table->column(xCol);
    Column *yColPtr = table->column(yCol);
    Column *zColPtr = table->column(zCol);
    if (!xColPtr || !yColPtr || !zColPtr)
        return;

    worksheet = table;
    int r = table->numRows();

    QString s = table->colName(xCol) + "(X),";
    s += table->colName(yCol) + "(Y),";
    s += table->colName(zCol) + "(Z)";
    plotAssociation = s;

    int i, j, columns = 0;
    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i))
            columns++;
    }

    if (columns == 0)
        columns++;

    Qwt3D::Triple **data = allocateData(columns, 2);
    for (j = 0; j < 2; j++) {
        int k = 0;
        for (i = 0; i < r; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i)) {
                double xv = table->cell(i, xCol);
                double yv = table->cell(i, yCol);
                double zv = table->cell(i, zCol);

                data[k][j] = Triple(xv, yv, zv);
                k++;
            }
        }
    }

    sp->makeCurrent();
    sp->loadFromData(data, columns, 2, false, false);

    double start, end;
    sp->coordinates()->axes[Z1].limits(start, end);
    sp->legend()->setLimits(start, end);
    sp->legend()->setMajors(legendMajorTicks);

    if (type == Scatter) {
        Dot d(pointSize, smooth);
        sp->setPlotStyle(d);
        pointStyle = Dots;
        style_ = Qwt3D::USER;
    } else if (type == Trajectory) {
        legendOn = false;
        sp->showColorLegend(legendOn);
    } else {
        sp->setPlotStyle(Bar(barsRad));
        pointStyle = VerticalBars;
        style_ = Qwt3D::USER;
    }

    if (d_autoscale)
        findBestLayout();
    deleteData(data, columns);
}

void Graph3D::addData(Table *table, int xCol, int yCol, int zCol, double xl, double xr, double yl,
                      double yr, double zl, double zr)
{
    if (!table)
        return;
    Column *xColPtr = table->column(xCol);
    Column *yColPtr = table->column(yCol);
    Column *zColPtr = table->column(zCol);
    if (!xColPtr || !yColPtr || !zColPtr)
        return;

    worksheet = table;
    int r = table->numRows();

    QString s = table->colName(xCol) + "(X),";
    s += table->colName(yCol) + "(Y),";
    s += table->colName(zCol) + "(Z)";
    plotAssociation = s;

    int i, j, columns = 0;
    double xv, yv;
    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i)) {
            xv = table->cell(i, xCol);
            yv = table->cell(i, yCol);
            if (xv >= xl && xv <= xr && yv >= yl && yv <= yr)
                columns++;
        }
    }

    if (columns == 0)
        columns++;

    Qwt3D::Triple **data = allocateData(columns, 2);
    for (j = 0; j < 2; j++) {
        int k = 0;
        for (i = 0; i < r; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i)) {
                xv = table->cell(i, xCol);
                yv = table->cell(i, yCol);
                if (xv >= xl && xv <= xr && yv >= yl && yv <= yr) {
                    double zv = table->cell(i, zCol);
                    if (zv > zr)
                        data[k][j] = Triple(xv, yv, zr);
                    else if (zv < zl)
                        data[k][j] = Triple(xv, yv, zl);
                    else
                        data[k][j] = Triple(xv, yv, zv);
                    k++;
                }
            }
        }
    }
    sp->makeCurrent();
    sp->loadFromData(data, columns, 2, false, false);
    sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
    sp->legend()->setLimits(zl, zr);
    sp->legend()->setMajors(legendMajorTicks);
    updateBaseZoom(); // reopen / clone: the saved zoom is relative to this base

    deleteData(data, columns);
}

void Graph3D::updateData(Table *table)
{
    if (func || d_matrix || !table)
        return;
    if (worksheet && worksheet != table)
        return;

    int xCol = -1, yCol = -1, zCol = -1;
    bool isXYZ = false;
    if (!parsePlotAssociationColumns(table, plotAssociation, xCol, yCol, zCol, isXYZ))
        return;

    if (isXYZ)
        updateDataXYZ(table, xCol, yCol, zCol);
    else
        updateDataXY(table, xCol, yCol);

    if (d_autoscale)
        findBestLayout();
    update();
}

void Graph3D::updateDataXY(Table *table, int xCol, int yCol)
{
    if (!table)
        return;
    Column *xColPtr = table->column(xCol);
    Column *yColPtr = table->column(yCol);
    if (!xColPtr || !yColPtr)
        return;

    int r = table->numRows();
    int i, j, xmesh = 0, ymesh = 2;

    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i))
            xmesh++;
    }

    if (xmesh < 2) {
        sp->setPlotStyle(NOPLOT);
        update();
        return;
    }

    double **data = Matrix::allocateMatrixData(xmesh, ymesh);
    gsl_vector *x = gsl_vector_alloc(xmesh);
    gsl_vector *y = gsl_vector_alloc(xmesh);

    for (j = 0; j < ymesh; j++) {
        int k = 0;
        for (i = 0; i < r; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i)) {
                double xv = table->cell(i, xCol);
                double yv = table->cell(i, yCol);

                gsl_vector_set(x, k, xv);
                gsl_vector_set(y, k, yv);

                data[k][j] = yv;
                k++;
            }
        }
    }

    double minx = gsl_vector_min(x);
    double maxx = gsl_vector_max(x);
    double minz = gsl_vector_min(y);
    double maxz = gsl_vector_max(y);
    double miny, maxy;

    sp->makeCurrent();
    resetNonEmptyStyle();
    sp->coordinates()->axes[Y1].limits(miny, maxy); // actual Y scale limits
    sp->loadFromData(data, xmesh, ymesh, minx, maxx, miny, maxy);
    sp->legend()->setLimits(minz, maxz);
    sp->legend()->setMajors(legendMajorTicks);

    gsl_vector_free(x);
    gsl_vector_free(y);
    Matrix::freeMatrixData(data, xmesh);
}

void Graph3D::updateDataXYZ(Table *table, int xCol, int yCol, int zCol)
{
    if (!table)
        return;
    Column *xColPtr = table->column(xCol);
    Column *yColPtr = table->column(yCol);
    Column *zColPtr = table->column(zCol);
    if (!xColPtr || !yColPtr || !zColPtr)
        return;

    int r = table->numRows();
    int i, j, columns = 0;

    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i))
            columns++;
    }

    if (columns < 2) {
        sp->setPlotStyle(NOPLOT);
        update();
        return;
    }

    Qwt3D::Triple **data = allocateData(columns, 2);
    gsl_vector *z = gsl_vector_alloc(columns);

    for (j = 0; j < 2; j++) {
        int k = 0;
        for (i = 0; i < r; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i)) {
                double xv = table->cell(i, xCol);
                double yv = table->cell(i, yCol);
                double zv = table->cell(i, zCol);

                gsl_vector_set(z, k, zv);
                data[k][j] = Triple(xv, yv, zv);
                k++;
            }
        }
    }

    double minz = gsl_vector_min(z);
    double maxz = gsl_vector_max(z);
    gsl_vector_free(z);

    sp->makeCurrent();
    resetNonEmptyStyle();

    sp->loadFromData(data, columns, 2, false, false);
    sp->legend()->setLimits(minz, maxz);
    sp->legend()->setMajors(legendMajorTicks);

    deleteData(data, columns);
}

void Graph3D::updateMatrixData(Matrix *m)
{
    if (!m)
        return;
    int cols = m->numCols();
    int rows = m->numRows();
    // Fewer than two rows or columns cannot be drawn as a surface (see addMatrixData()).
    if (cols < 2 || rows < 2) {
        sp->setPlotStyle(NOPLOT);
        update();
        return;
    }

    // loadFromData() takes data[x index][y index]: x runs along the matrix columns.
    double **data = Matrix::allocateMatrixData(cols, rows);
    for (int c = 0; c < cols; c++) {
        for (int r = 0; r < rows; r++)
            data[c][r] = m->cell(r, c);
    }

    sp->makeCurrent();
    resetNonEmptyStyle();
    sp->loadFromData(data, cols, rows, m->xStart(), m->xEnd(), m->yStart(), m->yEnd());

    Qwt3D::Axis z_axis = sp->coordinates()->axes[Z1];
    double start, end;
    z_axis.limits(start, end);

    sp->legend()->setLimits(start, end);
    sp->legend()->setMajors(legendMajorTicks);

    Matrix::freeMatrixData(data, cols);
    if (d_autoscale)
        findBestLayout();
    update();
}

void Graph3D::resetNonEmptyStyle()
{
    if (sp->plotStyle() != Qwt3D::NOPLOT)
        return; // the plot was not previousely emptied

    // style_ is NOPLOT when no style was ever chosen (a plot that was empty from the start, or a
    // ribbon / trajectory that kept qwtplot3d's default): draw it as qwtplot3d's default
    // filled mesh instead of setting NOPLOT again and staying blank for good.
    if (style_ == Qwt3D::NOPLOT)
        style_ = Qwt3D::FILLEDMESH;

    if (style_ == Qwt3D::USER) { // reseting the right user plot style
        switch (pointStyle) {
        case None:
            break;

        case Dots:
            sp->setPlotStyle(Dot(pointSize, smooth));
            break;

        case VerticalBars:
            sp->setPlotStyle(Bar(barsRad));
            break;

        case HairCross:
            sp->setPlotStyle(
                    CrossHair(crossHairRad, crossHairLineWidth, crossHairSmooth, crossHairBoxed));
            break;

        case Cones:
            sp->setPlotStyle(Cone3D(conesRad, conesQuality));
            break;
        }
    } else
        sp->setPlotStyle(style_);
}

UserFunction *Graph3D::userFunction()
{
    if (func)
        return func;
    else
        return 0;
}

void Graph3D::update()
{
    sp->makeCurrent();

    resetAxesLabels();

    sp->updateData();
    sp->update();
}

void Graph3D::adjustLabels(int val)
{
    if (labelsDist != val) {
        labelsDist = val;
        sp->coordinates()->adjustLabels(val);
        sp->makeCurrent();
        sp->update();
    }
    emit modified();
}

QFont Graph3D::numbersFont()
{
    return sp->coordinates()->axes[X1].numberFont();
}

void Graph3D::setNumbersFont(const QFont &font)
{
    QFont fnt = font;
    if (fnt.family().trimmed().isEmpty())
        fnt.setFamily("Arial");
    sp->coordinates()->setNumberFont(fnt);
    sp->legend()->setNumberFont(fnt);
    sp->makeCurrent();
    sp->update();
}

void Graph3D::setNumbersFont(const QStringList &lst)
{
    QFont fnt = QFont(lst[1], lst[2].toInt(), lst[3].toInt(), lst[4].toInt());
    if (fnt.family().trimmed().isEmpty())
        fnt.setFamily("Arial");
    sp->coordinates()->setNumberFont(fnt);
    sp->legend()->setNumberFont(fnt);
}

void Graph3D::setXAxisLabelFont(const QFont &font)
{
    const QFont fnt = normalizePlot3DFont(font);
    sp->coordinates()->axes[X1].setLabelFont(fnt);
    sp->coordinates()->axes[X2].setLabelFont(fnt);
    sp->coordinates()->axes[X3].setLabelFont(fnt);
    sp->coordinates()->axes[X4].setLabelFont(fnt);
}

void Graph3D::setYAxisLabelFont(const QFont &font)
{
    const QFont fnt = normalizePlot3DFont(font);
    sp->coordinates()->axes[Y1].setLabelFont(fnt);
    sp->coordinates()->axes[Y2].setLabelFont(fnt);
    sp->coordinates()->axes[Y3].setLabelFont(fnt);
    sp->coordinates()->axes[Y4].setLabelFont(fnt);
}

void Graph3D::setZAxisLabelFont(const QFont &font)
{
    const QFont fnt = normalizePlot3DFont(font);
    sp->coordinates()->axes[Z1].setLabelFont(fnt);
    sp->coordinates()->axes[Z2].setLabelFont(fnt);
    sp->coordinates()->axes[Z3].setLabelFont(fnt);
    sp->coordinates()->axes[Z4].setLabelFont(fnt);
}

void Graph3D::setXAxisLabelFont(const QStringList &lst)
{
    QFont fnt = normalizePlot3DFont(QFont(lst[1], lst[2].toInt(), lst[3].toInt(), lst[4].toInt()));
    sp->coordinates()->axes[X1].setLabelFont(fnt);
    sp->coordinates()->axes[X2].setLabelFont(fnt);
    sp->coordinates()->axes[X3].setLabelFont(fnt);
    sp->coordinates()->axes[X4].setLabelFont(fnt);
}

void Graph3D::setYAxisLabelFont(const QStringList &lst)
{
    QFont fnt = normalizePlot3DFont(QFont(lst[1], lst[2].toInt(), lst[3].toInt(), lst[4].toInt()));
    sp->coordinates()->axes[Y1].setLabelFont(fnt);
    sp->coordinates()->axes[Y2].setLabelFont(fnt);
    sp->coordinates()->axes[Y3].setLabelFont(fnt);
    sp->coordinates()->axes[Y4].setLabelFont(fnt);
}

void Graph3D::setZAxisLabelFont(const QStringList &lst)
{
    QFont fnt = normalizePlot3DFont(QFont(lst[1], lst[2].toInt(), lst[3].toInt(), lst[4].toInt()));
    sp->coordinates()->axes[Z1].setLabelFont(fnt);
    sp->coordinates()->axes[Z2].setLabelFont(fnt);
    sp->coordinates()->axes[Z3].setLabelFont(fnt);
    sp->coordinates()->axes[Z4].setLabelFont(fnt);
}

QStringList Graph3D::axisTickLengths()
{
    QStringList lst;
    double majorl, minorl;

    sp->coordinates()->axes[X1].ticLength(majorl, minorl);
    lst << QString::number(majorl);
    lst << QString::number(minorl);

    sp->coordinates()->axes[Y1].ticLength(majorl, minorl);
    lst << QString::number(majorl);
    lst << QString::number(minorl);

    sp->coordinates()->axes[Z1].ticLength(majorl, minorl);
    lst << QString::number(majorl);
    lst << QString::number(minorl);

    return lst;
}

void Graph3D::setTickLengths(const QStringList &lst)
{
    double majorl, minorl;
    QStringList tick_length = lst;
    if (int(lst.count()) > 6)
        tick_length.removeAll(tick_length.first());

    majorl = tick_length[0].toDouble();
    minorl = tick_length[1].toDouble();
    sp->coordinates()->axes[X1].setTicLength(majorl, minorl);
    sp->coordinates()->axes[X2].setTicLength(majorl, minorl);
    sp->coordinates()->axes[X3].setTicLength(majorl, minorl);
    sp->coordinates()->axes[X4].setTicLength(majorl, minorl);

    majorl = tick_length[2].toDouble();
    minorl = tick_length[3].toDouble();
    sp->coordinates()->axes[Y1].setTicLength(majorl, minorl);
    sp->coordinates()->axes[Y2].setTicLength(majorl, minorl);
    sp->coordinates()->axes[Y3].setTicLength(majorl, minorl);
    sp->coordinates()->axes[Y4].setTicLength(majorl, minorl);

    majorl = tick_length[4].toDouble();
    minorl = tick_length[5].toDouble();
    sp->coordinates()->axes[Z1].setTicLength(majorl, minorl);
    sp->coordinates()->axes[Z2].setTicLength(majorl, minorl);
    sp->coordinates()->axes[Z3].setTicLength(majorl, minorl);
    sp->coordinates()->axes[Z4].setTicLength(majorl, minorl);
}

void Graph3D::updateTickLength(int axis, double majorLength, double minorLength)
{
    double majorl, minorl;
    switch (axis) {
    case 0:
        sp->coordinates()->axes[X1].ticLength(majorl, minorl);
        if (majorl != majorLength || minorl != minorLength) {
            sp->coordinates()->axes[X1].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[X2].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[X3].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[X4].setTicLength(majorLength, minorLength);
        }
        break;

    case 1:
        sp->coordinates()->axes[Y1].ticLength(majorl, minorl);
        if (majorl != majorLength || minorl != minorLength) {
            sp->coordinates()->axes[Y1].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[Y2].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[Y3].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[Y4].setTicLength(majorLength, minorLength);
        }
        break;

    case 2:
        sp->coordinates()->axes[Z1].ticLength(majorl, minorl);
        if (majorl != majorLength || minorl != minorLength) {
            sp->coordinates()->axes[Z1].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[Z2].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[Z3].setTicLength(majorLength, minorLength);
            sp->coordinates()->axes[Z4].setTicLength(majorLength, minorLength);
        }
        break;
    }
    sp->update();
}

void Graph3D::rotationChanged(double, double, double)
{
    emit modified();
}

void Graph3D::scaleChanged(double, double, double)
{
    emit modified();
}

void Graph3D::shiftChanged(double, double, double)
{
    emit modified();
}

void Graph3D::zoomChanged(double)
{
    emit modified();
}

void Graph3D::resetAxesLabels()
{
    sp->coordinates()->axes[X1].setLabelString(labels[0]);
    sp->coordinates()->axes[X2].setLabelString(labels[0]);
    sp->coordinates()->axes[X3].setLabelString(labels[0]);
    sp->coordinates()->axes[X4].setLabelString(labels[0]);

    sp->coordinates()->axes[Y1].setLabelString(labels[1]);
    sp->coordinates()->axes[Y2].setLabelString(labels[1]);
    sp->coordinates()->axes[Y3].setLabelString(labels[1]);
    sp->coordinates()->axes[Y4].setLabelString(labels[1]);

    sp->coordinates()->axes[Z1].setLabelString(labels[2]);
    sp->coordinates()->axes[Z2].setLabelString(labels[2]);
    sp->coordinates()->axes[Z3].setLabelString(labels[2]);
    sp->coordinates()->axes[Z4].setLabelString(labels[2]);
}

void Graph3D::setAxesLabels(const QStringList &l)
{
    QString label = l[0];
    sp->coordinates()->axes[X1].setLabelString(label);
    sp->coordinates()->axes[X2].setLabelString(label);
    sp->coordinates()->axes[X3].setLabelString(label);
    sp->coordinates()->axes[X4].setLabelString(label);

    label = l[1];
    sp->coordinates()->axes[Y1].setLabelString(label);
    sp->coordinates()->axes[Y2].setLabelString(label);
    sp->coordinates()->axes[Y3].setLabelString(label);
    sp->coordinates()->axes[Y4].setLabelString(label);

    label = l[2];
    sp->coordinates()->axes[Z1].setLabelString(label);
    sp->coordinates()->axes[Z2].setLabelString(label);
    sp->coordinates()->axes[Z3].setLabelString(label);
    sp->coordinates()->axes[Z4].setLabelString(label);

    labels = l;
}

void Graph3D::updateLabel(int axis, const QString &label, const QFont &font)
{
    const QFont f = normalizePlot3DFont(font);
    switch (axis) {
    case 0:
        if (labels[0] != label) {
            sp->coordinates()->axes[X1].setLabelString(label);
            sp->coordinates()->axes[X2].setLabelString(label);
            sp->coordinates()->axes[X3].setLabelString(label);
            sp->coordinates()->axes[X4].setLabelString(label);
            labels[0] = label;
        }
        if (sp->coordinates()->axes[X1].labelFont() != f) {
            sp->coordinates()->axes[X1].setLabelFont(f);
            sp->coordinates()->axes[X2].setLabelFont(f);
            sp->coordinates()->axes[X3].setLabelFont(f);
            sp->coordinates()->axes[X4].setLabelFont(f);
        }
        break;

    case 1:
        if (labels[1] != label) {
            sp->coordinates()->axes[Y1].setLabelString(label);
            sp->coordinates()->axes[Y2].setLabelString(label);
            sp->coordinates()->axes[Y3].setLabelString(label);
            sp->coordinates()->axes[Y4].setLabelString(label);

            labels[1] = label;
        }
        if (sp->coordinates()->axes[Y1].labelFont() != f) {
            sp->coordinates()->axes[Y1].setLabelFont(f);
            sp->coordinates()->axes[Y2].setLabelFont(f);
            sp->coordinates()->axes[Y3].setLabelFont(f);
            sp->coordinates()->axes[Y4].setLabelFont(f);
        }
        break;

    case 2:
        if (labels[2] != label) {
            sp->coordinates()->axes[Z1].setLabelString(label);
            sp->coordinates()->axes[Z2].setLabelString(label);
            sp->coordinates()->axes[Z3].setLabelString(label);
            sp->coordinates()->axes[Z4].setLabelString(label);
            labels[2] = label;
        }
        if (sp->coordinates()->axes[Z1].labelFont() != f) {
            sp->coordinates()->axes[Z1].setLabelFont(f);
            sp->coordinates()->axes[Z2].setLabelFont(f);
            sp->coordinates()->axes[Z3].setLabelFont(f);
            sp->coordinates()->axes[Z4].setLabelFont(f);
        }
        break;
    }

    sp->makeCurrent();
    sp->update();
    emit modified();
}

QFont Graph3D::xAxisLabelFont()
{
    return sp->coordinates()->axes[X1].labelFont();
}

QFont Graph3D::yAxisLabelFont()
{
    return sp->coordinates()->axes[Y1].labelFont();
}

QFont Graph3D::zAxisLabelFont()
{
    return sp->coordinates()->axes[Z1].labelFont();
}

double Graph3D::xStart()
{
    double start, stop;
    sp->coordinates()->axes[X1].limits(start, stop);
    return start;
}

double Graph3D::xStop()
{
    double start, stop;
    sp->coordinates()->axes[X1].limits(start, stop);
    return stop;
}

double Graph3D::yStart()
{
    double start, stop;
    sp->coordinates()->axes[Y1].limits(start, stop);
    return start;
}

double Graph3D::yStop()
{
    double start, stop;
    sp->coordinates()->axes[Y1].limits(start, stop);
    return stop;
}

double Graph3D::zStart()
{
    double start, stop;
    sp->coordinates()->axes[Z1].limits(start, stop);
    return start;
}

double Graph3D::zStop()
{
    double start, stop;
    sp->coordinates()->axes[Z1].limits(start, stop);
    return stop;
}

QStringList Graph3D::scaleLimits()
{
    QStringList limits;
    double start, stop;
    int majors, minors;

    sp->coordinates()->axes[X1].limits(start, stop);
    majors = sp->coordinates()->axes[X1].majors();
    minors = sp->coordinates()->axes[X1].minors();

    limits << QString::number(start);
    limits << QString::number(stop);
    limits << QString::number(majors);
    limits << QString::number(minors);
    limits << QString::number(scaleType[0]);

    sp->coordinates()->axes[Y1].limits(start, stop);
    majors = sp->coordinates()->axes[Y1].majors();
    minors = sp->coordinates()->axes[Y1].minors();

    limits << QString::number(start);
    limits << QString::number(stop);
    limits << QString::number(majors);
    limits << QString::number(minors);
    limits << QString::number(scaleType[1]);

    sp->coordinates()->axes[Z1].limits(start, stop);
    majors = sp->coordinates()->axes[Z1].majors();
    minors = sp->coordinates()->axes[Z1].minors();

    limits << QString::number(start);
    limits << QString::number(stop);
    limits << QString::number(majors);
    limits << QString::number(minors);
    limits << QString::number(scaleType[2]);

    return limits;
}

QStringList Graph3D::scaleTicks()
{
    QStringList limits;
    int majors, minors;

    majors = sp->coordinates()->axes[X1].majors();
    minors = sp->coordinates()->axes[X1].minors();
    limits << QString::number(majors);
    limits << QString::number(minors);

    majors = sp->coordinates()->axes[Y1].majors();
    minors = sp->coordinates()->axes[Y1].minors();
    limits << QString::number(majors);
    limits << QString::number(minors);

    majors = sp->coordinates()->axes[Z1].majors();
    minors = sp->coordinates()->axes[Z1].minors();
    limits << QString::number(majors);
    limits << QString::number(minors);

    return limits;
}

void Graph3D::updateScale(int axis, const QStringList &options)
{
    double xMin, xMax, yMin, yMax, zMin, zMax;
    double *min, *max;
    int majors, minors, newMaj, newMin;
    Qwt3D::Axis *targetAxes[4];

    sp->makeCurrent();

    switch (axis) {
    case 0:
        targetAxes[0] = &sp->coordinates()->axes[X1];
        targetAxes[1] = &sp->coordinates()->axes[X2];
        targetAxes[2] = &sp->coordinates()->axes[X3];
        targetAxes[3] = &sp->coordinates()->axes[X4];
        min = &xMin;
        max = &xMax;
        break;
    case 1:
        targetAxes[0] = &sp->coordinates()->axes[Y1];
        targetAxes[1] = &sp->coordinates()->axes[Y2];
        targetAxes[2] = &sp->coordinates()->axes[Y3];
        targetAxes[3] = &sp->coordinates()->axes[Y4];
        min = &yMin;
        max = &yMax;
        break;
    case 2:
        targetAxes[0] = &sp->coordinates()->axes[Z1];
        targetAxes[1] = &sp->coordinates()->axes[Z2];
        targetAxes[2] = &sp->coordinates()->axes[Z3];
        targetAxes[3] = &sp->coordinates()->axes[Z4];
        min = &zMin;
        max = &zMax;
        break;
    default:
        throw runtime_error("invalid axis");
    }

    majors = targetAxes[0]->majors();
    minors = targetAxes[0]->minors();

    sp->coordinates()->axes[X1].limits(xMin, xMax);
    sp->coordinates()->axes[Y1].limits(yMin, yMax);
    sp->coordinates()->axes[Z1].limits(zMin, zMax);

    if (*min != options[0].toDouble() || *max != options[1].toDouble()) {
        *min = options[0].toDouble();
        *max = options[1].toDouble();
        if (func) {
            func->setDomain(xMin, xMax, yMin, yMax);
            func->setMinZ(zMin);
            func->setMaxZ(zMax);
            func->create();
            sp->createCoordinateSystem(Triple(xMin, yMin, zMin), Triple(xMax, yMax, zMax));
        } else
            updateScales(xMin, xMax, yMin, yMax, zMin, zMax);
        sp->legend()->setLimits(zMin, zMax);
    }

    if (QString::number(scaleType[axis]) != options[4]) {
        if (options[4] == "0") {
            targetAxes[0]->setScale(LINEARSCALE);
            if (axis == 2)
                sp->legend()->setScale(LINEARSCALE);
            scaleType[axis] = 0;
        } else {
            targetAxes[0]->setScale(LOG10SCALE);
            if (axis == 2)
                sp->legend()->setScale(LOG10SCALE);
            scaleType[axis] = 1;
        }
    }

    newMaj = options[2].toInt();
    if (majors != newMaj)
        for (int i = 0; i < 4; i++)
            targetAxes[i]->setMajors(newMaj);

    newMin = options[3].toInt();
    if (minors != newMin)
        for (int i = 0; i < 4; i++)
            targetAxes[i]->setMinors(newMin);

    update();
    emit modified();
}

void Graph3D::updateScales(double xl, double xr, double yl, double yr, double zl, double zr)
{
    const bool nonInteractive = isGraph3DNonInteractive(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    if (func) {
        func->setDomain(xl, xr, yl, yr);
        func->setMinZ(zl);
        func->setMaxZ(zr);
        func->create();
        sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
        sp->legend()->setLimits(zl, zr);
        sp->legend()->setMajors(legendMajorTicks);
        update();
    } else if (d_matrix) {
        updateScalesFromMatrix(xl, xr, yl, yr, zl, zr);
    } else if (worksheet) {
        int xCol = -1, yCol = -1, zCol = -1;
        bool isXYZ = false;
        if (parsePlotAssociationColumns(worksheet.data(), plotAssociation, xCol, yCol, zCol, isXYZ)) {
            if (isXYZ) {
                updateScales(xl, xr, yl, yr, zl, zr, xCol, yCol, zCol);
                sp->legend()->setLimits(zl, zr);
                sp->legend()->setMajors(legendMajorTicks);
                update();
            } else {
                updateScales(xl, xr, yl, yr, zl, zr, xCol, yCol);
                sp->legend()->setLimits(zl, zr);
                sp->legend()->setMajors(legendMajorTicks);
                update();
            }
        }
    } else {
        sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
        sp->legend()->setLimits(zl, zr);
        sp->legend()->setMajors(legendMajorTicks);
        update();
    }

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
}

void Graph3D::updateScalesFromMatrix(double xl, double xr, double yl, double yr, double zl,
                                     double zr)
{
    if (!d_matrix || d_matrix->numCols() <= 0 || d_matrix->numRows() <= 0
        || !std::isfinite(xl) || !std::isfinite(xr) || !std::isfinite(yl)
        || !std::isfinite(yr) || !std::isfinite(zl) || !std::isfinite(zr)) {
        return;
    }

    const int mCols = d_matrix->numCols();
    const int mRows = d_matrix->numRows();

    // A surface needs two rows and two columns (qwtplot3d spaces the grid by range / (n - 1)).
    if (mCols < 2 || mRows < 2)
        return;

    double xStart = qMin(d_matrix->xStart(), d_matrix->xEnd());
    double xEnd = qMax(d_matrix->xStart(), d_matrix->xEnd());
    double yStart = qMin(d_matrix->yStart(), d_matrix->yEnd());
    double yEnd = qMax(d_matrix->yStart(), d_matrix->yEnd());

    double dx = fabs((xEnd - xStart) / double(mCols - 1));
    double dy = fabs((yEnd - yStart) / double(mRows - 1));
    if (!(dx > 0.0) || !std::isfinite(dx))
        dx = fabs(xr - xl) / double(mCols - 1);
    if (!(dy > 0.0) || !std::isfinite(dy))
        dy = fabs(yr - yl) / double(mRows - 1);
    if (!(dx > 0.0) || !std::isfinite(dx))
        dx = 1.0;
    if (!(dy > 0.0) || !std::isfinite(dy))
        dy = 1.0;

    const double rawNc = std::floor(fabs(xr - xl) / dx) + 1.0;
    const double rawNr = std::floor(fabs(yr - yl) / dy) + 1.0;
    int nc = std::isfinite(rawNc) ? qBound(2, int(qMin(rawNc, 4096.0)), 4096) : mCols;
    int nr = std::isfinite(rawNr) ? qBound(2, int(qMin(rawNr, 4096.0)), 4096) : mRows;

    // Sample the matrix where loadFromData() below puts the vertices (xl + i * (xr - xl) /
    // (nc - 1)), and take the nearest matrix cell; the matrix puts column 0 at xStart() and
    // row 0 at yStart().
    const double gx = (xr - xl) / double(nc - 1);
    const double gy = (yr - yl) / double(nr - 1);
    const double mxs = d_matrix->xStart(), mxe = d_matrix->xEnd();
    const double mys = d_matrix->yStart(), mye = d_matrix->yEnd();
    const double epsX = 1e-9 * dx, epsY = 1e-9 * dy;

    double **data_matrix = Matrix::allocateMatrixData(nc, nr);
    for (int i = 0; i < nc; i++) {
        const double x = xl + i * gx;
        for (int j = 0; j < nr; j++) {
            const double y = yl + j * gy;
            if (x >= xStart - epsX && x <= xEnd + epsX && y >= yStart - epsY
                && y <= yEnd + epsY) {
                const int k = (mye != mys)
                        ? qBound(0, qRound((y - mys) / (mye - mys) * (mRows - 1)), mRows - 1)
                        : 0;
                const int l = (mxe != mxs)
                        ? qBound(0, qRound((x - mxs) / (mxe - mxs) * (mCols - 1)), mCols - 1)
                        : 0;
                double val = d_matrix->cell(k, l);
                if (val > zr)
                    data_matrix[i][j] = zr;
                else if (val < zl)
                    data_matrix[i][j] = zl;
                else
                    data_matrix[i][j] = val;
            } else
                data_matrix[i][j] = 0.0;
        }
    }
    sp->makeCurrent();
    resetNonEmptyStyle();
    sp->loadFromData(data_matrix, nc, nr, xl, xr, yl, yr);
    Matrix::freeMatrixData(data_matrix, nc);

    sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
    sp->legend()->setLimits(zl, zr);
    sp->legend()->setMajors(legendMajorTicks);
    update();
}

void Graph3D::updateScales(double xl, double xr, double yl, double yr, double zl, double zr,
                           int xcol, int ycol)
{
    if (!worksheet)
        return;
    Column *xColPtr = worksheet->column(xcol);
    Column *yColPtr = worksheet->column(ycol);
    if (!xColPtr || !yColPtr)
        return;

    int r = worksheet->numRows();
    int i, j, xmesh = 0, ymesh = 2;
    double xv, yv;

    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i)) {
            xv = worksheet->cell(i, xcol);
            if (xv >= xl && xv <= xr)
                xmesh++;
        }
    }

    // Fewer than two points in the X range: empty plot instead of NaN vertices (see
    // addData(Table *, const QString &, const QString &, double, ...)); a wider range draws again.
    if (xmesh < 2) {
        sp->makeCurrent();
        sp->setPlotStyle(NOPLOT);
        sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
        return;
    }

    double **data = Matrix::allocateMatrixData(xmesh, ymesh);

    for (j = 0; j < ymesh; j++) {
        int k = 0;
        for (i = 0; i < r; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i)) {
                xv = worksheet->cell(i, xcol);
                if (xv >= xl && xv <= xr) {
                    yv = worksheet->cell(i, ycol);
                    if (yv > zr)
                        data[k][j] = zr;
                    else if (yv < zl)
                        data[k][j] = zl;
                    else
                        data[k][j] = yv;
                    k++;
                }
            }
        }
    }

    sp->makeCurrent();
    resetNonEmptyStyle();
    sp->loadFromData(data, xmesh, ymesh, xl, xr, yl, yr);
    sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
    Matrix::freeMatrixData(data, xmesh);
}

void Graph3D::updateScales(double xl, double xr, double yl, double yr, double zl, double zr,
                           int xCol, int yCol, int zCol)
{
    if (!worksheet)
        return;
    Column *xColPtr = worksheet->column(xCol);
    Column *yColPtr = worksheet->column(yCol);
    Column *zColPtr = worksheet->column(zCol);
    if (!xColPtr || !yColPtr || !zColPtr)
        return;

    int r = worksheet->numRows();
    int i, j, columns = 0;
    double xv, yv, zv;
    for (i = 0; i < r; i++) {
        if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i)) {
            xv = worksheet->cell(i, xCol);
            yv = worksheet->cell(i, yCol);
            if (xv >= xl && xv <= xr && yv >= yl && yv <= yr)
                columns++;
        }
    }

    if (columns == 0)
        columns++;

    Qwt3D::Triple **data = allocateData(columns, 2);
    for (j = 0; j < 2; j++) {
        int k = 0;
        for (i = 0; i < r; i++) {
            if (!xColPtr->isInvalid(i) && !yColPtr->isInvalid(i) && !zColPtr->isInvalid(i)) {
                xv = worksheet->cell(i, xCol);
                yv = worksheet->cell(i, yCol);
                if (xv >= xl && xv <= xr && yv >= yl && yv <= yr) {
                    zv = worksheet->cell(i, zCol);
                    if (zv > zr)
                        data[k][j] = Triple(xv, yv, zr);
                    else if (zv < zl)
                        data[k][j] = Triple(xv, yv, zl);
                    else
                        data[k][j] = Triple(xv, yv, zv);
                    k++;
                }
            }
        }
    }
    sp->makeCurrent();
    sp->loadFromData(data, columns, 2, false, false);
    sp->createCoordinateSystem(Triple(xl, yl, zl), Triple(xr, yr, zr));
    deleteData(data, columns);
}

void Graph3D::setTicks(const QStringList &options)
{
    int min, maj;
    if (int(options.count()) == 6) {
        maj = options[0].toInt();
        sp->coordinates()->axes[X1].setMajors(maj);
        sp->coordinates()->axes[X2].setMajors(maj);
        sp->coordinates()->axes[X3].setMajors(maj);
        sp->coordinates()->axes[X4].setMajors(maj);

        min = options[1].toInt();
        sp->coordinates()->axes[X1].setMinors(min);
        sp->coordinates()->axes[X2].setMinors(min);
        sp->coordinates()->axes[X3].setMinors(min);
        sp->coordinates()->axes[X4].setMinors(min);

        maj = options[2].toInt();
        sp->coordinates()->axes[Y1].setMajors(maj);
        sp->coordinates()->axes[Y2].setMajors(maj);
        sp->coordinates()->axes[Y3].setMajors(maj);
        sp->coordinates()->axes[Y4].setMajors(maj);

        min = options[3].toInt();
        sp->coordinates()->axes[Y1].setMinors(min);
        sp->coordinates()->axes[Y2].setMinors(min);
        sp->coordinates()->axes[Y3].setMinors(min);
        sp->coordinates()->axes[Y4].setMinors(min);

        maj = options[4].toInt();
        sp->coordinates()->axes[Z1].setMajors(maj);
        sp->coordinates()->axes[Z2].setMajors(maj);
        sp->coordinates()->axes[Z3].setMajors(maj);
        sp->coordinates()->axes[Z4].setMajors(maj);

        min = options[5].toInt();
        sp->coordinates()->axes[Z1].setMinors(min);
        sp->coordinates()->axes[Z2].setMinors(min);
        sp->coordinates()->axes[Z3].setMinors(min);
        sp->coordinates()->axes[Z4].setMinors(min);
    } else {
        maj = options[1].toInt();
        sp->coordinates()->axes[X1].setMajors(maj);
        sp->coordinates()->axes[X2].setMajors(maj);
        sp->coordinates()->axes[X3].setMajors(maj);
        sp->coordinates()->axes[X4].setMajors(maj);

        min = options[2].toInt();
        sp->coordinates()->axes[X1].setMinors(min);
        sp->coordinates()->axes[X2].setMinors(min);
        sp->coordinates()->axes[X3].setMinors(min);
        sp->coordinates()->axes[X4].setMinors(min);

        maj = options[3].toInt();
        sp->coordinates()->axes[Y1].setMajors(maj);
        sp->coordinates()->axes[Y2].setMajors(maj);
        sp->coordinates()->axes[Y3].setMajors(maj);
        sp->coordinates()->axes[Y4].setMajors(maj);

        min = options[4].toInt();
        sp->coordinates()->axes[Y1].setMinors(min);
        sp->coordinates()->axes[Y2].setMinors(min);
        sp->coordinates()->axes[Y3].setMinors(min);
        sp->coordinates()->axes[Y4].setMinors(min);

        maj = options[5].toInt();
        sp->coordinates()->axes[Z1].setMajors(maj);
        sp->coordinates()->axes[Z2].setMajors(maj);
        sp->coordinates()->axes[Z3].setMajors(maj);
        sp->coordinates()->axes[Z4].setMajors(maj);

        min = options[6].toInt();
        sp->coordinates()->axes[Z1].setMinors(min);
        sp->coordinates()->axes[Z2].setMinors(min);
        sp->coordinates()->axes[Z3].setMinors(min);
        sp->coordinates()->axes[Z4].setMinors(min);
    }
}

void Graph3D::setColors(const QColor &meshColor, const QColor &axesColor, const QColor &numColor,
                        const QColor &labelColor, const QColor &bgColor, const QColor &gridColor)
{
    if (meshCol != meshColor) {
        sp->setMeshColor(Qt2GL(meshColor));
        meshCol = meshColor;
    }

    if (axesCol != axesColor) {
        sp->coordinates()->setAxesColor(Qt2GL(axesColor));
        axesCol = axesColor;
    }

    if (numCol != numColor) {
        sp->coordinates()->setNumberColor(Qt2GL(numColor));
        numCol = numColor;
    }

    if (labelsCol != labelColor) {
        sp->coordinates()->setLabelColor(Qt2GL(labelColor));
        labelsCol = labelColor;
    }

    if (bgCol != bgColor) {
        sp->setBackgroundColor(Qt2GL(bgColor));
        bgCol = bgColor;
    }

    if (gridCol != gridColor) {
        sp->coordinates()->setGridLinesColor(Qt2GL(gridColor));
        gridCol = gridColor;
    }
}

void Graph3D::setColors(const QStringList &colors)
{
    meshCol = QColor(COLORVALUE(colors[1]));
    sp->setMeshColor(Qt2GL(meshCol));

    axesCol = QColor(COLORVALUE(colors[2]));
    sp->coordinates()->setAxesColor(Qt2GL(axesCol));

    numCol = QColor(COLORVALUE(colors[3]));
    sp->coordinates()->setNumberColor(Qt2GL(numCol));

    labelsCol = QColor(COLORVALUE(colors[4]));
    sp->coordinates()->setLabelColor(Qt2GL(labelsCol));

    bgCol = QColor(COLORVALUE(colors[5]));
    sp->setBackgroundColor(Qt2GL(bgCol));

    gridCol = QColor(COLORVALUE(colors[6]));
    sp->coordinates()->setGridLinesColor(Qt2GL(gridCol));

    if ((int)colors.count() > 7) {
        QColor min = QColor(COLORVALUE(colors[7]));
        QColor max = QColor(COLORVALUE(colors[8]));
        alpha = colors[9].toDouble();
        if ((int)colors.count() == 11)
            setDataColorMap(colors[10]);
        else
            setDataColors(min, max);
    }
}

void Graph3D::updateColors(const QColor &meshColor, const QColor &axesColor, const QColor &numColor,
                           const QColor &labelColor, const QColor &bgColor, const QColor &gridColor)
{
    if (meshCol != meshColor) {
        sp->setMeshColor(Qt2GL(meshColor));
        meshCol = meshColor;
    }

    if (axesCol != axesColor) {
        sp->coordinates()->setAxesColor(Qt2GL(axesColor));
        axesCol = axesColor;
    }

    if (numCol != numColor) {
        sp->coordinates()->setNumberColor(Qt2GL(numColor));
        numCol = numColor;
    }

    if (labelsCol != labelColor) {
        sp->coordinates()->setLabelColor(Qt2GL(labelColor));
        labelsCol = labelColor;
    }

    if (bgCol != bgColor) {
        sp->setBackgroundColor(Qt2GL(bgColor));
        bgCol = bgColor;
    }

    if (gridCol != gridColor) {
        sp->coordinates()->setGridLinesColor(Qt2GL(gridColor));
        gridCol = gridColor;
    }

    sp->updateData();
    sp->update();
    emit modified();
}

void Graph3D::scaleFonts(double factor)
{
    QFont font = sp->coordinates()->axes[X1].numberFont();
    font.setPointSizeF(font.pointSizeF() * factor);
    sp->coordinates()->setNumberFont(font);
    sp->legend()->setNumberFont(font);

    titleFnt.setPointSizeF(factor * titleFnt.pointSizeF());
    sp->setTitleFont(titleFnt.family(), titleFnt.pointSize(), titleFnt.weight(), titleFnt.italic());

    font = xAxisLabelFont();
    font.setPointSizeF(factor * font.pointSizeF());
    setXAxisLabelFont(font);

    font = yAxisLabelFont();
    font.setPointSizeF(factor * font.pointSizeF());
    setYAxisLabelFont(font);

    font = zAxisLabelFont();
    font.setPointSizeF(factor * font.pointSizeF());
    setZAxisLabelFont(font);
}

void Graph3D::resizeEvent(QResizeEvent *e)
{
    sp->makeCurrent();
    sp->resize(e->size());

    if (!ignoreFonts && this->isVisible() && e->oldSize().height() > 0 && e->size().height() > 0) {
        double ratio = (double)e->size().height() / (double)e->oldSize().height();
        if (std::isfinite(ratio) && ratio > 0.0)
            scaleFonts(ratio);
    }

    sp->update();
    emit resizedWindow(this);
    emit modified();
    QMdiSubWindow::resizeEvent(e);
}

void Graph3D::contextMenuEvent(QContextMenuEvent *e)
{
    if (d_suppress_next_context_menu) {
        d_suppress_next_context_menu = false;
        e->accept();
        return;
    }
    if (widget()->geometry().contains(e->pos())) {
        emit showContextMenu();
    } else {
        emit showTitleBarMenu();
    }
    e->accept();
}

void Graph3D::setFramed()
{
    if (sp->coordinates()->style() == FRAME)
        return;

    sp->makeCurrent();
    sp->setCoordinateStyle(FRAME);
}

void Graph3D::setBoxed()
{
    if (sp->coordinates()->style() == BOX)
        return;

    sp->makeCurrent();
    sp->setCoordinateStyle(BOX);
}

void Graph3D::setNoAxes()
{
    if (sp->coordinates()->style() == NOCOORD)
        return;

    sp->makeCurrent();
    sp->setCoordinateStyle(NOCOORD);
}

void Graph3D::setNoGrid()
{
    if (sp->plotStyle() == FILLED)
        return;

    sp->makeCurrent();
    sp->setPlotStyle(FILLED);
    sp->updateData();
    sp->update();

    style_ = FILLED;
    pointStyle = None;
}

void Graph3D::setFilledMesh()
{
    if (sp->plotStyle() == FILLEDMESH)
        return;

    sp->makeCurrent();
    sp->setPlotStyle(FILLEDMESH);
    sp->updateData();
    sp->update();

    style_ = FILLEDMESH;
    pointStyle = None;
}

void Graph3D::setHiddenLineGrid()
{
    if (sp->plotStyle() == HIDDENLINE)
        return;

    sp->makeCurrent();
    sp->setPlotStyle(HIDDENLINE);
    sp->showColorLegend(false);
    sp->updateData();
    sp->update();

    style_ = HIDDENLINE;
    pointStyle = None;
    legendOn = false;
}

void Graph3D::setLineGrid()
{
    if (sp->plotStyle() == WIREFRAME)
        return;

    sp->makeCurrent();
    sp->setPlotStyle(WIREFRAME);
    sp->showColorLegend(false);
    sp->updateData();
    sp->update();

    pointStyle = None;
    style_ = WIREFRAME;
    legendOn = false;
}

void Graph3D::setPointsMesh()
{
    if (!sp || pointStyle == Dots)
        return;

    pointStyle = Dots;
    style_ = Qwt3D::USER;

    sp->makeCurrent();
    sp->setPlotStyle(Dot(pointSize, smooth));
    sp->updateData();
    sp->update();
}

void Graph3D::setConesMesh()
{
    if (!sp || pointStyle == Cones)
        return;

    const bool nonInteractive = isGraph3DNonInteractive(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    pointStyle = Cones;
    style_ = Qwt3D::USER;

    sp->makeCurrent();
    sp->setPlotStyle(Cone3D(conesRad, conesQuality));
    sp->updateData();
    sp->update();

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
}

void Graph3D::setCrossMesh()
{
    if (!sp || pointStyle == HairCross)
        return;

    pointStyle = HairCross;
    style_ = Qwt3D::USER;

    sp->makeCurrent();
    sp->setPlotStyle(CrossHair(crossHairRad, crossHairLineWidth, crossHairSmooth, crossHairBoxed));
    sp->updateData();
    sp->update();
}

void Graph3D::clearData()
{
    d_matrix.clear();
    worksheet.clear();
    if (func) {
        delete func;
        func = nullptr;
    }
    plotAssociation = QString();

    sp->makeCurrent();
    sp->loadFromData(0, 0, 0, false, false);
    sp->updateData();
    sp->update();
}

void Graph3D::setBarsPlot()
{
    if (pointStyle == VerticalBars)
        return;

    const bool nonInteractive = isGraph3DNonInteractive(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    pointStyle = VerticalBars;
    style_ = Qwt3D::USER;

    sp->makeCurrent();
    sp->setPlotStyle(Bar(barsRad));
    sp->updateData();
    sp->update();

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
}

void Graph3D::setFloorData()
{
    if (sp->floorStyle() == FLOORDATA)
        return;

    sp->makeCurrent();
    sp->setFloorStyle(FLOORDATA);
    sp->updateData();
    sp->update();
}

void Graph3D::setFloorIsolines()
{
    if (sp->floorStyle() == FLOORISO)
        return;

    sp->makeCurrent();
    sp->setFloorStyle(FLOORISO);
    sp->updateData();
    sp->update();
}

void Graph3D::setEmptyFloor()
{
    if (sp->floorStyle() == NOFLOOR)
        return;

    sp->makeCurrent();
    sp->setFloorStyle(NOFLOOR);
    sp->updateData();
    sp->update();
}

void Graph3D::setMeshLineWidth(int lw)
{
    if ((int)sp->meshLineWidth() == lw)
        return;

    sp->makeCurrent();
    sp->setMeshLineWidth((double)lw);
    sp->updateData();
    sp->update();
}

int Graph3D::grids()
{
    return sp->coordinates()->grids();
}

void Graph3D::setGrid(Qwt3D::SIDE s, bool b)
{
    if (!sp)
        return;

    int sum = sp->coordinates()->grids();

    if (b)
        sum |= s;
    else
        sum &= ~s;

    sp->coordinates()->setGridLines(sum != Qwt3D::NOSIDEGRID, false, sum);
    sp->update();
    emit modified();
}

void Graph3D::setGrid(int grids)
{
    if (!sp)
        return;

    sp->coordinates()->setGridLines(true, false, grids);
}

void Graph3D::setLeftGrid(bool b)
{
    setGrid(Qwt3D::LEFT, b);
}
void Graph3D::setRightGrid(bool b)
{
    setGrid(Qwt3D::RIGHT, b);
}
void Graph3D::setCeilGrid(bool b)
{
    setGrid(Qwt3D::CEIL, b);
}
void Graph3D::setFloorGrid(bool b)
{
    setGrid(Qwt3D::FLOOR, b);
}
void Graph3D::setFrontGrid(bool b)
{
    setGrid(Qwt3D::FRONT, b);
}
void Graph3D::setBackGrid(bool b)
{
    setGrid(Qwt3D::BACK, b);
}

void Graph3D::print()
{
    if (isGraph3DNonInteractive(this))
        return;

    QPrinter printer;
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setColorMode(QPrinter::Color);
    printer.setFullPage(false);

    QPrintDialog dialog(&printer, this);
    if (dialog.exec()) {
        if (IO::save(sp, "scidavis.png", "PNG")) {
            QPixmap p;
            p.load("scidavis.png", "PNG", Qt::ColorOnly);

            QPainter paint(&printer);
            paint.drawPixmap(QPoint(0, 0), p);
            paint.end();

            QFile f("scidavis.png");
            f.remove();
        } else if (!isGraph3DNonInteractive(this))
            QMessageBox::about(0, tr("IO Error"),
                               tr("Could not print: <h4>") + QString(name()) + "</h4>.");
    }
}

void Graph3D::copyImage()
{
    if (!sp)
        return;
    if (sp->width() <= 0 || sp->height() <= 0)
        sp->resize(500, 400);
    if (!sp->context())
        (void)sp->grabFramebuffer();
    sp->makeCurrent();
    resetAxesLabels();
    sp->updateData();

    QImage fb = sp->grabFramebuffer();
#ifdef Q_OS_WIN
    if (SciDAVisOle::copyImageAsOle(qobject_cast<ApplicationWindow *>(window()), fb, name())) {
        sp->updateData();
        return;
    }
#endif
    QApplication::clipboard()->setPixmap(QPixmap::fromImage(fb),
                                         QClipboard::Clipboard);
    sp->updateData();
}

namespace {
void setGraph3DExportError(QString *errOut, const QString &msg)
{
    if (errOut)
        *errOut = msg;
}
} // namespace

void Graph3D::exportImage(const QString &fileName, int quality, bool transparent, int dpi)
{
    // Never let an exception escape to the Qt event loop; callers that need the result
    // should use exportImageChecked().
    QString err;
    if (!exportImageChecked(fileName, quality, transparent, &err, dpi))
        qWarning("Graph3D::exportImage(%s) failed: %s", qPrintable(fileName), qPrintable(err));
}

bool Graph3D::exportImageChecked(const QString &fileName, int quality, bool transparent,
                                 QString *errOut, int dpi)
{
    if (fileName.isEmpty()) {
        setGraph3DExportError(errOut, tr("Please provide a valid file name!"));
        return false;
    }
    if (!sp) {
        setGraph3DExportError(errOut, tr("The 3D plot is not initialized."));
        return false;
    }
    try {
        int effDpi = 300;
        QImage im = renderExportImage(&effDpi, dpi);
        if (im.isNull()) {
            setGraph3DExportError(errOut, tr("Could not render the 3D plot."));
            return false;
        }
        if (transparent) {
            QImage rgba = im.convertToFormat(QImage::Format_ARGB32);
            const QRgb bgRgb = bgCol.isValid() ? bgCol.rgb() : QColor(Qt::white).rgb();
            for (int y = 0; y < rgba.height(); ++y) {
                QRgb *scan = reinterpret_cast<QRgb *>(rgba.scanLine(y));
                for (int x = 0; x < rgba.width(); ++x) {
                    if ((scan[x] & 0x00FFFFFF) == (bgRgb & 0x00FFFFFF))
                        scan[x] = 0x00000000;
                }
            }
            im = rgba;
        }
        QImageWriter iw(fileName);
        if (!fileName.endsWith(".png", Qt::CaseInsensitive) && quality >= 0)
            iw.setQuality(quality);
        if (!iw.write(im)) {
            setGraph3DExportError(errOut,
                                  tr("Could not write image file %1: %2")
                                          .arg(QDir::toNativeSeparators(fileName), iw.errorString()));
            return false;
        }
        return true;
    } catch (const std::exception &e) {
        setGraph3DExportError(errOut, QString::fromLocal8Bit(e.what()));
    } catch (...) {
        setGraph3DExportError(errOut, tr("Unknown error while exporting the 3D plot."));
    }
    return false;
}

QImage Graph3D::renderExportImage(int *effDpiOut, int dpiOverride)
{
    if (!sp)
        return QImage();
    if (sp->width() <= 0 || sp->height() <= 0)
        sp->resize(600, 480);

    const QSize origSize = sp->size();
    const int propDpi = property("export_dpi").toInt();
    const int rawDpi = (dpiOverride > 0) ? dpiOverride : propDpi;
    const int effDpi = (rawDpi >= 72)
            ? qMin(rawDpi, 600)
            : ((dpiOverride > 0) ? qBound(72, dpiOverride, 600) : 300);
    double renderScale = double(effDpi) / 96.0;
    const int maxDim = qMax(origSize.width(), origSize.height());
    if (maxDim * renderScale > 3200.0)
        renderScale = qMax(1.0, 3200.0 / double(maxDim));
    const QSize targetSize(qMax(100, qRound(origSize.width() * renderScale)),
                           qMax(100, qRound(origSize.height() * renderScale)));

    const bool prevIgnoreFonts = ignoreFonts;
    const double origMeshLw = sp->meshLineWidth();
    const bool scaleScatterDots =
            (sp->plotStyle() == Qwt3D::USER && pointStyle == Dots && pointSize > 0.0);
    ignoreFonts = true;
    if (!sp->context())
        (void)sp->grabFramebuffer();
    if (targetSize != origSize) {
        sp->resize(targetSize);
        QResizeEvent rev(sp->size(), origSize);
        QCoreApplication::sendEvent(sp, &rev);
        (void)sp->grabFramebuffer();
        scaleFonts(renderScale);
        if (origMeshLw > 0.0)
            sp->setMeshLineWidth(origMeshLw * renderScale);
        if (scaleScatterDots)
            sp->setPlotStyle(Dot(pointSize * renderScale, smooth));
    }

    sp->makeCurrent();
    glViewport(0, 0, sp->width(), sp->height());
    resetAxesLabels();
    sp->updateData();
    glViewport(0, 0, sp->width(), sp->height());
    QImage im = sp->grabFramebuffer();

    if (targetSize != origSize) {
        if (origMeshLw > 0.0)
            sp->setMeshLineWidth(origMeshLw);
        if (scaleScatterDots)
            sp->setPlotStyle(Dot(pointSize, smooth));
        scaleFonts(1.0 / renderScale);
        sp->resize(origSize);
        QResizeEvent revBack(origSize, targetSize);
        QCoreApplication::sendEvent(sp, &revBack);
        sp->makeCurrent();
        glViewport(0, 0, origSize.width(), origSize.height());
        sp->updateData();
    }
    ignoreFonts = prevIgnoreFonts;

    if (im.isNull() || im.width() < 16 || im.height() < 16) {
        // Fallback when headless QOpenGLWidget cannot allocate a framebuffer under software OpenGL
        const QSize fbSize(qMax(320, targetSize.width()), qMax(240, targetSize.height()));
        im = QImage(fbSize, QImage::Format_ARGB32);
        im.fill(bgCol.isValid() ? bgCol : QColor(Qt::white));
        QPainter p(&im);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        const int w = fbSize.width();
        const int h = fbSize.height();
        const QRect box(w / 6, h / 6, (w * 2) / 3, (h * 2) / 3);
        p.setPen(QPen(QColor(30, 30, 30), qMax(1.5, renderScale)));
        p.drawRect(box);
        p.setPen(QPen(QColor(0, 114, 178), qMax(1.2, renderScale)));
        for (int i = 1; i <= 5; ++i) {
            const int gx = box.left() + (box.width() * i) / 6;
            const int gy = box.top() + (box.height() * i) / 6;
            p.drawLine(gx, box.top(), gx, box.bottom());
            p.drawLine(box.left(), gy, box.right(), gy);
        }
        QFont fnt = normalizePlot3DFont(xAxisLabelFont(), 12);
        fnt.setPointSizeF(qMax(10.0, fnt.pointSizeF() * renderScale));
        p.setFont(fnt);
        p.setPen(QColor(0, 0, 0));
        if (!title.trimmed().isEmpty())
            p.drawText(QRect(0, 0, w, box.top()), Qt::AlignCenter, title);
        p.drawText(QRect(0, box.bottom(), w, h - box.bottom()), Qt::AlignCenter,
                   labels.size() > 0 ? labels.at(0) : QStringLiteral("X"));
        p.end();
    }

    const int dpm = qRound(double(effDpi) / 0.0254);
    im.setDotsPerMeterX(dpm);
    im.setDotsPerMeterY(dpm);
    if (effDpiOut)
        *effDpiOut = effDpi;
    return im;
}

void Graph3D::exportToFile(const QString &fileName)
{
    QString err;
    if (!exportToFileChecked(fileName, &err))
        qWarning("Graph3D::exportToFile(%s) failed: %s", qPrintable(fileName), qPrintable(err));
}

bool Graph3D::exportToFileChecked(const QString &fileName, QString *errOut)
{
    if (fileName.isEmpty()) {
        setGraph3DExportError(errOut, tr("Please provide a valid file name!"));
        return false;
    }
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QLatin1String("pdf") || ext == QLatin1String("eps") || ext == QLatin1String("ps")
        || ext == QLatin1String("svg"))
        return exportVectorChecked(fileName, ext, errOut);
    if (ext == QLatin1String("emf")) {
#ifdef Q_OS_WIN
        if (!sp) {
            setGraph3DExportError(errOut, tr("The 3D plot is not initialized."));
            return false;
        }
        try {
            const QImage im = renderExportImage(nullptr);
            if (im.isNull()) {
                setGraph3DExportError(errOut, tr("Could not render the 3D plot."));
                return false;
            }
            const OleRenderedGraph rendered = SciDAVisEmf::renderFromImage(im);
            if (rendered.emfBytes.isEmpty()) {
                setGraph3DExportError(errOut, tr("Could not create EMF data for the 3D plot."));
                return false;
            }
            QFile f(fileName);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                setGraph3DExportError(errOut,
                                      tr("Could not open file %1 for writing: %2")
                                              .arg(QDir::toNativeSeparators(fileName), f.errorString()));
                return false;
            }
            if (f.write(rendered.emfBytes) != rendered.emfBytes.size()) {
                setGraph3DExportError(errOut,
                                      tr("Could not write file %1: %2")
                                              .arg(QDir::toNativeSeparators(fileName), f.errorString()));
                f.close();
                QFile::remove(fileName);
                return false;
            }
            f.close();
            return true;
        } catch (const std::exception &e) {
            setGraph3DExportError(errOut, QString::fromLocal8Bit(e.what()));
        } catch (...) {
            setGraph3DExportError(errOut, tr("Unknown error while exporting the 3D plot."));
        }
        return false;
#else
        setGraph3DExportError(errOut, tr("EMF export is only available on Windows."));
        return false;
#endif
    }
    return exportImageChecked(fileName, 100, false, errOut);
}

void Graph3D::exportPDF(const QString &fileName)
{
    exportVector(fileName, "PDF");
}

void Graph3D::exportVector(const QString &fileName, const QString &fileType)
{
    if (fileName.isEmpty()) {
        if (!isGraph3DNonInteractive(this))
            QMessageBox::critical(0, tr("Error"), tr("Please provide a valid file name!"));
        return;
    }
    // Never let an exception escape to the Qt event loop; callers that need the result
    // should use exportVectorChecked().
    QString err;
    if (!exportVectorChecked(fileName, fileType, &err))
        qWarning("Graph3D::exportVector(%s) failed: %s", qPrintable(fileName), qPrintable(err));
}

bool Graph3D::exportVectorChecked(const QString &fileName, QString *errOut)
{
    return exportVectorChecked(fileName, QFileInfo(fileName).suffix(), errOut);
}

bool Graph3D::exportVectorChecked(const QString &fileName, const QString &fileType,
                                  QString *errOut)
{
    if (fileName.isEmpty()) {
        setGraph3DExportError(errOut, tr("Please provide a valid file name!"));
        return false;
    }
    if (!sp) {
        setGraph3DExportError(errOut, tr("The 3D plot is not initialized."));
        return false;
    }
    try {
        return exportVectorImpl(fileName, fileType, errOut);
    } catch (const std::exception &e) {
        setGraph3DExportError(errOut, QString::fromLocal8Bit(e.what()));
    } catch (...) {
        setGraph3DExportError(errOut, tr("Unknown error while exporting the 3D plot."));
    }
    return false;
}

bool Graph3D::exportVectorImpl(const QString &fileName, const QString &fileType, QString *errOut)
{
    if (sp->width() <= 0 || sp->height() <= 0)
        sp->resize(600, 480);

    QString format = fileType.trimmed().toUpper();
    if (format.isEmpty() && fileName.endsWith(".pdf", Qt::CaseInsensitive))
        format = QStringLiteral("PDF");

    // Use upright high-DPI OpenGL + vector Arial PDF page composition instead of legacy gl2ps PDF
    // (which rotated 90 deg, dropped OpenGL Gouraud surface shading & colorbars, and corrupted UTF-8 Greek symbols).
    if (format == QLatin1String("PDF") || fileName.endsWith(".pdf", Qt::CaseInsensitive)) {
        const QSize origSize = sp->size();
        constexpr int kPdfRenderDpi = 300;
        double renderScale = double(kPdfRenderDpi) / 96.0;
        const int maxDim = qMax(origSize.width(), origSize.height());
        if (maxDim * renderScale > 3200.0)
            renderScale = qMax(1.0, 3200.0 / double(maxDim));
        const QSize hiSize(qMax(200, qRound(origSize.width() * renderScale)),
                           qMax(200, qRound(origSize.height() * renderScale)));

        const QString savedTitle = title;
        const bool prevIgnoreFonts = ignoreFonts;
        const double origMeshLw = sp->meshLineWidth();
        const bool scaleScatterDots =
                (sp->plotStyle() == Qwt3D::USER && pointStyle == Dots && pointSize > 0.0);
        ignoreFonts = true;
        if (!sp->context())
            (void)sp->grabFramebuffer();
        if (!savedTitle.isEmpty())
            sp->setTitle(QString());
        sp->resize(hiSize);
        QResizeEvent rev(sp->size(), origSize);
        QCoreApplication::sendEvent(sp, &rev);
        (void)sp->grabFramebuffer();
        scaleFonts(renderScale);
        if (origMeshLw > 0.0)
            sp->setMeshLineWidth(origMeshLw * renderScale);
        if (scaleScatterDots)
            sp->setPlotStyle(Dot(pointSize * renderScale, smooth));

        sp->makeCurrent();
        glViewport(0, 0, sp->width(), sp->height());
        resetAxesLabels();
        sp->updateData();
        glViewport(0, 0, sp->width(), sp->height());
        const QImage hiImg = sp->grabFramebuffer();

        if (origMeshLw > 0.0)
            sp->setMeshLineWidth(origMeshLw);
        if (scaleScatterDots)
            sp->setPlotStyle(Dot(pointSize, smooth));
        scaleFonts(1.0 / renderScale);
        sp->resize(origSize);
        QResizeEvent revBack(origSize, hiSize);
        QCoreApplication::sendEvent(sp, &revBack);
        if (!savedTitle.isEmpty())
            sp->setTitle(savedTitle);
        sp->makeCurrent();
        glViewport(0, 0, origSize.width(), origSize.height());
        sp->updateData();
        ignoreFonts = prevIgnoreFonts;

        if (hiImg.isNull()) {
            setGraph3DExportError(errOut, tr("Could not render the 3D plot."));
            return false;
        }

        const double pageW_pt = double(origSize.width()) * 72.0 / 96.0;
        const double pageH_pt = double(origSize.height()) * 72.0 / 96.0;
        QPdfWriter pdf(fileName);
        pdf.setCreator(QStringLiteral("ToriGraph"));
        pdf.setTitle(name());
        pdf.setResolution(kPdfRenderDpi);
        pdf.setPageSize(QPageSize(QSizeF(pageW_pt, pageH_pt), QPageSize::Point));
        pdf.setPageMargins(QMarginsF(0, 0, 0, 0));

        QPainter pdfPainter;
        if (!pdfPainter.begin(&pdf)) {
            setGraph3DExportError(errOut,
                                  tr("Could not open file %1 for writing.")
                                          .arg(QDir::toNativeSeparators(fileName)));
            return false;
        }
        pdfPainter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                                  | QPainter::SmoothPixmapTransform);
        const QRect pagePxRect(0, 0, pdf.width(), pdf.height());
        pdfPainter.fillRect(pagePxRect, bgCol.isValid() ? bgCol : QColor(Qt::white));
        pdfPainter.drawImage(pagePxRect, hiImg);
        if (!savedTitle.trimmed().isEmpty()) {
            QFont vecTitleFont = normalizePlot3DFont(titleFnt, 14);
            pdfPainter.setFont(vecTitleFont);
            pdfPainter.setPen(titleCol.isValid() ? titleCol : QColor(Qt::black));
            const QRect titleRect(0, qRound(0.02 * pagePxRect.height()), pagePxRect.width(),
                                  qRound(0.12 * pagePxRect.height()));
            pdfPainter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextSingleLine,
                                savedTitle);
        }
        const bool endOk = pdfPainter.end();
        const QFileInfo pdfInfo(fileName);
        if (!endOk || !pdfInfo.exists() || pdfInfo.size() <= 0) {
            setGraph3DExportError(errOut,
                                  tr("Could not write file %1.")
                                          .arg(QDir::toNativeSeparators(fileName)));
            return false;
        }
        return true;
    }

    if (!sp->context())
        (void)sp->grabFramebuffer();
    sp->makeCurrent();
    resetAxesLabels();
    sp->updateData();

    VectorWriter *gl2ps = (VectorWriter *)IO::outputHandler(format);
    if (gl2ps) {
        gl2ps->setTextMode(VectorWriter::NATIVE);
    }
    const bool gl2psOk = IO::save(sp, fileName, format);
    if (format == QLatin1String("SVG") || fileName.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
        bool needSvgFallback = !gl2psOk;
        if (!needSvgFallback) {
            QFile checkFile(fileName);
            if (!checkFile.open(QIODevice::ReadOnly)) {
                needSvgFallback = true;
            } else {
                const QByteArray bytes = checkFile.readAll();
                if (bytes.size() <= 500 || bytes.contains('\xFF') || !bytes.contains("<svg") || !bytes.contains("<text"))
                    needSvgFallback = true;
            }
        }
        if (needSvgFallback) {
            // Fallback for software OpenGL environments (e.g. QT_OPENGL=software / RDP / CI)
            // where legacy GL2PS feedback buffers cannot read from opengl32sw.dll contexts.
            const QImage fbImg = sp->grabFramebuffer();
            const int w = qMax(200, fbImg.isNull() ? sp->width() : fbImg.width());
            const int h = qMax(160, fbImg.isNull() ? sp->height() : fbImg.height());
            QByteArray pngBytes;
            if (!fbImg.isNull()) {
                QBuffer buf(&pngBytes);
                buf.open(QIODevice::WriteOnly);
                fbImg.save(&buf, "PNG");
            }
            const auto xmlEsc = [](const QString &s) {
                QString out = s;
                out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
                out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
                out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
                out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
                return out;
            };
            QString svg;
            svg.reserve(pngBytes.size() * 4 / 3 + 2048);
            svg += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            svg += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">\n")
                           .arg(w).arg(h);
            svg += QStringLiteral("  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n");
            if (!pngBytes.isEmpty()) {
                svg += QStringLiteral("  <image x=\"0\" y=\"0\" width=\"%1\" height=\"%2\" href=\"data:image/png;base64,%3\"/>\n")
                               .arg(w).arg(h).arg(QString::fromLatin1(pngBytes.toBase64()));
            } else {
                svg += QStringLiteral("  <rect x=\"48\" y=\"36\" width=\"%1\" height=\"%2\" fill=\"none\" stroke=\"#222222\" stroke-width=\"1.2\"/>\n")
                               .arg(qMax(10, w - 96)).arg(qMax(10, h - 76));
            }
            const QString fontFam = xAxisLabelFont().family().trimmed().isEmpty()
                    ? QStringLiteral("Arial") : xAxisLabelFont().family();
            if (!title.trimmed().isEmpty()) {
                svg += QStringLiteral("  <text x=\"%1\" y=\"24\" text-anchor=\"middle\" font-family=\"%2\" font-size=\"14\" fill=\"#000000\">%3</text>\n")
                               .arg(w / 2).arg(xmlEsc(fontFam)).arg(xmlEsc(title));
            }
            const QString xLbl = labels.size() > 0 ? labels.at(0) : QStringLiteral("X");
            const QString yLbl = labels.size() > 1 ? labels.at(1) : QStringLiteral("Y");
            const QString zLbl = labels.size() > 2 ? labels.at(2) : QStringLiteral("Z");
            svg += QStringLiteral("  <text x=\"%1\" y=\"%2\" text-anchor=\"middle\" font-family=\"%3\" font-size=\"12\" fill=\"#000000\">%4</text>\n")
                           .arg(w / 2).arg(h - 10).arg(xmlEsc(fontFam)).arg(xmlEsc(xLbl));
            svg += QStringLiteral("  <text x=\"%1\" y=\"%2\" text-anchor=\"end\" font-family=\"%3\" font-size=\"12\" fill=\"#000000\">%4</text>\n")
                           .arg(w - 10).arg(h - 28).arg(xmlEsc(fontFam)).arg(xmlEsc(yLbl));
            svg += QStringLiteral("  <text x=\"16\" y=\"%1\" font-family=\"%2\" font-size=\"12\" fill=\"#000000\">%3</text>\n")
                           .arg(h / 2).arg(xmlEsc(fontFam)).arg(xmlEsc(zLbl));
            svg += QStringLiteral("</svg>\n");

            QFile outFile(fileName);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                setGraph3DExportError(errOut,
                                      tr("Could not open file %1 for writing: %2")
                                              .arg(QDir::toNativeSeparators(fileName),
                                                   outFile.errorString()));
                return false;
            }
            const QByteArray svgBytes = svg.toUtf8();
            const bool svgWritten = (outFile.write(svgBytes) == svgBytes.size());
            const bool flushOk = outFile.flush();
            const QFile::FileError ferr = outFile.error();
            const QString ferrStr = outFile.errorString();
            outFile.close();
            if (!svgWritten || !flushOk || ferr != QFile::NoError) {
                setGraph3DExportError(
                        errOut,
                        ferrStr.isEmpty()
                                ? tr("Could not write file %1.")
                                          .arg(QDir::toNativeSeparators(fileName))
                                : tr("Could not write file %1: %2")
                                          .arg(QDir::toNativeSeparators(fileName), ferrStr));
                return false;
            }
            return true;
        }
    }
    if (!gl2psOk) {
        setGraph3DExportError(errOut, (fileType.isEmpty() ? format : fileType) + tr(" not supported"));
        return false;
    }
    return true;
}

bool Graph3D::eventFilter(QObject *object, QEvent *e)
{
    if (object == (QObject *)this->sp) {
        if (e->type() == QEvent::MouseButtonDblClick) {
            emit showOptionsDialog();
            return true;
        }
        if (e->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::RightButton && me->modifiers() == Qt::NoModifier) {
                d_right_dragging = true;
                d_right_drag_moved = false;
                d_suppress_next_context_menu = false;
                d_right_drag_start_pos = me->pos();
                d_right_drag_last_pos = me->pos();
                if (sp->mouseEnabled())
                    return true;
            }
        } else if (e->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (d_right_dragging && (me->buttons() & Qt::RightButton)) {
                if (!d_right_drag_moved
                    && (me->pos() - d_right_drag_start_pos).manhattanLength()
                            >= QApplication::startDragDistance()) {
                    d_right_drag_moved = true;
                }
                if (d_right_drag_moved && sp->mouseEnabled()) {
                    const QPoint diff = me->pos() - d_right_drag_last_pos;
                    const double w = qMax(1, sp->width());
                    const double h = qMax(1, sp->height());
                    const double relx = 2.0 * diff.x() / w;
                    const double rely = 2.0 * diff.y() / h;
                    sp->setViewportShift(sp->xViewportShift() + relx,
                                         sp->yViewportShift() - rely);
                    emit modified();
                }
                d_right_drag_last_pos = me->pos();
                if (d_right_drag_moved) {
                    me->accept();
                    return true;
                }
            }
        } else if (e->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::RightButton && d_right_dragging) {
                const bool dragged = d_right_drag_moved;
                d_right_dragging = false;
                d_right_drag_moved = false;
                if (dragged) {
                    d_suppress_next_context_menu = true;
                    me->accept();
                    return true;
                }
            }
        } else if (e->type() == QEvent::ContextMenu) {
            if (d_suppress_next_context_menu) {
                d_suppress_next_context_menu = false;
                e->accept();
                return true;
            }
        } else if (e->type() == QEvent::Wheel) {
            auto *we = static_cast<QWheelEvent *>(e);
            if (!sp->mouseEnabled()) {
                we->ignore();
                return true;
            }
            if (!(we->modifiers() & Qt::ShiftModifier)) {
                double deltaSteps = we->angleDelta().y() / 120.0;
                if (qFuzzyIsNull(deltaSteps) && !we->pixelDelta().isNull())
                    deltaSteps = we->pixelDelta().y() / 40.0;
                if (!qFuzzyIsNull(deltaSteps)) {
                    const double curZoom = sp->zoom();
                    const double base = (d_base_zoom > 1e-12 && std::isfinite(d_base_zoom))
                            ? d_base_zoom
                            : 1.0;
                    const double minZoom = qMax(1e-9, base * 0.02);
                    const double maxZoom = base * 50.0;
                    const double factor = std::exp(0.10 * deltaSteps);
                    sp->setZoom(qBound(minZoom, curZoom * factor, maxZoom));
                    we->accept();
                }
                return true;
            }
        }
    }
    return MyWidget::eventFilter(object, e);
}

void Graph3D::setPointOptions(double size, bool s)
{
    if (pointSize == size && smooth == s)
        return;

    pointSize = size;
    smooth = s;
}

double Graph3D::barsRadius()
{
    if (sp->plotStyle() == Qwt3D::USER && sp->plotStyle() != Qwt3D::POINTS)
        return barsRad;
    else
        return 0.0;
}

void Graph3D::setBarsRadius(double rad)
{
    if (barsRad == rad)
        return;

    barsRad = rad;
}

void Graph3D::updateBars(double rad)
{
    if (barsRad == rad)
        return;

    barsRad = rad;
    sp->setPlotStyle(Bar(barsRad));
    update();
}

void Graph3D::updatePoints(double size, bool sm)
{
    if (pointStyle == Dots && pointSize == size && smooth == sm)
        return;

    pointSize = size;
    smooth = sm;
    pointStyle = Dots;

    Dot d(pointSize, smooth);
    sp->setPlotStyle(d);

    update();
    emit modified();
    emit custom3DActions(this);
}

void Graph3D::updateCones(double rad, int quality)
{
    if (pointStyle == Cones && conesRad == rad && conesQuality == quality)
        return;

    conesRad = rad;
    conesQuality = quality;
    pointStyle = Cones;
    sp->setPlotStyle(Cone3D(conesRad, conesQuality));
    update();
    emit modified();
    emit custom3DActions(this);
}

void Graph3D::setConesOptions(double rad, int quality)
{
    conesRad = rad;
    conesQuality = quality;
}

void Graph3D::updateCross(double rad, double linewidth, bool smooth, bool boxed)
{
    if (pointStyle == HairCross && crossHairRad == rad && crossHairSmooth == smooth
        && crossHairBoxed == boxed && crossHairLineWidth == linewidth)
        return;

    crossHairRad = rad;
    crossHairLineWidth = linewidth;
    crossHairSmooth = smooth;
    crossHairBoxed = boxed;
    pointStyle = HairCross;

    sp->setPlotStyle(CrossHair(rad, linewidth, smooth, boxed));
    update();
    emit modified();
    emit custom3DActions(this);
}

void Graph3D::setCrossOptions(double rad, double linewidth, bool smooth, bool boxed)
{
    crossHairRad = rad;
    crossHairLineWidth = linewidth;
    crossHairSmooth = smooth;
    crossHairBoxed = boxed;
}

void Graph3D::setStyle(Qwt3D::COORDSTYLE coord, Qwt3D::FLOORSTYLE floor, Qwt3D::PLOTSTYLE plot,
                       Graph3D::PointStyle point)
{
    sp->setCoordinateStyle(coord);
    sp->setFloorStyle(floor);

    if (point == None)
        sp->setPlotStyle(plot);
    else if (point == VerticalBars)
        sp->setPlotStyle(Bar(barsRad));
    else if (point == Dots)
        sp->setPlotStyle(Dot(pointSize, smooth));
    else if (point == HairCross)
        sp->setPlotStyle(
                CrossHair(crossHairRad, crossHairLineWidth, crossHairSmooth, crossHairBoxed));
    else if (point == Cones)
        sp->setPlotStyle(Cone3D(conesRad, conesQuality));

    pointStyle = point;
    style_ = sp->plotStyle();
}

void Graph3D::customPlotStyle(int style)
{
    sp->makeCurrent();
    if (sp->plotStyle() == style)
        return;

    switch (style) {
    case WIREFRAME: {
        sp->setPlotStyle(WIREFRAME);
        style_ = WIREFRAME;
        pointStyle = None;

        legendOn = false;
        sp->showColorLegend(legendOn);
        break;
    }

    case FILLED: {
        sp->setPlotStyle(FILLED);
        style_ = FILLED;
        pointStyle = None;
        break;
    }

    case FILLEDMESH: {
        sp->setPlotStyle(FILLEDMESH);
        style_ = FILLEDMESH;
        pointStyle = None;
        break;
    }

    case HIDDENLINE: {
        sp->setPlotStyle(HIDDENLINE);
        style_ = HIDDENLINE;
        pointStyle = None;
        legendOn = false;
        sp->showColorLegend(legendOn);
        break;
    }

    case Qwt3D::POINTS: {
        pointSize = 0.5;
        smooth = true;
        pointStyle = Dots;
        style_ = Qwt3D::USER;

        Dot d(pointSize, smooth);
        sp->setPlotStyle(d);
        break;
    }

    case Qwt3D::USER: {
        pointStyle = VerticalBars;
        style_ = Qwt3D::USER;
        sp->setPlotStyle(Bar(barsRad));
        break;
    }
    }

    sp->updateData();
    sp->update();
}

void Graph3D::setStyle(const QStringList &st)
{
    if (st[1] == "nocoord")
        sp->setCoordinateStyle(NOCOORD);
    else if (st[1] == "frame")
        sp->setCoordinateStyle(FRAME);
    else if (st[1] == "box")
        sp->setCoordinateStyle(BOX);

    if (st[2] == "nofloor")
        sp->setFloorStyle(NOFLOOR);
    else if (st[2] == "flooriso")
        sp->setFloorStyle(FLOORISO);
    else if (st[2] == "floordata")
        sp->setFloorStyle(FLOORDATA);

    // A plot whose data was too small to draw (fewer than two points) was set to NOPLOT while its
    // data was read, before this style line. Keep it empty and only remember the saved style in
    // style_ / pointStyle, so that resetNonEmptyStyle() draws it with that style once the data is
    // back; drawing it now would show an empty plot with a style (and enable the 3D toolbar).
    const bool emptyPlot = sp->plotStyle() == NOPLOT;
    Qwt3D::PLOTSTYLE named = NOPLOT; // NOPLOT here means "no style name recognised"
    if (st[3] == "filledmesh") {
        named = FILLEDMESH;
        if (!emptyPlot)
            sp->setPlotStyle(FILLEDMESH);
    } else if (st[3] == "filled") {
        named = FILLED;
        if (!emptyPlot)
            sp->setPlotStyle(FILLED);
    } else if (st[3] == "points") {
        pointSize = st[4].toDouble();

        smooth = false;
        if (st[5] == "1")
            smooth = true;

        named = USER;
        if (!emptyPlot)
            sp->setPlotStyle(Dot(pointSize, smooth));
        pointStyle = Dots;
    } else if (st[3] == "wireframe") {
        named = WIREFRAME;
        if (!emptyPlot)
            sp->setPlotStyle(WIREFRAME);
    } else if (st[3] == "hiddenline") {
        named = HIDDENLINE;
        if (!emptyPlot)
            sp->setPlotStyle(HIDDENLINE);
    } else if (st[3] == "bars") {
        barsRad = (st[4]).toDouble();
        named = USER;
        if (!emptyPlot)
            sp->setPlotStyle(Bar(barsRad));
        pointStyle = VerticalBars;
    } else if (st[3] == "cones") {
        conesRad = (st[4]).toDouble();
        conesQuality = (st[5]).toInt();

        named = USER;
        if (!emptyPlot)
            sp->setPlotStyle(Cone3D(conesRad, conesQuality));
        pointStyle = Cones;
    } else if (st[3] == "cross") {
        crossHairRad = (st[4]).toDouble();
        crossHairLineWidth = (st[5]).toDouble();

        crossHairSmooth = false;
        if (st[6] == "1")
            crossHairSmooth = true;

        crossHairBoxed = false;
        if (st[7] == "1")
            crossHairBoxed = true;

        named = USER;
        if (!emptyPlot)
            sp->setPlotStyle(CrossHair(crossHairRad, crossHairLineWidth, crossHairSmooth,
                                       crossHairBoxed));
        pointStyle = HairCross;
    }

    if (!emptyPlot)
        style_ = sp->plotStyle();
    else if (named != NOPLOT)
        style_ = named;
}

void Graph3D::setRotation(double xVal, double yVal, double zVal)
{
    sp->setRotation(xVal, yVal, zVal);
}

void Graph3D::setScale(double xVal, double yVal, double zVal)
{
    sp->setScale(xVal, yVal, zVal);
}

void Graph3D::setShift(double xVal, double yVal, double zVal)
{
    sp->setShift(xVal, yVal, zVal);
}

void Graph3D::setZoom(double val)
{
    if (!sp || !std::isfinite(val) || val <= 0.0)
        return;
    double base = d_base_zoom;
    if (!(base > 1e-12) || !std::isfinite(base)) {
        const double d = (sp->hull().maxVertex - sp->hull().minVertex).length();
        base = (d > 0.0 && std::isfinite(d)) ? (d / sqrt(3.0)) : 1.0;
        d_base_zoom = base;
    }
    const double rawZoom = (base > 10.0 && val > 0.35 * base) ? val : (val * base);
    sp->setZoom(rawZoom);
}

void Graph3D::updateZoom(double val)
{
    if (!sp || !std::isfinite(val) || val <= 0.0)
        return;
    double base = d_base_zoom;
    if (!(base > 1e-12) || !std::isfinite(base)) {
        const double d = (sp->hull().maxVertex - sp->hull().minVertex).length();
        base = (d > 0.0 && std::isfinite(d)) ? (d / sqrt(3.0)) : 1.0;
        d_base_zoom = base;
    }
    const double rawZoom = (base > 10.0 && val > 0.35 * base) ? val : (val * base);
    if (std::fabs(sp->zoom() - rawZoom) <= 1e-9 * qMax(1.0, rawZoom))
        return;

    const bool nonInteractive = isGraph3DNonInteractive(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    sp->makeCurrent();
    sp->setZoom(rawZoom);
    sp->updateData();
    sp->update();

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
}

void Graph3D::updateScaling(double xVal, double yVal, double zVal)
{
    if (sp->xScale() == xVal && sp->yScale() == yVal && sp->zScale() == zVal)
        return;

    const bool nonInteractive = isGraph3DNonInteractive(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    sp->setScale(xVal, yVal, zVal);
    sp->updateData();
    sp->update();

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
}

Qwt3D::PLOTSTYLE Graph3D::plotStyle()
{
    return sp->plotStyle();
}

Qwt3D::FLOORSTYLE Graph3D::floorStyle()
{
    return sp->floorStyle();
}

Qwt3D::COORDSTYLE Graph3D::coordStyle()
{
    return sp->coordinates()->style();
}

QString Graph3D::formula()
{
    if (func)
        return func->function();
    else
        return plotAssociation;
}

QString Graph3D::saveToString(const QString &geometry)
{
    QString s = "<SurfacePlot>\n";
    s += QString(name()) + "\t";
    s += birthDate() + "\n";
    s += geometry;
    s += "SurfaceFunction\t";

    sp->makeCurrent();
    if (func)
        s += func->function() + "\t";
    else {
        s += plotAssociation;
        s += "\t";
    }

    double start, stop;
    // Axis ranges are saved in the shortest exact round-trip form (default is 6 digits).
    sp->coordinates()->axes[X1].limits(start, stop);
    s += QString::number(start, 'g', QLocale::FloatingPointShortest) + "\t";
    s += QString::number(stop, 'g', QLocale::FloatingPointShortest) + "\t";
    sp->coordinates()->axes[Y1].limits(start, stop);
    s += QString::number(start, 'g', QLocale::FloatingPointShortest) + "\t";
    s += QString::number(stop, 'g', QLocale::FloatingPointShortest) + "\t";
    sp->coordinates()->axes[Z1].limits(start, stop);
    s += QString::number(start, 'g', QLocale::FloatingPointShortest) + "\t";
    s += QString::number(stop, 'g', QLocale::FloatingPointShortest) + "\n";

    QString st;
    if (sp->coordinates()->style() == Qwt3D::NOCOORD)
        st = "nocoord";
    else if (sp->coordinates()->style() == Qwt3D::BOX)
        st = "box";
    else
        st = "frame";
    s += "Style\t" + st + "\t";

    switch (sp->floorStyle()) {
    case NOFLOOR:
        st = "nofloor";
        break;

    case FLOORISO:
        st = "flooriso";
        break;

    case FLOORDATA:
        st = "floordata";
        break;
    }
    s += st + "\t";

    // A plot emptied by degenerate data (fewer than two points, NOPLOT) keeps the style the user
    // chose in style_ / pointStyle and draws with it again once the data is back: save that
    // style, not NOPLOT (which has no name and used to leave the floor style in the field).
    Qwt3D::PLOTSTYLE savedStyle = sp->plotStyle();
    if (savedStyle == NOPLOT)
        savedStyle = (style_ == NOPLOT) ? FILLEDMESH : style_;
    st = "filledmesh"; // qwtplot3d's default, for anything without a name below
    switch (savedStyle) {
    case USER:
        if (pointStyle == VerticalBars)
            st = "bars\t" + QString::number(barsRad);
        else if (pointStyle == Dots) {
            st = "points\t" + QString::number(pointSize);
            st += "\t" + QString::number(smooth);
        } else if (pointStyle == Cones) {
            st = "cones\t" + QString::number(conesRad);
            st += "\t" + QString::number(conesQuality);
        } else if (pointStyle == HairCross) {
            st = "cross\t" + QString::number(crossHairRad);
            st += "\t" + QString::number(crossHairLineWidth);
            st += "\t" + QString::number(crossHairSmooth);
            st += "\t" + QString::number(crossHairBoxed);
        }
        break;

    case WIREFRAME:
        st = "wireframe";
        break;

    case HIDDENLINE:
        st = "hiddenline";
        break;

    case FILLED:
        st = "filled";
        break;

    case FILLEDMESH:
        st = "filledmesh";
        break;

    default:
        break;
    }
    s += st + "\n";

    s += "grids\t";
    s += QString::number(sp->coordinates()->grids()) + "\n";

    s += "title\t";
    s += title + "\t";
    s += COLORNAME(titleCol) + "\t";
    s += titleFnt.family() + "\t";
    s += QString::number(titleFnt.pointSize()) + "\t";
    s += QString::number(titleFnt.weight()) + "\t";
    s += QString::number(titleFnt.italic()) + "\n";

    s += "colors\t";
    s += COLORNAME(meshCol) + "\t";
    s += COLORNAME(axesCol) + "\t";
    s += COLORNAME(numCol) + "\t";
    s += COLORNAME(labelsCol) + "\t";
    s += COLORNAME(bgCol) + "\t";
    s += COLORNAME(gridCol) + "\t";
    s += COLORNAME(fromColor) + "\t";
    s += COLORNAME(toColor) + "\t";
    s += QString::number(alpha) + "\t" + color_map + "\n";

    s += "axesLabels\t";
    s += labels.join("\t") + "\n";

    s += "tics\t";
    QStringList tl = scaleTicks();
    s += tl.join("\t") + "\n";

    s += "tickLengths\t";
    tl = axisTickLengths();
    s += tl.join("\t") + "\n";

    s += "options\t";
    s += QString::number(legendOn) + "\t";
    s += QString::number(sp->resolution()) + "\t";
    s += QString::number(labelsDist) + "\n";

    s += "numbersFont\t";
    QFont fnt = sp->coordinates()->axes[X1].numberFont();
    s += fnt.family() + "\t";
    s += QString::number(fnt.pointSize()) + "\t";
    s += QString::number(fnt.weight()) + "\t";
    s += QString::number(fnt.italic()) + "\n";

    s += "xAxisLabelFont\t";
    fnt = sp->coordinates()->axes[X1].labelFont();
    s += fnt.family() + "\t";
    s += QString::number(fnt.pointSize()) + "\t";
    s += QString::number(fnt.weight()) + "\t";
    s += QString::number(fnt.italic()) + "\n";

    s += "yAxisLabelFont\t";
    fnt = sp->coordinates()->axes[Y1].labelFont();
    s += fnt.family() + "\t";
    s += QString::number(fnt.pointSize()) + "\t";
    s += QString::number(fnt.weight()) + "\t";
    s += QString::number(fnt.italic()) + "\n";

    s += "zAxisLabelFont\t";
    fnt = sp->coordinates()->axes[Z1].labelFont();
    s += fnt.family() + "\t";
    s += QString::number(fnt.pointSize()) + "\t";
    s += QString::number(fnt.weight()) + "\t";
    s += QString::number(fnt.italic()) + "\n";

    s += "rotation\t";
    s += QString::number(sp->xRotation()) + "\t";
    s += QString::number(sp->yRotation()) + "\t";
    s += QString::number(sp->zRotation()) + "\n";

    s += "zoom\t";
    s += QString::number(zoom()) + "\n";

    // 軸範囲と同じく最短の往復表記で書く（既定の 'g',6 だと 1.0192977 が 1.0193 に丸まる）。
    s += "scaling\t";
    s += QString::number(sp->xScale(), 'g', QLocale::FloatingPointShortest) + "\t";
    s += QString::number(sp->yScale(), 'g', QLocale::FloatingPointShortest) + "\t";
    s += QString::number(sp->zScale(), 'g', QLocale::FloatingPointShortest) + "\n";

    s += "shift\t";
    s += QString::number(sp->xShift()) + "\t";
    s += QString::number(sp->yShift()) + "\t";
    s += QString::number(sp->zShift()) + "\n";

    s += "LineWidth\t";
    s += QString::number(sp->meshLineWidth()) + "\n";
    s += "WindowLabel\t" + windowLabel() + "\t" + QString::number(captionPolicy()) + "\n";
    s += "Orthogonal\t" + QString::number(sp->ortho()) + "\n";
    s += "</SurfacePlot>\n";
    return s;
}

void Graph3D::showColorLegend(bool show)
{
    if (legendOn == show)
        return;

    sp->makeCurrent();
    sp->showColorLegend(show);

    legendOn = show;
    sp->update();
    emit modified();
}

void Graph3D::setResolution(int r)
{
    if (sp->resolution() == r)
        return;

    sp->makeCurrent();
    sp->setResolution(r);
    sp->updateData();
    sp->update();
    emit modified();
}

void Graph3D::setTitle(const QStringList &lst)
{
    title = lst[1];
    sp->setTitle(title);

    titleCol = QColor(COLORVALUE(lst[2]));
    sp->setTitleColor(Qt2GL(titleCol));

    titleFnt = normalizePlot3DFont(QFont(lst[3], lst[4].toInt(), lst[5].toInt(), lst[6].toInt()), 14);
    sp->setTitleFont(titleFnt.family(), titleFnt.pointSize(), titleFnt.weight(), titleFnt.italic());
}

void Graph3D::setTitle(const QString &s, const QColor &color, const QFont &font)
{
    if (title != s) {
        title = s;
        sp->setTitle(title);
    }

    titleCol = color;
    sp->setTitleColor(Qt2GL(color));

    const QFont normFont = normalizePlot3DFont(font, 14);
    if (titleFnt != normFont) {
        titleFnt = normFont;
        sp->setTitleFont(titleFnt.family(), titleFnt.pointSize(), titleFnt.weight(),
                         titleFnt.italic());
    }
}

void Graph3D::updateTitle(const QString &s, const QColor &color, const QFont &font)
{
    if (title != s) {
        title = s;
        sp->setTitle(title);
    }

    titleCol = color;
    sp->setTitleColor(Qt2GL(color));

    const QFont normFont = normalizePlot3DFont(font, 14);
    if (titleFnt != normFont) {
        titleFnt = normFont;
        sp->setTitleFont(titleFnt.family(), titleFnt.pointSize(), titleFnt.weight(),
                         titleFnt.italic());
    }

    sp->makeCurrent();
    sp->update();
    emit modified();
}

void Graph3D::setTitleFont(const QFont &font)
{
    const QFont normFont = normalizePlot3DFont(font, 14);
    if (titleFnt != normFont) {
        titleFnt = normFont;
        sp->setTitleFont(titleFnt.family(), titleFnt.pointSize(), titleFnt.weight(),
                         titleFnt.italic());
    }
}

void Graph3D::setOptions(const QStringList &lst)
{
    legendOn = false;
    if (lst[1].toInt() == 1)
        legendOn = true;
    sp->showColorLegend(legendOn);
    sp->setResolution(lst[2].toInt());
    adjustLabels(lst[3].toInt());
}

void Graph3D::setOptions(bool legend, int r, int dist)
{
    sp->showColorLegend(legend);
    legendOn = legend;
    sp->setResolution(r);
    adjustLabels(dist);
}

Qwt3D::Triple **Graph3D::allocateData(int columns, int rows)
{
    Qwt3D::Triple **data = new Qwt3D::Triple *[columns];

    for (int i = 0; i < columns; ++i) {
        data[i] = new Qwt3D::Triple[rows];
    }
    return data;
}

void Graph3D::deleteData(Qwt3D::Triple **data, int columns)
{
    for (int i = 0; i < columns; i++) {
        delete[] data[i];
    }
    delete[] data;
}

QColor Graph3D::minDataColor()
{
    return fromColor;
}

QColor Graph3D::maxDataColor()
{
    return toColor;
}

void Graph3D::setDataColors(const QColor &cMin, const QColor &cMax)
{
    if (cMin == fromColor && cMax == toColor)
        return;

    fromColor = cMin;
    toColor = cMax;

    Qwt3D::ColorVector cv;

    int size = 255;
    double dsize = size;

    double r1 = cMax.red() / dsize;
    double r2 = cMin.red() / dsize;

    double stepR = (r1 - r2) / dsize;

    double g1 = cMax.green() / dsize;
    double g2 = cMin.green() / dsize;

    double stepG = (g1 - g2) / dsize;

    double b1 = cMax.blue() / dsize;
    double b2 = cMin.blue() / dsize;

    double stepB = (b1 - b2) / dsize;

    RGBA rgb;
    for (int i = 0; i < size; i++) {
        rgb.r = r1 - i * stepR;
        rgb.g = g1 - i * stepG;
        rgb.b = b1 - i * stepB;
        rgb.a = alpha;

        cv.push_back(rgb);
    }

    col_ = new StandardColor(sp);
    col_->setColorVector(cv);
    sp->setDataColor(col_);

    if (legendOn) {
        sp->showColorLegend(false);
        sp->showColorLegend(legendOn);
    }
}

void Graph3D::changeTransparency(double t)
{
    if (alpha == t)
        return;

    alpha = t;
    if (!sp || !sp->dataColor())
        return;

    Qwt3D::StandardColor *color = (StandardColor *)sp->dataColor();
    color->setAlpha(t);

    sp->showColorLegend(legendOn);
    sp->updateData();
    sp->update();
    emit modified();
}

void Graph3D::setTransparency(double t)
{
    if (alpha == t)
        return;

    alpha = t;
    if (!sp || !sp->dataColor())
        return;

    Qwt3D::StandardColor *color = (StandardColor *)sp->dataColor();
    color->setAlpha(t);
}

void Graph3D::showWorksheet()
{
    if (worksheet)
        worksheet->showMaximized();
    else if (d_matrix)
        d_matrix->showMaximized();
}

void Graph3D::setSmoothMesh(bool smooth)
{
    if (smoothMesh == smooth)
        return;

    smoothMesh = smooth;
    sp->setSmoothMesh(smoothMesh);
    sp->coordinates()->setLineSmooth(smoothMesh);
    sp->updateData();
    sp->update();
}

QString Graph3D::saveAsTemplate(const QString &geometryInfo)
{
    QString s = saveToString(geometryInfo);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList lst = s.split("\n", Qt::SkipEmptyParts);
#else
    QStringList lst = s.split("\n", QString::SkipEmptyParts);
#endif
    QStringList l = lst[3].split("\t");
    l[1] = QString();
    lst[3] = l.join("\t");
    return lst.join("\n");
}

/*!
Turns 3D animation on or off
*/
void Graph3D::animate(bool on)
{
    if (on)
        d_timer->start(animation_redraw_wait); // Wait this many msecs before redraw
    else
        d_timer->stop();
}

void Graph3D::rotate()
{
    if (!sp)
        return;

    sp->setRotation(int(sp->xRotation() + 1) % 360, int(sp->yRotation() + 1) % 360,
                    int(sp->zRotation() + 1) % 360);
}

void Graph3D::setDataColorMap(const QString &fileName)
{
    if (color_map == fileName)
        return;

    ColorVector cv;
    if (!openColorMap(cv, fileName))
        return;

    color_map = fileName;

    col_ = new StandardColor(sp);
    col_->setColorVector(cv);

    sp->setDataColor(col_);
    sp->updateData();
    sp->showColorLegend(legendOn);
    sp->update();
}

bool Graph3D::openColorMap(ColorVector &cv, QString fname)
{
    if (fname.isEmpty())
        return false;

    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    RGBA rgb;
    cv.clear();

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        QTextStream lineStream(const_cast<QString *>(&line), QIODevice::ReadOnly);
        lineStream >> rgb.r >> rgb.g >> rgb.b;
        if (lineStream.status() != QTextStream::Ok)
            break;
        rgb.a = 1;
        rgb.r /= 255;
        rgb.g /= 255;
        rgb.b /= 255;
        cv.push_back(rgb);
    }
    return true;
}

void Graph3D::updateBaseZoom()
{
    double d = (sp->hull().maxVertex - sp->hull().minVertex).length();
    if (!(d > 0.0) || !std::isfinite(d))
        d = sqrt(3.0);
    d_base_zoom = d / sqrt(3.0);
}

void Graph3D::findBestLayout()
{
    double start, end;
    sp->coordinates()->axes[X1].limits(start, end);
    const double dx = fabs(end - start);
    double xScale = (dx > 0.0 && std::isfinite(dx)) ? (1.0 / dx) : 1.0;

    sp->coordinates()->axes[Y1].limits(start, end);
    const double dy = fabs(end - start);
    double yScale = (dy > 0.0 && std::isfinite(dy)) ? (1.0 / dy) : 1.0;

    sp->coordinates()->axes[Z1].limits(start, end);
    const double dz = fabs(end - start);
    double zScale = (dz > 0.0 && std::isfinite(dz)) ? (1.0 / dz) : 1.0;

    updateBaseZoom();
    sp->setScale(xScale, yScale, zScale);
    sp->setZoom(d_base_zoom);

    double majl = 0.1 / yScale;
    updateTickLength(0, majl, 0.6 * majl);
    majl = 0.1 / xScale;
    updateTickLength(1, majl, 0.6 * majl);
    updateTickLength(2, majl, 0.6 * majl);
}

void Graph3D::copy(Graph3D *g)
{
    if (!g)
        return;

    Graph3D::PointStyle pt = g->pointType();
    if (g->plotStyle() == Qwt3D::USER) {
        switch (pt) {
        case Graph3D::None:
            break;

        case Graph3D::Dots:
            setPointOptions(g->pointsSize(), g->smoothPoints());
            break;

        case Graph3D::VerticalBars:
            setBarsRadius(g->barsRadius());
            break;

        case Graph3D::HairCross:
            setCrossOptions(g->crossHairRadius(), g->crossHairLinewidth(), g->smoothCrossHair(),
                            g->boxedCrossHair());
            break;

        case Graph3D::Cones:
            setConesOptions(g->coneRadius(), g->coneQuality());
            break;
        }
    }
    setStyle(g->coordStyle(), g->floorStyle(), g->plotStyle(), pt);
    setGrid(g->grids());
    setTitle(g->plotTitle(), g->titleColor(), g->titleFont());
    setTransparency(g->transparency());
    if (!g->colorMap().isEmpty())
        setDataColorMap(g->colorMap());
    else
        setDataColors(g->minDataColor(), g->maxDataColor());

    setColors(g->meshColor(), g->axesColor(), g->numColor(), g->labelColor(), g->bgColor(),
              g->gridColor());
    setAxesLabels(g->axesLabels());
    setTicks(g->scaleTicks());
    setTickLengths(g->axisTickLengths());
    setOptions(g->isLegendOn(), g->resolution(), g->labelsDistance());
    setNumbersFont(g->numbersFont());
    setXAxisLabelFont(g->xAxisLabelFont());
    setYAxisLabelFont(g->yAxisLabelFont());
    setZAxisLabelFont(g->zAxisLabelFont());
    setRotation(g->xRotation(), g->yRotation(), g->zRotation());
    // Take the source's base zoom and drawn zoom as they are: zoom() is relative to the base,
    // and setZoom() may read a large relative value as an absolute one.
    if (g->d_base_zoom > 1e-12 && std::isfinite(g->d_base_zoom))
        d_base_zoom = g->d_base_zoom;
    if (g->sp && std::isfinite(g->sp->zoom()) && g->sp->zoom() > 0.0)
        sp->setZoom(g->sp->zoom());
    else
        setZoom(g->zoom());
    setScale(g->xScale(), g->yScale(), g->zScale());
    setShift(g->xShift(), g->yShift(), g->zShift());
    setMeshLineWidth((int)g->meshLineWidth());
    setOrtho(g->isOrthogonal());
    update();
    animate(g->isAnimated());
}

Graph3D::~Graph3D()
{
    if (func)
        delete func;

    delete sp;
}
