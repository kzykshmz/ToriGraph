/***************************************************************************
        File                 : NumericDateTimeBaseFilter.h
        Project              : SciDAVis
        --------------------------------------------------------------------
        Copyright            : (C) 2021 by Suthiro
        Email (use @ for *)  : soviet_man*list.ru
        Description          : Base for conversion filters double <-> QDateTime
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
#ifndef NUMERIC_DATETIME_BASE_FILTER_H
#define NUMERIC_DATETIME_BASE_FILTER_H

#include "../AbstractSimpleFilter.h"
#include <QDateTime>
#include <QDate>
#include <QTime>

class QTimeZone;

/**
 * \brief Base for conversion filters double <-> QDateTime.
 * This class is intended to hold all logic for conversion between double to QDateTime and vice
 * versa.
 */

class NumericDateTimeBaseFilter : public AbstractSimpleFilter
{
    Q_OBJECT
public:
    // default offset date (zero offset): 1970-01-01 00:00 wall-clock time (Qt::UTC)
    static const QDateTime zeroOffsetDate;
    // scoped enumeration to prevent potential mixing with many other similiar names
    enum class UnitInterval : int { Year, Month, Day, Hour, Minute, Second, Millisecond };

    NumericDateTimeBaseFilter(const UnitInterval unit = UnitInterval::Day,
                              const QDateTime &date_time_0 = zeroOffsetDate)
        : m_unit_interval(unit),
          m_date_time_0(wallClock(date_time_0.isValid() ? date_time_0 : zeroOffsetDate)){};

    NumericDateTimeBaseFilter(const NumericDateTimeBaseFilter &other)
        : m_unit_interval(other.m_unit_interval), m_date_time_0(other.m_date_time_0){};

    //! Save to XML
    void writeExtraAttributes(QXmlStreamWriter *writer) const override;

    bool load(XmlStreamReader *reader) override;

    UnitInterval getUnitInterval() const { return m_unit_interval; }
    //! The base date/time as wall-clock time in the PC's zone (like the cells of a column).
    QDateTime getBaseDateTime() const { return localWallClock(m_date_time_0); }

    // Date/time columns hold wall-clock times without a zone. The conversions therefore work on
    // the date and time fields (as if they were UTC), never on the instant in the PC's zone: with
    // the default base, 2024-03-01 00:00 is the whole number 19783 and 0 is 1970-01-01 00:00 on
    // every PC, and a day is always 24 h (no DST jump).

    // convert the given date to double wrt unit, offset and base date
    double offsetToDouble(const QDateTime &m_offset) const;
    // convert the given numerical offset to DateTime wrt unit and base date
    QDateTime makeDateTime(double input_value) const;

    //! The wall-clock time `wall` (its date and time fields) as a local date/time, the form the
    //! cells of a date/time column hold. A clock time that does not exist locally (skipped when
    //! DST starts) is moved forward by the gap instead of becoming invalid (an empty cell).
    static QDateTime localWallClock(const QDateTime &wall);
    //! The same in the time zone `zone` (localWallClock() uses the PC's zone); lets the DST gap
    //! be checked on a PC whose own zone has none.
    static QDateTime wallClockInZone(const QDateTime &wall, const QTimeZone &zone);

protected:
    //! `dt`'s date and time fields as a Qt::UTC date/time (the zone-free wall-clock time).
    static QDateTime wallClock(const QDateTime &dt)
    {
        return QDateTime(dt.date(), dt.time(), Qt::UTC);
    }

private:
    UnitInterval m_unit_interval;
    QDateTime m_date_time_0;

    void setUnitInterval(const UnitInterval unit);
    void setBaseDateTime(const QDateTime &date);
};

#endif // ifndef NUMERIC_DATETIME_BASE_FILTER_H