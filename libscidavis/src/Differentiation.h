/***************************************************************************
    File                 : Differentiation.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Numerical differentiation of data sets

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
#ifndef DIFFERENTIATION_H
#define DIFFERENTIATION_H

#include "Filter.h"

class Differentiation : public Filter
{
    Q_OBJECT

public:
    Differentiation(ApplicationWindow *parent, Graph *g);
    Differentiation(ApplicationWindow *parent, Graph *g, const QString &curveTitle);
    Differentiation(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                    double end);

    //! Notice about how X was treated (sorted by X, or the turning points of a scan that goes
    //! back); empty when X was monotonic.
    QString xHandlingNotice() const { return d_notice; }
    //! Why the last run() refused to differentiate; empty if it did not.
    QString errorMessage() const { return d_error_message; }

protected:
    QString logInfo() override;

private:
    void init();
    void output() override;

    QString d_notice;
    QString d_error_message;
};
#endif
