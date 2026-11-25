/***************************************************************************
    File                 : Interpolation.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Numerical interpolation of data sets

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
#ifndef INTERPOLATION_H
#define INTERPOLATION_H

#include "Filter.h"

class QwtPlotCurve;
class Interpolation : public Filter
{
    Q_OBJECT

public:
    enum InterpolationMethod { Linear, Cubic, Akima };

    Interpolation(ApplicationWindow *parent, Graph *g, const QString &curveTitle, int m = 0);
    Interpolation(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                  double end, int m = 0);

    int method() { return d_method; };
    void setMethod(int m);
    void setMethod(InterpolationMethod m) { setMethod((int)m); };

    //! Takes every valid point of the curve for the interpolant; the output grid is
    //! [start, end] cut to the X range of the data (no extrapolation).
    void setDataCurve(int curve, double start, double end) override;
    //! true when the requested range reached past the data and the grid was cut to it
    bool rangeClipped() const { return d_range_clipped; }
    //! X range of the output grid
    double outputFrom() const { return d_from; }
    double outputTo() const { return d_to; }
    //! X range of the data the interpolant is built from
    double dataXMin() const { return d_data_x_min; }
    double dataXMax() const { return d_data_x_max; }

protected:
    bool isDataAcceptable() const override;

private:
    void init(int m);
    void calculateOutputData(std::vector<double>&  x, std::vector<double>& y) override;

    //! the interpolation method
    int d_method;
    bool d_range_clipped = false;
    double d_data_x_min = 0.0;
    double d_data_x_max = 0.0;
};

#endif
