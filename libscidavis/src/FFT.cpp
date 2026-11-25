/***************************************************************************
    File                 : FFT.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Numerical FFT of data sets

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
#include "FFT.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "ColorButton.h"
#include "core/column/Column.h"

#include <QApplication>
#include <QMessageBox>
#include <QLocale>

#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_fft_halfcomplex.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

void ensureUniformGridAndSampling(std::vector<double> &x, std::vector<double> &yInterleaved,
                                  double &samplingOut)
{
    const size_t n = x.size();
    if (n < 2 || yInterleaved.size() < 2 * n)
        return;

    struct Sample {
        double x;
        double re;
        double im;
    };
    std::vector<Sample> pts;
    pts.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (std::isfinite(x[i]) && std::isfinite(yInterleaved[2 * i])
            && std::isfinite(yInterleaved[2 * i + 1])) {
            pts.push_back({ x[i], yInterleaved[2 * i], yInterleaved[2 * i + 1] });
        }
    }
    if (pts.size() < 2)
        return;

    if (pts.back().x < pts.front().x)
        std::reverse(pts.begin(), pts.end());

    bool sorted = true;
    for (size_t i = 1; i < pts.size(); ++i) {
        if (pts[i].x < pts[i - 1].x) {
            sorted = false;
            break;
        }
    }
    if (!sorted) {
        std::stable_sort(pts.begin(), pts.end(),
                         [](const Sample &a, const Sample &b) { return a.x < b.x; });
    }

    const double xMin = pts.front().x;
    const double xMax = pts.back().x;
    const double span = xMax - xMin;
    if (!std::isfinite(span) || span <= 0.0)
        return;

    const double uniformDt = span / static_cast<double>(n - 1);
    if (!std::isfinite(uniformDt) || uniformDt <= 0.0)
        return;

    bool isUniform = (pts.size() == n && sorted);
    if (isUniform) {
        for (size_t i = 1; i < n; ++i) {
            const double step = pts[i].x - pts[i - 1].x;
            if (step <= 0.0 || std::fabs(step - uniformDt) > 1e-2 * uniformDt) {
                isUniform = false;
                break;
            }
        }
    }

    samplingOut = uniformDt;
    if (isUniform)
        return;

    // Resample onto a uniform grid over [xMin, xMax] via linear interpolation
    std::vector<double> newX(n);
    std::vector<double> newY(2 * n, 0.0);
    size_t j = 0;
    const size_t m = pts.size();
    for (size_t i = 0; i < n; ++i) {
        const double gx = (i + 1 == n) ? xMax : (xMin + static_cast<double>(i) * uniformDt);
        newX[i] = gx;
        while (j + 2 < m && pts[j + 1].x < gx)
            ++j;
        const double x0 = pts[j].x;
        const double x1 = pts[j + 1].x;
        const double dx = x1 - x0;
        double t = 0.0;
        if (dx > 0.0)
            t = std::clamp((gx - x0) / dx, 0.0, 1.0);
        newY[2 * i] = pts[j].re + t * (pts[j + 1].re - pts[j].re);
        newY[2 * i + 1] = pts[j].im + t * (pts[j + 1].im - pts[j].im);
    }
    x = std::move(newX);
    yInterleaved = std::move(newY);
}

} // namespace

FFT::FFT(ApplicationWindow *parent, Table *t, const QString &realColName,
         const QString &imagColName)
    : Filter(parent, t)
{
    init();
    setDataFromTable(t, realColName, imagColName);
}

FFT::FFT(ApplicationWindow *parent, Graph *g, const QString &curveTitle) : Filter(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
    // intersperse 0 imaginary components
    std::vector<double> tmp(2 * d_x.size(), 0.0);
    for (size_t i = 0; i < d_x.size(); ++i)
        tmp[2 * i] = d_y[i];
    d_y = std::move(tmp);
    ensureUniformGridAndSampling(d_x, d_y, d_sampling);
}

void FFT::init()
{
    setObjectName(tr("FFT"));
    d_inverse = false;
    d_normalize = true;
    d_shift_order = true;
    d_real_col = -1;
    d_imag_col = -1;
    d_sampling = 1.0;
}

QList<Column *> FFT::fftTable()
{
    if (d_x.empty() || d_y.size() < 2 * d_x.size()) {
        d_init_err = true;
        return QList<Column *>();
    }

    std::vector<double> amp;

    gsl_fft_complex_wavetable *wavetable = nullptr;
    gsl_fft_complex_workspace *workspace = nullptr;

    try {
        amp.resize(d_x.size());
        wavetable = gsl_fft_complex_wavetable_alloc(d_x.size());
        workspace = gsl_fft_complex_workspace_alloc(d_x.size());
        if ((nullptr == wavetable) || (nullptr == workspace))
            throw std::bad_alloc();
    } catch (const std::bad_alloc &) {
        if (!isSilentOrNonInteractive()
            && !(qApp && qApp->property("torigraph_noninteractive").toBool())) {
            QMessageBox::critical((ApplicationWindow *)parent(), tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Could not allocate memory, operation aborted!"));
        }
        if (nullptr != wavetable)
            gsl_fft_complex_wavetable_free(wavetable);
        if (nullptr != workspace)
            gsl_fft_complex_workspace_free(workspace);
        d_init_err = true;
        return QList<Column *>();
    }

    // A negative interval (X in descending order) describes the same grid step.
    const double absSampling = std::fabs(d_sampling);
    const double safeSampling = (std::isfinite(absSampling) && absSampling > 0.0) ? absSampling : 1.0;
    double df = 1.0 / (static_cast<double>(d_x.size()) * safeSampling); // frequency sampling
    double aMax = 0.0; // max amplitude
    QList<Column *> columns;
    if (!d_inverse) {
        columns << new Column(tr("Frequency"), SciDAVis::ColumnMode::Numeric);
        gsl_fft_complex_forward(d_y.data(), 1, d_x.size(), wavetable, workspace);
    } else {
        columns << new Column(tr("Time"), SciDAVis::ColumnMode::Numeric);
        gsl_fft_complex_inverse(d_y.data(), 1, d_x.size(), wavetable, workspace);
    }

    gsl_fft_complex_wavetable_free(wavetable);
    gsl_fft_complex_workspace_free(workspace);

    if (d_shift_order) {
        const size_t n = d_x.size();
        const int n2 = static_cast<int>(n / 2);
        for (size_t i = 0; i < n; i++)
            d_x[i] = (static_cast<int>(i) - n2) * df;
        if (n > 1) {
            const size_t shiftPairs = (n + 1) / 2;
            std::rotate(d_y.begin(),
                        d_y.begin() + static_cast<std::ptrdiff_t>(2 * shiftPairs),
                        d_y.begin() + static_cast<std::ptrdiff_t>(2 * n));
        }
    } else {
        for (size_t i = 0; i < d_x.size(); i++)
            d_x[i] = static_cast<double>(i) * df;
    }

    for (size_t i = 0; i < d_x.size(); i++) {
        size_t i2 = 2 * i;
        double a = std::hypot(d_y[i2], d_y[i2 + 1]);
        amp[i] = a;
        if (a > aMax)
            aMax = a;
    }

    columns << new Column(tr("Real"), SciDAVis::ColumnMode::Numeric);
    columns << new Column(tr("Imaginary"), SciDAVis::ColumnMode::Numeric);
    columns << new Column(tr("Amplitude"), SciDAVis::ColumnMode::Numeric);
    columns << new Column(tr("Angle"), SciDAVis::ColumnMode::Numeric);
    for (int i = 0; i < static_cast<int>(d_x.size()); i++) {
        int i2 = 2 * i;
        const double re = d_y[i2];
        const double im = d_y[i2 + 1];
        columns.at(0)->setValueAt(i, d_x[i]);
        columns.at(1)->setValueAt(i, re);
        columns.at(2)->setValueAt(i, im);
        if (d_normalize)
            columns.at(3)->setValueAt(i, (aMax > 0.0) ? (amp[i] / aMax) : 0.0);
        else
            columns.at(3)->setValueAt(i, amp[i]);
        columns.at(4)->setValueAt(i, (re == 0.0 && im == 0.0) ? 0.0 : std::atan2(im, re));
    }

    columns.at(0)->setPlotDesignation(SciDAVis::X);
    columns.at(1)->setPlotDesignation(SciDAVis::Y);
    columns.at(2)->setPlotDesignation(SciDAVis::Y);
    columns.at(3)->setPlotDesignation(SciDAVis::Y);
    columns.at(4)->setPlotDesignation(SciDAVis::Y);
    return columns;
}

void FFT::output()
{
    QList<Column *> columns;
    if (!d_y.empty())
        columns = fftTable();

    if (!columns.isEmpty())
        output(columns);
}

void FFT::output(QList<Column *> columns)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    QString tableName = app->generateUniqueName(objectName());
    Table *t = app->newHiddenTable(tableName, d_explanation, columns);
    MultiLayer *ml = app->multilayerPlot(t, QStringList() << tableName + "_" + tr("Amplitude"), 0);
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (g) {
        g->setCurvePen(0, QPen(d_curveColor, 1));

        Plot *plot = g->plotWidget();
        plot->setTitle(QString());
        if (!d_inverse)
            plot->setAxisTitle(QwtPlot::xBottom, tr("Frequency") + " (" + tr("Hz") + ")");
        else
            plot->setAxisTitle(QwtPlot::xBottom, tr("Time") + +" (" + tr("s") + ")");

        plot->setAxisTitle(QwtPlot::yLeft, tr("Amplitude"));
        plot->replot();
    }
    ml->showMaximized();
}

void FFT::setDataFromTable(Table *t, const QString &realColName, const QString &imagColName)
{
    if (t && d_table != t)
        d_table = t;

    if (!d_table) {
        d_init_err = true;
        return;
    }

    d_real_col = d_table->colIndex(realColName);
    if (d_real_col < 0) {
        d_init_err = true;
        return;
    }

    if (!imagColName.isEmpty())
        d_imag_col = d_table->colIndex(imagColName);

    size_t rows = d_table->numRows();
    Column *xc = nullptr;
    if (rows >= 2) {
        const int xcol = d_table->colX(d_real_col);
        if (xcol >= 0 && xcol != d_real_col && xcol != d_imag_col) {
            if (Column *cand = d_table->column(xcol)) {
                if (cand->columnMode() == SciDAVis::ColumnMode::Numeric)
                    xc = cand;
            }
        }
    }

    size_t n2 = 2 * rows;
    try {
        d_y.assign(n2, 0.0);
        d_x.resize(rows);
    } catch (const std::bad_alloc &e) {
        if (!isSilentOrNonInteractive()
            && !(qApp && qApp->property("torigraph_noninteractive").toBool())) {
            QMessageBox::critical((ApplicationWindow *)parent(), tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Could not allocate memory, operation aborted!\n")
                                          + tr("Allocator returned: ") + e.what());
        }
        d_init_err = true;
        return;
    }

    for (size_t i = 0; i < d_x.size(); i++) {
        size_t i2 = 2 * i;
        if (xc) {
            const int r = static_cast<int>(i);
            d_x[i] = xc->isInvalid(r) ? std::numeric_limits<double>::quiet_NaN() : xc->valueAt(r);
        } else {
            d_x[i] = static_cast<double>(i);
        }
        d_y[i2] = d_table->cell(static_cast<int>(i), d_real_col);
        d_y[i2 + 1] = (d_imag_col >= 0) ? d_table->cell(static_cast<int>(i), d_imag_col) : 0.0;
    }

    if (xc)
        ensureUniformGridAndSampling(d_x, d_y, d_sampling);
}
