/***************************************************************************
    File                 : NumericDateTimeBaseFilter.cpp
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

#include "NumericDateTimeBaseFilter.h"
#include <QTimeZone>
#include <math.h>

// 1970-01-01 00:00 as wall-clock time. It used to be QDateTime::fromMSecsSinceEpoch(0), the epoch
// instant in the PC's zone (1970-01-01 09:00 on a JST PC), so that whole days fell on 09:00.
const QDateTime NumericDateTimeBaseFilter::zeroOffsetDate =
        QDateTime(QDate(1970, 1, 1), QTime(0, 0), Qt::UTC);
static const double milliSecondsInDay = 86400000.0;

void NumericDateTimeBaseFilter::writeExtraAttributes(QXmlStreamWriter *writer) const
{
    // the wall-clock base as milliseconds since 1970-01-01 00:00 (zone-free); "base_wall" tells
    // load() that the number is not an instant in the PC's zone (files written before it)
    writer->writeAttribute("base_datetime", QString::number(m_date_time_0.toMSecsSinceEpoch()));
    writer->writeAttribute("base_wall", QStringLiteral("1"));
    writer->writeAttribute("unit", QString::number(static_cast<int>(m_unit_interval)));
}

bool NumericDateTimeBaseFilter::load(XmlStreamReader *reader)
{
    QXmlStreamAttributes attribs = reader->attributes();
    QString base_datetimeStr = attribs.value(reader->namespaceUri().toString(), "base_datetime").toString();
    QString unitStr = attribs.value(reader->namespaceUri().toString(), "unit").toString();
    const bool baseIsWall =
            attribs.value(reader->namespaceUri().toString(), "base_wall").toString()
            == QLatin1String("1");
    if (AbstractSimpleFilter::load(reader)) {
        bool ok;
        UnitInterval unit = static_cast<UnitInterval>(unitStr.toInt(&ok));
        if (!ok)
            {
                reader->raiseError(tr("NumericDateTimeBaseFilter: invalid unit, defaulting to years"));
                unit = UnitInterval::Year;
            }
        setUnitInterval(unit);
        qint64 msecs = base_datetimeStr.toLongLong(&ok);
        if (!ok)
            {
                reader->raiseError(tr("NumericDateTimeBaseFilter: invalid offset, defaulting to zero"));
                msecs = 0;
            }
        QDateTime base_datetime;
        if (baseIsWall)
            base_datetime = QDateTime::fromMSecsSinceEpoch(msecs, Qt::UTC);
        else if (msecs == 0)
            // older files: 0 was the default base (the epoch instant), which is now the
            // wall-clock 1970-01-01 00:00
            base_datetime = zeroOffsetDate;
        else
            // older files: an instant in the PC's zone; its local fields are the clock time the
            // user chose in the column dialog
            base_datetime = QDateTime::fromMSecsSinceEpoch(msecs);
        setBaseDateTime(base_datetime);
    } else
        return false;

    return !reader->hasError();
}

void NumericDateTimeBaseFilter::setUnitInterval(const UnitInterval unit)
{
    // if unit is valid, assign and return
    switch (unit) {
    case UnitInterval::Day:
    case UnitInterval::Hour:
    case UnitInterval::Millisecond:
    case UnitInterval::Minute:
    case UnitInterval::Month:
    case UnitInterval::Second:
    case UnitInterval::Year:
        m_unit_interval = unit;
        return;
    }
    // if not, assign default value
    m_unit_interval = UnitInterval::Day;
}

void NumericDateTimeBaseFilter::setBaseDateTime(const QDateTime &date)
{
    m_date_time_0 = wallClock(date.isValid() ? date : zeroOffsetDate);
}

double NumericDateTimeBaseFilter::offsetToDouble(const QDateTime &input) const
{
    if (!input.isValid() || !m_date_time_0.isValid())
        return 0.0;
    // zone-free arithmetic on the date and time fields (see the header)
    const QDateTime tDateTime = wallClock(input);
    double retVal = 0.0;
    switch (m_unit_interval) {
    case UnitInterval::Year: {
        // brutto
        int years = tDateTime.date().year() - m_date_time_0.date().year();
        // consider base date 2020/12/15 and tDateTime 2022/01/01, got 2 years in the line above
        // but in fact it is only one full year + some days, therefore value is to be corrected
        if (tDateTime < m_date_time_0.addYears(years))
            years -= 1;
        retVal += years;
        auto previousYearOffset = m_date_time_0.addYears(years);
        auto nextYearOffset = m_date_time_0.addYears(years + 1);
        auto daysToNextYear = previousYearOffset.daysTo(nextYearOffset);
        if (daysToNextYear > 0)
            retVal += double(previousYearOffset.msecsTo(tDateTime)) / double(daysToNextYear)
                    / milliSecondsInDay;
        return retVal;
    }
    case UnitInterval::Month: {
        // brutto, including the whole years in between (the month numbers alone gave
        // 2020-01-01 -> 2021-03-01 as 2 months instead of 14)
        int months = (tDateTime.date().year() - m_date_time_0.date().year()) * 12
                + (tDateTime.date().month() - m_date_time_0.date().month());
        // as for years: base 2020/01/15 and tDateTime 2020/03/01 is one full month + some days
        if (tDateTime < m_date_time_0.addMonths(months))
            months -= 1;
        retVal += months;
        auto previousMonthsOffset = m_date_time_0.addMonths(months);
        auto nextMonthsOffset = m_date_time_0.addMonths(months + 1);
        auto daysToNextMonth = previousMonthsOffset.daysTo(nextMonthsOffset);
        if (daysToNextMonth > 0)
            retVal += double(previousMonthsOffset.msecsTo(tDateTime)) / double(daysToNextMonth)
                    / milliSecondsInDay;
        return retVal;
    }
    case UnitInterval::Day: {
        qint64 days = m_date_time_0.date().daysTo(tDateTime.date());
        if (tDateTime < m_date_time_0.addDays(days))
            days -= 1;
        retVal += days;
        retVal += double(m_date_time_0.addDays(days).msecsTo(tDateTime)) / milliSecondsInDay;
        return retVal;
    }
    case UnitInterval::Hour:
        return double(m_date_time_0.msecsTo(tDateTime)) / 1000.0 / 3600.0;
    case UnitInterval::Minute:
        return double(m_date_time_0.msecsTo(tDateTime)) / 1000.0 / 60.0;
    case UnitInterval::Second:
        return double(m_date_time_0.msecsTo(tDateTime)) / 1000.0;
    case UnitInterval::Millisecond:
        return double(m_date_time_0.msecsTo(tDateTime));
    }
    // return default value
    return retVal;
}

QDateTime NumericDateTimeBaseFilter::makeDateTime(double input_value) const
{
    if (!std::isfinite(input_value) || !m_date_time_0.isValid())
        return QDateTime();
    const double floored = std::floor(input_value);
    // Reject values outside qint64/QDateTime representable range to avoid UB in float->int casts.
    if (std::fabs(floored) > 1e15)
        return QDateTime();
    QDateTime dateTime = m_date_time_0;
    const qint64 fullUnits = static_cast<qint64>(floored);
    qint64 residual = 0;
    switch (m_unit_interval) {
    // add full number of units to dateTime
    case UnitInterval::Year: {
        if (fullUnits < -10000000LL || fullUnits > 10000000LL)
            return QDateTime();
        dateTime = m_date_time_0.addYears(static_cast<int>(fullUnits));
        const QDateTime nextYear = m_date_time_0.addYears(static_cast<int>(fullUnits) + 1);
        const qint64 daysToNextYear = dateTime.daysTo(nextYear);
        if (daysToNextYear > 0)
            residual = qRound64((input_value - double(fullUnits)) * double(daysToNextYear)
                                * milliSecondsInDay);
        break;
    }
    case UnitInterval::Month: {
        if (fullUnits < -100000000LL || fullUnits > 100000000LL)
            return QDateTime();
        dateTime = m_date_time_0.addMonths(static_cast<int>(fullUnits));
        const QDateTime nextMonth = m_date_time_0.addMonths(static_cast<int>(fullUnits) + 1);
        const qint64 daysToNextMonth = dateTime.daysTo(nextMonth);
        if (daysToNextMonth > 0)
            residual = qRound64((input_value - double(fullUnits)) * double(daysToNextMonth)
                                * milliSecondsInDay);
        break;
    }
    case UnitInterval::Day: {
        if (fullUnits < -3650000000LL || fullUnits > 3650000000LL)
            return QDateTime();
        dateTime = dateTime.addDays(fullUnits);
        residual = qRound64((input_value - double(fullUnits)) * milliSecondsInDay);
        break;
    }
    case UnitInterval::Hour: {
        const double ms = input_value * 3600.0 * 1000.0;
        if (!std::isfinite(ms) || std::fabs(ms) > 1e16)
            return QDateTime();
        residual = qRound64(ms);
        break;
    }
    case UnitInterval::Minute: {
        const double ms = input_value * 60.0 * 1000.0;
        if (!std::isfinite(ms) || std::fabs(ms) > 1e16)
            return QDateTime();
        residual = qRound64(ms);
        break;
    }
    case UnitInterval::Second: {
        const double ms = input_value * 1000.0;
        if (!std::isfinite(ms) || std::fabs(ms) > 1e16)
            return QDateTime();
        residual = qRound64(ms);
        break;
    }
    case UnitInterval::Millisecond: {
        if (std::fabs(input_value) > 1e16)
            return QDateTime();
        residual = qRound64(input_value);
        break;
    }
    }
    // add the residual difference in dates as milliseconds; back to the local wall-clock time the
    // cells of a date/time column hold
    dateTime = dateTime.addMSecs(residual);
    return localWallClock(dateTime);
}

namespace {
//! The clock time `wall` (its date and time fields) in a zone, built by `make(date, time)`. A clock
//! time that does not exist there falls in the hour skipped when DST starts (02:00-02:59 on the
//! day clocks go forward in New York); Qt 5 makes such a date/time invalid, which left an empty
//! cell. It is kept as written instead, with the offset in force before the gap
//! (Qt::OffsetFromUTC): its date and time fields stay 02:30, which is what the cell shows and what
//! the conversion to a number reads, so a number -> date/time -> number round trip keeps the
//! value. Moving it forward by the gap (02:30 -> 03:30) made 02:00-02:59 coincide with the real
//! 03:00-03:59 and turned 2.5 h into 3.5 h on the way back.
template<typename Make>
QDateTime existingClockTime(const QDateTime &wall, Make make)
{
    const QDateTime dt = make(wall.date(), wall.time());
    if (dt.isValid() || !wall.isValid())
        return dt;
    int offsetSecs = 0;
    for (int days = 1; days <= 3; ++days) {
        const QDateTime before = make(wall.date().addDays(-days), wall.time());
        if (before.isValid()) {
            offsetSecs = before.offsetFromUtc();
            break;
        }
    }
    return QDateTime(wall.date(), wall.time(), Qt::OffsetFromUTC, offsetSecs);
}
} // namespace

QDateTime NumericDateTimeBaseFilter::localWallClock(const QDateTime &wall)
{
    return existingClockTime(wall, [](const QDate &d, const QTime &t) { return QDateTime(d, t); });
}

QDateTime NumericDateTimeBaseFilter::wallClockInZone(const QDateTime &wall, const QTimeZone &zone)
{
    return existingClockTime(
            wall, [&zone](const QDate &d, const QTime &t) { return QDateTime(d, t, zone); });
}
