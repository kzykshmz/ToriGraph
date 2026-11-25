/***************************************************************************
    File                 : QwtHistogram.h
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
#ifndef QWT_HISTOGRAM_H
#define QWT_HISTOGRAM_H

#include "QwtBarCurve.h"

//! Histogram class
/**
 * The curve points are the bin centres (x) and counts (y), so fits, analysis and the data
 * readouts see the centre of each bin. The bars are drawn from centre - width/2 to
 * centre + width/2, i.e. from the left bin edge over one bin width.
 */
class QwtHistogram : public QwtBarCurve
{
public:
    QwtHistogram(Table *t, const QString &name, int startRow, int endRow);

    void copy(const QwtHistogram *h);

    QwtDoubleRect boundingRect() const;

    void setBinning(bool autoBin, double size, double begin, double end);
    bool autoBinning() { return d_autoBin; };
    double begin() { return d_begin; };
    double end() { return d_end; };
    double binSize() { return d_bin_size; };
    //! Bin width used to draw the bars (the bin size, else the spacing of the first two bins).
    double binWidthForDrawing() const;

    virtual bool loadData();
    void initData(const QVector<double> &Y, int size);
    //! The points are histogram bins, not table rows: no point maps to a row (point editing
    //! tools must not write to the table).
    int tableRow(int) const override { return -1; }

    double mean() { return d_mean; };
    double standardDeviation() { return d_standard_deviation; };
    double minimum() { return d_min; };
    double maximum() { return d_max; };

private:
    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap, int from,
              int to) const;

    bool d_autoBin;
    double d_bin_size, d_begin, d_end;

    //! Variables storing statistical information
    double d_mean, d_standard_deviation, d_min, d_max;
};

#endif // QWT_HISTOGRAM_H
