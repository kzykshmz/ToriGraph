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

struct MergeRange
{
    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;
};

struct Sheet
{
    QString name;
    QString state; //!< "" (visible), "hidden" or "veryHidden"
    QVector<QVector<Cell>> rows; //!< rows[r][c], A1 = rows[0][0]
    QVector<MergeRange> mergedCells; //!< parsed <mergeCells> ranges (0-based)
    int columnCount = 0;
    int uncachedFormulas = 0; //!< formula cells (<f>) without a cached <v>/<is> value
    int trimmedStrayCells = 0; //!< distant isolated outlier cells trimmed to prevent OOM
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
    bool commentsRow = false; //!< third header row became Comments (Origin 3-row layout)
    int dataRows = 0;
    int numericColumns = 0;
    int textColumns = 0;
    int dateColumns = 0;
    int nonNumericCells = 0; //!< stray non-numeric cells marked invalid in >=90% numeric columns
    int trimmedStrayCells = 0; //!< distant isolated outlier cells omitted before dense allocation
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
    QStringList hiddenSheetsSkipped; //!< hidden/veryHidden sheets omitted when visible sheets exist
    int uncachedFormulas = 0; //!< formula cells lacking cached values across imported sheets
    int nonNumericCells = 0; //!< stray non-numeric cells marked invalid across numeric columns
    int trimmedStrayCells = 0; //!< distant isolated outlier cells omitted before dense allocation
    QString summary; //!< one-line translated summary for the status bar
    QString workbook; //!< workbook holding the sheets (empty: separate worksheets)
};

//! Imports the workbook into `app`: one new table per non-empty worksheet (or only `sheet`).
//! A fresh project's untouched initial table is replaced by the first sheet.
//! With `asWorkbook` and two or more imported sheets, the tables become the sheet tabs of one
//! workbook named after the file (D8); otherwise they stay separate worksheets.
bool importIntoProject(ApplicationWindow *app, const QString &path, const QString &sheet,
                       QString *outError, ImportResult *outResult = nullptr,
                       bool asWorkbook = true, bool includeHidden = false);

//! Saved choice of the import dialog: put the sheets of a file into one workbook (default true).
bool importAsWorkbookSetting();
void setImportAsWorkbookSetting(bool on);

//! File menu / ribbon entry: file dialog (several files allowed), import, report.
void importWithDialog(ApplicationWindow *app);

//! Imports one dropped workbook; shows the reason in a message box when it cannot be read.
bool importDroppedFile(ApplicationWindow *app, const QString &path, QString *outSummary);

//! Registers the Japanese translations of this module's tr() strings (idempotent).
void registerTranslations();

} // namespace XlsxImport

#endif // TORIGRAPH_XLSXIMPORT_H
