/***************************************************************************
    File                 : SettingsTransfer.h
    Project              : ToriGraph
    Description          : Export / import / reset of all user settings (QSettings) as an
                           INI file, and lab-wide defaults read from
                           "torigraph-lab-defaults.ini" next to the executable.
 ***************************************************************************/
#ifndef SETTINGS_TRANSFER_H
#define SETTINGS_TRANSFER_H

#include <QString>

class ApplicationWindow;
class QAction;
class QSettings;

class SettingsTransfer
{
public:
    enum ActionId { ActExport = 0, ActImport, ActReset, ActCount };

    //! File name of the lab defaults ("torigraph-lab-defaults.ini").
    static QString labDefaultsFileName();
    //! Lab defaults file in the folder of the running executable (may not exist).
    static QString labDefaultsPath();

    //! True for machine-/user-local keys (recent projects, local folder paths, window geometry,
    //! last update check timestamps) that must not be shared via Export Settings or Lab Defaults.
    static bool isPersonalKey(const QString &key);

    //! Writes every portable key of \a src (excluding lab-defaults bookkeeping and personal keys)
    //! to the INI file \a path, replacing its contents. Returns the number of keys written, or -1.
    static int exportToIni(QSettings &src, const QString &path, QString *error = nullptr);
    //! Copies every key of the INI file \a path into \a dst (keys present in the file
    //! overwrite \a dst; other keys of \a dst are kept). Returns the key count, or -1.
    static int importFromIni(QSettings &dst, const QString &path, QString *error = nullptr);
    //! Removes every key of \a s.
    static void resetAll(QSettings &s);

    //! Lab defaults (priority: the user's own value > lab defaults > built-in default).
    //! For each key of \a labIniPath: written to \a user when \a user has no such key, or
    //! when the user's value is still the one seeded from an earlier lab file (so a revised
    //! lab file reaches users who did not change that setting). Seeded values are recorded
    //! under "LabDefaults/Applied/<key>". Returns the number of keys written (0 if the file
    //! does not exist).
    static int applyLabDefaults(QSettings &user, const QString &labIniPath);

    //! After an import or a reset the settings held in memory must not be written back
    //! (ApplicationWindow::saveSettings() returns early until the next start).
    static bool saveSuppressed();
    static void setSaveSuppressed(bool on);

    //! Application-owned menu actions (object names "actionSettingsExport", ...),
    //! retranslated on language change. Safe to add to several menus.
    static QAction *action(ApplicationWindow *app, ActionId id);

    static void exportInteractive(ApplicationWindow *app);
    static void importInteractive(ApplicationWindow *app);
    static void resetInteractive(ApplicationWindow *app);
};

#endif
