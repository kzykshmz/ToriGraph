/***************************************************************************
    File                 : ImportOPJU.h
    Project              : ToriGraph / SciDAVis
    Description          : Native Origin 2018+ Unicode ZIP-based (.opju) importer
 ***************************************************************************/

#ifndef IMPORTOPJU_H
#define IMPORTOPJU_H

#include <QString>

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
    };

    //! Import an .opju file into the given ApplicationWindow.
    //! Returns true on success; returns false and populates outError on failure.
    static bool importFile(ApplicationWindow *app, const QString &fileName,
                           QString *outError = nullptr);

    //! Import an .opju file and optionally report imported object counts.
    static bool importFile(ApplicationWindow *app, const QString &fileName,
                           QString *outError, ImportStats *outStats);

    //! Fast magic-bytes probe: returns true if the file begins with a ZIP signature.
    static bool isZipOpjuFile(const QString &fileName);

    //! Fast magic-bytes probe: returns true if the file begins with Origin 2018+ CPYUA/CPYU header.
    static bool isCpyuaOpjuFile(const QString &fileName);

    //! Returns true if the file is a supported .opju container (either ZIP or Origin CPYUA).
    static bool isSupportedOpjuFile(const QString &fileName);
};

#endif // IMPORTOPJU_H
