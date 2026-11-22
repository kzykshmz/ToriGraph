/***************************************************************************
    File                 : XlsxImport.h
    Project              : ToriGraph / SciDAVis
    Description          : Excel 2007+ workbook (.xlsx / .xlsm) reader without
                           external libraries (ZIP via ZipReader, XML via
                           QXmlStreamReader). Every worksheet becomes a table.
 ***************************************************************************/

#ifndef TORIGRAPH_XLSXIMPORT_H
#define TORIGRAPH_XLSXIMPORT_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

class ApplicationWindow;
class Column;
class QWidget;

namespace XlsxImport {

struct Cell
{
    enum Type { Empty, Number, Text, Bool, Error };
    Type type = Empty;
    double number = 0.0;
    QString text;
    //! number formatted with a date/time number format (Excel serial date)
    bool isDate = false;
    //! the date/time format shows a calendar date (y/d/m) / a time of day (h/m/s)
    bool hasDatePart = false;
    bool hasTimePart = false;
};

struct Sheet
{
    QString name;
    QString state; //!< "" (visible), "hidden" or "veryHidden"
    QVector<QVector<Cell>> rows; //!< rows[r][c], A1 = rows[0][0]
    int columnCount = 0;
};

struct Workbook
{
    QVector<Sheet> sheets; //!< worksheets in workbook (tab) order
    bool date1904 = false;
    int skippedNonWorksheets = 0; //!< chart sheets / dialog sheets left out
};

//! Reads a workbook file. `onlySheet` (case-insensitive sheet name) limits the result to one
//! worksheet. Returns false with a user-readable (translated) reason for encrypted /
//! password-protected workbooks, legacy .xls files, damaged archives and missing parts.
bool readWorkbook(const QString &path, Workbook &out, QString *outError,
                  const QString &onlySheet = QString());
bool readWorkbookData(const QByteArray &data, Workbook &out, QString *outError,
                      const QString &onlySheet = QString());

//! Excel serial day number -> local date/time (1900 or 1904 date system).
QDateTime serialToDateTime(double serial, bool date1904);

//! What sheetToColumns() decided for one sheet.
struct SheetLayout
{
    bool headerRow = false; //!< first used row became column names (Long Name)
    bool unitsRow = false; //!< second used row became Units
    int dataRows = 0;
    int numericColumns = 0;
    int textColumns = 0;
    int dateColumns = 0;
};

//! Converts a worksheet to table columns (caller owns them). Leading empty rows/columns and
//! trailing empty rows/columns are dropped. Returns an empty list for an empty sheet.
QList<Column *> sheetToColumns(const Sheet &sheet, bool date1904, SheetLayout *outLayout = nullptr);

//! Window name "<book>_<sheet>" with characters that window names cannot hold replaced.
QString tableBaseName(const QString &bookBaseName, const QString &sheetName);

struct ImportResult
{
    QStringList tables; //!< names of the created tables
    QStringList emptySheets; //!< sheets without any cell value (not imported)
    QString summary; //!< one-line translated summary for the status bar
};

//! Imports the workbook into `app`: one new table per non-empty worksheet (or only `sheet`).
//! A fresh project's untouched initial table is replaced by the first sheet.
bool importIntoProject(ApplicationWindow *app, const QString &path, const QString &sheet,
                       QString *outError, ImportResult *outResult = nullptr);

//! File menu / ribbon entry: file dialog (several files allowed), import, report.
void importWithDialog(ApplicationWindow *app);

//! Imports one dropped workbook; shows the reason in a message box when it cannot be read.
bool importDroppedFile(ApplicationWindow *app, const QString &path, QString *outSummary);

//! Registers the Japanese translations of this module's tr() strings (idempotent).
void registerTranslations();

} // namespace XlsxImport

#endif // TORIGRAPH_XLSXIMPORT_H
