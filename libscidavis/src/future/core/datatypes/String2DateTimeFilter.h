/***************************************************************************
    File                 : String2DateTimeFilter.h
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
#ifndef STRING2DATE_TIME_FILTER_H
#define STRING2DATE_TIME_FILTER_H

#include "core/AbstractSimpleFilter.h"
#include <QDateTime>
#include <QDate>
#include <QTime>

/**
 * \brief Conversion filter QString -> QDateTime.
 *
 * The standard use of this filter is explicitly specifiying the date/time format of the strings
 * on the input, either in the constructor or via setFormat().
 * However, if the input fails to comply to this format, String2DateTimeFilter
 * tries to guess the format, using internal lists of common date and time formats (#date_formats
 * and #time_formats).
 */
class String2DateTimeFilter : public AbstractSimpleFilter
{
    Q_OBJECT

public:
    //! Standard constructor.
    explicit String2DateTimeFilter(QString format = "yyyy-MM-dd hh:mm:ss.zzz")
        : d_format(format) { }
    //! Set the format string to be used for conversion.
    void setFormat(const QString &format);
    //! Return the format string
    /**
     * The default format string is "yyyy-MM-dd hh:mm:ss.zzz".
     * \sa QDate::toString()
     */
    QString format() const { return d_format; }

    //! \name UTC offset of the date/time values of the column
    //! The first ISO 8601 value with a zone designator ("Z", "+09:00", ...) that this filter
    //! reads fixes the offset; a later value in another offset is converted to it, so the same
    //! instant gets the same wall-clock time. Values without a designator are wall-clock times in
    //! that offset and are not converted.
    //@{
    bool hasZoneOffset() const { return d_has_zone_offset; }
    //! Offset east of UTC in seconds (meaningful if hasZoneOffset())
    int zoneOffsetSeconds() const { return d_zone_offset_secs; }
    //! Take over the offset of another filter (the converter of a column type change)
    void copyZoneOffset(const String2DateTimeFilter &other)
    {
        d_has_zone_offset = other.d_has_zone_offset;
        d_zone_offset_secs = other.d_zone_offset_secs;
    }
    //@}

    //! Return the data type of the column
    virtual SciDAVis::ColumnDataType dataType() const { return SciDAVis::TypeQDateTime; }

    //! \name XML related functions
    //@{
    virtual void writeExtraAttributes(QXmlStreamWriter *writer) const;
    virtual bool load(XmlStreamReader *reader);
    //@}

signals:
    void formatChanged();

private:
    friend class String2DateTimeFilterSetFormatCmd;
    //! The format string.
    QString d_format;
    //! Whether d_zone_offset_secs was fixed by a value with a zone designator
    mutable bool d_has_zone_offset = false;
    //! UTC offset (seconds east) of the first value with a zone designator
    mutable int d_zone_offset_secs = 0;

    static const QStringList date_formats;
    static const QStringList time_formats;

public:
    virtual QDateTime dateTimeAt(int row) const;
    //! The clock time `text` spells, as a Qt::UTC date/time (zone-free; invalid if it cannot be
    //! read). dateTimeAt() turns it into the local date/time of the cell with
    //! NumericDateTimeBaseFilter::localWallClock(). Reading a zone designator fixes the offset of
    //! the column like dateTimeAt() does.
    QDateTime wallClockFromText(const QString &text) const;
    virtual QDate dateAt(int row) const { return dateTimeAt(row).date(); }
    virtual QTime timeAt(int row) const { return dateTimeAt(row).time(); }
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

protected:
    //! Using typed ports: only string inputs are accepted.
    virtual bool inputAcceptable(int, const AbstractColumn *source)
    {
        return source->dataType() == SciDAVis::TypeQString;
    }
};

class String2DateTimeFilterSetFormatCmd : public QUndoCommand
{
public:
    String2DateTimeFilterSetFormatCmd(String2DateTimeFilter *target, const QString &new_format);

    virtual void redo();
    virtual void undo();

private:
    String2DateTimeFilter *d_target;
    QString d_other_format;
};

#endif // ifndef STRING2DATE_TIME_FILTER_H
