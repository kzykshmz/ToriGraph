/***************************************************************************
    File                 : String2DateTimeFilter.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Tilman Benkert,
                           Knut Franke
    Email (use @ for *)  : thzs*gmx.net, knut.franke*gmx.de
    Description          : Conversion filter QString -> QDateTime.

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
#include "String2DateTimeFilter.h"
#include <QRegularExpression>
#include <QStringList>
#include "lib/XmlStreamReader.h"
#include <QXmlStreamWriter>

namespace {
//! ISO 8601 date and time as written by Python's datetime.isoformat() and most loggers:
//! "2023-12-31T23:59:59", "...T23:59:59.123456", "...T23:59:59Z", "...T23:59:59+09:00",
//! the basic form "20231231T235959", and a time of day with more than three decimals.
//!
//! Only strings the older fallback below cannot read are taken here: a 'T' between date and
//! time, a time zone designator, or a fraction of a second longer than milliseconds. A date and
//! time separated by a space keeps its old parsing.
//!
//! Time zones: date/time columns hold wall-clock times without a zone (QDateTime in local time
//! built from date and time fields; the numeric conversions use those fields), so the clock time
//! written in the string is kept and the designator is checked but not applied. Converting to the
//! PC's zone would make the same file show other times on another PC.
QDateTime parseIso8601DateTime(const QString &input)
{
    static const QRegularExpression extended(QStringLiteral(
            "^(?:(\\d{4})-(\\d{1,2})-(\\d{1,2})([Tt ]))?"
            "(\\d{1,2}):(\\d{2})(?::(\\d{2})(?:[.,](\\d+))?)?"
            "\\s*([Zz]|[+-]\\d{2}(?::?\\d{2})?)?$"));
    static const QRegularExpression basic(QStringLiteral(
            "^(\\d{4})(\\d{2})(\\d{2})([Tt])(\\d{2})(\\d{2})(?:(\\d{2})(?:[.,](\\d+))?)?"
            "([Zz]|[+-]\\d{2}(?::?\\d{2})?)?$"));
    QRegularExpressionMatch m = extended.match(input);
    if (!m.hasMatch())
        m = basic.match(input);
    if (!m.hasMatch())
        return QDateTime();

    const bool hasDate = !m.captured(1).isEmpty();
    const bool sepIsT = m.captured(4).compare(QStringLiteral("T"), Qt::CaseInsensitive) == 0;
    const QString fraction = m.captured(8);
    const QString zone = m.captured(9);
    if (!sepIsT && zone.isEmpty() && fraction.size() <= 3)
        return QDateTime(); // the older formats read this one

    if (!zone.isEmpty() && zone.compare(QStringLiteral("Z"), Qt::CaseInsensitive) != 0) {
        const int zoneHours = zone.mid(1, 2).toInt();
        const int zoneMinutes = zone.size() > 3 ? zone.right(2).toInt() : 0;
        if (zoneHours > 23 || zoneMinutes > 59)
            return QDateTime();
    }

    QDate date(1900, 1, 1); // a time of day alone: the date the older fallback uses
    if (hasDate) {
        date = QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
        if (!date.isValid())
            return QDateTime();
    }
    const QTime time(m.captured(5).toInt(), m.captured(6).toInt(),
                     m.captured(7).isEmpty() ? 0 : m.captured(7).toInt());
    if (!time.isValid())
        return QDateTime();
    QDateTime result(date, time);
    if (!fraction.isEmpty()) {
        // fraction of a second -> milliseconds, rounded (".5" is 500 ms, ".123456" is 123 ms)
        const qint64 msecs = qRound64((QStringLiteral("0.") + fraction).toDouble() * 1000.0);
        result = result.addMSecs(msecs);
    }
    return result;
}
} // namespace

const QStringList String2DateTimeFilter::date_formats = {
    "yyyy-M-d", // ISO 8601 w/ and w/o leading zeros
    "yyyy/M/d",
    "d/M/yyyy", // European style day/month order (this order seems to be used in more countries
                // than the US style M/d/yyyy)
    "d/M/yy", "d-M-yyyy", "d-M-yy",
    "d.M.yyyy", // German style
    "d.M.yy", "M/yyyy",
    "d.M.", // German form w/o year
    "yyyyMMdd"
};

const QStringList String2DateTimeFilter::time_formats = {
    "h", "h ap", "h:mm", "h:mm ap", "h:mm:ss", "h:mm:ss.zzz", "h:mm:ss:zzz", "mm:ss.zzz", "hmmss"
};

QDateTime String2DateTimeFilter::dateTimeAt(int row) const
{
    if (!d_inputs.value(0))
        return QDateTime();
    QString input_value = d_inputs.value(0)->textAt(row);
    if (input_value.isEmpty())
        return QDateTime();

    // first try the selected format string d_format
    QDateTime result = QDateTime::fromString(input_value, d_format);
    if (result.isValid())
        return result;

    // ISO 8601 with 'T', a time zone or sub-millisecond digits (see parseIso8601DateTime)
    const QDateTime iso = parseIso8601DateTime(input_value.trimmed());
    if (iso.isValid())
        return iso;

        // fallback:
        // try other format strings built from date_formats and time_formats
        // comma and space are valid separators between date and time
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList strings = input_value.simplified().split(",", Qt::SkipEmptyParts);
    if (strings.size() == 1)
        strings = strings.at(0).split(" ", Qt::SkipEmptyParts);
#else
    QStringList strings = input_value.simplified().split(",", QString::SkipEmptyParts);
    if (strings.size() == 1)
        strings = strings.at(0).split(" ", QString::SkipEmptyParts);
#endif

    if (strings.size() < 1)
        return result; // invalid date/time from first attempt

    QDate date_result;
    QTime time_result;

    QString date_string = strings.at(0).trimmed();
    QString time_string;
    if (strings.size() > 1)
        time_string = strings.at(1).trimmed();
    else
        time_string = date_string;

    // try to find a valid date
    for (const auto& format : date_formats) {
        date_result = QDate::fromString(date_string, format);
        if (date_result.isValid())
            break;
    }
    // try to find a valid time
    for (const auto& format : time_formats) {
        time_result = QTime::fromString(time_string, format);
        if (time_result.isValid())
            break;
    }

    if (!date_result.isValid() && time_result.isValid())
        date_result.setDate(1900, 1, 1); // this is what QDateTime does e.g. for
                                         // QDateTime::fromString("00:00","hh:mm");
    return QDateTime(date_result, time_result);
}

void String2DateTimeFilter::writeExtraAttributes(QXmlStreamWriter *writer) const
{
    writer->writeAttribute("format", format());
}

bool String2DateTimeFilter::load(XmlStreamReader *reader)
{
    QXmlStreamAttributes attribs = reader->attributes();
    QString str = attribs.value(reader->namespaceUri().toString(), "format").toString();

    if (AbstractSimpleFilter::load(reader))
        setFormat(str);
    else
        return false;

    return !reader->hasError();
}

void String2DateTimeFilter::setFormat(const QString &format)
{
    exec(new String2DateTimeFilterSetFormatCmd(this, format));
}

String2DateTimeFilterSetFormatCmd::String2DateTimeFilterSetFormatCmd(String2DateTimeFilter *target,
                                                                     const QString &new_format)
    : d_target(target), d_other_format(new_format)
{
    if (d_target->parentAspect())
        setText(QObject::tr("%1: set date-time format to %2")
                        .arg(d_target->parentAspect()->name())
                        .arg(new_format));
    else
        setText(QObject::tr("set date-time format to %1").arg(new_format));
}

void String2DateTimeFilterSetFormatCmd::redo()
{
    QString tmp = d_target->d_format;
    d_target->d_format = d_other_format;
    d_other_format = tmp;
    emit d_target->formatChanged();
}

void String2DateTimeFilterSetFormatCmd::undo()
{
    redo();
}
