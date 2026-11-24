/***************************************************************************
    File                 : ImportOPJU.h
    Project              : ToriGraph / SciDAVis
    Description          : Native Origin 2018+ Unicode ZIP-based (.opju) importer
 ***************************************************************************/

#ifndef IMPORTOPJU_H
#define IMPORTOPJU_H

#include <QString>
#include <QStringList>

class ApplicationWindow;

//! Native importer for Origin 2018+ Unicode ZIP-based project files (.opju).
class ImportOPJU
{
public:
    struct ImportStats
    {
        int foldersImported = 0;
        int tablesImported = 0;
        int matricesImported = 0;
        int graphsImported = 0;
        int notesImported = 0;
        int datasetsSkipped = 0;
        int graphsSkipped = 0;
        int matricesSkipped = 0;
        bool isCpyuaContainer = false;
        QStringList warnings;
    };

    //! Import an .opju file into the given ApplicationWindow.
    //! Returns true on success; returns false and populates outError on failure.
    static bool importFile(ApplicationWindow *app, const QString &fileName,
                           QString *outError = nullptr);

    //! Import an .opju file with an explicit Windows code page (e.g. 932 = Shift-JIS; 0 = auto).
    static bool importFile(ApplicationWindow *app, const QString &fileName,
                           QString *outError, int codePage);

    //! Import an .opju file and optionally report imported object counts. `codePage` is the
    //! Windows code page (e.g. 932) for 8-bit text of CPYUA files; 0 = automatic detection.
    static bool importFile(ApplicationWindow *app, const QString &fileName,
                           QString *outError, ImportStats *outStats, int codePage = 0);

    //! Fast magic-bytes probe: returns true if the file begins with a ZIP signature.
    static bool isZipOpjuFile(const QString &fileName);

    //! Fast magic-bytes probe: returns true if the file begins with Origin 2018+ CPYUA/CPYU header.
    static bool isCpyuaOpjuFile(const QString &fileName);

    //! Returns true if the file is a supported .opju container (either ZIP or Origin CPYUA).
    static bool isSupportedOpjuFile(const QString &fileName);
};

#endif // IMPORTOPJU_H
