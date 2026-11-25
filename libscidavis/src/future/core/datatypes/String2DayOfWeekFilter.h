/***************************************************************************
    File                 : String2DayOfWeekFilter.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Knut Franke, Tilman Benkert
    Email (use @ for *)  : knut.franke*gmx.de, thzs*gmx.net
    Description          : Conversion filter String -> QDateTime, interpreting
                           the input as days of the week (either numeric or "Mon" etc).

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
#ifndef STRING2DAYOFWEEK_FILTER_H
#define STRING2DAYOFWEEK_FILTER_H

#include "../AbstractSimpleFilter.h"
#include <QDateTime>
#include <QLocale>
#include <math.h>
#include "lib/XmlStreamReader.h"
#include <QXmlStreamWriter>

//! Conversion filter String -> QDateTime, interpreting the input as days of the week (either numeric or "Mon" etc).
class String2DayOfWeekFilter : public AbstractSimpleFilter
{
    Q_OBJECT

public:
    virtual QDate dateAt(int row) const { return dateTimeAt(row).date(); }

    virtual QTime timeAt(int row) const { return dateTimeAt(row).time(); }

    virtual QDateTime dateTimeAt(int row) const
    {
        if (!d_inputs.value(0))
            return QDateTime();

        QString input_value = d_inputs.value(0)->textAt(row);
        if (input_value.isEmpty())
            return QDateTime();
        bool ok;
        int day_value = input_value.toInt(&ok);
        if (!ok) {
#if QT_VERSION <= 0x040300
            // workaround for Qt bug #171920
            QDate temp = QDate(1900, 1, 1);
            for (int i = 1; i <= 7; i++)
                if ((input_value.toLower() == QDate::longDayName(i).toLower())
                    || (input_value.toLower() == QDate::shortDayName(i).toLower())) {
                    temp = QDate(1900, 1, i);
                    break;
                }

#else
            QDate temp = QDate::fromString(input_value, "ddd");
            if (!temp.isValid())
                temp = QDate::fromString(input_value, "dddd");
#endif
            if (temp.isValid())
                day_value = temp.dayOfWeek();
            else
                day_value = englishDayOfWeek(input_value);
            if (day_value < 1)
                return QDateTime();
        }

        // Don't use Julian days here since support for years < 1 is bad
        // Use 1900-01-01 instead (a Monday)
        QDate result_date = QDate(1900, 1, 1).addDays(day_value - 1);
        QTime result_time = QTime(0, 0, 0, 0);
        return QDateTime(result_date, result_time);
    }
    virtual bool isInvalid(int row) const
    {
        const AbstractColumn *col = d_inputs.value(0);
        if (!col)
            return false;
        return !(dateTimeAt(row).isValid()) || col->isInvalid(row);
    }
    virtual bool isInvalid(Interval<int> i) const
    {
        if (!d_inputs.value(0))
            return false;
        for (int row = i.start(); row <= i.end(); row++) {
            if (!isInvalid(row))
                return false;
        }
        return true;
    }
    virtual QList<Interval<int>> invalidIntervals() const
    {
        IntervalAttribute<bool> validity;
        if (d_inputs.value(0)) {
            int rows = d_inputs.value(0)->rowCount();
            for (int i = 0; i < rows; i++)
                validity.setValue(i, isInvalid(i));
        }
        return validity.intervals();
    }

    //! Return the data type of the column
    virtual SciDAVis::ColumnDataType dataType() const { return SciDAVis::TypeQDateTime; }

    //! 1 (Monday) .. 7 (Sunday) for an English day name, full ("Monday") or short ("Mon"), in
    //! any letter case; 0 otherwise. QDate::fromString() above reads the names of the system
    //! locale only, so on a Japanese PC "Monday" was not a day of the week.
    static int englishDayOfWeek(const QString &text)
    {
        const QString t = text.trimmed();
        if (t.isEmpty())
            return 0;
        const QLocale c = QLocale::c();
        for (int d = 1; d <= 7; ++d) {
            if (t.compare(c.dayName(d, QLocale::LongFormat), Qt::CaseInsensitive) == 0
                || t.compare(c.dayName(d, QLocale::ShortFormat), Qt::CaseInsensitive) == 0)
                return d;
        }
        return 0;
    }

protected:
    virtual bool inputAcceptable(int, const AbstractColumn *source)
    {
        return source->dataType() == SciDAVis::TypeQString;
    }
};

#endif // ifndef STRING2DAYOFWEEK_FILTER_H
