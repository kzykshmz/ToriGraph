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
//! A date and time read by parseIso8601DateTime()
struct IsoDateTime
{
    //! wall-clock time as written (Qt::UTC, so that adding seconds ignores the PC's DST)
    QDateTime wall;
    //! a zone designator was given
    bool hasZone = false;
    //! offset east of UTC in seconds ("Z": 0)
    int offsetSecs = 0;
};

//! ISO 8601 date and time as written by Python's datetime.isoformat() and most loggers:
//! "2023-12-31T23:59:59", "...T23:59:59.123456", "...T23:59:59Z", "...T23:59:59+09:00",
//! the basic form "20231231T235959", and a time of day with more than three decimals.
//!
//! Only strings the older fallback below cannot read are taken here: a 'T' between date and
//! time, a time zone designator after a date, or a fraction of a second (any number of
//! digits, "." or ","; the fallback reads exactly three digits after "."). A date and time
//! separated by a space without a fraction or zone keeps its old parsing.
//!
//! A zone designator is read only after a date and 'T' (or in the basic form), or after a date
//! and a time with seconds separated by a space: "2024-01-02 09:00:00+09:00",
//! "2024-01-02 09:00:00.123456+09:00" and "2024-01-02 09:00:00Z" are what Python's
//! str(datetime) and pandas write. After a time of day alone or a space-separated date and a
//! time without seconds, "+hh:mm" / "-hh:mm" is far more likely a time range ("09:00-17:00",
//! "2024-01-02 09:00-10:30") and is left to the older parsing, which rejects it.
//!
//! Time zones: date/time columns hold wall-clock times without a zone (QDateTime in local time
//! built from date and time fields; the numeric conversions use those fields). The offset is
//! therefore not applied against the PC's zone (the same file would show other times on another
//! PC); String2DateTimeFilter::dateTimeAt() keeps the clock time as written and only converts
//! values whose offset differs from the first offset of the column.
IsoDateTime parseIso8601DateTime(const QString &input)
{
    static const QRegularExpression extended(QStringLiteral(
            "^(?:(\\d{4})-(\\d{1,2})-(\\d{1,2})([Tt ]))?"
            "(\\d{1,2}):(\\d{2})(?::(\\d{2})(?:[.,](\\d+))?)?"
            "\\s*([Zz]|[+-]\\d{2}(?::?\\d{2})?)?$"));
    static const QRegularExpression basic(QStringLiteral(
            "^(\\d{4})(\\d{2})(\\d{2})([Tt])(\\d{2})(\\d{2})(?:(\\d{2})(?:[.,](\\d+))?)?"
            "([Zz]|[+-]\\d{2}(?::?\\d{2})?)?$"));
    IsoDateTime out;
    QRegularExpressionMatch m = extended.match(input);
    if (!m.hasMatch())
        m = basic.match(input);
    if (!m.hasMatch())
        return out;

    const bool hasDate = !m.captured(1).isEmpty();
    const bool sepIsT = m.captured(4).compare(QStringLiteral("T"), Qt::CaseInsensitive) == 0;
    const QString fraction = m.captured(8);
    const QString zone = m.captured(9);
    const bool hasSeconds = !m.captured(7).isEmpty();
    if (!zone.isEmpty() && !(hasDate && (sepIsT || hasSeconds)))
        return out; // "09:00-17:00", "2024-01-02 09:00-10:30": not a zone, the older formats decide
    if (!sepIsT && zone.isEmpty() && fraction.isEmpty())
        return out; // the older formats read this one

    if (!zone.isEmpty()) {
        out.hasZone = true;
        if (zone.compare(QStringLiteral("Z"), Qt::CaseInsensitive) != 0) {
            const int zoneHours = zone.mid(1, 2).toInt();
            const int zoneMinutes = zone.size() > 3 ? zone.right(2).toInt() : 0;
            if (zoneHours > 23 || zoneMinutes > 59)
                return out;
            out.offsetSecs = (zoneHours * 60 + zoneMinutes) * 60;
            if (zone.startsWith(QLatin1Char('-')))
                out.offsetSecs = -out.offsetSecs;
        }
    }

    QDate date(1900, 1, 1); // a time of day alone: the date the older fallback uses
    if (hasDate) {
        date = QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
        if (!date.isValid())
            return out;
    }
    const int seconds = hasSeconds ? m.captured(7).toInt() : 0;
    // a leap second (23:59:60) is kept on its own day as the last millisecond before the next
    // second (23:59:59.999), so that it still sorts between 23:59:59 and the next day's 00:00
    const bool leapSecond = (seconds == 60);
    const QTime time(m.captured(5).toInt(), m.captured(6).toInt(), leapSecond ? 59 : seconds,
                     leapSecond ? 999 : 0);
    if (!time.isValid())
        return out;
    QDateTime result(date, time, Qt::UTC);
    if (!fraction.isEmpty() && !leapSecond) {
        // fraction of a second -> milliseconds, rounded (".5" is 500 ms, ".123456" is 123 ms)
        const qint64 msecs = qRound64((QStringLiteral("0.") + fraction).toDouble() * 1000.0);
        result = result.addMSecs(msecs);
    }
    out.wall = result;
    return out;
}

//! A time of day for the fallback parsing: String2DateTimeFilter::time_formats, plus a fraction of
//! a second with any number of digits after "." or "," ("10:00:00.5" is 500 ms; the "zzz" of the
//! formats takes exactly three digits), a leap second ("23:59:60" -> 23:59:59.999) and a 12-hour
//! clock with am / pm.
QTime parseTimeOfDay(const QString &text, const QStringList &formats)
{
    // "10:00:15 pm", "10:00 AM", "10:00:00.5 p.m.": the 12-hour clock in English whatever the
    // locale (QTime's "ap" follows the locale's AM/PM texts)
    static const QRegularExpression ampm(
            QStringLiteral("^(\\d{1,2}(?::\\d{2}(?::\\d{2}(?:[.,]\\d+)?)?)?)"
                           "\\s*([AaPp])\\.?[Mm]\\.?$"));
    const QRegularExpressionMatch am = ampm.match(text);
    if (am.hasMatch()) {
        QString clock = am.captured(1);
        if (!clock.contains(QLatin1Char(':')))
            clock += QStringLiteral(":00");
        const QTime t = parseTimeOfDay(clock, formats);
        if (!t.isValid() || t.hour() < 1 || t.hour() > 12)
            return QTime();
        const bool pm = am.captured(2).compare(QStringLiteral("p"), Qt::CaseInsensitive) == 0;
        const int hour = (t.hour() % 12) + (pm ? 12 : 0);
        return QTime(hour, t.minute(), t.second(), t.msec());
    }
    static const QRegularExpression leap(QStringLiteral("^(\\d{1,2}:\\d{2}):60(?:[.,]\\d+)?$"));
    const QRegularExpressionMatch lm = leap.match(text);
    if (lm.hasMatch()) {
        const QTime t = QTime::fromString(lm.captured(1), QStringLiteral("h:mm"));
        return t.isValid() ? QTime(t.hour(), t.minute(), 59, 999) : QTime();
    }
    static const QRegularExpression frac(
            QStringLiteral("^(\\d{1,2}:\\d{2}(?::\\d{2})?)[.,](\\d+)$"));
    const QRegularExpressionMatch fm = frac.match(text);
    if (fm.hasMatch()) {
        const QString whole = fm.captured(1);
        // "h:mm:ss.f" with seconds; "mm:ss.f" (the "mm:ss.zzz" format) without
        QTime t = QTime::fromString(whole, QStringLiteral("h:mm:ss"));
        if (!t.isValid() && whole.count(QLatin1Char(':')) == 1)
            t = QTime::fromString(QStringLiteral("0:") + whole, QStringLiteral("h:mm:ss"));
        if (!t.isValid())
            return QTime();
        const double fraction = (QStringLiteral("0.") + fm.captured(2)).toDouble();
        return t.addMSecs(qMin(999, int(qRound64(fraction * 1000.0))));
    }
    for (const auto &format : formats) {
        const QTime t = QTime::fromString(text, format);
        if (t.isValid())
            return t;
    }
    return QTime();
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
    const IsoDateTime iso = parseIso8601DateTime(input_value.trimmed());
    if (iso.wall.isValid()) {
        QDateTime wall = iso.wall;
        if (iso.hasZone) {
            // The first offset seen fixes the offset of the column; a value written in another
            // offset is moved to it so that the same instant gets the same clock time and the
            // column sorts by instant ("...T15:00:00Z" and "...T00:00:00+09:00" of the next day
            // are one instant). A column with a single offset keeps the times as written.
            if (!d_has_zone_offset) {
                d_has_zone_offset = true;
                d_zone_offset_secs = iso.offsetSecs;
            } else if (iso.offsetSecs != d_zone_offset_secs) {
                wall = wall.addSecs(d_zone_offset_secs - iso.offsetSecs);
            }
        }
        return QDateTime(wall.date(), wall.time()); // local wall-clock time like the other paths
    }

    // fallback:
    // try other format strings built from date_formats and time_formats
    // comma and space are valid separators between date and time; a comma between the seconds
    // and their fraction ("10:00:00,5") is not
    QString text = input_value.simplified();
    static const QRegularExpression decimalComma(QStringLiteral("(\\d:\\d{2}:\\d{2}),(\\d)"));
    text.replace(decimalComma, QStringLiteral("\\1.\\2"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList strings = text.split(",", Qt::SkipEmptyParts);
    if (strings.size() == 1)
        strings = strings.at(0).split(" ", Qt::SkipEmptyParts);
#else
    QStringList strings = text.split(",", QString::SkipEmptyParts);
    if (strings.size() == 1)
        strings = strings.at(0).split(" ", QString::SkipEmptyParts);
#endif

    if (strings.size() < 1)
        return result; // invalid date/time from first attempt

    QDate date_result;
    const QString date_string = strings.at(0).trimmed();
    // The words after the date. A word in parentheses is a note, not a time: the day of the week
    // in "2024/01/02 (Tue)" or "2024/01/02 (Tue) 10:00", also in full-width parentheses.
    QStringList words;
    for (int i = 1; i < strings.size(); ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QStringList parts = strings.at(i).split(" ", Qt::SkipEmptyParts);
#else
        const QStringList parts = strings.at(i).split(" ", QString::SkipEmptyParts);
#endif
        for (const QString &word : parts) {
            const bool note = (word.startsWith(QLatin1Char('(')) && word.endsWith(QLatin1Char(')')))
                    || (word.startsWith(QChar(0xFF08)) && word.endsWith(QChar(0xFF09)));
            if (!note)
                words << word;
        }
    }
    // The time is the first word after the date, with the next word when that is am / pm
    // ("10:00 pm", "10:00:00.5 p.m."). Words after the time are dropped as before, e.g. a zone
    // name the conversion cannot use ("2024-01-02 10:00:00 UTC", "2024-01-02 10:00 JST"): the
    // clock time as written is kept.
    const bool hasTimePart = !words.isEmpty();
    QString time_string = date_string;
    if (hasTimePart) {
        time_string = words.at(0);
        static const QRegularExpression ampmWord(QStringLiteral("^[AaPp]\\.?[Mm]\\.?$"));
        if (words.size() > 1 && ampmWord.match(words.at(1)).hasMatch())
            time_string += QLatin1Char(' ') + words.at(1);
    }

    // try to find a valid date
    for (const auto& format : date_formats) {
        date_result = QDate::fromString(date_string, format);
        if (date_result.isValid())
            break;
    }
    // try to find a valid time
    const QTime time_result = parseTimeOfDay(time_string, time_formats);

    if (hasTimePart) {
        // A date and a time were written: both have to be readable. A time that is not
        // ("2024-03-01 25:00", "2024-03-01 abc", "2024-01-02 09:00-10:30") used to give the
        // date at 00:00 without notice; it is invalid (an empty cell) now.
        if (!date_result.isValid() || !time_result.isValid())
            return QDateTime();
        return QDateTime(date_result, time_result);
    }
    if (date_result.isValid()) // a date alone is the start of that day
        return QDateTime(date_result, time_result.isValid() ? time_result : QTime(0, 0));
    if (time_result.isValid())
        // a time alone on 1900-01-01: what QDateTime::fromString("00:00", "hh:mm") gives
        return QDateTime(QDate(1900, 1, 1), time_result);
    return QDateTime();
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
