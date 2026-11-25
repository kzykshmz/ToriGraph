/***************************************************************************
    File                 : QwtHistogram.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Histogram class

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
#include "QwtHistogram.h"
#include "Plot.h"
#include "core/column/Column.h"
#include <QPainter>
#include <QLocale>
#include <cmath>
#include <qwt_scale_map.h>

#include <gsl/gsl_vector.h>
#include <gsl/gsl_histogram.h>

namespace {

void computeNiceHistogramBins(double minVal, double maxVal, int sampleCount, double &outBegin,
                              double &outEnd, double &outBinSize, int &outNumBins)
{
    if (!std::isfinite(minVal) || !std::isfinite(maxVal)) {
        outBegin = 0.0;
        outEnd = 1.0;
        outBinSize = 0.1;
        outNumBins = 10;
        return;
    }
    if (minVal > maxVal)
        qSwap(minVal, maxVal);

    const double span = maxVal - minVal;
    if (span <= 0.0 || !std::isfinite(span)) {
        const double pad = qMax(1e-6, 0.05 * std::fabs(minVal));
        outBegin = minVal - pad;
        outEnd = maxVal + pad;
        outNumBins = 5;
        outBinSize = (outEnd - outBegin) / double(outNumBins);
        return;
    }

    const int targetBins =
            qBound(8, int(std::ceil(std::log2(double(qMax(2, sampleCount))) + 1.0)), 20);
    const double rawStep = span / double(targetBins);
    const double mag = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double norm = rawStep / mag;
    double niceMult = 1.0;
    if (norm <= 1.4)
        niceMult = 1.0;
    else if (norm <= 2.2)
        niceMult = 2.0;
    else if (norm <= 3.5)
        niceMult = 2.5;
    else if (norm <= 7.0)
        niceMult = 5.0;
    else
        niceMult = 10.0;

    double step = niceMult * mag;
    if (step <= 0.0 || !std::isfinite(step))
        step = span / 10.0;

    double begin = std::floor(minVal / step) * step;
    double end = std::ceil(maxVal / step) * step;
    if (std::fabs(begin) < 1e-12 * step)
        begin = 0.0;
    if (end <= begin)
        end = begin + step;

    int nBins = qBound(1, qRound((end - begin) / step), 200);
    end = begin + double(nBins) * step;
    if (maxVal >= end) {
        end += step;
        ++nBins;
    }

    outBegin = begin;
    outEnd = end;
    outBinSize = step;
    outNumBins = nBins;
}

} // namespace

QwtHistogram::QwtHistogram(Table *t, const QString &name, int startRow, int endRow)
    : QwtBarCurve(QwtBarCurve::Vertical, t, "dummy", name, startRow, endRow)
{
    d_autoBin = true;
    d_bin_size = 1.0;
    d_begin = 0.0;
    d_end = 10.0;
    d_mean = 0.0;
    d_standard_deviation = 0.0;
    d_min = 0.0;
    d_max = 0.0;
    setGap(0);
    // The bar base class marks itself as vertical bars; a histogram must report its own type,
    // otherwise copies (Graph::copy, clone) are built as plain bar curves that lose the binning.
    setType(Graph::Histogram);
}

void QwtHistogram::copy(const QwtHistogram *h)
{
    QwtBarCurve::copy((const QwtBarCurve *)h);

    d_autoBin = h->d_autoBin;
    d_bin_size = h->d_bin_size;
    d_begin = h->d_begin;
    d_end = h->d_end;
    d_mean = h->d_mean;
    d_standard_deviation = h->d_standard_deviation;
    d_min = h->d_min;
    d_max = h->d_max;
}

void QwtHistogram::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                        int from, int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    if (from < 0)
        from = 0;
    if (to < 0 || to >= dataSize())
        to = dataSize() - 1;
    if (from > to)
        return;

    painter->save();
    const double sc = Plot::printDeviceScale();
    QPen hPen = QwtPlotCurve::pen();
    if (hPen.style() != Qt::NoPen) {
        hPen.setCosmetic(false);
        hPen.setWidthF(qMax(1.0, hPen.widthF()) * sc);
    }
    painter->setPen(hPen);
    painter->setBrush(QwtPlotCurve::brush());

    const double ref = yMap.xTransform(baseline());
    // The points are bin centres; each bar spans [centre - width/2, centre + width/2].
    const double halfBin = 0.5 * binWidthForDrawing();
    double dx = 0.0;
    if (halfBin > 0.0) {
        dx = std::fabs(xMap.xTransform(x(from) + halfBin) - xMap.xTransform(x(from) - halfBin));
    }
    if (dx <= 1e-6)
        dx = qMax(6.0 * sc, std::fabs(xMap.pDist()) / 10.0);

    const double bar_width = qMax(1.0, dx * (1.0 - gap() * 0.01));
    const double half_width = 0.5 * (dx - bar_width);
    const double xOffset = 0.01 * offset() * bar_width;

    for (int i = from; i <= to; i++) {
        const double px1 = xMap.xTransform(x(i) - halfBin);
        const double py1 = yMap.xTransform(y(i));
        if (!std::isfinite(px1) || !std::isfinite(py1) || !std::isfinite(ref))
            continue;
        const double top = qMin(py1, ref);
        const double h = std::fabs(ref - py1);
        painter->drawRect(QRectF(px1 + half_width + xOffset, top, bar_width, qMax(0.0, h)));
    }

    painter->restore();
}

double QwtHistogram::binWidthForDrawing() const
{
    if (d_bin_size > 0.0 && std::isfinite(d_bin_size))
        return d_bin_size;
    if (dataSize() >= 2) {
        const double w = std::fabs(x(1) - x(0));
        if (w > 0.0 && std::isfinite(w))
            return w;
    }
    return 0.0;
}

QwtDoubleRect QwtHistogram::boundingRect() const
{
    QwtDoubleRect rect = QwtPlotCurve::boundingRect();
    if (dataSize() <= 0)
        return rect;
    double bw = binWidthForDrawing();
    if (bw <= 0.0 || !std::isfinite(bw))
        bw = 1.0;

    // points are bin centres: half a bin to the outer bar edges plus a 15 % bin margin
    rect.setLeft(rect.left() - 0.65 * bw);
    rect.setRight(rect.right() + 0.65 * bw);
    rect.setTop(0.0);
    rect.setBottom(qMax(1.0, 1.15 * rect.bottom()));
    return rect;
}

void QwtHistogram::setBinning(bool autoBin, double size, double begin, double end)
{
    d_autoBin = autoBin;
    d_bin_size = size;
    d_begin = begin;
    d_end = end;
}

bool QwtHistogram::loadData()
{
    if (!d_table)
        return false;

    Column *x_dummy = nullptr;
    Column *y_col_ptr = nullptr;
    resolveColumns(x_dummy, y_col_ptr);
    int ycol = d_table->colIndex(title().text());
    if (!y_col_ptr && ycol >= 0 && ycol < d_table->numCols())
        y_col_ptr = d_table->column(ycol);
    if (!y_col_ptr)
        return false;

    const int startR = qMax(0, d_start_row);
    const int endR = (d_end_row < 0) ? (y_col_ptr->rowCount() - 1)
                                     : qMin(d_end_row, y_col_ptr->rowCount() - 1);
    if (startR > endR)
        return false;

    QVarLengthArray<double> Y(endR - startR + 1);
    const auto yColType = (ycol >= 0 && ycol < d_table->numCols())
            ? d_table->columnType(ycol)
            : y_col_ptr->columnMode();
    int size = 0;
    double minVal = 0.0, maxVal = 0.0;
    for (int row = startR; row <= endR; row++) {
        if (y_col_ptr->isInvalid(row) || y_col_ptr->isMasked(row))
            continue;
        double val = 0.0;
        if (yColType == SciDAVis::ColumnMode::Text) {
            const QString yval = y_col_ptr->textAt(row);
            bool valid_data = false;
            val = QLocale().toDouble(yval, &valid_data);
            if (!valid_data)
                val = QLocale::c().toDouble(yval, &valid_data);
            if (!valid_data)
                continue;
        } else {
            val = y_col_ptr->valueAt(row);
        }
        if (!std::isfinite(val))
            continue;
        if (size == 0) {
            minVal = maxVal = val;
        } else {
            if (val < minVal)
                minVal = val;
            if (val > maxVal)
                maxVal = val;
        }
        Y[size++] = val;
    }

    if (size < 1) {
        double X[2] = { 0.0, 1.0 };
        double Z[2] = { 0.0, 0.0 };
        setData(X, Z, 2);
        return false;
    }

    int n = 10;
    gsl_histogram *h = nullptr;
    if (d_autoBin || !std::isfinite(d_bin_size) || d_bin_size <= 0.0 || !std::isfinite(d_begin)
        || !std::isfinite(d_end) || d_end <= d_begin) {
        computeNiceHistogramBins(minVal, maxVal, size, d_begin, d_end, d_bin_size, n);
        h = gsl_histogram_alloc(n);
        if (!h)
            return false;
        const double gslUpper = (maxVal >= d_end - 1e-12 * d_bin_size)
                ? (d_end + 1e-9 * d_bin_size)
                : d_end;
        gsl_histogram_set_ranges_uniform(h, d_begin, gslUpper);
    } else {
        n = qBound(1, int(std::ceil((d_end - d_begin) / d_bin_size - 1e-9)), 10000);
        h = gsl_histogram_alloc(n);
        if (!h)
            return false;

        std::vector<double> range(static_cast<size_t>(n) + 1);
        for (int i = 0; i <= n; i++)
            range[static_cast<size_t>(i)] = d_begin + double(i) * d_bin_size;
        if (maxVal >= range.back() - 1e-12 * d_bin_size && maxVal <= range.back() + 1e-9 * d_bin_size)
            range.back() += 1e-9 * d_bin_size;

        gsl_histogram_set_ranges(h, range.data(), n + 1);
    }

    for (int i = 0; i < size; i++)
        gsl_histogram_increment(h, Y[i]);

    std::vector<double> X(n);
    Y.resize(n);
    for (int i = 0; i < n; i++) {
        Y[i] = gsl_histogram_get(h, i);
        X[i] = d_begin + (double(i) + 0.5) * d_bin_size; // bin centre
    }
    setData(X.data(), Y.data(), n);

    d_mean = gsl_histogram_mean(h);
    d_standard_deviation = gsl_histogram_sigma(h);
    d_min = minVal;
    d_max = maxVal;

    gsl_histogram_free(h);

    return true;
}

void QwtHistogram::initData(const QVector<double> &Y, int size)
{
    QVector<double> finiteY;
    finiteY.reserve(qMax(0, size));
    double minVal = 0.0, maxVal = 0.0;
    for (int i = 0; i < size && i < Y.size(); ++i) {
        const double v = Y[i];
        if (!std::isfinite(v))
            continue;
        if (finiteY.isEmpty()) {
            minVal = maxVal = v;
        } else {
            if (v < minVal)
                minVal = v;
            if (v > maxVal)
                maxVal = v;
        }
        finiteY.push_back(v);
    }

    if (finiteY.isEmpty()) {
        double x[2] = { 0.0, 1.0 };
        double y[2] = { 0.0, 0.0 };
        setData(x, y, 2);
        return;
    }

    int n = 10;
    computeNiceHistogramBins(minVal, maxVal, finiteY.size(), d_begin, d_end, d_bin_size, n);
    gsl_histogram *h = gsl_histogram_alloc(n);
    if (!h)
        return;

    const double gslUpper = (maxVal >= d_end - 1e-12 * d_bin_size)
            ? (d_end + 1e-9 * d_bin_size)
            : d_end;
    gsl_histogram_set_ranges_uniform(h, d_begin, gslUpper);

    for (double v : finiteY)
        gsl_histogram_increment(h, v);

    std::vector<double> x(n), y(n);
    for (int i = 0; i < n; i++) {
        y[i] = gsl_histogram_get(h, i);
        x[i] = d_begin + (double(i) + 0.5) * d_bin_size; // bin centre
    }

    setData(x.data(), y.data(), n);

    d_autoBin = true;
    d_mean = gsl_histogram_mean(h);
    d_standard_deviation = gsl_histogram_sigma(h);
    d_min = minVal;
    d_max = maxVal;

    gsl_histogram_free(h);
}
