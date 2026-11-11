/***************************************************************************
    File                 : PolynomialFit.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Polynomial Fit and Linear Fit classes

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
#ifndef POLYNOMIALFIT_H
#define POLYNOMIALFIT_H

#include "Fit.h"

class PolynomialFit : public Fit
{
    Q_OBJECT

public:
    PolynomialFit(ApplicationWindow *parent, Graph *g, int order = 2, bool legend = false);
    PolynomialFit(ApplicationWindow *parent, Graph *g, QString &curveTitle, int order = 2,
                  bool legend = false);
    PolynomialFit(ApplicationWindow *parent, Graph *g, QString &curveTitle, double start,
                  double end, int order = 2, bool legend = false);
    ~PolynomialFit() override;

    QString legendInfo() override;
    void fit() override;

    void setApparentFit(bool apparent = true) { d_apparent_fit = apparent; }
    bool apparentFit() const { return d_apparent_fit; }

    static QString generateFormula(int order);
    static QStringList generateParameterList(int order);

private:
    void init();
    bool calculateFitCurveData(const std::vector<double> &, std::vector<double> &,
                               std::vector<double> &) override;

    int d_order;
    bool show_legend;
    bool d_apparent_fit = false;
    bool d_app_log_x = false;
    bool d_app_log_y = false;
};

class LinearFit : public Fit
{
    Q_OBJECT

public:
    LinearFit(ApplicationWindow *parent, Graph *g);
    LinearFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle);
    LinearFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
              double end);
    ~LinearFit() override;

    void fit() override;

    void setApparentFit(bool apparent = true) { d_apparent_fit = apparent; }
    bool apparentFit() const { return d_apparent_fit; }

    bool fitWithXYErrors(const QString &xErrColName = QString(),
                         const QString &yErrColName = QString());
    double yorkSlope() const { return d_york_slope; }
    double yorkIntercept() const { return d_york_intercept; }
    double yorkSlopeError() const { return d_york_slope_err; }
    double yorkInterceptError() const { return d_york_intercept_err; }

private:
    void init();
    bool calculateFitCurveData(const std::vector<double> &, std::vector<double> &,
                               std::vector<double> &) override;

    bool d_apparent_fit = false;
    bool d_app_log_x = false;
    bool d_app_log_y = false;

    double d_york_slope = 0.0;
    double d_york_intercept = 0.0;
    double d_york_slope_err = 0.0;
    double d_york_intercept_err = 0.0;
};
#endif
