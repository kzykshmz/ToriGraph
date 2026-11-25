/***************************************************************************
    File                 : Column.cpp
    Project              : SciDAVis
    Description          : Aspect that manages a column
    --------------------------------------------------------------------
    Copyright            : (C) 2007-2009 Tilman Benkert (thzs*gmx.net)
                           (replace * with @ in the email addresses)

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

#include "core/column/Column.h"
#include "core/column/ColumnPrivate.h"
#include "core/column/columncommands.h"
#include "core/datatypes/NumericDateTimeBaseFilter.h"
#include "lib/XmlStreamReader.h"
#include "IconLoader.h"
#include <QIcon>
#include <QXmlStreamWriter>
#include <QtDebug>

#include <algorithm>

namespace {
//! The intervals sorted by their start, overlapping and touching ones merged (invalid ones
//! dropped): the form in which masks are compared and combined in linear time.
QList<Interval<int>> normalizedIntervals(QList<Interval<int>> list)
{
    list.erase(std::remove_if(list.begin(), list.end(),
                              [](const Interval<int> &iv) { return iv.start() > iv.end(); }),
               list.end());
    std::sort(list.begin(), list.end(),
              [](const Interval<int> &a, const Interval<int> &b) { return a.start() < b.start(); });
    QList<Interval<int>> out;
    out.reserve(list.size());
    for (const Interval<int> &iv : list) {
        if (!out.isEmpty() && iv.start() <= out.last().end() + 1)
            out.last().setEnd(qMax(out.last().end(), iv.end()));
        else
            out.append(iv);
    }
    return out;
}

//! \c from minus \c minus; both normalized (see normalizedIntervals), so is the result
QList<Interval<int>> subtractedIntervals(const QList<Interval<int>> &from,
                                         const QList<Interval<int>> &minus)
{
    QList<Interval<int>> out;
    int j = 0;
    const int n = minus.size();
    for (const Interval<int> &iv : from) {
        int s = iv.start();
        const int e = iv.end();
        while (j < n && minus.at(j).end() < s)
            ++j;
        int k = j;
        while (k < n && minus.at(k).start() <= e) {
            if (minus.at(k).start() > s)
                out.append(Interval<int>(s, minus.at(k).start() - 1));
            s = qMax(s, minus.at(k).end() + 1);
            if (minus.at(k).end() >= e)
                break; // it may also cover the start of the next interval
            ++k;
        }
        if (s <= e)
            out.append(Interval<int>(s, e));
        j = k;
    }
    return out;
}
} // namespace

Column::Column(const QString &name, SciDAVis::ColumnMode mode) : AbstractColumn(name)
{
    d_column_private = new Private(this, mode);
    d_string_io = new ColumnStringIO(this);
    d_column_private->inputFilter()->input(0, d_string_io);
    outputFilter()->input(0, this);
    addChild(d_column_private->inputFilter());
    addChild(outputFilter());
}

template<>
void Column::initPrivate(std::unique_ptr<QVector<qreal>> d, IntervalAttribute<bool> v)
{
    d_column_private = new Private(this, SciDAVis::TypeDouble, SciDAVis::ColumnMode::Numeric, d.release(), v);
}

template<>
void Column::initPrivate(std::unique_ptr<QStringList> d, IntervalAttribute<bool> v)
{
    d_column_private =
            new Private(this, SciDAVis::TypeQString, SciDAVis::ColumnMode::Text, d.release(), v);
}

template<>
void Column::initPrivate(std::unique_ptr<QList<QDateTime>> d, IntervalAttribute<bool> v)
{
    d_column_private = new Private(this, SciDAVis::TypeQDateTime, SciDAVis::ColumnMode::DateTime,
                                   d.release(), v);
}

void Column::init()
{
    d_string_io = new ColumnStringIO(this);
    d_column_private->inputFilter()->input(0, d_string_io);
    outputFilter()->input(0, this);
    addChild(d_column_private->inputFilter());
    addChild(outputFilter());
}

Column::~Column()
{
    delete d_string_io;
    delete d_column_private;
}

void Column::setColumnMode(SciDAVis::ColumnMode mode, AbstractFilter *conversion_filter)
{
    if (mode != columnMode()) // mode changed
    {
        beginMacro(QObject::tr("%1: change column type").arg(name()));
        AbstractSimpleFilter *old_input_filter = inputFilter();
        AbstractSimpleFilter *old_output_filter = outputFilter();
        exec(new ColumnSetModeCmd(d_column_private, mode, conversion_filter));
        if (d_column_private->inputFilter() != old_input_filter) {
            removeChild(old_input_filter);
            addChild(d_column_private->inputFilter());
            d_column_private->inputFilter()->input(0, d_string_io);
        }
        if (outputFilter() != old_output_filter) {
            removeChild(old_output_filter);
            addChild(outputFilter());
            outputFilter()->input(0, this);
        }
        endMacro();
    }
    // mode is not changed, but DateTime numeric converter changed
    else if ((mode == SciDAVis::ColumnMode::DateTime) && (nullptr != conversion_filter)) {
        auto numeric_datetime_converter =
                reinterpret_cast<NumericDateTimeBaseFilter *>(conversion_filter);
        if (nullptr != numeric_datetime_converter)
            // the ownership of converter is taken
            d_column_private->setNumericDateTimeFilter(numeric_datetime_converter);
    }
}

bool Column::copy(const AbstractColumn *other)
{
    Q_CHECK_PTR(other);
    if (other->dataType() != dataType())
        return false;
    const Column *srcCol = qobject_cast<const Column *>(other);
    if (!srcCol || srcCol == this) {
        exec(new ColumnFullCopyCmd(d_column_private, other));
        return true;
    }
    const QList<Interval<int>> srcMasks = srcCol->maskedIntervals();
    if (srcMasks.isEmpty() && maskedIntervals().isEmpty()) {
        exec(new ColumnFullCopyCmd(d_column_private, other));
        return true;
    }
    beginMacro(tr("%1: copy %2").arg(name(), other->name()));
    exec(new ColumnFullCopyCmd(d_column_private, other));
    // all the masks in one command (one setMasked() per interval grew with their square)
    replaceMaskedIntervals(normalizedIntervals(srcMasks));
    endMacro();
    return true;
}

bool Column::copy(const AbstractColumn *source, int source_start, int dest_start, int num_rows)
{
    Q_CHECK_PTR(source);
    if (source->dataType() != dataType())
        return false;
    const Column *srcCol = qobject_cast<const Column *>(source);
    if (!srcCol || num_rows <= 0 || source_start < 0 || dest_start < 0
        || (srcCol == this && source_start == dest_start)) {
        exec(new ColumnPartialCopyCmd(d_column_private, source, source_start, dest_start, num_rows));
        return true;
    }
    // Preserve mask flags when copying between Column instances (not filter/StringIO edits).
    const int srcEnd = source_start + num_rows - 1;
    const int dstEnd = dest_start + num_rows - 1;
    QVector<Interval<int>> mappedMasks;
    const QList<Interval<int>> srcMasks = srcCol->maskedIntervals();
    for (const Interval<int> &iv : srcMasks) {
        const int s = qMax(iv.start(), source_start);
        const int e = qMin(iv.end(), srcEnd);
        if (s <= e) {
            mappedMasks.append(Interval<int>(dest_start + (s - source_start),
                                             dest_start + (e - source_start)));
        }
    }
    bool dstHadMasks = false;
    const QList<Interval<int>> dstMasks = maskedIntervals();
    for (const Interval<int> &iv : dstMasks) {
        if (iv.end() >= dest_start && iv.start() <= dstEnd) {
            dstHadMasks = true;
            break;
        }
    }
    if (!dstHadMasks && mappedMasks.isEmpty()) {
        exec(new ColumnPartialCopyCmd(d_column_private, source, source_start, dest_start, num_rows));
        return true;
    }
    beginMacro(tr("%1: partial copy %2").arg(name(), source->name()));
    exec(new ColumnPartialCopyCmd(d_column_private, source, source_start, dest_start, num_rows));
    // the destination rows take the source's masks, in one command
    QList<Interval<int>> masks = subtractedIntervals(normalizedIntervals(dstMasks),
                                                     { Interval<int>(dest_start, dstEnd) });
    for (const Interval<int> &iv : mappedMasks)
        masks.append(iv);
    replaceMaskedIntervals(normalizedIntervals(masks));
    endMacro();
    return true;
}

void Column::insertRows(int before, int count)
{
    if (count > 0)
        exec(new ColumnInsertEmptyRowsCmd(d_column_private, before, count));
}

void Column::removeRows(int first, int count)
{
    if (count > 0)
        exec(new ColumnRemoveRowsCmd(d_column_private, first, count));
}

void Column::setPlotDesignation(SciDAVis::PlotDesignation pd)
{
    if (pd != plotDesignation())
        exec(new ColumnSetPlotDesignationCmd(d_column_private, pd));
}

QString Column::longName() const
{
    return d_column_private->longName();
}

void Column::setLongName(const QString &s)
{
    if (s != longName())
        exec(new ColumnSetLongNameCmd(d_column_private, s));
}

QString Column::units() const
{
    return d_column_private->units();
}

void Column::setUnits(const QString &s)
{
    if (s != units())
        exec(new ColumnSetUnitsCmd(d_column_private, s));
}

QString Column::formattedAxisTitle(UnitFormat fmt) const
{
    auto stripOuterBrackets = [](QString s) -> QString {
        s = s.trimmed();
        while (s.size() >= 3) {
            const QChar f = s.front();
            const QChar l = s.back();
            const bool matched =
                    (f == QLatin1Char('(') && l == QLatin1Char(')'))
                    || (f == QLatin1Char('[') && l == QLatin1Char(']'))
                    || (f == QChar(0xFF08) && l == QChar(0xFF09))
                    || (f == QChar(0xFF3B) && l == QChar(0xFF3D));
            if (!matched)
                break;
            s = s.mid(1, s.size() - 2).trimmed();
        }
        return s;
    };

    const QString ln = longName().trimmed();
    const QString base = ln.isEmpty() ? name() : ln;
    QString u = stripOuterBrackets(units());
    if (u.isEmpty())
        return base;

    const QString uLower = u.toLower();
    const bool isDimensionless = (u == QLatin1String("-") || u == QStringLiteral("\u2013")
                                  || u == QStringLiteral("\u2014") || u == QStringLiteral("\u2212")
                                  || uLower == QLatin1String("none")
                                  || uLower == QLatin1String("dimensionless"));
    if (isDimensionless) {
        if (fmt == UnitFormat::ISIJSlash)
            return base;
        if (fmt == UnitFormat::Bracket)
            return QString("%1 [-]").arg(base);
        return QString("%1 (-)").arg(base);
    }

    const bool isArbUnits = (uLower == QLatin1String("a.u.") || uLower == QLatin1String("a.u")
                             || uLower == QLatin1String("au")
                             || uLower == QLatin1String("arb. units")
                             || uLower == QLatin1String("arb. unit")
                             || uLower == QLatin1String("arb.units")
                             || uLower == QLatin1String("arbitrary units"));
    if (isArbUnits)
        u = QStringLiteral("a.u.");

    // Avoid duplicating "/ <Units>" if base already ends with it
    const int slashIdx = base.lastIndexOf(QLatin1Char('/'));
    if (slashIdx >= 0) {
        const QString tail = stripOuterBrackets(base.mid(slashIdx + 1));
        if (!tail.isEmpty() && tail.compare(u, Qt::CaseInsensitive) == 0)
            return base;
    }

    if (fmt == UnitFormat::ISIJSlash)
        return QString("%1 / %2").arg(base, u);
    if (fmt == UnitFormat::Bracket)
        return QString("%1 [%2]").arg(base, u);
    return QString("%1 (%2)").arg(base, u);
}

void Column::clear()
{
    exec(new ColumnClearCmd(d_column_private));
}

void Column::notifyReplacement(const AbstractColumn *replacement)
{
    emit aboutToBeReplaced(this, replacement);
}

void Column::clearValidity()
{
    exec(new ColumnClearValidityCmd(d_column_private));
}

void Column::clearMasks()
{
    exec(new ColumnClearMasksCmd(d_column_private));
}

void Column::setInvalid(Interval<int> i, bool invalid)
{
    exec(new ColumnSetInvalidCmd(d_column_private, i, invalid));
}

void Column::setInvalid(int row, bool invalid)
{
    setInvalid(Interval<int>(row, row), invalid);
}

void Column::setMasked(Interval<int> i, bool mask)
{
    exec(new ColumnSetMaskedCmd(d_column_private, i, mask));
}

void Column::setMasked(int row, bool mask)
{
    setMasked(Interval<int>(row, row), mask);
}

void Column::replaceInvalidIntervals(const QList<Interval<int>> &intervals)
{
    exec(new ColumnReplaceValidityCmd(d_column_private, IntervalAttribute<bool>(intervals)));
}

void Column::replaceMaskedIntervals(const QList<Interval<int>> &intervals)
{
    exec(new ColumnReplaceMaskingCmd(d_column_private, IntervalAttribute<bool>(intervals)));
}

bool Column::setMaskedIntervals(const QList<Interval<int>> &intervals, bool mask)
{
    const QList<Interval<int>> given = normalizedIntervals(intervals);
    if (given.isEmpty())
        return false;
    const QList<Interval<int>> current = normalizedIntervals(maskedIntervals());
    QList<Interval<int>> result;
    if (mask) {
        result = current;
        for (const Interval<int> &iv : given)
            result.append(iv);
        result = normalizedIntervals(result);
    } else {
        result = subtractedIntervals(current, given);
    }
    if (result == current)
        return false;
    auto *cmd = new ColumnReplaceMaskingCmd(d_column_private, IntervalAttribute<bool>(result));
    if (!mask)
        cmd->setText(QObject::tr("%1: unmask cells").arg(name()));
    exec(cmd);
    return true;
}

void Column::setFormula(Interval<int> i, QString formula)
{
    exec(new ColumnSetFormulaCmd(d_column_private, i, formula));
}

void Column::setFormula(int row, QString formula)
{
    setFormula(Interval<int>(row, row), formula);
}

void Column::clearFormulas()
{
    exec(new ColumnClearFormulasCmd(d_column_private));
}

void Column::setTextAt(int row, const QString &new_value)
{
    exec(new ColumnSetTextCmd(d_column_private, row, new_value));
}

void Column::replaceTexts(int first, const QStringList &new_values)
{
    if (!new_values.isEmpty())
        exec(new ColumnReplaceTextsCmd(d_column_private, first, new_values));
}

void Column::setDateAt(int row, const QDate &new_value)
{
    setDateTimeAt(row, QDateTime(new_value, timeAt(row)));
}

void Column::setTimeAt(int row, const QTime &new_value)
{
    setDateTimeAt(row, QDateTime(dateAt(row), new_value));
}

void Column::setDateTimeAt(int row, const QDateTime &new_value)
{
    exec(new ColumnSetDateTimeCmd(d_column_private, row, new_value));
}

void Column::replaceDateTimes(int first, const QList<QDateTime> &new_values)
{
    if (!new_values.isEmpty())
        exec(new ColumnReplaceDateTimesCmd(d_column_private, first, new_values));
}

void Column::setValueAt(int row, double new_value)
{
    exec(new ColumnSetValueCmd(d_column_private, row, new_value));
}

void Column::replaceValues(int first, const QVector<qreal> &new_values)
{
    if (!new_values.isEmpty())
        exec(new ColumnReplaceValuesCmd(d_column_private, first, new_values));
}

QString Column::textAt(int row) const
{
    return d_column_private->textAt(row);
}

QDate Column::dateAt(int row) const
{
    return d_column_private->dateAt(row);
}

QTime Column::timeAt(int row) const
{
    return d_column_private->timeAt(row);
}

QDateTime Column::dateTimeAt(int row) const
{
    return d_column_private->dateTimeAt(row);
}

double Column::valueAt(int row) const
{
    return d_column_private->valueAt(row);
}

QIcon Column::icon() const
{
    switch (dataType()) {
    case SciDAVis::TypeDouble:
        return IconLoader::load("row-numbers");
    case SciDAVis::TypeQString:
        return IconLoader::load("text-label");
    case SciDAVis::TypeQDateTime:
        return IconLoader::load("log-window");
    }
    return QIcon();
}

void Column::save(QXmlStreamWriter *writer) const
{
    writer->writeStartElement("column");
    writeBasicAttributes(writer);
    writer->writeAttribute("type", SciDAVis::enumValueToString(dataType(), "ColumnDataType"));
    writer->writeAttribute("mode", SciDAVis::enumValueToString(static_cast<int>(columnMode()), "ColumnMode"));
    writer->writeAttribute("plot_designation",
                           SciDAVis::enumValueToString(plotDesignation(), "PlotDesignation"));
    writer->writeAttribute("long_name", longName());
    writer->writeAttribute("units", units());
    const QVariant labelRoleProp = property("_tg_label_role");
    if (labelRoleProp.isValid())
        writer->writeAttribute("label_role", labelRoleProp.toBool() ? "1" : "0");
    writeCommentElement(writer);
    writer->writeStartElement("input_filter");
    d_column_private->inputFilter()->save(writer);
    writer->writeEndElement();
    writer->writeStartElement("output_filter");
    outputFilter()->save(writer);
    writer->writeEndElement();
    QList<Interval<int>> masks = maskedIntervals();
    foreach (Interval<int> interval, masks) {
        writer->writeStartElement("mask");
        writer->writeAttribute("start_row", QString::number(interval.start()));
        writer->writeAttribute("end_row", QString::number(interval.end()));
        writer->writeEndElement();
    }
    QList<Interval<int>> formulas = formulaIntervals();
    foreach (Interval<int> interval, formulas) {
        writer->writeStartElement("formula");
        writer->writeAttribute("start_row", QString::number(interval.start()));
        writer->writeAttribute("end_row", QString::number(interval.end()));
        writer->writeCharacters(formula(interval.start()));
        writer->writeEndElement();
    }
    int i;
    switch (dataType()) {
    case SciDAVis::TypeDouble:
        for (i = 0; i < rowCount(); i++) {
            writer->writeStartElement("row");
            writer->writeAttribute("type",
                                   SciDAVis::enumValueToString(dataType(), "ColumnDataType"));
            writer->writeAttribute("index", QString::number(i));
            writer->writeAttribute("invalid", isInvalid(i) ? "yes" : "no");
            writer->writeCharacters(QString::number(valueAt(i), 'e', 16));
            writer->writeEndElement();
        }
        break;
    case SciDAVis::TypeQString:
        for (i = 0; i < rowCount(); i++) {
            writer->writeStartElement("row");
            writer->writeAttribute("type",
                                   SciDAVis::enumValueToString(dataType(), "ColumnDataType"));
            writer->writeAttribute("index", QString::number(i));
            writer->writeAttribute("invalid", isInvalid(i) ? "yes" : "no");
            writer->writeCharacters(textAt(i));
            writer->writeEndElement();
        }
        break;

    case SciDAVis::TypeQDateTime: {
        { // this conversion is needed to store base class;
            // Month/Day columns used to carry no filter (null): fall back to the default one.
            const NumericDateTimeBaseFilter *storedFilter =
                    d_column_private->getNumericDateTimeFilter();
            QScopedPointer<NumericDateTimeBaseFilter> fallbackFilter;
            if (!storedFilter) {
                fallbackFilter.reset(new NumericDateTimeBaseFilter());
                storedFilter = fallbackFilter.data();
            }
            NumericDateTimeBaseFilter numericFilter(*storedFilter);
            writer->writeStartElement("numericDateTimeFilter");
            numericFilter.save(writer);
            writer->writeEndElement();
        }
        for (i = 0; i < rowCount(); i++) {
            writer->writeStartElement("row");
            writer->writeAttribute("type",
                                   SciDAVis::enumValueToString(dataType(), "ColumnDataType"));
            writer->writeAttribute("index", QString::number(i));
            writer->writeAttribute("invalid", isInvalid(i) ? "yes" : "no");
#if QT_VERSION < 0x040400 // avoid a bug in Qt < 4.4
            QString str = dateTimeAt(i).toString("yyyy-dd-MM hh:mm:ss:zzz");
            int should_be_length = 4;
            int actual_length = dateTimeAt(i).toString("yyyy").length();
            while (actual_length < should_be_length) {
                str.prepend("0");
                actual_length++;
            }
            writer->writeCharacters(str);
#else
            writer->writeCharacters(dateTimeAt(i).toString("yyyy-dd-MM hh:mm:ss:zzz"));
#endif
            writer->writeEndElement();
        }
        break;
    }
    }
    writer->writeEndElement(); // "column"
}

//! The <row> elements of a column being loaded (see Column::load / XmlApplyRows).
struct Column::RowBuffer
{
    enum RowState : char { NotRead = 0, Valid = 1, Invalid = 2 };
    QVector<double> values;
    QStringList texts;
    QList<QDateTime> dateTimes;
    //! per row: NotRead (a row missing from the file: an empty cell, as resizeTo() made it),
    //! Valid or Invalid
    QVector<char> state;

    //! Makes room for \c row like Private::resizeTo(): rows added are 0 / empty / invalid.
    void reserveRow(int row, SciDAVis::ColumnDataType type)
    {
        const int oldSize = state.size();
        if (row < oldSize)
            return;
        state.resize(row + 1); // new entries are NotRead
        switch (type) {
        case SciDAVis::TypeDouble:
            values.resize(row + 1);
            break;
        case SciDAVis::TypeQString:
            for (int i = oldSize; i <= row; ++i)
                texts.append(QString());
            break;
        case SciDAVis::TypeQDateTime:
            for (int i = oldSize; i <= row; ++i)
                dateTimes.append(QDateTime());
            break;
        }
    }
};

bool Column::load(XmlStreamReader *reader)
{
    if (reader->isStartElement() && reader->name() == "column") {
        if (!readBasicAttributes(reader))
            return false;

        QXmlStreamAttributes attribs = reader->attributes();
        QString str;

        // read type
        str = attribs.value(reader->namespaceUri().toString(), "type").toString();
        if (str.isEmpty()) {
            reader->raiseError(tr("column type missing"));
            return false;
        }
        int type_code = SciDAVis::enumStringToValue(str, "ColumnDataType");
        if (type_code == -1) {
            reader->raiseError(tr("column type invalid"));
            return false;
        }
        // read mode
        str = attribs.value(reader->namespaceUri().toString(), "mode").toString();
        if (str.isEmpty()) {
            reader->raiseError(tr("column mode missing"));
            return false;
        }
        int mode_code = SciDAVis::enumStringToValue(str, "ColumnMode");
        if (mode_code == -1) {
            reader->raiseError(tr("column mode invalid"));
            return false;
        }
        setColumnMode((SciDAVis::ColumnMode)mode_code);
        if (type_code != int(dataType())) {
            reader->raiseError(tr("column type or mode invalid"));
            return false;
        }
        // read plot designation
        str = attribs.value(reader->namespaceUri().toString(), "plot_designation").toString();
        int pd_code = SciDAVis::enumStringToValue(str, "PlotDesignation");
        if (str.isEmpty())
            setPlotDesignation(SciDAVis::noDesignation);
        else if (pd_code == -1) {
            reader->raiseError(tr("column plot designation invalid"));
            return false;
        } else
            setPlotDesignation((SciDAVis::PlotDesignation)pd_code);

        setLongName(attribs.value(reader->namespaceUri().toString(), "long_name").toString());
        setUnits(attribs.value(reader->namespaceUri().toString(), "units").toString());
        const QString labelRoleStr =
                attribs.value(reader->namespaceUri().toString(), "label_role").toString();
        if (!labelRoleStr.isEmpty())
            setProperty("_tg_label_role",
                        (labelRoleStr == QLatin1String("1")
                         || labelRoleStr.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0));
        else
            setProperty("_tg_label_role", QVariant());
        setComment("");
        if (rowCount() > 0)
            removeRows(0, rowCount());
        clearValidity();
        clearMasks();
        clearFormulas();
        // The <row> elements are collected and stored in one step at the end: setting them one
        // by one scanned the list of empty cells read so far for every row, so a column with
        // scattered empty cells took a time growing with the square of the rows to open.
        RowBuffer rows;
        // likewise the <mask> elements: one setMasked() each copied all the masks read so far
        // (a column with 25000 scattered masked runs took minutes and gigabytes to open)
        QList<Interval<int>> masks;
        auto applyRead = [this, &rows, &masks]() {
            XmlApplyRows(rows);
            if (!masks.isEmpty())
                setMaskedIntervals(masks, true);
            masks.clear();
        };
        // read child elements
        while (!reader->atEnd()) {
            reader->readNext();

            if (reader->isEndElement())
                break;

            if (reader->isStartElement()) {
                bool ret_val = true;
                if (reader->name() == "comment")
                    ret_val = readCommentElement(reader);
                else if (reader->name() == "numericDateTimeFilter")
                    ret_val = XmlReadNumericDateTimeFilter(reader);
                else if (reader->name() == "input_filter")
                    ret_val = XmlReadInputFilter(reader);
                else if (reader->name() == "output_filter")
                    ret_val = XmlReadOutputFilter(reader);
                else if (reader->name() == "mask")
                    ret_val = XmlReadMask(reader, &masks);
                else if (reader->name() == "formula")
                    ret_val = XmlReadFormula(reader);
                else if (reader->name() == "row")
                    ret_val = XmlReadRow(reader, &rows);
                else // unknown element
                {
                    reader->raiseWarning(tr("unknown element '%1'").arg(reader->name().toString()));
                    if (!reader->skipToEndElement()) {
                        applyRead();
                        return false;
                    }
                }
                if (!ret_val) {
                    applyRead();
                    return false;
                }
            }
        }
        applyRead();
    } else // no column element
        reader->raiseError(tr("no column element found"));

    return !reader->error();
}

bool Column::XmlReadNumericDateTimeFilter(XmlStreamReader *reader)
{
    Q_ASSERT(reader->isStartElement() && reader->name() == "numericDateTimeFilter");
    if (!reader->skipToNextTag())
        return false;
    // A column read as Month/Day after being built as Numeric used to have no filter (null).
    if (!d_column_private->getNumericDateTimeFilter())
        d_column_private->setNumericDateTimeFilter(new NumericDateTimeBaseFilter());
    if (!d_column_private->getNumericDateTimeFilter()->load(reader))
        return false;
    if (!reader->skipToNextTag())
        return false;
    Q_ASSERT(reader->isEndElement() && reader->name() == "numericDateTimeFilter");
    return true;
}

bool Column::XmlReadInputFilter(XmlStreamReader *reader)
{
    Q_ASSERT(reader->isStartElement() && reader->name() == "input_filter");
    if (!reader->skipToNextTag())
        return false;
    if (!d_column_private->inputFilter()->load(reader))
        return false;
    if (!reader->skipToNextTag())
        return false;
    Q_ASSERT(reader->isEndElement() && reader->name() == "input_filter");
    return true;
}

bool Column::XmlReadOutputFilter(XmlStreamReader *reader)
{
    Q_ASSERT(reader->isStartElement() && reader->name() == "output_filter");
    if (!reader->skipToNextTag())
        return false;
    if (!outputFilter()->load(reader))
        return false;
    if (!reader->skipToNextTag())
        return false;
    Q_ASSERT(reader->isEndElement() && reader->name() == "output_filter");
    return true;
}

bool Column::XmlReadMask(XmlStreamReader *reader, QList<Interval<int>> *masks)
{
    Q_ASSERT(reader->isStartElement() && reader->name() == "mask");

    bool ok1, ok2;
    int start, end;
    start = reader->readAttributeInt("start_row", &ok1);
    end = reader->readAttributeInt("end_row", &ok2);
    if (!ok1 || !ok2) {
        reader->raiseError(tr("invalid or missing start or end row"));
        return false;
    }
    masks->append(Interval<int>(start, end));
    if (!reader->skipToEndElement())
        return false;

    return true;
}

bool Column::XmlReadFormula(XmlStreamReader *reader)
{
    Q_ASSERT(reader->isStartElement() && reader->name() == "formula");

    bool ok1, ok2;
    int start, end;
    start = reader->readAttributeInt("start_row", &ok1);
    end = reader->readAttributeInt("end_row", &ok2);
    if (!ok1 || !ok2) {
        reader->raiseError(tr("invalid or missing start or end row"));
        return false;
    }
    setFormula(Interval<int>(start, end), reader->readElementText());

    return true;
}

void Column::XmlApplyRows(RowBuffer &rows)
{
    const int n = rows.state.size();
    if (n == 0)
        return;
    // The column has no rows here (load() removed them): replace*() fill rows 0..n-1 and mark
    // them valid; the empty cells are then set in one step from the runs of empty rows.
    switch (dataType()) {
    case SciDAVis::TypeDouble:
        replaceValues(0, rows.values);
        break;
    case SciDAVis::TypeQString:
        replaceTexts(0, rows.texts);
        break;
    case SciDAVis::TypeQDateTime:
        replaceDateTimes(0, rows.dateTimes);
        break;
    }
    QList<Interval<int>> invalid;
    int runStart = -1;
    for (int i = 0; i <= n; ++i) {
        const bool empty = i < n && rows.state.at(i) != RowBuffer::Valid;
        if (empty && runStart < 0)
            runStart = i;
        else if (!empty && runStart >= 0) {
            invalid.append(Interval<int>(runStart, i - 1));
            runStart = -1;
        }
    }
    if (!invalid.isEmpty() || !invalidIntervals().isEmpty())
        replaceInvalidIntervals(invalid);
    rows = RowBuffer();
}

bool Column::XmlReadRow(XmlStreamReader *reader, RowBuffer *rows)
{
    Q_ASSERT(reader->isStartElement() && reader->name() == "row");

    QString str;
    int type_code;

    QXmlStreamAttributes attribs = reader->attributes();
    // verfiy type
    str = attribs.value(reader->namespaceUri().toString(), "type").toString();
    type_code = SciDAVis::enumStringToValue(str, "ColumnDataType");
    if (str.isEmpty() || type_code == -1 || type_code != int(dataType())) {
        reader->raiseError(tr("invalid or missing row type"));
        return false;
    }

    bool ok;
    int index = reader->readAttributeInt("index", &ok);
    if (!ok || index < 0) {
        reader->raiseError(tr("invalid or missing row index"));
        return false;
    }
    const bool markedInvalid =
            attribs.value(reader->namespaceUri().toString(), "invalid") == QLatin1String("yes");

    // A later row with the same index replaces the earlier one (value and empty flag), as
    // setting the cells one by one did.
    str = reader->readElementText();
    switch (dataType()) {
    case SciDAVis::TypeDouble: {
        double value = str.toDouble(&ok);
        if (!ok) {
            reader->raiseError(tr("invalid row value"));
            return false;
        }
        rows->reserveRow(index, SciDAVis::TypeDouble);
        rows->values[index] = value;
        rows->state[index] = markedInvalid ? RowBuffer::Invalid : RowBuffer::Valid;
        break;
    }
    case SciDAVis::TypeQString:
        rows->reserveRow(index, SciDAVis::TypeQString);
        rows->texts[index] = str;
        rows->state[index] = markedInvalid ? RowBuffer::Invalid : RowBuffer::Valid;
        break;

    case SciDAVis::TypeQDateTime: {
        // save() writes the cell's date and time fields ("yyyy-dd-MM hh:mm:ss:zzz"). They are
        // read as a wall-clock time and made local like the cells of a conversion: read as a
        // local time at once, a clock time skipped when DST starts (02:30 on 2024-03-10 in New
        // York) was invalid in Qt 5 and became an empty cell on reopening.
        QDateTime date_time;
        const int sep = str.indexOf(QLatin1Char(' '));
        const QDate date = sep > 0 ? QDate::fromString(str.left(sep), "yyyy-dd-MM") : QDate();
        const QTime time = sep > 0 ? QTime::fromString(str.mid(sep + 1), "hh:mm:ss:zzz") : QTime();
        if (date.isValid() && time.isValid())
            date_time = NumericDateTimeBaseFilter::localWallClock(QDateTime(date, time, Qt::UTC));
        else
            date_time = QDateTime::fromString(str, "yyyy-dd-MM hh:mm:ss:zzz");
        rows->reserveRow(index, SciDAVis::TypeQDateTime);
        rows->dateTimes[index] = date_time;
        rows->state[index] = (markedInvalid || !date_time.isValid()) ? RowBuffer::Invalid
                                                                      : RowBuffer::Valid;
        break;
    }
    }

    return true;
}
SciDAVis::ColumnDataType Column::dataType() const
{
    return d_column_private->dataType();
}

SciDAVis::ColumnMode Column::columnMode() const
{
    return d_column_private->columnMode();
}
int Column::rowCount() const
{
    return d_column_private->rowCount();
}

SciDAVis::PlotDesignation Column::plotDesignation() const
{
    return d_column_private->plotDesignation();
}

AbstractSimpleFilter *Column::outputFilter() const
{
    return d_column_private->outputFilter();
}

AbstractSimpleFilter *Column::inputFilter() const
{
    return d_column_private->inputFilter();
}

NumericDateTimeBaseFilter *Column::numericDateTimeBaseFilter() const
{
    return d_column_private->getNumericDateTimeFilter();
}

bool Column::isInvalid(int row) const
{
    return d_column_private->isInvalid(row);
}

bool Column::isInvalid(Interval<int> i) const
{
    return d_column_private->isInvalid(i);
}

QList<Interval<int>> Column::invalidIntervals() const
{
    return d_column_private->invalidIntervals();
}

bool Column::isMasked(int row) const
{
    return d_column_private->isMasked(row);
}

bool Column::isMasked(Interval<int> i) const
{
    return d_column_private->isMasked(i);
}

QList<Interval<int>> Column::maskedIntervals() const
{
    return d_column_private->maskedIntervals();
}

QString Column::formula(int row) const
{
    return d_column_private->formula(row);
}

QList<Interval<int>> Column::formulaIntervals() const
{
    return d_column_private->formulaIntervals();
}

namespace {
quint64 s_formula_epoch = 1;
}

quint64 Column::formulaEpoch()
{
    return s_formula_epoch;
}

void Column::touchFormulaEpoch()
{
    ++s_formula_epoch;
}

void Column::notifyDisplayChange()
{
    emit dataChanged(this); // all cells must be repainted
    emit aspectDescriptionChanged(this); // the icon for the type changed
}

QString ColumnStringIO::textAt(int row) const
{
    if (d_setting)
        return d_to_set;
    else
        return d_owner->d_column_private->outputFilter()->output(0)->textAt(row);
}

void ColumnStringIO::setTextAt(int row, const QString &value)
{
    d_setting = true;
    d_to_set = value;
    d_owner->copy(d_owner->d_column_private->inputFilter()->output(0), 0, row, 1);
    d_setting = false;
    d_to_set.clear();
}

bool ColumnStringIO::copy(const AbstractColumn *other)
{
    if (other->columnMode() != SciDAVis::ColumnMode::Text)
        return false;
    d_owner->d_column_private->inputFilter()->input(0, other);
    d_owner->copy(d_owner->d_column_private->inputFilter()->output(0));
    d_owner->d_column_private->inputFilter()->input(0, this);
    return true;
}

bool ColumnStringIO::copy(const AbstractColumn *source, int source_start, int dest_start,
                          int num_rows)
{
    if (source->columnMode() != SciDAVis::ColumnMode::Text)
        return false;
    d_owner->d_column_private->inputFilter()->input(0, source);
    d_owner->copy(d_owner->d_column_private->inputFilter()->output(0), source_start, dest_start,
                  num_rows);
    d_owner->d_column_private->inputFilter()->input(0, this);
    return true;
}

void ColumnStringIO::replaceTexts(int start_row, const QStringList &texts)
{
    Column tmp("tmp", texts);
    copy(&tmp, 0, start_row, texts.size());
}
