/***************************************************************************
    File                 : AsciiTableImportFilter.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2008-2009 Knut Franke
    Email (use @ for *)  : Knut.Franke*gmx.net
    Description          : Import an ASCII file as Table.

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

#ifndef ASCII_TABLE_IMPORT_FILTER_H
#define ASCII_TABLE_IMPORT_FILTER_H

#include "core/AbstractImportFilter.h"
#include <QByteArray>
#include <QLocale>
#include <QString>

namespace future {
class Table;
}

//! Import an ASCII file as Table.
/**
 * This is a complete rewrite of equivalent functionality previously found in Table.
 *
 * Originally, this filter wasn't meant to handle conversion of ASCII text to other
 * data types. While it's conceptually elegant and flexible to import text data as
 * text columns, testing this in everyday usage revealed that, for the common use case
 * of importing lots of numeric data, having to convert imported tables to numeric
 * by hand is a bit of a nuisance. Therefore, AsciiTableImportFilter has an option
 * for converting all columns to Numeric. Importing non-numeric data is possible by
 * disabling this and (optionally) converting texts to other formats using Table's
 * type control tab.
 *
 * TODO: port options GUI from ImportTableDialog
 */
class AsciiTableImportFilter : public AbstractImportFilter
{
    Q_OBJECT

public:
    AsciiTableImportFilter()
        : d_ignored_lines(0),
          d_separator("\t"),
          d_first_row_names_columns(true),
          d_trim_whitespace(false),
          d_simplify_whitespace(false),
          d_convert_to_numeric(false),
          d_numeric_locale(QLocale::c())
    {
    }
    virtual AbstractAspect *importAspect(QIODevice &input);
    virtual QStringList fileExtensions() const;
    virtual QString name() const { return QObject::tr("ASCII table"); }

    //! What the last import decided and what it left out (shown to the user after a drop import).
    struct ImportReport
    {
        QString encoding; //!< detected text encoding ("UTF-8", "Shift-JIS", ...)
        QString separator; //!< effective column separator ("\t", ",", ";", " ")
        int skippedLines = 0; //!< non-blank lines that were neither data nor header/unit rows
        int headerLines = 0; //!< header (name) and unit rows used for column metadata
        bool unitsRowDetected = false;
        int dataRows = 0;
        int columns = 0;
        int missingCells = 0; //!< empty / missing cells in numeric columns
        int textColumns = 0; //!< columns that had to be imported as text
        int rasBlocks = 0; //!< number of *RAS_INT_START blocks (Rigaku .ras)
        QString preamble; //!< metadata lines above the data (kept as the table comment)
    };
    //! Report of the most recent import done by this filter in this process.
    static ImportReport lastImportReport();
    //! Short human-readable summary of an ImportReport (translated).
    static QString describeImportReport(const ImportReport &report);
    //! Import for drag & drop: fixed defaults (independent of the last dialog settings),
    //! automatic separator/encoding detection and detection of a metadata preamble above the
    //! numeric block (instrument files such as .mpt/.dta/.asc).
    static future::Table *importDroppedTextData(const QByteArray &rawBytes,
                                                const QString &tableName,
                                                const QString &filePathHint,
                                                ImportReport *outReport = nullptr);

    static QString decodeLabTextAuto(const QByteArray &rawBytes,
                                     const QString &encodingHint = QString());
    static void splitHeaderNameAndUnits(const QString &rawHeader, QString &outShortName,
                                        QString &outLongName, QString &outUnits);
    static future::Table *importLabTextData(const QByteArray &rawBytes, const QString &tableName,
                                            const QString &filePathHint = QString(),
                                            const QString &separatorHint = QString(),
                                            const QString &encodingHint = QString(),
                                            int unitsRowHint = -1 /* -1=auto, 0=none, 1=row2 is units */);
    static future::Table *importVaspTextData(const QByteArray &rawBytes, const QString &tableName,
                                             const QString &filePathHint = QString(),
                                             double *outFermiEV = nullptr);

    ACCESSOR(int, ignored_lines);
    Q_PROPERTY(int ignored_lines READ ignored_lines WRITE set_ignored_lines)

    QString separator() const
    {
        QString result = d_separator;
        return result.replace("\t", "\\t");
    }
    void set_separator(const QString &value)
    {
        d_separator = value;
        d_separator.replace("\\t", "\t");
    }
    Q_PROPERTY(QString separator READ separator WRITE set_separator);

    ACCESSOR(bool, first_row_names_columns);
    Q_PROPERTY(bool first_row_names_columns READ first_row_names_columns WRITE
                       set_first_row_names_columns)

    ACCESSOR(bool, trim_whitespace);
    Q_PROPERTY(bool trim_whitespace READ trim_whitespace WRITE set_trim_whitespace)

    ACCESSOR(bool, simplify_whitespace);
    Q_PROPERTY(bool simplify_whitespace READ simplify_whitespace WRITE set_simplify_whitespace)

    ACCESSOR(bool, convert_to_numeric);
    Q_PROPERTY(bool convert_to_numeric READ convert_to_numeric WRITE set_convert_to_numeric)

    ACCESSOR(QLocale, numeric_locale);
    Q_PROPERTY(QLocale numeric_locale READ numeric_locale WRITE set_numeric_locale)

private:
    int d_ignored_lines;
    QString d_separator;
    bool d_first_row_names_columns;
    bool d_trim_whitespace;
    bool d_simplify_whitespace;
    bool d_convert_to_numeric;
    QLocale d_numeric_locale;
};

#endif // ifndef ASCII_TABLE_IMPORT_FILTER_H
