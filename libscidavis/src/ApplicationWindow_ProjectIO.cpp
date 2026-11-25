/***************************************************************************
        File                 : ApplicationWindow_ProjectIO.cpp
        Project              : SciDAVis
        Description          : SciDAVis's main window (project open/load/save, templates, legacy XML loaders & crash recovery)
    --------------------------------------------------------------------
    Copyright            : (C) 2006-2009 Knut Franke (knut.franke*gmx.de)
    Copyright            : (C) 2006-2009 Tilman Benkert (thzs*gmx.net)
    Copyright            : (C) 2004-2007 by Ion Vasilief (ion_vasilief*yahoo.fr)
                           (replace * with @ in the email address)

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

#include "globals.h"
#include "ApplicationWindow.h"
#include "CurvesDialog.h"
#include "PlotDialog.h"
#include "AxesDialog.h"
#include "LineDialog.h"
#include "TextDialog.h"
#include "ExportDialog.h"
#include "ErrDialog.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ImageMarker.h"
#include "Graph.h"
#include "Plot.h"
#include "Grid.h"
#include "PlotWizard.h"
#include "PolynomFitDialog.h"
#include "ExpDecayDialog.h"
#include "FunctionDialog.h"
#include "FitDialog.h"
#include "SurfaceDialog.h"
#include "Graph3D.h"
#include "Plot3DDialog.h"
#include "ImageDialog.h"
#include "MultiLayer.h"
#include "LayerDialog.h"
#include "DataSetDialog.h"
#include "IntDialog.h"
#include "ConfigDialog.h"
#ifdef ORIGIN_IMPORT
#include "importOPJ.h"
#include "ImportOPJU.h"
#endif
#include "AssociationsDialog.h"
#include "RenameWindowDialog.h"
#include "QwtErrorPlotCurve.h"
#include "InterpolationDialog.h"
#include "ImportASCIIDialog.h"
#include "XlsxImport.h"
#include "PptxExport.h"
#include "Workbook.h"
#include "ImageExportDialog.h"
#include "SmoothCurveDialog.h"
#include "FilterDialog.h"
#include "FFTDialog.h"
#include "Note.h"
#include "Folder.h"
#include "FindDialog.h"
#include "ScaleDraw.h"
#include "ScriptingLangDialog.h"
#include "TableStatistics.h"
#include "Fit.h"
#include "MultiPeakFit.h"
#include "PolynomialFit.h"
#include "NonLinearFit.h"
#include "SigmoidalFit.h"
#include "FunctionCurve.h"
#include "QwtPieCurve.h"
#include "Spectrogram.h"
#include "Differentiation.h"
#include "SmoothFilter.h"
#include "FFTFilter.h"
#include "Convolution.h"
#include "Correlation.h"
#include "CurveRangeDialog.h"
#include "ColorButton.h"
#include "QwtHistogram.h"
#include "BoxCurve.h"
#include "QwtBarCurve.h"
#include "OpenProjectDialog.h"
#include "IconLoader.h"
#include "core/Project.h"
#include "core/column/Column.h"
#include "core/datatypes/Double2StringFilter.h"
#include "lib/XmlStreamReader.h"
#include "table/future_Table.h"
#include "table/AsciiTableImportFilter.h"

// TODO: move tool-specific code to an extension manager
#include "ScreenPickerTool.h"
#include "DataPickerTool.h"
#include "TranslateCurveTool.h"
#include "MultiPeakFitTool.h"
#include "LineProfileTool.h"
#include "OriginGadgets.h"
#include "CurveMathTools.h"
#include "AdvancedSignalProc.h"
#include "GraphStatsAndSurfaceFit.h"
#include "ExtraPlotTypes.h"
#include "SettingsTransfer.h"
#include "RecalcEngine.h"
#include "I18nJapanese.h"
#include "GraphMiniToolbar.h"
#include "GraphInspectorDock.h"
#include "ModernUiTheme.h"
#include "ModernRibbonAndHud.h"
#include "UxNavigation.h"
#include "AutoUpdater.h"
#include "PythonAutomationServer.h"
#include "PiecewiseAndRobustFit.h"
#include "GraphDigitizer.h"

#include <stdio.h>
#include <optional>
#include <stdlib.h>
#include <cmath>
#include <algorithm>
#include <gsl/gsl_errno.h>

#include <QFileDialog>
#include <QScopedPointer>
#include <QInputDialog>
#include <QProgressDialog>
#include <QPrintDialog>
#include <QPixmapCache>
#include <QMenuBar>
#include <QClipboard>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTranslator>
#include <QSplitter>
#include <QApplication>
#include <QMessageBox>
#include <QPrinter>
#include <QActionGroup>
#include <QAction>
#include <QToolBar>
#include <QKeySequence>
#include <QImageReader>
#include <QImageWriter>
#include <QDateTime>
#include <QShortcut>
#include <QDockWidget>
#include <QTextStream>
#include <QVarLengthArray>
#include <QList>
#include <QUrl>
#include <QDesktopServices>
#include <QStatusBar>
#include <QToolButton>
#include <QSignalMapper>
#include <QUndoStack>
#include <QtDebug>
#include <QDialogButtonBox>
#include <QUndoView>
#include <QUndoStack>
#include <QTemporaryFile>
#include <QDebug>
#include <QTextCodec>
#include <QScrollBar>
#include <QMimeData>
#include <QBuffer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFontDatabase>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QHeaderView>
#include <QHash>
#include <QSlider>
#include <QMouseEvent>
#include <QKeyEvent>
#include <qwt_symbol.h>
#include <qwt_plot_canvas.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_errno.h>
#include "OleServer.h"
#include "PlotCurve.h"
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>

#include <zlib.h>

#include <iostream>
#include <memory>
using namespace std;

#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QLockFile>
#include <QFileSystemWatcher>
#include <QCryptographicHash>
#include <QScreen>
#include <QFrame>
#include <QScopedValueRollback>

#ifdef Q_OS_WIN
#include <io.h> // for _commit()
#include <qt_windows.h> // ReplaceFileW / MoveFileExW
#else
#include <unistd.h> // for fsync()
#include <signal.h> // for kill()
#endif

#include "ApplicationWindow_Internal.h"

using namespace std;
using namespace Qwt3D;
using namespace ApplicationWindowInternal;

namespace {

//! Flush a QFile's buffers and ask the OS to commit them to the storage device.
static bool flushAndCommitFile(QFile &f)
{
    if (!f.flush())
        return false;
#ifdef Q_OS_WIN
    return _commit(f.handle()) == 0;
#else
    return fsync(f.handle()) == 0;
#endif
}

/*!
 * Move the completely written file \a newPath over \a dstPath, keeping the previous
 * version of \a dstPath as "<dstPath>~".
 *
 * The existing file is never opened for writing or truncated: if the replacement cannot be
 * performed (e.g. because a sync client or a virus scanner keeps the file locked), the old
 * file stays as it is, \a newPath is left on disk and false is returned.
 */
static bool replaceFileWithBackup(const QString &newPath, const QString &dstPath, QString *errOut)
{
    const QString backupPath = dstPath + QLatin1Char('~');
#ifdef Q_OS_WIN
    const std::wstring wNew = QDir::toNativeSeparators(newPath).toStdWString();
    const std::wstring wDst = QDir::toNativeSeparators(dstPath).toStdWString();
    const std::wstring wBak = QDir::toNativeSeparators(backupPath).toStdWString();

    DWORD lastErr = 0;
    // Sync clients (Dropbox, OneDrive) and virus scanners hold files for a short moment only,
    // so retry for about two seconds before giving up.
    for (int attempt = 0; attempt < 20; ++attempt) {
        BOOL ok = FALSE;
        if (QFile::exists(dstPath)) {
            ok = ReplaceFileW(wDst.c_str(), wNew.c_str(), wBak.c_str(),
                              REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr);
        } else {
            // First save, or ReplaceFileW already moved the old file to the backup name
            // (ERROR_UNABLE_TO_MOVE_REPLACEMENT_2) before failing.
            ok = MoveFileExW(wNew.c_str(), wDst.c_str(), MOVEFILE_WRITE_THROUGH);
        }
        if (ok)
            return true;
        lastErr = GetLastError();
        const bool transient = lastErr == ERROR_SHARING_VIOLATION || lastErr == ERROR_LOCK_VIOLATION
                || lastErr == ERROR_ACCESS_DENIED || lastErr == ERROR_UNABLE_TO_REMOVE_REPLACED
                || lastErr == ERROR_UNABLE_TO_MOVE_REPLACEMENT
                || lastErr == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2;
        if (!transient)
            break;
        Sleep(100);
    }

    // ReplaceFileW is not available on every file system (some FAT/exFAT and network volumes).
    // Fall back to two plain renames. This is not atomic, but it still never truncates the
    // existing file.
    if (QFile::exists(newPath)) {
        if (!QFile::exists(dstPath)) {
            if (QFile::rename(newPath, dstPath))
                return true;
        } else {
            // Park the current version under a scratch name first, so that the existing "~"
            // backup is only given up once the new file is in place.
            const QString parkedPath = dstPath + QLatin1String(".old~");
            QFile::remove(parkedPath);
            if (QFile::rename(dstPath, parkedPath)) {
                if (QFile::rename(newPath, dstPath)) {
                    QFile::remove(backupPath);
                    QFile::rename(parkedPath, backupPath);
                    return true;
                }
                QFile::rename(parkedPath, dstPath); // put the previous version back
            }
        }
    }

    if (errOut)
        *errOut = qt_error_string(static_cast<int>(lastErr ? lastErr : GetLastError()));
    return false;
#else
    // rename(2) atomically replaces an existing destination
    if ((QFile::exists(dstPath)
         && rename(QFile::encodeName(dstPath), QFile::encodeName(backupPath)) != 0)
        || rename(QFile::encodeName(newPath), QFile::encodeName(dstPath)) != 0) {
        if (errOut)
            *errOut = qt_error_string(errno);
        return false;
    }
    return true;
#endif
}

/*!
 * gzip-compress \a srcPath into \a gzPath. \a gzPath is replaced only after the compressed
 * data has been completely written (the previous version is kept as "<gzPath>~").
 * \a srcPath is removed on success.
 */
bool compressFileGzipUnicode(const QString &srcPath, const QString &gzPath, QString *errOut = nullptr)
{
    QFile src(srcPath);
    if (!src.open(QIODevice::ReadOnly)) {
        if (errOut)
            *errOut = src.errorString();
        return false;
    }

    const QString tmpGzPath = gzPath + QLatin1String(".new");
    QFile::remove(tmpGzPath);
    QFile dst(tmpGzPath);
    if (!dst.open(QIODevice::WriteOnly)) {
        if (errOut)
            *errOut = dst.errorString();
        src.close();
        return false;
    }

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, 9, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        dst.close();
        QFile::remove(tmpGzPath);
        src.close();
        return false;
    }

    unsigned char inBuf[16384];
    unsigned char outBuf[16384];
    bool ok = true;
    int flushMode = Z_NO_FLUSH;

    do {
        qint64 nRead = src.read(reinterpret_cast<char *>(inBuf), sizeof(inBuf));
        if (nRead < 0) {
            ok = false;
            break;
        }
        flushMode = src.atEnd() ? Z_FINISH : Z_NO_FLUSH;
        zs.avail_in = static_cast<uInt>(nRead);
        zs.next_in = inBuf;

        do {
            zs.avail_out = sizeof(outBuf);
            zs.next_out = outBuf;
            const int ret = deflate(&zs, flushMode);
            if (ret == Z_STREAM_ERROR) {
                ok = false;
                break;
            }
            const qint64 have = static_cast<qint64>(sizeof(outBuf) - zs.avail_out);
            if (have > 0 && dst.write(reinterpret_cast<const char *>(outBuf), have) != have) {
                ok = false;
                break;
            }
        } while (zs.avail_out == 0);
    } while (ok && flushMode != Z_FINISH);

    deflateEnd(&zs);
    ok = ok && flushAndCommitFile(dst);
    if (!ok && errOut)
        *errOut = dst.error() != QFile::NoError ? dst.errorString() : src.errorString();
    dst.close();
    src.close();

    if (!ok) {
        QFile::remove(tmpGzPath);
        return false;
    }
    // On failure the compressed data stays in "<gzPath>.new" so that nothing is lost.
    if (!replaceFileWithBackup(tmpGzPath, gzPath, errOut))
        return false;
    QFile::remove(srcPath);
    return true;
}

class ObsoletePurgeCmd final : public QUndoCommand
{
public:
    void redo() override { setObsolete(true); }
};

} // anonymous namespace

static QString normalizedProjectFilePath(const QString &path)
{
    if (path.isEmpty() || path == QLatin1String("untitled"))
        return QString();
    const QFileInfo fi(path);
    const QString canon = fi.canonicalFilePath();
    const QString cleaned = QDir::cleanPath(canon.isEmpty() ? fi.absoluteFilePath() : canon);
#ifdef Q_OS_WIN
    return cleaned.toLower();
#else
    return cleaned;
#endif
}

static ApplicationWindow *findOpenProjectWindow(const QString &fn)
{
    const QString target = normalizedProjectFilePath(fn);
    if (target.isEmpty())
        return nullptr;
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        ApplicationWindow *aw = qobject_cast<ApplicationWindow *>(tlw);
        if (!aw || !aw->isVisible() || isBeingDestroyed(aw))
            continue;
        if (aw->projectname.isEmpty() || aw->projectname == QLatin1String("untitled"))
            continue;
        if (normalizedProjectFilePath(aw->projectname) == target)
            return aw;
    }
    return nullptr;
}

static void activateExistingProjectWindow(ApplicationWindow *aw)
{
    if (!aw)
        return;
    if (aw->isMinimized())
        aw->showNormal();
    aw->show();
    aw->raise();
    aw->activateWindow();
}

// A project file is read in two passes over the same QTextStream: the first builds folders,
// tables, matrices and notes, the second graphs, 3D plots and the results log. Each pass skips
// the bodies of its own blocks but not those of the other pass, so a body line starting with
// "<folder>" or equal to "</folder>" (note text, results log) looks like a folder marker to the
// pass that does not skip it, and counting markers puts later windows into the wrong folder.
// The first pass therefore also skips the second pass's blocks (this function) and records,
// for every genuine marker, the folder it makes current keyed by the stream position right
// after the marker line; the second pass follows only the markers found at those positions.
//
// Skips the body of a <multiLayer>, <SurfacePlot> or <log> block whose opening line `opening`
// has just been read, with the same end-of-block rules as the second pass.
static void skipSecondPassProjectBlock(QTextStream &t, const QString &opening)
{
    QString s = opening;
    if (s == QLatin1String("<multiLayer>")) {
        while (s != QLatin1String("</multiLayer>") && !t.atEnd()) {
            s = t.readLine();
            if (s.isNull())
                return;
            if (s.left(7) == QLatin1String("<graph>")) {
                while (s != QLatin1String("</graph>") && !t.atEnd()) {
                    s = t.readLine();
                    if (s.isNull())
                        return;
                }
            }
        }
    } else if (s == QLatin1String("<SurfacePlot>")) {
        while (s != QLatin1String("</SurfacePlot>") && !t.atEnd()) {
            s = t.readLine();
            if (s.isNull())
                return;
        }
    } else if (s.left(5) == QLatin1String("<log>") && !s.contains(QLatin1String("</log>"))) {
        do {
            s = t.readLine();
        } while (!s.isNull() && s != QLatin1String("</log>")
                 && !s.endsWith(QLatin1String("</log>")) && !t.atEnd());
    }
}

static bool isSecondPassProjectBlockStart(const QString &s)
{
    return s == QLatin1String("<multiLayer>") || s == QLatin1String("<SurfacePlot>")
            || s.left(5) == QLatin1String("<log>");
}

// Second pass: applies the folder marker line `s` (a "<folder>..." or "</folder>" line just
// read from `t`) using the folders recorded by the first pass (see skipSecondPassProjectBlock).
// A line at a position the first pass did not record is body text of a block the first pass
// skipped (note, table, ...) and is ignored. Without usable positions the folder is looked up
// by name as before.
static void applySecondPassFolderMarker(QTextStream &t, const QString &s,
                                        const QHash<qint64, QPointer<Folder>> &folderAtMarker,
                                        bool markerKeysValid, Folder *&currentFolder,
                                        Folder *rootFolder)
{
    const bool opening = s.left(8) == QLatin1String("<folder>");
    const qint64 key = markerKeysValid ? t.pos() : qint64(-1);
    if (key >= 0) {
        const auto it = folderAtMarker.constFind(key);
        if (it == folderAtMarker.constEnd())
            return; // not a marker for the first pass: body text of a skipped block
        if (Folder *f = it.value().data()) {
            currentFolder = f;
            return;
        }
    }
    if (opening) {
        const QStringList list = s.split(QLatin1Char('\t'));
        Folder *sub = (list.count() >= 2) ? currentFolder->findSubfolder(list[1]) : nullptr;
        if (sub)
            currentFolder = sub;
    } else {
        Folder *parent = static_cast<Folder *>(currentFolder->parent());
        currentFolder = parent ? parent : rootFolder;
    }
}

void ApplicationWindow::open()
{
    if (isNonInteractiveContext(this))
        return;
    if (guardProjectActionOutsideOleEmbedded(tr("open another project")))
        return;
    OpenProjectDialog *open_dialog = new OpenProjectDialog(this, d_extended_open_dialog);
    open_dialog->setDirectory(workingDir);
    auto &settings = getSettings();
    open_dialog->setCodec(settings.value("/General/Dialogs/LastUsedOriginLocale", "").toString());
    if (open_dialog->exec() != QDialog::Accepted || open_dialog->selectedFiles().isEmpty())
        return;
    workingDir = open_dialog->directory().path();
    settings.setValue("/General/Dialogs/LastUsedOriginLocale", open_dialog->codec());

    auto isSupportedDataPath = [](const QString &p) {
        return p.endsWith(".xlsx", Qt::CaseInsensitive)
                || p.endsWith(".xlsm", Qt::CaseInsensitive)
                || p.endsWith(".csv", Qt::CaseInsensitive)
                || p.endsWith(".tsv", Qt::CaseInsensitive)
                || p.endsWith(".tab", Qt::CaseInsensitive)
                || p.endsWith(".txt", Qt::CaseInsensitive)
                || p.endsWith(".dat", Qt::CaseInsensitive);
    };

    switch (open_dialog->openMode()) {
    case OpenProjectDialog::NewProject: {
        QString fn = open_dialog->selectedFiles()[0];
        QFileInfo fi(fn);

        if (projectname != "untitled") {
            if (normalizedProjectFilePath(fn) == normalizedProjectFilePath(projectname)) {
                QMessageBox::warning(this, tr("File opening error"),
                                     tr("The file: <b>%1</b> is the current file!").arg(fn));
                return;
            }
        }

        const bool isProjectFile = fn.endsWith(".sciprj", Qt::CaseInsensitive)
                || fn.endsWith(".sciprj~", Qt::CaseInsensitive)
                || fn.endsWith(".qti", Qt::CaseInsensitive)
                || fn.endsWith(".qti~", Qt::CaseInsensitive)
                || fn.endsWith(".sciprj.gz", Qt::CaseInsensitive)
                || fn.endsWith(".qti.gz", Qt::CaseInsensitive)
                || fn.endsWith(".opj", Qt::CaseInsensitive)
                || fn.endsWith(".opju", Qt::CaseInsensitive)
                || fn.endsWith(".ogm", Qt::CaseInsensitive)
                || fn.endsWith(".ogw", Qt::CaseInsensitive)
                || fn.endsWith(".ogg", Qt::CaseInsensitive)
                || fn.endsWith(".org", Qt::CaseInsensitive);
        const bool isDataFile = isSupportedDataPath(fn);

        if (isProjectFile || isDataFile) {
            if (!fi.exists()) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("The file: <b>%1</b> doesn't exist!").arg(fn));
                return;
            }

            if (isProjectFile) {
                if (ApplicationWindow *existing = findOpenProjectWindow(fn)) {
                    activateExistingProjectWindow(existing);
                    return;
                }
            }

            if (isDataFile)
                asciiDirPath = workingDir;
            saveSettings(); // the recent projects must be saved

            ApplicationWindow *a = open(fn);
            if (a && a != this) {
                a->workingDir = workingDir;
                if (!d_ole_embedded
                    && (fn.endsWith(".sciprj", Qt::CaseInsensitive)
                        || fn.endsWith(".sciprj~", Qt::CaseInsensitive)
                        || fn.endsWith(".sciprj.gz", Qt::CaseInsensitive)
                        || fn.endsWith(".qti.gz", Qt::CaseInsensitive)
                        || fn.endsWith(".qti", Qt::CaseInsensitive)
                        || fn.endsWith(".qti~", Qt::CaseInsensitive)
                        || fn.endsWith(".opj", Qt::CaseInsensitive)
                        || fn.endsWith(".opju", Qt::CaseInsensitive)
                        || fn.endsWith(".ogg", Qt::CaseInsensitive)
                        || fn.endsWith(".org", Qt::CaseInsensitive)))
                    this->close();
            }
        } else if (fn.endsWith(".xls", Qt::CaseInsensitive)
                   || fn.endsWith(".ods", Qt::CaseInsensitive)) {
            QMessageBox::information(
                    this, tr("File opening error"),
                    tr("Old Excel (.xls) and OpenDocument (.ods) files cannot be read directly.\n"
                       "Save them as an Excel Workbook (.xlsx) or as CSV and drop that file, or "
                       "copy the cells in Excel (Ctrl+C) and paste them into a worksheet here "
                       "(Ctrl+V)."));
            return;
        } else {
            QMessageBox::critical(
                    this, tr("File opening error"),
                    tr("The file <b>%1</b> cannot be opened as a project or worksheet.<br>"
                       "Supported formats: ToriGraph projects (.sciprj, .sciprj.gz), Excel "
                       "workbooks (.xlsx, .xlsm), and text data (.csv, .tsv, .txt, .dat).")
                            .arg(fn));
            return;
        }
        break;
    }
    case OpenProjectDialog::NewFolder: {
        const QString selFn = open_dialog->selectedFiles()[0];
        if (isSupportedDataPath(selFn))
            open(selFn);
        else
            appendProject(selFn);
        break;
    }
    }
}

ApplicationWindow *ApplicationWindow::open(const QString &fn, const QStringList &scriptArgs)
{
    if (fn.endsWith(".opju", Qt::CaseInsensitive))
        return importOPJ(fn);
    else if (fn.endsWith(".opj", Qt::CaseInsensitive) || fn.endsWith(".ogm", Qt::CaseInsensitive)
             || fn.endsWith(".ogw", Qt::CaseInsensitive) || fn.endsWith(".ogg", Qt::CaseInsensitive)
             || fn.endsWith(".org", Qt::CaseInsensitive))
#ifdef ORIGIN_IMPORT
        return importOPJ(fn);
#else
    {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(
                    this, tr("File opening error"),
                    tr("ToriGraph currently does not support Origin import."));
        }
        return 0;
    }
#endif
    else if (fn.endsWith(".py", Qt::CaseInsensitive))
        return loadScript(fn, scriptArgs);
    else if (fn.endsWith(".sciprj", Qt::CaseInsensitive)
             || fn.endsWith(".sciprj.gz", Qt::CaseInsensitive)
             || fn.endsWith(".qti", Qt::CaseInsensitive)
             || fn.endsWith(".qti.gz", Qt::CaseInsensitive)
             || fn.endsWith(".sciprj~", Qt::CaseInsensitive)
             || fn.endsWith(".sciprj.gz~", Qt::CaseInsensitive)
             || fn.endsWith(".qti~", Qt::CaseInsensitive)
             || fn.endsWith(".qti.gz~", Qt::CaseInsensitive)) {
        if (!isNonInteractiveContext(this)) {
            if (projectname != QLatin1String("untitled")
                && normalizedProjectFilePath(fn) == normalizedProjectFilePath(projectname)) {
                activateExistingProjectWindow(this);
                return this;
            }
            if (ApplicationWindow *existing = findOpenProjectWindow(fn)) {
                activateExistingProjectWindow(existing);
                return existing;
            }
        }
        return openProject(fn);
    } else if (fn.endsWith(".xlsx", Qt::CaseInsensitive)
             || fn.endsWith(".xlsm", Qt::CaseInsensitive)) {
        QString summary;
        if (!XlsxImport::importDroppedFile(this, fn, &summary))
            return nullptr;
        if (statusBar() && !summary.isEmpty())
            statusBar()->showMessage(tr("Imported %1").arg(summary), 15000);
        return this;
    } else if (fn.endsWith(".csv", Qt::CaseInsensitive)
               || fn.endsWith(".tsv", Qt::CaseInsensitive)
               || fn.endsWith(".tab", Qt::CaseInsensitive)
               || fn.endsWith(".txt", Qt::CaseInsensitive)
               || fn.endsWith(".dat", Qt::CaseInsensitive)) {
        QFile f(fn);
        if (!f.open(QIODevice::ReadOnly)) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("The file: <b>%1</b> doesn't exist!").arg(fn));
            }
            return nullptr;
        }
        const QByteArray raw = f.readAll();
        f.close();

        const QFileInfo fi(fn);
        const QString baseTitle =
                fi.completeBaseName().isEmpty() ? tr("Table") : fi.completeBaseName();
        AsciiTableImportFilter::ImportReport report;
        std::unique_ptr<future::Table> ft(
                AsciiTableImportFilter::importDroppedTextData(raw, baseTitle, fn, &report));
        if (!ft || ft->columnCount() < 1 || ft->rowCount() < 1) {
            const QString summary = AsciiTableImportFilter::describeImportReport(report);
            const QString errMsg = tr("Could not parse any worksheet data from \"%1\"%2.")
                                           .arg(fi.fileName(),
                                                summary.isEmpty() ? QString()
                                                                  : (QStringLiteral("\n") + summary));
            setProperty("torigraph_last_import_error", errMsg);
            if (!isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Data Import Failed"), errMsg);
            return nullptr;
        }

        if (projectname == QLatin1String("untitled")) {
            const QList<MyWidget *> allWins = windowsList();
            if (allWins.size() == 1) {
                if (Table *cand = qobject_cast<Table *>(allWins.first())) {
                    if (cand->d_future_table && cand->windowLabel().isEmpty()
                        && dependingPlots(cand->name()).isEmpty() && cand->columnCount() <= 2) {
                        bool untouched = true;
                        for (int c = 0; c < cand->columnCount() && untouched; ++c) {
                            Column *col = cand->column(c);
                            if (!col || !col->longName().isEmpty() || !col->units().isEmpty()
                                || !col->comment().isEmpty() || !col->formulaIntervals().isEmpty()) {
                                untouched = false;
                                break;
                            }
                            for (int r = 0; r < col->rowCount(); ++r) {
                                const bool empty = (col->columnMode()
                                                    == SciDAVis::ColumnMode::Numeric)
                                        ? col->isInvalid(r)
                                        : col->textAt(r).isEmpty();
                                if (!empty) {
                                    untouched = false;
                                    break;
                                }
                            }
                        }
                        if (untouched) {
                            cand->askOnCloseEvent(false);
                            cand->d_future_table->remove();
                        }
                    }
                }
            }
        }

        QList<Column *> cols;
        for (int c = 0; c < ft->columnCount(); ++c) {
            Column *src = ft->column(c);
            if (!src)
                continue;
            Column *dst = new Column(src->name(), src->columnMode());
            dst->copy(src);
            dst->setPlotDesignation(src->plotDesignation());
            dst->setLongName(src->longName());
            dst->setUnits(src->units());
            dst->setComment(src->comment());
            if (auto *srcOut = dynamic_cast<Double2StringFilter *>(src->outputFilter())) {
                if (auto *dstOut = dynamic_cast<Double2StringFilter *>(dst->outputFilter())) {
                    dstOut->setNumericFormat(srcOut->numericFormat());
                    dstOut->setNumDigits(srcOut->numDigits());
                }
            }
            cols << dst;
        }
        Table *t = newTable(generateUniqueName(baseTitle), fn, cols);
        if (!t) {
            qDeleteAll(cols);
            return nullptr;
        }
        if (t->d_future_table) {
            t->d_future_table->setImportSource(fn, QString(), report.separator, report.encoding,
                                               0, -1, t->numCols());
        }
        offerPlotAfterImport(t);
        modifiedProject();
        if (statusBar()) {
            statusBar()->showMessage(
                    tr("Imported %1")
                            .arg(fi.fileName() + QStringLiteral(": ")
                                 + AsciiTableImportFilter::describeImportReport(report)),
                    15000);
        }
        return this;
    } else
        return plotFile(fn);
}

void ApplicationWindow::openRecentProject()
{
    if (isNonInteractiveContext(this))
        return;
    QAction *trigger = qobject_cast<QAction *>(sender());
    if (!trigger)
        return;
    QString fn = trigger->text();
    int pos = fn.indexOf(" ", 0);
    fn = fn.right(fn.length() - pos - 1);
    if (guardProjectActionOutsideOleEmbedded(tr("open a recent project"), fn))
        return;

    QFile f(fn);
    if (!f.exists()) {
        if (!d_headless_mode && !d_ole_embedded && !m_batch) {
            QMessageBox::critical(this, tr("File Open Error"),
                                  tr("The file: <b> %1 </b> <p>does not exist anymore!"
                                     "<p>It will be removed from the list.")
                                          .arg(fn));
        }

        recentProjects.removeAll(fn);
        updateRecentProjectsList();
        return;
    }

    if (projectname != "untitled") {
        if (normalizedProjectFilePath(fn) == normalizedProjectFilePath(projectname)) {
            if (!d_headless_mode && !d_ole_embedded && !m_batch) {
                QMessageBox::warning(this, tr("File opening error"),
                                     tr("The file: <p><b> %1 </b><p> is the current file!")
                                             .arg(fn));
            }
            return;
        }
    }

    if (!fn.isEmpty()) {
        if (ApplicationWindow *existing = findOpenProjectWindow(fn)) {
            activateExistingProjectWindow(existing);
            return;
        }
        saveSettings(); // the recent projects must be saved
        ApplicationWindow *a = open(fn);
        if (a && a != this && !d_ole_embedded
            && (fn.endsWith(".sciprj", Qt::CaseInsensitive)
                || fn.endsWith(".sciprj~", Qt::CaseInsensitive)
                || fn.endsWith(".sciprj.gz", Qt::CaseInsensitive)
                || fn.endsWith(".qti.gz", Qt::CaseInsensitive)
                || fn.endsWith(".qti", Qt::CaseInsensitive)
                || fn.endsWith(".qti~", Qt::CaseInsensitive)
                || fn.endsWith(".opj", Qt::CaseInsensitive)
                || fn.endsWith(".opju", Qt::CaseInsensitive)
                || fn.endsWith(".ogg", Qt::CaseInsensitive)
                || fn.endsWith(".org", Qt::CaseInsensitive)))
            this->close();
    }
}

QFile *ApplicationWindow::openCompressedFile(const QString &fn)
{
    const bool nonInteractive = isNonInteractiveContext(this);
    QFile inFile(fn);
    if (!inFile.open(QIODevice::ReadOnly)) {
        if (!nonInteractive)
            QMessageBox::critical(this, tr("File opening error"), tr("zlib can't open %1.").arg(fn));
        return 0;
    }

    QTemporaryFile *file = new QTemporaryFile();
    if (!file || !file->open()) {
        inFile.close();
        delete file;
        if (!nonInteractive) {
            QMessageBox::critical(
                    this, tr("File opening error"),
                    tr("Can't create temporary file for writing uncompressed copy of %1.").arg(fn));
        }
        return 0;
    }

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 15 + 32) != Z_OK) {
        inFile.close();
        file->close();
        delete file;
        if (!nonInteractive)
            QMessageBox::critical(this, tr("File opening error"), tr("zlib can't open %1.").arg(fn));
        return 0;
    }

    unsigned char inBuf[16384];
    unsigned char outBuf[16384];
    bool ok = true;
    bool sawStreamEnd = false;
    bool doneAllMembers = false;
    qint64 totalBytesRead = 0;
    const bool hasBackupSibling = !fn.endsWith(QLatin1String("~")) && QFile::exists(fn + "~");

    while (!inFile.atEnd() && !doneAllMembers) {
        const qint64 nRead = inFile.read(reinterpret_cast<char *>(inBuf), sizeof(inBuf));
        if (nRead < 0) {
            if (!nonInteractive && !hasBackupSibling)
                QMessageBox::critical(this, tr("File opening error"), inFile.errorString());
            ok = false;
            break;
        }
        if (nRead == 0)
            break;
        totalBytesRead += nRead;

        zs.avail_in = static_cast<uInt>(nRead);
        zs.next_in = inBuf;

        while (zs.avail_in > 0) {
            sawStreamEnd = false;
            zs.avail_out = sizeof(outBuf);
            zs.next_out = outBuf;
            const int ret = inflate(&zs, Z_NO_FLUSH);
            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR
                || ret == Z_STREAM_ERROR) {
                if (!nonInteractive && !hasBackupSibling) {
                    const QString zMsg = zs.msg ? QString::fromUtf8(zs.msg)
                                                : tr("The file <b>%1</b> is not a valid project file.")
                                                          .arg(fn);
                    QMessageBox::critical(this, tr("File opening error"), zMsg);
                }
                ok = false;
                break;
            }
            const qint64 have = static_cast<qint64>(sizeof(outBuf) - zs.avail_out);
            if (have > 0 && file->write(reinterpret_cast<const char *>(outBuf), have) != have) {
                if (!nonInteractive) {
                    QMessageBox::critical(
                            this, tr("File opening error"),
                            tr("Error writing to temporary file: %1").arg(file->errorString()));
                }
                ok = false;
                break;
            }
            if (ret == Z_STREAM_END) {
                const uInt remAvail = zs.avail_in;
                const QByteArray filePeek = (remAvail < 2) ? inFile.peek(2) : QByteArray();
                unsigned char b0 = 0, b1 = 0;
                bool hasTwo = false;
                if (remAvail >= 2) {
                    b0 = zs.next_in[0];
                    b1 = zs.next_in[1];
                    hasTwo = true;
                } else if (remAvail == 1 && filePeek.size() >= 1) {
                    b0 = zs.next_in[0];
                    b1 = static_cast<unsigned char>(filePeek.at(0));
                    hasTwo = true;
                } else if (remAvail == 0 && filePeek.size() >= 2) {
                    b0 = static_cast<unsigned char>(filePeek.at(0));
                    b1 = static_cast<unsigned char>(filePeek.at(1));
                    hasTwo = true;
                }
                if (hasTwo && b0 == 0x1f && b1 == 0x8b) {
                    inflateReset(&zs);
                    sawStreamEnd = false;
                    if (zs.avail_in == 0)
                        break;
                } else {
                    sawStreamEnd = true;
                    doneAllMembers = true;
                    break;
                }
            }
        }
        if (!ok)
            break;
    }

    inflateEnd(&zs);
    inFile.close();

    if (ok && (totalBytesRead <= 0 || !sawStreamEnd)) {
        if (!nonInteractive && !hasBackupSibling) {
            QMessageBox::critical(this, tr("File opening error"),
                                  tr("The file <b>%1</b> is not a valid project file.").arg(fn));
        }
        ok = false;
    }

    if (!ok || !file->flush()) {
        file->close();
        delete file;
        return 0;
    }

    file->reset();
    return file;
}

QString ApplicationWindow::projectLockFilePath(const QString &fn) const
{
    const QString norm = normalizedProjectFilePath(fn);
    if (norm.isEmpty() || d_ole_embedded || fn.endsWith(QLatin1Char('~')))
        return QString();

    const QString recDir = normalizedProjectFilePath(recoveryDirPath());
    if (!recDir.isEmpty() && norm.startsWith(recDir))
        return QString();

    const QString lockDir = QDir::tempPath() + QStringLiteral("/torigraph_locks");
    const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(norm.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    return lockDir + QStringLiteral("/prj_") + hash + QStringLiteral(".lock");
}

bool ApplicationWindow::isProjectFileLockedByOther(const QString &fn, qint64 *holderPidOut) const
{
    if (holderPidOut)
        *holderPidOut = 0;
    const QString lockPath = projectLockFilePath(fn);
    if (lockPath.isEmpty() || !QFileInfo::exists(lockPath))
        return false;

    QLockFile probe(lockPath);
    probe.setStaleLockTime(0);
    if (probe.tryLock(50)) {
        probe.unlock();
        return false;
    }

    qint64 holderPid = 0;
    QString holderHost, holderApp;
    probe.getLockInfo(&holderPid, &holderHost, &holderApp);
    if (holderPid == QCoreApplication::applicationPid())
        return false;
    if (holderPidOut)
        *holderPidOut = holderPid;
    return true;
}

bool ApplicationWindow::acquireProjectFileLock(const QString &fn, bool interactive)
{
    const QString lockPath = projectLockFilePath(fn);
    if (lockPath.isEmpty())
        return true;

    QDir().mkpath(QFileInfo(lockPath).absolutePath());

    auto *newLock = new QLockFile(lockPath);
    newLock->setStaleLockTime(0);
    if (newLock->tryLock(50)) {
        releaseProjectFileLock();
        d_project_lock = newLock;
        d_project_read_only_lock = false;
        return true;
    }

    qint64 holderPid = 0;
    QString holderHost, holderApp;
    newLock->getLockInfo(&holderPid, &holderHost, &holderApp);
    if (holderPid == QCoreApplication::applicationPid() || !interactive) {
        delete newLock;
        d_project_read_only_lock = false;
        return true;
    }

    delete newLock;
    QMessageBox box(QMessageBox::Warning, tr("Project Already Open"),
                    tr("The project <b>%1</b> appears to be open in another ToriGraph process"
                       "%2.<br><br>Opening it writable in two places can overwrite changes.")
                            .arg(QDir::toNativeSeparators(fn).toHtmlEscaped(),
                                 holderPid > 0 ? tr(" (PID %1)").arg(holderPid) : QString()),
                    QMessageBox::NoButton, this);
    QPushButton *roBtn = box.addButton(tr("Open Read-Only"), QMessageBox::AcceptRole);
    QPushButton *cancelBtn = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(cancelBtn);
    box.exec();
    if (box.clickedButton() == roBtn) {
        releaseProjectFileLock();
        d_project_read_only_lock = true;
        return true;
    }
    return false;
}

void ApplicationWindow::releaseProjectFileLock()
{
    d_project_read_only_lock = false;
    if (d_project_lock) {
        if (d_project_lock->isLocked())
            d_project_lock->unlock();
        delete d_project_lock;
        d_project_lock = nullptr;
    }
}

void ApplicationWindow::untrackOpenedProjectFile()
{
    if (d_project_watcher && !d_watched_project_path.isEmpty())
        d_project_watcher->removePath(d_watched_project_path);
    d_watched_project_path.clear();
    d_watched_project_mtime = QDateTime();
    d_watched_project_size = -1;
    d_external_change_notified = false;
}

void ApplicationWindow::trackOpenedProjectFile(const QString &fn)
{
    untrackOpenedProjectFile();
    if (fn.isEmpty() || fn == QLatin1String("untitled") || fn.endsWith(QLatin1Char('~'))
        || d_ole_embedded) {
        return;
    }
    const QString norm = normalizedProjectFilePath(fn);
    const QString recDir = normalizedProjectFilePath(recoveryDirPath());
    if (!recDir.isEmpty() && norm.startsWith(recDir))
        return;

    QFileInfo fi(fn);
    if (!fi.exists())
        return;
    d_watched_project_path = fi.absoluteFilePath();
    d_watched_project_mtime = fi.lastModified();
    d_watched_project_size = fi.size();
    d_external_change_notified = false;

    if (!isNonInteractiveContext(this)) {
        if (!d_project_watcher) {
            d_project_watcher = new QFileSystemWatcher(this);
            connect(d_project_watcher, &QFileSystemWatcher::fileChanged, this,
                    [this](const QString &changedPath) {
                        if (d_ignore_watcher_changes || d_external_change_notified)
                            return;
                        if (normalizedProjectFilePath(changedPath)
                            != normalizedProjectFilePath(d_watched_project_path)) {
                            return;
                        }
                        QFileInfo cfi(d_watched_project_path);
                        if (!cfi.exists())
                            return;
                        if (cfi.lastModified() == d_watched_project_mtime
                            && cfi.size() == d_watched_project_size) {
                            return;
                        }
                        d_external_change_notified = true;
                        if (statusBar()) {
                            statusBar()->showMessage(
                                    tr("Warning: '%1' was modified on disk by another program "
                                       "(e.g. cloud sync).")
                                            .arg(cfi.fileName()),
                                    15000);
                        }
                    });
        }
        d_project_watcher->addPath(d_watched_project_path);
    }
}

bool ApplicationWindow::checkExternalModificationBeforeSave(bool *outSavedAsNew)
{
    if (isNonInteractiveContext(this) || d_watched_project_path.isEmpty())
        return true;
    if (normalizedProjectFilePath(d_watched_project_path)
        != normalizedProjectFilePath(projectname)) {
        return true;
    }
    QFileInfo fi(d_watched_project_path);
    if (!fi.exists())
        return true;
    if (fi.lastModified() == d_watched_project_mtime && fi.size() == d_watched_project_size
        && !d_external_change_notified) {
        return true;
    }

    QMessageBox box(QMessageBox::Warning, tr("File Changed on Disk"),
                    tr("The project file <b>%1</b> has been modified on disk since you opened or "
                       "last saved it (for example by another program or cloud sync).<br><br>"
                       "What would you like to do?")
                            .arg(QDir::toNativeSeparators(projectname).toHtmlEscaped()),
                    QMessageBox::NoButton, this);
    QPushButton *saveAsBtn = box.addButton(tr("Save As New File..."), QMessageBox::AcceptRole);
    QPushButton *overwriteBtn = box.addButton(tr("Overwrite Disk File"), QMessageBox::DestructiveRole);
    QPushButton *cancelBtn = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(saveAsBtn);
    box.exec();
    QAbstractButton *clicked = box.clickedButton();
    if (clicked == saveAsBtn) {
        const bool okAs = saveProjectAs();
        if (outSavedAsNew)
            *outSavedAsNew = okAs;
        return false;
    }
    if (clicked == overwriteBtn)
        return true;
    Q_UNUSED(cancelBtn)
    if (outSavedAsNew)
        *outSavedAsNew = false;
    return false;
}

bool ApplicationWindow::offerBackupIfPresent(const QString &corruptFn, bool *outAccepted)
{
    if (outAccepted)
        *outAccepted = false;
    if (isNonInteractiveContext(this) || corruptFn.endsWith(QLatin1String("~"))
        || !QFile::exists(corruptFn + QStringLiteral("~"))) {
        return false;
    }
    const int choice = QMessageBox::question(
            this, tr("File opening error"),
            tr("The file <b>%1</b> is corrupted, but there exists a backup copy.<br>Do you "
               "want to open the backup instead?")
                    .arg(corruptFn),
            QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape);
    if (choice == QMessageBox::Yes) {
        if (outAccepted)
            *outAccepted = true;
        QMessageBox::information(
                this, tr("Opening backup copy"),
                tr("The original (corrupt) file is being left untouched, in case you want to "
                   "try rescuing data manually. If you want to continue working with the "
                   "automatically restored backup copy, you have to explicitly overwrite the "
                   "original file."));
        return true;
    }
    return false;
}

bool ApplicationWindow::guardProjectActionOutsideOleEmbedded(const QString &actionDesc,
                                                           const QString &filePathToOpen)
{
    if (!d_ole_embedded)
        return false;
    if (!isNonInteractiveContext(this)) {
        const QString body = tr("This window is editing an embedded PowerPoint graph object, "
                                "so it cannot %1 here without losing your embedded changes.\n\n"
                                "Use File -> Save to update PowerPoint, or Save Project As... "
                                "to save a separate .sciprj file.")
                                     .arg(actionDesc);
        if (!filePathToOpen.isEmpty()) {
            const auto btn = QMessageBox::question(
                    this, tr("Embedded PowerPoint Object"),
                    body + QStringLiteral("\n\n")
                            + tr("Open \"%1\" in a separate ToriGraph window?")
                                      .arg(QFileInfo(filePathToOpen).fileName()),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (btn == QMessageBox::Yes) {
                QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                        QStringList() << filePathToOpen);
            }
        } else {
            QMessageBox::information(this, tr("Embedded PowerPoint Object"), body);
        }
    }
    return true;
}

bool ApplicationWindow::isInternalHelperWindow(const MyWidget *w)
{
    if (!w)
        return false;
    if (w->property("_tg_internal_helper").toBool())
        return true;
    const QString oname = w->objectName();
    const QString wname = const_cast<MyWidget *>(w)->name();
    return oname.startsWith(QLatin1String("GadgetFit_"))
            || wname.startsWith(QLatin1String("GadgetFit_"));
}

bool ApplicationWindow::loadProject(const QString &fn)
{
    return loadProjectImpl(fn, nullptr);
}

bool ApplicationWindow::loadProjectImpl(const QString &fn, ApplicationWindow **outReplacement)
{
    if (outReplacement)
        *outReplacement = nullptr;
    const bool nonInteractive = isNonInteractiveContext(this);
    const bool isBackupTilde = fn.endsWith(QLatin1String("~"));
    renamedTables.clear();
    setProperty("_tg_load_truncated", false);
    unique_ptr<QFile> file;

    if (!isBackupTilde && !acquireProjectFileLock(fn, !nonInteractive))
        return false;

    if (fn.endsWith(".gz", Qt::CaseInsensitive) || fn.endsWith(".gz~", Qt::CaseInsensitive)) {
        file.reset(openCompressedFile(fn));
        if (!file) {
            releaseProjectFileLock();
            bool backupAccepted = false;
            if (offerBackupIfPresent(fn, &backupAccepted))
                return loadProjectImpl(fn + "~", outReplacement);
            if (!backupAccepted && !nonInteractive
                && !fn.endsWith(QLatin1String("~")) && QFile::exists(fn + "~")) {
                QMessageBox::critical(
                        this, tr("File opening error"),
                        tr("The file <b>%1</b> is not a valid project file.").arg(fn));
            }
            return false;
        }
    } else {
        file.reset(new QFile(fn));
        if (!file->open(QIODevice::ReadOnly)) {
            releaseProjectFileLock();
            if (!nonInteractive)
                QMessageBox::critical(this, tr("File opening error"), file->errorString());
            return false;
        }
    }

    QTextStream t(file.get());
    t.setCodec(QTextCodec::codecForName("UTF-8"));
    QString s;
    QStringList list;

    s = t.readLine();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    list = s.split(QRegExp("\\s"), Qt::SkipEmptyParts);
#else
    list = s.split(QRegExp("\\s"), QString::SkipEmptyParts);
#endif
    if (list.count() < 2 || (list[0] != "SciDAVis" && list[0] != "ToriGraph" && list[0] != "QtiPlot")) {
        releaseProjectFileLock();
        bool backupAccepted = false;
        if (offerBackupIfPresent(fn, &backupAccepted))
            return loadProjectImpl(fn + "~", outReplacement);
        if (!backupAccepted && !nonInteractive) {
            QMessageBox::critical(this, tr("File opening error"),
                                  tr("The file <b>%1</b> is not a valid project file.").arg(fn));
        }
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList vl = list[1].split(".", Qt::SkipEmptyParts);
#else
    QStringList vl = list[1].split(".", QString::SkipEmptyParts);
#endif
    while (vl.size() < 3)
        vl << QStringLiteral("0");
    if (fn.endsWith(".qti", Qt::CaseInsensitive) || fn.endsWith(".qti.gz", Qt::CaseInsensitive)
        || fn.endsWith(".qti~", Qt::CaseInsensitive)
        || fn.endsWith(".qti.gz~", Qt::CaseInsensitive)) {
        d_file_version = 100 * (vl[0]).toInt() + 10 * (vl[1]).toInt() + (vl[2]).toInt();
        if (d_file_version > 90) {
            releaseProjectFileLock();
            if (!nonInteractive) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("ToriGraph does not support QtiPlot project files from "
                                         "versions later than 0.9.0."));
            }
            return false;
        }
    } else
        d_file_version = ((vl[0]).toInt() << 16) + ((vl[1]).toInt() << 8) + (vl[2]).toInt();

    QString effectiveProjectName = fn;
    if (isBackupTilde) {
        const QString stripped = fn.left(fn.size() - 1);
        if (stripped.endsWith(".sciprj", Qt::CaseInsensitive)
            || stripped.endsWith(".sciprj.gz", Qt::CaseInsensitive)) {
            effectiveProjectName = stripped;
        } else {
            effectiveProjectName = QStringLiteral("untitled");
        }
    }
    projectname = effectiveProjectName;
    setWindowTitle(tr("ToriGraph") + " - " + projectname
                   + (isBackupTilde ? (QLatin1Char(' ') + tr("[backup]")) : QString()));

    QFileInfo fi(isBackupTilde && projectname != QLatin1String("untitled") ? projectname : fn);
    QString baseName = fi.fileName();

    if (d_file_version < 73)
        t.readLine();

    s = t.readLine();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    list = s.split("\t", Qt::SkipEmptyParts);
#else
    list = s.split("\t", QString::SkipEmptyParts);
#endif
    if (list.count() >= 2 && list[0] == "<scripting-lang>") {
        if (!setScriptingLang(list[1], true) && !nonInteractive)
            QMessageBox::warning(
                    this, tr("File opening error"),
                    tr("The file \"%1\" was created using \"%2\" as scripting language.\n\n"
                       "Initializing support for this language FAILED; I'm using \"%3\" instead.\n"
                       "Various parts of this file may not be displayed as expected.")
                            .arg(fn)
                            .arg(list[1])
                            .arg(scriptEnv->objectName()));

        s = t.readLine();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        list = s.split("\t", Qt::SkipEmptyParts);
#else
        list = s.split("\t", QString::SkipEmptyParts);
#endif
    }
    int aux = 0, widgets = (list.count() >= 2) ? list[1].toInt() : 0;

    QString titleBase = tr("Window") + ": ";
    QString title = titleBase + "1/" + QString::number(widgets) + "  ";

    std::unique_ptr<QProgressDialog> progress;
    if (!nonInteractive) {
        progress.reset(new QProgressDialog());
        progress->setWindowModality(Qt::WindowModal);
        progress->setRange(0, widgets);
        progress->setMinimumWidth(width() / 2);
        progress->setWindowTitle(tr("Opening file") + ": " + baseName);
        progress->setLabelText(title);
        progress->activateWindow();
    }
    auto wasProgressCanceled = [&progress]() -> bool {
        return progress && progress->wasCanceled();
    };
    auto updateProgressStep = [&](int step) {
        if (!progress)
            return;
        if (step == 1 || step >= widgets || (step % 8 == 0)) {
            progress->setLabelText(titleBase + QString::number(step) + "/"
                                   + QString::number(widgets));
            progress->setValue(step);
        }
    };

    BatchWindowUpdates batchGuard(this);

    Folder *cf = projectFolder();
    folders.blockSignals(true);
    blockSignals(true);
    // rename project folder item
    FolderListItem *item = (FolderListItem *)folders.topLevelItem(0);
    item->setText(0, fi.baseName());
    item->folder()->setName(fi.baseName());

    bool truncatedProject = false;
    // Folder made current by each genuine <folder> / </folder> line of the first pass, keyed by
    // the stream position after that line (see skipSecondPassProjectBlock). The second pass
    // re-enters exactly these folders: looking them up by name would send the windows of a
    // second same-named sibling folder into the first one.
    QHash<qint64, QPointer<Folder>> folderAtMarker;
    bool markerKeysValid = true;
    auto recordFolderMarker = [&t, &folderAtMarker, &markerKeysValid](Folder *f) {
        const qint64 key = t.pos();
        if (key < 0)
            markerKeysValid = false;
        else
            folderAtMarker.insert(key, QPointer<Folder>(f));
    };
    // process tables and matrix information
    while (!t.atEnd() && !wasProgressCanceled()) {
        s = t.readLine(1048576); // workaround for safely reading very big lines
        if (s.isNull())
            break;
        list.clear();
        if (s.left(8) == "<folder>") {
            list = s.split("\t");
            if (list.count() < 4) {
                truncatedProject = true;
                break;
            }
            Folder &f = current_folder->addChild<Folder>(list[1]);
            f.setBirthDate(list[2]);
            f.setModificationDate(list[3]);
            if (list.count() > 4)
                if (list[4] == "current")
                    cf = &f;

            FolderListItem *fli = new FolderListItem(current_folder->folderListItem(), &f);
            fli->setText(0, list[1]);
            f.setFolderListItem(fli);

            current_folder = &f;
            recordFolderMarker(&f);
        } else if (s == "<table>") {
            ++aux;
            openTable(this, t);
            updateProgressStep(aux);
        } else if (s.startsWith(QLatin1String("<workbook>\t"))) {
            WorkbookManager::instance(this)->restoreFromLine(s, current_folder, renamedTables);
        } else if (s.left(17) == "<TableStatistics>") {
            QStringList lst;
            while (s != "</TableStatistics>" && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
                lst << s;
            }
            if (s != "</TableStatistics>") {
                truncatedProject = true;
                break;
            }
            if (!lst.isEmpty())
                lst.pop_back();
            openTableStatistics(lst);
        } else if (s == "<matrix>") {
            ++aux;
            QStringList lst;
            while (s != "</matrix>" && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
                lst << s;
            }
            if (s != "</matrix>") {
                truncatedProject = true;
                break;
            }
            if (!lst.isEmpty())
                lst.pop_back();
            openMatrix(this, lst);
            updateProgressStep(aux);
        } else if (s == "<note>") {
            ++aux;
            for (int i = 0; i < 3 && !t.atEnd(); i++) {
                s = t.readLine();
                if (s.isNull())
                    break;
                list << s;
            }
            if (list.size() < 3) {
                truncatedProject = true;
                break;
            }
            Note *m = openNote(this, list);
            QStringList cont;
            while (s != "</note>" && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
                cont << s;
            }
            if (s != "</note>") {
                truncatedProject = true;
                break;
            }
            if (!cont.isEmpty())
                cont.pop_back();
            if (m)
                m->restore(cont);
            updateProgressStep(aux);
        } else if (s == "</folder>") {
            Folder *parent = (Folder *)current_folder->parent();
            if (!parent)
                current_folder = projectFolder();
            else
                current_folder = parent;
            recordFolderMarker(current_folder);
        } else if (isSecondPassProjectBlockStart(s)) {
            skipSecondPassProjectBlock(t, s);
        }
    }

    if (wasProgressCanceled()) {
        releaseProjectFileLock();
        saved = true;
        close();
        return false;
    }

    // process the rest
    t.seek(0);
    current_folder = projectFolder();

    MultiLayer *plot = 0;
    while (!t.atEnd() && !wasProgressCanceled()) {
        s = t.readLine(1048576); // workaround for safely reading very big lines
        if (s.isNull())
            break;
        if (s.left(8) == "<folder>" || s == "</folder>") {
            applySecondPassFolderMarker(t, s, folderAtMarker, markerKeysValid, current_folder,
                                        projectFolder());
        } else if (s == "<multiLayer>") { // process multilayers information
            ++aux;

            s = t.readLine();
            QStringList graph = s.split("\t");
            if (s.isNull() || graph.size() < 4) {
                truncatedProject = true;
                break;
            }
            QString caption = graph[0];
            plot = multilayerPlot(caption);
            plot->setCols(graph[1].toInt());
            plot->setRows(graph[2].toInt());

            setListViewDate(caption, graph[3]);
            plot->setBirthDate(graph[3]);

            restoreWindowGeometry(this, plot, t.readLine());
            plot->blockSignals(true);

            if (d_file_version > 71 && !t.atEnd()) {
                QStringList lst = t.readLine().split("\t");
                const QString unescLbl = QString(lst.value(1))
                                                 .replace(QLatin1String("&#9;"), QLatin1String("\t"))
                                                 .replace(QLatin1String("<br>"), QLatin1String("\n"));
                plot->setWindowLabel(unescLbl);
                setListViewLabel(plot->name(), unescLbl);
                if (lst.length() > 2) {
                    plot->setCaptionPolicy((MyWidget::CaptionPolicy)lst[2].toInt());
                } else {
                    if (!nonInteractive) {
                        QMessageBox::warning(this, tr("File opening error"),
                                             tr("Invalid WindowLabel line:\n'%1'\nin file %2.")
                                                     .arg(lst.join(" "))
                                                     .arg(fn));
                    }
                    plot->setCaptionPolicy(MyWidget::CaptionPolicy::Name);
                    // Partial fix for sf #403
                    if (!t.atEnd())
                        t.readLine();
                }
            }
            if (d_file_version > 83 && !t.atEnd()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                const auto skipEmpty = Qt::SkipEmptyParts;
#else
                const auto skipEmpty = QString::SkipEmptyParts;
#endif
                QStringList lst = t.readLine().split("\t", skipEmpty);
                if (lst.size() >= 5)
                    plot->setMargins(lst[1].toInt(), lst[2].toInt(), lst[3].toInt(), lst[4].toInt());
                if (!t.atEnd()) {
                    lst = t.readLine().split("\t", skipEmpty);
                    if (lst.size() >= 3)
                        plot->setSpacing(lst[1].toInt(), lst[2].toInt());
                }
                if (!t.atEnd()) {
                    lst = t.readLine().split("\t", skipEmpty);
                    if (lst.size() >= 3)
                        plot->setLayerCanvasSize(lst[1].toInt(), lst[2].toInt());
                }
                if (!t.atEnd()) {
                    lst = t.readLine().split("\t", skipEmpty);
                    if (lst.size() >= 3)
                        plot->setAlignement(lst[1].toInt(), lst[2].toInt());
                }
            }

            while (s != "</multiLayer>" && !t.atEnd()) { // open layers
                s = t.readLine();
                if (s.isNull())
                    break;
                if (s.left(7) == "<graph>") {
                    list.clear();
                    while (s != "</graph>" && !t.atEnd()) {
                        s = t.readLine();
                        if (s.isNull())
                            break;
                        list << s;
                    }
                    if (s != "</graph>") {
                        truncatedProject = true;
                        break;
                    }
                    openGraph(this, plot, list);
                } else if (s.trimmed().startsWith("<Tori")) {
                    plot->restorePhase5Tag(s.trimmed());
                }
            }
            if (s != "</multiLayer>")
                truncatedProject = true;
            plot->finalizePhase5Restore();
            plot->blockSignals(false);
            if (!hidden(plot)) {
                bool otherSavedActive = false;
                if (Folder *pf = plot->folder()) {
                    MyWidget *sa = qobject_cast<MyWidget *>(
                            pf->property("torigraph_saved_active_obj").value<QObject *>());
                    if (sa && sa != plot && !hidden(sa)
                        && sa->property("torigraph_saved_active").toBool()) {
                        otherSavedActive = true;
                    }
                }
                if (!otherSavedActive)
                    activateSubWindow(plot);
            }
            updateProgressStep(aux);
        } else if (s == "<SurfacePlot>") { // process 3D plots information
            list.clear();
            ++aux;
            while (s != "</SurfacePlot>" && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
                list << s;
            }
            if (s != "</SurfacePlot>") {
                truncatedProject = true;
                break;
            }
            openSurfacePlot(this, list);
            updateProgressStep(aux);
        } else if (s.left(5) == "<log>") { // process analysis information
            if (s.contains(QLatin1String("</log>"))) {
                logInfo += s.mid(5).section(QLatin1String("</log>"), 0, 0);
            } else {
                s = t.readLine();
                while (s != "</log>" && !s.endsWith(QLatin1String("</log>")) && !t.atEnd()) {
                    if (s.isNull())
                        break;
                    logInfo += s + "\n";
                    s = t.readLine();
                }
                if (s.endsWith(QLatin1String("</log>")) && s != QLatin1String("</log>"))
                    logInfo += s.left(s.size() - 6);
                else if (s != QLatin1String("</log>"))
                    truncatedProject = true;
            }
            results->setText(logInfo);
        }
    }

    if (wasProgressCanceled()) {
        releaseProjectFileLock();
        saved = true;
        close();
        return false;
    }

    if (truncatedProject || property("_tg_load_truncated").toBool()) {
        progress.reset();
        folders.blockSignals(false);
        blockSignals(false);
        releaseProjectFileLock();
        untrackOpenedProjectFile();
        bool backupAccepted = false;
        if (offerBackupIfPresent(fn, &backupAccepted) && backupAccepted) {
            ApplicationWindow *bak = openProject(fn + "~");
            if (bak != nullptr) {
                if (outReplacement) {
                    *outReplacement = bak;
                    return false;
                }
                if (!d_headless_mode && !d_ole_embedded && !m_batch && !nonInteractive)
                    bak->show();
                else
                    delete bak;
                return false;
            }
        }
        if (!nonInteractive) {
            QMessageBox::critical(
                    this, tr("File opening error"),
                    tr("The project file <b>%1</b> is incomplete or corrupted (unexpected end of "
                       "file) and could not be loaded safely.")
                            .arg(fn));
        }
        return false;
    }

    logInfo = logInfo.remove("</log>\n", Qt::CaseInsensitive);

    folders.setCurrentItem(cf->folderListItem());
    folders.blockSignals(false);
    // change folder to user defined current folder
    changeFolder(cf, true);
    updateExplorerFolderPaneVisibility();

    for (MyWidget *mw : windowsList()) {
        if (mw) {
            mw->setProperty("torigraph_saved_active", QVariant());
            if (Folder *mf = mw->folder())
                mf->setProperty("torigraph_saved_active_obj", QVariant());
        }
    }

    blockSignals(false);
    renamedTables.clear();

    if (!d_headless_mode && !d_ole_embedded && !m_batch && !isNonInteractiveContext(this))
        show();
    executeNotes();
    savedProject();

    if (isBackupTilde) {
        recentProjects.removeAll(fn);
        updateRecentProjectsList();
        modifiedProject();
    } else {
        recentProjects.removeAll(fn);
        recentProjects.push_front(fn);
        updateRecentProjectsList();
        trackOpenedProjectFile(fn);
    }

    return true;
}

ApplicationWindow *ApplicationWindow::openProject(const QString &fn)
{
    unique_ptr<ApplicationWindow> app(new ApplicationWindow);
    app->setHeadlessMode(d_headless_mode);
    app->applyUserSettings();
    ApplicationWindow *replacement = nullptr;
    if (app->loadProjectImpl(fn, &replacement))
        return app.release();
    return replacement;
}

// TODO: rewrite the template system
void ApplicationWindow::openTemplate()
{
    if (isNonInteractiveContext(this))
        return;
    QString filter = tr("ToriGraph 2D Graph Template") + " (*.qpt);;";
    filter += tr("ToriGraph 3D Surface Template") + " (*.qst);;";
    filter += tr("ToriGraph Table Template") + " (*.qtt);;";
    filter += tr("ToriGraph Matrix Template") + " (*.qmt)";

    QString fn = QFileDialog::getOpenFileName(this, tr("Open Template File"), templatesDir, filter);
    if (!fn.isEmpty()) {
        QFileInfo fi(fn);
        templatesDir = fi.absolutePath();
        if (fn.contains(".qmt", Qt::CaseSensitive) || fn.contains(".qpt", Qt::CaseSensitive)
            || fn.contains(".qtt", Qt::CaseSensitive) || fn.contains(".qst", Qt::CaseSensitive)) {
            if (!fi.exists()) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("The file: <b>%1</b> doesn't exist!").arg(fn));
                return;
            }
            QFile f(fn);
            if (!f.open(QIODevice::ReadOnly)) {
                QMessageBox::critical(
                        this, tr("File opening error"),
                        tr("Could not open template file: <b>%1</b> (%2)").arg(fn, f.errorString()));
                return;
            }
            QTextStream t(&f);
            t.setCodec(QTextCodec::codecForName("UTF-8"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList l = t.readLine().split(QRegExp("\\s"), Qt::SkipEmptyParts);
#else
            QStringList l = t.readLine().split(QRegExp("\\s"), QString::SkipEmptyParts);
#endif
            if (l.size() < 2) {
                f.close();
                QMessageBox::critical(
                        this, tr("File opening error"),
                        tr("The file: <b> %1 </b> was not created using ToriGraph!").arg(fn));
                return;
            }
            QString fileType = l[0];
            if ((fileType != "SciDAVis") && (fileType != "ToriGraph") && (fileType != "QtiPlot")) {
                f.close();
                QMessageBox::critical(
                        this, tr("File opening error"),
                        tr("The file: <b> %1 </b> was not created using ToriGraph!").arg(fn));
                return;
            }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList vl = l[1].split(".", Qt::SkipEmptyParts);
#else
            QStringList vl = l[1].split(".", QString::SkipEmptyParts);
#endif
            while (vl.size() < 3)
                vl << QStringLiteral("0");
            if (fileType == "QtiPlot") {
                d_file_version = 100 * (vl[0]).toInt() + 10 * (vl[1]).toInt() + (vl[2]).toInt();
                if (d_file_version > 90) {
                    f.close();
                    QMessageBox::critical(this, tr("File opening error"),
                                          tr("ToriGraph does not support QtiPlot template files "
                                             "from versions later than 0.9.0."));
                    return;
                }
            } else
                d_file_version = ((vl[0]).toInt() << 16) + ((vl[1]).toInt() << 8) + (vl[2]).toInt();

            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

            MyWidget *w = 0;
            QString templateType;
            t >> templateType;

            if (templateType == "<SurfacePlot>") {
                t.skipWhiteSpace();
                QStringList lst;
                while (!t.atEnd())
                    lst << t.readLine();
                w = openSurfacePlot(this, lst);
                if (Graph3D *g3 = qobject_cast<Graph3D *>(w))
                    g3->clearData();
            } else {
                int rows = 0, cols = 0;
                t >> rows;
                t >> cols;
                t.skipWhiteSpace();
                QString geometry = t.readLine();

                if (templateType == "<multiLayer>") { // FIXME: workarounds for template
                    MultiLayer *ml = new MultiLayer("", &d_workspace, 0);
                    w = ml;
                    ml->setAttribute(Qt::WA_DeleteOnClose);
                    QString label = generateUniqueName(tr("Graph"));
                    initBareMultilayerPlot(ml, label.replace(QRegExp("_"), "-"));
                    if (ml) {
                        ml->setCols(cols);
                        ml->setRows(rows);
                        restoreWindowGeometry(this, ml, geometry);
                        if (d_file_version > 83 && !t.atEnd()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                            const auto skipEmpty = Qt::SkipEmptyParts;
#else
                            const auto skipEmpty = QString::SkipEmptyParts;
#endif
                            QStringList lst = t.readLine().split("\t", skipEmpty);
                            if (lst.size() >= 5)
                                ml->setMargins(lst[1].toInt(), lst[2].toInt(), lst[3].toInt(),
                                               lst[4].toInt());
                            if (!t.atEnd()) {
                                lst = t.readLine().split("\t", skipEmpty);
                                if (lst.size() >= 3)
                                    ml->setSpacing(lst[1].toInt(), lst[2].toInt());
                            }
                            if (!t.atEnd()) {
                                lst = t.readLine().split("\t", skipEmpty);
                                if (lst.size() >= 3)
                                    ml->setLayerCanvasSize(lst[1].toInt(), lst[2].toInt());
                            }
                            if (!t.atEnd()) {
                                lst = t.readLine().split("\t", skipEmpty);
                                if (lst.size() >= 3)
                                    ml->setAlignement(lst[1].toInt(), lst[2].toInt());
                            }
                        }
                        while (!t.atEnd()) { // open layers
                            QString s = t.readLine();
                            if (s.isNull())
                                break;
                            if (s.left(7) == "<graph>") {
                                QStringList lst;
                                while (s != "</graph>" && !t.atEnd()) {
                                    s = t.readLine();
                                    if (s.isNull())
                                        break;
                                    lst << s;
                                }
                                openGraph(this, ml, lst);
                            } else if (s.trimmed().startsWith("<Tori")) {
                                ml->restorePhase5Tag(s.trimmed());
                            }
                        }
                        ml->finalizePhase5Restore();
                    }
                } else if (templateType == "<table>" || templateType == "<matrix>") {
                    if (templateType == "<table>")
                        w = newTable(tr("Table1"), qMax(1, rows), qMax(1, cols));
                    else
                        w = newMatrix(qMax(1, rows), qMax(1, cols));
                    if (w) {
                        QStringList lst;
                        while (!t.atEnd())
                            lst << t.readLine();
                        w->restore(lst);
                        restoreWindowGeometry(this, w, geometry);
                    }
                }
            }

            f.close();
            if (w) {
                switch (w->status()) {
                case MyWidget::Maximized:
                    w->setMaximized();
                    break;
                case MyWidget::Minimized:
                    w->setMinimized();
                    break;
                case MyWidget::Hidden:
                    w->setHidden();
                    break;
                case MyWidget::Normal:
                    w->setNormal();
                    break;
                }
                customMenu((MyWidget *)w);
                customToolBars((MyWidget *)w);
            }
            QApplication::restoreOverrideCursor();
        } else {
            QMessageBox::critical(
                    this, tr("File opening error"),
                    tr("The file: <b>%1</b> is not a ToriGraph template file!").arg(fn));
            return;
        }
    }
}

QByteArray ApplicationWindow::saveProjectToByteArray()
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    if (!buf.open(QIODevice::WriteOnly))
        return {};
    QTextStream t(&buf);
    t.setCodec(QTextCodec::codecForName("UTF-8"));
    t << SciDAVis::schemaVersion() + " project file\n";
    t << "<scripting-lang>\t" + QString(scriptEnv->objectName()) + "\n";
    t << "<windows>\t" + QString::number(projectFolder()->windowCount(true)) + "\n";
    t.flush();
    rawSaveFolder(projectFolder(), &buf);
    t << "<log>\n" + logInfo + "</log>";
    t.flush();
    buf.close();
    return bytes;
}

QByteArray ApplicationWindow::saveWindowsToByteArray(const QList<MyWidget *> &windows)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    if (!buf.open(QIODevice::WriteOnly))
        return {};
    QTextStream t(&buf);
    t.setCodec(QTextCodec::codecForName("UTF-8"));
    t << SciDAVis::schemaVersion() + " project file\n";
    t << "<scripting-lang>\t" + QString(scriptEnv->objectName()) + "\n";
    t << "<windows>\t" + QString::number(windows.count()) + "\n";
    t.flush();
    WorkbookManager *wm = WorkbookManager::existing(this);
    for (MyWidget *w : windows) {
        if (!w)
            continue;
        if (Table *tb = qobject_cast<Table *>(w)) {
            QString geom = windowGeometryInfo(tb);
            if (hidden(tb) || tb->status() == MyWidget::Hidden) {
                bool promoteBgSheet = false;
                QRect refRect = tb->frameGeometry();
                if (wm) {
                    const QString bk = wm->bookOf(tb);
                    if (!bk.isEmpty()) {
                        if (Table *cur = wm->currentSheet(bk)) {
                            if (cur != tb && !hidden(cur) && cur->status() != MyWidget::Hidden) {
                                const bool curIncluded = windows.contains(cur);
                                if (!curIncluded) {
                                    bool earlierSheetIncluded = false;
                                    for (Table *s : wm->sheets(bk)) {
                                        if (!s || !windows.contains(s))
                                            continue;
                                        if (s != tb)
                                            earlierSheetIncluded = true;
                                        break;
                                    }
                                    promoteBgSheet = !earlierSheetIncluded;
                                }
                                if (cur->frameGeometry().width() >= 260
                                    && cur->frameGeometry().height() >= 200) {
                                    refRect = cur->frameGeometry();
                                }
                            }
                        }
                    }
                }
                const int gx = qMax(0, refRect.x());
                const int gy = qMax(0, refRect.y());
                const int gw = (refRect.width() >= 260) ? refRect.width() : 640;
                const int gh = (refRect.height() >= 200) ? refRect.height() : 400;
                geom = promoteBgSheet
                        ? QStringLiteral("geometry\t%1\t%2\t%3\t%4\t\n").arg(gx).arg(gy).arg(gw).arg(gh)
                        : QStringLiteral("geometry\t%1\t%2\t%3\t%4\thidden\n").arg(gx).arg(gy).arg(gw).arg(gh);
            }
            tb->saveToDevice(&buf, geom);
        } else {
            t << w->saveToString(windowGeometryInfo(w));
            t.flush();
        }
    }
    if (wm) {
        const QString wbLines = wm->saveBooksForWindows(windows);
        if (!wbLines.isEmpty())
            t << wbLines;
    }
    t << "<log>\n</log>";
    t.flush();
    buf.close();
    return bytes;
}

bool ApplicationWindow::saveProject()
{
    if (d_ole_embedded) {
        QPointer<ApplicationWindow> self(this);
        setProperty("_toriLastOleSaveOk", QVariant(true));
        setProperty("_toriLastOleSaveError", QVariant());
        if (auto onUpdate = d_ole_on_update)
            onUpdate();
        if (!self)
            return false;
        if (!d_ole_embedded)
            return false;
        const QVariant okProp = property("_toriLastOleSaveOk");
        const bool oleOk = okProp.isValid() ? okProp.toBool() : true;
        if (!oleOk) {
            const QString err = property("_toriLastOleSaveError").toString();
            modifiedProject();
            if (!isNonInteractiveContext(this)) {
                QMessageBox::warning(
                        this, tr("Could not update embedded PowerPoint object"),
                        tr("Saving back into PowerPoint failed%1.\n\n"
                           "Use File -> Save Project As (.sciprj) to keep a standalone backup copy.")
                                .arg(err.isEmpty() ? QString() : QStringLiteral(": ") + err));
                if (!self)
                    return false;
            }
            if (statusBar())
                statusBar()->showMessage(tr("Embedded OLE save failed — project not saved."), 10000);
            return false;
        }
        markProjectSaved();
        return true;
    }

    if (projectname == "untitled" || projectname.endsWith(".opj", Qt::CaseInsensitive)
        || projectname.endsWith(".opju", Qt::CaseInsensitive)
        || projectname.endsWith(".ogm", Qt::CaseInsensitive)
        || projectname.endsWith(".ogw", Qt::CaseInsensitive)
        || projectname.endsWith(".ogg", Qt::CaseInsensitive)
        || projectname.endsWith(".org", Qt::CaseInsensitive)
        || projectname.endsWith("~")) {
        // An untitled (or foreign/backup-suffixed) project is only saved when the user picks a
        // valid target path; report that result so closing can proceed after a successful save.
        return saveProjectAs();
    }

    if (d_project_read_only_lock) {
        if (!isNonInteractiveContext(this)) {
            const int choice = QMessageBox::warning(
                    this, tr("Read-Only Project Lock"),
                    tr("This file was opened read-only because another ToriGraph process held a "
                       "lock on <b>%1</b>.\n\nSave under a different name?")
                            .arg(QDir::toNativeSeparators(projectname).toHtmlEscaped()),
                    tr("Save Project As..."), tr("Cancel"), QString(), 0, 1);
            if (choice == 0)
                return saveProjectAs();
        }
        return false;
    }

    bool savedAsNew = false;
    if (!checkExternalModificationBeforeSave(&savedAsNew))
        return savedAsNew;

    d_ignore_watcher_changes = true;
    const bool wroteOk = writeProjectFile(projectFolder(), projectname);
    d_ignore_watcher_changes = false;

    if (!wroteOk) {
        // Nothing was saved: keep the project marked as modified, so that closing still asks
        // and the save action stays available.
        modifiedProject();
        statusBar()->showMessage(tr("The project was NOT saved."), 10000);
        return false;
    }

    acquireProjectFileLock(projectname, !isNonInteractiveContext(this));
    trackOpenedProjectFile(projectname);

    setWindowTitle("ToriGraph - " + projectname);
    markProjectSaved();
    updateUndoRedoActionsForActiveWindow();

    if (autoSave) {
        if (savingTimerId)
            killTimer(savingTimerId);
        savingTimerId = startTimer(autoSaveTime * 60000);
    } else
        savingTimerId = 0;

    QApplication::restoreOverrideCursor();
    return true;
}

bool ApplicationWindow::saveProjectAs()
{
    if (isNonInteractiveContext(this))
        return false;
    QString filter = tr("ToriGraph project") + " (*.sciprj);;";
    filter += tr("Compressed ToriGraph project") + " (*.sciprj.gz)";

    QString selectedFilter;
    QString fn = QFileDialog::getSaveFileName(this, tr("Save Project As"), workingDir, filter,
                                              &selectedFilter);
    if (!fn.isEmpty()) {
        QFileInfo fi(fn);
        workingDir = fi.absolutePath();
        QString baseName = fi.fileName();
        if (!baseName.endsWith(".sciprj", Qt::CaseInsensitive)
            && !baseName.endsWith(".sciprj.gz", Qt::CaseInsensitive)) {
            fn.append(".sciprj");
            if (selectedFilter.contains(".gz"))
                fn.append(".gz");
        }

        if (d_ole_embedded) {
            if (!writeProjectFile(projectFolder(), fn))
                return false;
            recentProjects.removeAll(fn);
            recentProjects.push_front(fn);
            updateRecentProjectsList();
            if (statusBar())
                statusBar()->showMessage(tr("Saved standalone copy to %1").arg(fn), 8000);
            return true;
        }

        const QString normTarget = normalizedProjectFilePath(fn);
        const QString normCurrent = normalizedProjectFilePath(projectname);
        if (d_project_read_only_lock && !normTarget.isEmpty() && normTarget == normCurrent) {
            QMessageBox::warning(
                    this, tr("Read-Only Project Lock"),
                    tr("This file was opened read-only because another ToriGraph process held a "
                       "lock on <b>%1</b>.\n\nChoose a different file name to save a copy.")
                            .arg(QDir::toNativeSeparators(fn).toHtmlEscaped()));
            return false;
        }

        qint64 lockHolderPid = 0;
        if (isProjectFileLockedByOther(fn, &lockHolderPid)) {
            QMessageBox::warning(
                    this, tr("Read-Only Project Lock"),
                    tr("The project <b>%1</b> appears to be open in another ToriGraph process%2 "
                       "and cannot be overwritten.\n\nChoose a different file name.")
                            .arg(QDir::toNativeSeparators(fn).toHtmlEscaped(),
                                 lockHolderPid > 0 ? tr(" (PID %1)").arg(lockHolderPid) : QString()));
            return false;
        }

        const QString previousProjectName = projectname;
        const bool prevReadOnlyLock = d_project_read_only_lock;
        projectname = fn;
        d_project_read_only_lock = false;

        if (!saveProject()) {
            // keep pointing at the file that really holds the last saved state
            projectname = previousProjectName;
            d_project_read_only_lock = prevReadOnlyLock;
            return false;
        }
        recentProjects.removeAll(fn);
        recentProjects.push_front(fn);
        updateRecentProjectsList();

        QFileInfo savedInfo(fn);
        QString savedBaseName = savedInfo.baseName();
        FolderListItem *item = (FolderListItem *)folders.topLevelItem(0);
        if (item) {
            item->setText(0, savedBaseName);
            if (item->folder())
                item->folder()->setName(savedBaseName);
        }
        return true;
    }
    return false;
}

void ApplicationWindow::saveAsTemplate()
{
    if (isNonInteractiveContext(this))
        return;
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w)
        return;

    QString filter;
    if (w->inherits("Matrix"))
        filter = tr("ToriGraph Matrix Template") + " (*.qmt)";
    else if (w->inherits("MultiLayer"))
        filter = tr("ToriGraph 2D Graph Template") + " (*.qpt)";
    else if (w->inherits("Table"))
        filter = tr("ToriGraph Table Template") + " (*.qtt)";
    else if (w->inherits("Graph3D"))
        filter = tr("ToriGraph 3D Surface Template") + " (*.qst)";

    QString selectedFilter;
    QDir().mkpath(templatesDir);
    QString fn =
            QFileDialog::getSaveFileName(this, tr("Save Window As Template"),
                                         templatesDir + "/" + w->name(), filter, &selectedFilter);
    if (!fn.isEmpty()) {
        QFileInfo fi(fn);
        templatesDir = fi.absolutePath();
        QString baseName = fi.fileName();
        if (!baseName.contains(".")) {
            selectedFilter = selectedFilter.right(5).left(4);
            fn.append(selectedFilter);
        }

        // QSaveFile writes to a temporary file and only replaces an existing template on a
        // successful commit(), so a failed write no longer truncates the user's old template.
        QSaveFile f(fn);
        if (!f.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Export Error"),
                                  tr("Could not write to file: <br><h4> %1 </h4><p>Please verify "
                                     "that you have the right to write to this location!")
                                          .arg(fn));
            return;
        }
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        QString text = SciDAVis::schemaVersion() + " template file\n";
        text += w->saveAsTemplate(windowGeometryInfo(w));
        QTextStream t(&f);
        t.setCodec(QTextCodec::codecForName("UTF-8"));
        t << text;
        t.flush();
        bool writeOk = (t.status() == QTextStream::Ok) && (f.error() == QFile::NoError);
        if (writeOk)
            writeOk = f.commit();
        else
            f.cancelWriting();
        QApplication::restoreOverrideCursor();
        if (!writeOk) {
            QMessageBox::critical(this, tr("Export Error"),
                                  tr("Could not write to file: <br><h4> %1 </h4><p>%2")
                                          .arg(fn, f.errorString()));
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Auto-save / crash recovery
//
// Auto-saving never touches the project file the user is working on (and therefore neither its
// "~" backup, the modified flag nor the undo history). Instead, a recovery copy is written to
// the per-user application data folder. It is removed whenever the project gets saved, loaded
// or closed regularly; if it is still there at the next start, the previous session ended
// unexpectedly and the user is asked whether the copy should be restored.
// ---------------------------------------------------------------------------------------------
static bool s_crash_recovery_enabled = false;

static QString s_recovery_dir_override;

static std::function<ApplicationWindow::RecoveryAnswer(const QString &)> s_recovery_prompt_override;

void ApplicationWindow::setCrashRecoveryEnabled(bool on)
{
    s_crash_recovery_enabled = on;
}

void ApplicationWindow::setRecoveryDirPathOverride(const QString &path)
{
    s_recovery_dir_override = path;
}

void ApplicationWindow::setCrashRecoveryPromptOverride(
        std::function<RecoveryAnswer(const QString &)> answer)
{
    s_recovery_prompt_override = std::move(answer);
}

QString ApplicationWindow::recoveryDirPath()
{
    if (!s_recovery_dir_override.isEmpty())
        return s_recovery_dir_override;
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty())
        base = QDir::tempPath() + QStringLiteral("/ToriGraph");
    return base + QStringLiteral("/recovery");
}

void ApplicationWindow::writeRecoveryFile()
{
    if (!s_crash_recovery_enabled || saved || d_ole_embedded || d_headless_mode || m_batch
        || isNonInteractiveContext(this))
        return;
    // A modal dialog may be in the middle of changing the project; try again at the next tick.
    if (QApplication::activeModalWidget())
        return;

    const QString dirPath = recoveryDirPath();
    if (!QDir().mkpath(dirPath))
        return;
    if (d_recovery_id.isEmpty())
        d_recovery_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QByteArray bytes = saveProjectToByteArray();
    if (bytes.isEmpty())
        return;

    const QString base = dirPath + QLatin1Char('/') + d_recovery_id;
    QSaveFile data(base + QStringLiteral(".sciprj"));
    if (!data.open(QIODevice::WriteOnly) || data.write(bytes) != bytes.size() || !data.commit())
        return;

    const QDateTime now = QDateTime::currentDateTime();
    QJsonObject meta;
    meta[QStringLiteral("project")] = projectname;
    meta[QStringLiteral("pid")] = static_cast<double>(QCoreApplication::applicationPid());
    meta[QStringLiteral("saved_at")] = now.toString(Qt::ISODate);
    QSaveFile metaFile(base + QStringLiteral(".json"));
    if (metaFile.open(QIODevice::WriteOnly)) {
        metaFile.write(QJsonDocument(meta).toJson(QJsonDocument::Compact));
        metaFile.commit();
    }

    statusBar()->showMessage(
            tr("Recovery copy saved at %1 (the project file itself was not changed).")
                    .arg(QLocale().toString(now.time(), QLocale::ShortFormat)),
            6000);
}

void ApplicationWindow::removeRecoveryFile()
{
    if (d_recovery_id.isEmpty())
        return;
    const QString base = recoveryDirPath() + QLatin1Char('/') + d_recovery_id;
    QFile::remove(base + QStringLiteral(".json"));
    QFile::remove(base + QStringLiteral(".sciprj"));
}

//! Is the process that wrote a recovery copy at \a savedAt still running?
static bool recoveryOwnerIsRunning(qint64 pid, const QDateTime &savedAt)
{
    if (pid <= 0)
        return false;
    if (pid == QCoreApplication::applicationPid())
        return true;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return false;
    bool running = false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(h, &exitCode) && exitCode == STILL_ACTIVE) {
        running = true;
        // Process ids get reused: a process started after the copy was written is not its owner.
        FILETIME ftCreate, ftExit, ftKernel, ftUser;
        if (savedAt.isValid() && GetProcessTimes(h, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
            ULARGE_INTEGER u;
            u.LowPart = ftCreate.dwLowDateTime;
            u.HighPart = ftCreate.dwHighDateTime;
            // FILETIME: 100 ns ticks since 1601-01-01 UTC
            const qint64 createdMs = static_cast<qint64>(u.QuadPart / 10000ULL) - 11644473600000LL;
            if (createdMs > savedAt.toMSecsSinceEpoch() + 2000)
                running = false;
        }
    }
    CloseHandle(h);
    return running;
#else
    Q_UNUSED(savedAt)
    return ::kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

void ApplicationWindow::checkCrashRecovery()
{
    if (!s_crash_recovery_enabled || d_ole_embedded || d_headless_mode || m_batch
        || isNonInteractiveContext(this))
        return;

    QDir dir(recoveryDirPath());
    if (!dir.exists())
        return;

    // A copy whose ".json" is missing (the metadata write failed, or the process died between
    // the two writes) is never offered, so it would stay forever. Remove such leftovers once
    // they are clearly stale; a running instance rewrites its copy at every auto-save tick.
    {
        const QDateTime staleBefore = QDateTime::currentDateTime().addSecs(-24 * 3600);
        const QFileInfoList dataFiles = dir.entryInfoList(
                QStringList() << QStringLiteral("*.sciprj"), QDir::Files);
        for (const QFileInfo &dfi : dataFiles) {
            if (QFileInfo::exists(dir.filePath(dfi.completeBaseName() + QStringLiteral(".json"))))
                continue;
            if (dfi.lastModified() < staleBefore)
                QFile::remove(dfi.absoluteFilePath());
        }
    }

    const QStringList metaFiles = dir.entryList(QStringList() << QStringLiteral("*.json"),
                                                QDir::Files, QDir::Time);
    for (const QString &metaName : metaFiles) {
        const QString metaPath = dir.filePath(metaName);
        const QString dataPath =
                dir.filePath(QFileInfo(metaName).completeBaseName() + QStringLiteral(".sciprj"));

        QFile mf(metaPath);
        if (!mf.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject meta = QJsonDocument::fromJson(mf.readAll()).object();
        mf.close();

        const QDateTime savedAt =
                QDateTime::fromString(meta.value(QStringLiteral("saved_at")).toString(), Qt::ISODate);
        if (recoveryOwnerIsRunning(static_cast<qint64>(meta.value(QStringLiteral("pid")).toDouble()),
                                   savedAt))
            continue; // belongs to another ToriGraph window that is still open

        if (!QFile::exists(dataPath) || QFileInfo(dataPath).size() <= 0) {
            QFile::remove(metaPath);
            QFile::remove(dataPath);
            continue;
        }

        const QString origProject = meta.value(QStringLiteral("project")).toString();
        const bool hadFile = !origProject.isEmpty() && origProject != QLatin1String("untitled");
        const QString what = hadFile ? QDir::toNativeSeparators(origProject)
                                     : tr("a project that had not been saved yet");

        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Restore unsaved work?"));
        box.setText(tr("ToriGraph was not closed properly last time. An automatically saved "
                       "copy of your work was found:<br><br><b>%1</b><br>Saved: %2")
                            .arg(what.toHtmlEscaped(),
                                 QLocale().toString(savedAt, QLocale::ShortFormat)));
        box.setInformativeText(
                hadFile ? tr("Restoring opens the copy as a modified project. The project file "
                             "on disk is only changed when you save.")
                        : tr("Restoring opens the copy as a new, unsaved project."));
        QPushButton *restoreBtn = box.addButton(tr("Restore"), QMessageBox::AcceptRole);
        QPushButton *discardBtn = box.addButton(tr("Discard Copy"), QMessageBox::DestructiveRole);
        QPushButton *laterBtn = box.addButton(tr("Decide Later"), QMessageBox::RejectRole);
        box.setDefaultButton(restoreBtn);
        QAbstractButton *answer = nullptr;
        if (s_recovery_prompt_override) {
            switch (s_recovery_prompt_override(metaPath)) {
            case RecoveryRestore:
                answer = restoreBtn;
                break;
            case RecoveryDiscard:
                answer = discardBtn;
                break;
            case RecoveryDecideLater:
                answer = laterBtn;
                break;
            }
        } else {
            box.exec();
            answer = box.clickedButton();
        }

        if (answer == discardBtn) {
            QFile::remove(metaPath);
            QFile::remove(dataPath);
            continue;
        }
        if (answer != restoreBtn)
            continue; // keep the copy and ask again at the next start

        ApplicationWindow *restored = openProject(dataPath);
        if (!restored) {
            QMessageBox::warning(this, tr("Restore unsaved work?"),
                                 tr("The recovery copy could not be opened. It is kept in:<br>%1")
                                         .arg(QDir::toNativeSeparators(dataPath).toHtmlEscaped()));
            continue;
        }

        // The window shows the restored state, but nothing of it is on disk under the
        // project's own name yet.
        restored->recentProjects.removeAll(dataPath);
        restored->updateRecentProjectsList();
        restored->projectname = hadFile ? origProject : QStringLiteral("untitled");
        restored->setWindowTitle(QStringLiteral("ToriGraph - ") + restored->projectname
                                 + QStringLiteral(" ") + tr("[restored]"));
        restored->modifiedProject();
        // Take over the copy: it is removed as soon as the restored project is saved or closed.
        restored->d_recovery_id = QFileInfo(metaName).completeBaseName();
        // The metadata still names the crashed process. Claim it for this process right away
        // (writeRecoveryFile() would only do so at the next auto-save tick, and not at all for a
        // project that is not modified), so that another ToriGraph started meanwhile does not
        // offer the same copy again while this window is open.
        {
            QJsonObject newMeta = meta;
            newMeta[QStringLiteral("project")] = restored->projectname;
            newMeta[QStringLiteral("pid")] =
                    static_cast<double>(QCoreApplication::applicationPid());
            newMeta[QStringLiteral("saved_at")] =
                    QDateTime::currentDateTime().toString(Qt::ISODate);
            QSaveFile metaOut(metaPath);
            if (metaOut.open(QIODevice::WriteOnly)) {
                metaOut.write(QJsonDocument(newMeta).toJson(QJsonDocument::Compact));
                metaOut.commit();
            }
        }

        // Replace the empty start-up window; a project opened from the command line stays open.
        if (restored != this) {
            recentProjects.removeAll(dataPath);
            if (saved && projectname == QLatin1String("untitled"))
                close();
        }
        return; // one restored project per start; remaining copies are offered next time
    }
}

Note *ApplicationWindow::openNote(ApplicationWindow *app, const QStringList &flist)
{
    if (!app || flist.size() < 3)
        return nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList lst = flist[0].split("\t", Qt::SkipEmptyParts);
#else
    QStringList lst = flist[0].split("\t", QString::SkipEmptyParts);
#endif
    if (lst.isEmpty())
        return nullptr;
    QString caption = lst[0];
    Note *w = app->newNote(caption);
    if (!w)
        return nullptr;
    if (lst.count() >= 2) {
        app->setListViewDate(caption, lst[1]);
        w->setBirthDate(lst[1]);
    }
    restoreWindowGeometry(app, w, flist[1]);

    lst = flist[2].split("\t");
    const QString unescNoteLbl = QString(lst.value(1)).replace(QLatin1String("&#9;"), QLatin1String("\t"));
    w->setWindowLabel(unescNoteLbl);
    w->setCaptionPolicy((MyWidget::CaptionPolicy)lst.value(2).toInt());
    app->setListViewLabel(w->name(), unescNoteLbl);
    return w;
}

namespace {
static bool stringContainsWhitespace(const QString &s)
{
    for (QChar ch : s) {
        if (ch.isSpace())
            return true;
    }
    return false;
}

static void sanitizeLoadedWindowName(ApplicationWindow *app, MyWidget *w, const QString &xmlName,
                                     const QString &fallbackPrefix)
{
    if (!app || !w)
        return;
    const QString curName = w->name();
    auto isUsed = [&](const QString &candName) -> bool {
        if (app->isBatchWindowUpdatesActive())
            return app->isBatchNameUsedByOther(candName, w);
        for (MyWidget *cand : app->windowsList()) {
            if (cand && cand != w && cand->name() == candName)
                return true;
        }
        for (MyWidget *hw : app->hiddenWindows) {
            if (hw && hw != w && hw->name() == candName)
                return true;
        }
        return false;
    };

    const bool crossWindowCollision = isUsed(curName);
    if (!stringContainsWhitespace(curName) && !crossWindowCollision) {
        app->syncBatchWindowName(w);
        return;
    }

    QString base = !xmlName.isEmpty() ? xmlName.trimmed() : curName.trimmed();
    base.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    if (base.isEmpty())
        base = fallbackPrefix;

    QString target = base;
    if (isUsed(base)) {
        target = app->isBatchWindowUpdatesActive() ? app->nextBatchUniqueName(base, w)
                                                   : app->generateUniqueName(base);
    }
    target.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    w->setName(target);
    int sfx = 2;
    while (stringContainsWhitespace(w->name()) || isUsed(w->name())) {
        w->setName(base + QStringLiteral("_") + QString::number(sfx++));
    }
    app->syncBatchWindowName(w);
}

static QString remapRenamedTableRef(const QStringList &renamedTables, const QString &ref)
{
    if (renamedTables.isEmpty() || ref.isEmpty())
        return ref;
    int bestIdx = -1;
    int bestLen = -1;
    for (int k = 0; k + 1 < renamedTables.size(); k += 2) {
        const QString &oldCap = renamedTables.at(k);
        if (oldCap.isEmpty())
            continue;
        if ((ref == oldCap || ref.startsWith(oldCap + QLatin1Char('_')))
            && oldCap.length() >= bestLen) {
            bestIdx = k;
            bestLen = oldCap.length();
        }
    }
    if (bestIdx >= 0) {
        const QString &oldCap = renamedTables.at(bestIdx);
        const QString &newCap = renamedTables.at(bestIdx + 1);
        return newCap + ref.mid(oldCap.length());
    }
    return ref;
}

// Curve tags written after the curves (custom label, colour map, error band) name their columns
// as "<table>_<column>"; a table renamed while loading (appendProject name clash,
// sanitizeLoadedWindowName) must be renamed there too, as the curve lines are, or the tag no
// longer finds its curve.
static QString remapToriTagColumnRefs(const QStringList &renamedTables, const QString &line)
{
    if (renamedTables.isEmpty()
        || !(line.startsWith(QLatin1String("<ToriCurveLabel"))
             || line.startsWith(QLatin1String("<ToriPointColorMap"))
             || line.startsWith(QLatin1String("<ToriErrorBand"))))
        return line;
    static const QRegularExpression re(QStringLiteral("\\s(x|y|z|err|size)=\"([^\"]*)\""));
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += line.mid(last, m.capturedStart(2) - last);
        out += remapRenamedTableRef(renamedTables, m.captured(2));
        last = m.capturedEnd(2);
    }
    out += line.mid(last);
    return out;
}

static QString remapSurfacePlotSource(const QStringList &renamedTables, const QString &src)
{
    if (renamedTables.isEmpty() || src.isEmpty())
        return src;
    if (src.startsWith(QStringLiteral("matrix<"), Qt::CaseSensitive)
        && src.endsWith(QLatin1Char('>'), Qt::CaseInsensitive)) {
        const QString inner = src.mid(7, src.length() - 8);
        return QStringLiteral("matrix<") + remapRenamedTableRef(renamedTables, inner)
                + QLatin1Char('>');
    }
    const int xDelim = src.indexOf(QStringLiteral("(X),"), 0, Qt::CaseSensitive);
    if (xDelim > 0) {
        const int yDelim = src.indexOf(QStringLiteral("(Y),"), xDelim + 4, Qt::CaseSensitive);
        if (yDelim > xDelim + 4 && src.endsWith(QStringLiteral("(Z)"), Qt::CaseSensitive)) {
            const QString xRef = src.left(xDelim);
            const QString yRef = src.mid(xDelim + 4, yDelim - (xDelim + 4));
            const QString zRef = src.mid(yDelim + 4, src.length() - (yDelim + 4) - 3);
            return remapRenamedTableRef(renamedTables, xRef) + QStringLiteral("(X),")
                    + remapRenamedTableRef(renamedTables, yRef) + QStringLiteral("(Y),")
                    + remapRenamedTableRef(renamedTables, zRef) + QStringLiteral("(Z)");
        }
        if (yDelim < 0 && src.endsWith(QStringLiteral("(Y)"), Qt::CaseSensitive)) {
            const QString xRef = src.left(xDelim);
            const QString yRef = src.mid(xDelim + 4, src.length() - (xDelim + 4) - 3);
            return remapRenamedTableRef(renamedTables, xRef) + QStringLiteral("(X),")
                    + remapRenamedTableRef(renamedTables, yRef) + QStringLiteral("(Y)");
        }
    }
    if (src.endsWith(QStringLiteral("(Y)"), Qt::CaseSensitive)
        || src.contains(QStringLiteral("(Z)"), Qt::CaseSensitive)) {
        QStringList parts = src.split(QLatin1Char(','));
        for (QString &part : parts) {
            if (part.endsWith(QStringLiteral("(X)"), Qt::CaseSensitive)
                || part.endsWith(QStringLiteral("(Y)"), Qt::CaseSensitive)
                || part.endsWith(QStringLiteral("(Z)"), Qt::CaseSensitive)) {
                const QString suffix = part.right(3);
                part = remapRenamedTableRef(renamedTables, part.left(part.length() - 3)) + suffix;
            }
        }
        return parts.join(QLatin1Char(','));
    }
    return src;
}
} // namespace

// TODO: most of this code belongs into matrix
Matrix *ApplicationWindow::openMatrix(ApplicationWindow *app, const QStringList &flist)
{
    // An empty or cut-short <matrix> block cannot be restored: report it through the same
    // truncation flag openTable uses instead of reading past the end of flist.
    if (flist.size() < (app->d_file_version < 0x000200 ? 1 : 2)) {
        app->setProperty("_tg_load_truncated", true);
        return nullptr;
    }
    if (app->d_file_version < 0x000200) {
        QStringList::const_iterator line = flist.begin();

        QStringList list = (*line).split("\t");
        if (list.size() < 4) {
            app->setProperty("_tg_load_truncated", true);
            return nullptr;
        }
        const QString rawCaption = list[0];
        QString caption = rawCaption;
        caption.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
        if (caption.isEmpty())
            caption = QStringLiteral("Matrix");
        int rows = list[1].toInt();
        int cols = list[2].toInt();

        Matrix *w = app->newMatrix(caption, rows, cols);
        sanitizeLoadedWindowName(app, w, caption, QStringLiteral("Matrix"));
        if (!rawCaption.isEmpty() && w->name() != rawCaption)
            app->renamedTables << rawCaption << w->name();
        app->setListViewDate(w->name(), list[3]);
        w->setBirthDate(list[3]);

        for (line++; line != flist.end(); line++) {
            QStringList fields = (*line).split("\t");
            if (fields[0] == "geometry") {
                restoreWindowGeometry(app, w, *line);
            } else if (fields[0] == "ColWidth") {
                w->setColumnsWidth(fields[1].toInt());
            } else if (fields[0] == "Formula") {
                w->setFormula(fields[1]);
            } else if (fields[0] == "<formula>") {
                QString formula;
                for (line++; line != flist.end() && *line != "</formula>"; line++)
                    formula += *line + "\n";
                formula.truncate(formula.length() - 1);
                w->setFormula(formula);
            } else if (fields[0] == "TextFormat") {
                if (fields[1] == "f")
                    w->setTextFormat('f', fields[2].toInt());
                else
                    w->setTextFormat('e', fields[2].toInt());
            } else if (fields[0] == "WindowLabel") { // d_file_version > 71
                const QString unescLbl = QString(fields.value(1)).replace(QLatin1String("&#9;"), QLatin1String("\t"));
                w->setWindowLabel(unescLbl);
                w->setCaptionPolicy((MyWidget::CaptionPolicy)fields.value(2).toInt());
                app->setListViewLabel(w->name(), unescLbl);
            } else if (fields[0] == "Coordinates") { // d_file_version > 81
                w->setCoordinates(fields[1].toDouble(), fields[2].toDouble(), fields[3].toDouble(),
                                  fields[4].toDouble());
            } else // <data> or values
                break;
        }
        if (*line == "<data>")
            line++;

        QElapsedTimer t;
        t.start();
        // read and set table values
        for (; line != flist.end() && *line != "</data>"; line++) {
            QStringList fields = (*line).split("\t");
            int row = fields[0].toInt();
            for (int col = 0; col < cols; col++) {
                QString cell = fields[col + 1];
                if (cell.isEmpty())
                    continue;

                if (d_file_version < 90)
                    w->setCell(row, col, QLocale::c().toDouble(cell));
                else if (d_file_version >= 0x000100)
                    w->setCell(row, col, QLocale().toDouble(cell));
                else
                    w->setText(row, col, cell);
            }
            if (t.elapsed() > 1000) {
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                t.start();
            }
        }

        return w;
    } else {
        int length = flist.at(0).toInt();
        int index = 1;
        QString xml(flist.at(index++));
        while (xml.length() < length && index < flist.size())
            xml += '\n' + flist.at(index++);
        XmlStreamReader reader(xml);
        while (!reader.atEnd() && !reader.isStartElement())
            reader.readNext();
        QString xmlName = reader.attributes().value(QStringLiteral("name")).toString();
        if (xmlName.isEmpty())
            xmlName = reader.attributes().value(reader.namespaceUri().toString(), QStringLiteral("name")).toString();

        static quint64 s_loading_matrix_seq = 0;
        QString tempCap = QStringLiteral("__loading_matrix_%1__").arg(++s_loading_matrix_seq);
        while (app->alreadyUsedName(tempCap))
            tempCap = QStringLiteral("__loading_matrix_%1__").arg(++s_loading_matrix_seq);
        Matrix *w = app->newMatrix(tempCap, 1, 1);
        if (w->d_future_matrix->load(&reader) == false) {
            QString msg_text = reader.errorString();
            if (!isNonInteractiveContext(this))
                QMessageBox::critical(this, tr("Error reading matrix from project file"), msg_text);
        }
        if (reader.hasWarnings()) {
            QString msg_text =
                    tr("The following problems occured when loading the project file:\n");
            QStringList warnings = reader.warningStrings();
            foreach (QString str, warnings)
                msg_text += str + "\n";
            if (!isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Project loading partly failed"), msg_text);
        }

        sanitizeLoadedWindowName(app, w, xmlName, QStringLiteral("Matrix"));

        w->setObjectName(w->name());
        app->renameListViewItem(tempCap, w->name());
        w->setCaptionPolicy(w->captionPolicy());
        if (!xmlName.isEmpty() && w->name() != xmlName)
            app->renamedTables << xmlName << w->name();

        if (index < flist.size())
            restoreWindowGeometry(app, w, flist.at(index));

        if (!app->hidden(w)) {
            bool otherSavedActive = false;
            if (Folder *wf = w->folder()) {
                MyWidget *sa = qobject_cast<MyWidget *>(
                        wf->property("torigraph_saved_active_obj").value<QObject *>());
                if (sa && sa != w && !app->hidden(sa)
                    && sa->property("torigraph_saved_active").toBool()) {
                    otherSavedActive = true;
                }
            }
            if (!otherSavedActive)
                activateSubWindow(w);
        }
        return w;
    }
}

// TODO: most of this code belongs into Table
Table *ApplicationWindow::openTable(ApplicationWindow *app, QTextStream &stream)
{
    if (app->d_file_version < 0x000200) {
        QStringList flist;
        QString s;
        while (s != "</table>" && !stream.atEnd()) {
            s = stream.readLine();
            if (s.isNull())
                break;
            flist << s;
        }
        if (s != "</table>") {
            if (app)
                app->setProperty("_tg_load_truncated", true);
            return nullptr;
        }
        if (!flist.isEmpty() && flist.last() == QLatin1String("</table>"))
            flist.pop_back();
        if (flist.isEmpty()) {
            if (app)
                app->setProperty("_tg_load_truncated", true);
            return nullptr;
        }
        QStringList::const_iterator line = flist.begin();

        QStringList list = (*line).split("\t");
        if (list.size() < 4) {
            if (app)
                app->setProperty("_tg_load_truncated", true);
            return nullptr;
        }
        const QString rawCaption = list[0];
        QString caption = rawCaption;
        caption.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
        if (caption.isEmpty())
            caption = QStringLiteral("Table");
        int rows = list[1].toInt();
        int cols = list[2].toInt();

        Table *w = app->newTable(caption, rows, cols);
        // Renames while the table is read (unique name, header) are recorded in renamedTables
        // and applied by the loader, not propagated one by one.
        Table::RenamePropagationBlocker renameBlocker(w);
        sanitizeLoadedWindowName(app, w, caption, QStringLiteral("Table"));
        if (!rawCaption.isEmpty() && w->name() != rawCaption)
            app->renamedTables << rawCaption << w->name();
        app->setListViewDate(w->name(), list[3]);
        w->setBirthDate(list[3]);

        for (line++; line != flist.end(); line++) {
            QStringList fields = (*line).split("\t");
            if (fields[0] == "geometry" || fields[0] == "tgeometry") {
                restoreWindowGeometry(app, w, *line);
            } else if (fields[0] == "header") {
                fields.pop_front();
                if (d_file_version >= 78)
                    w->importV0x0001XXHeader(fields);
                else {
                    w->setColPlotDesignation(list[4].toInt(), SciDAVis::X);
                    w->setColPlotDesignation(list[6].toInt(), SciDAVis::Y);
                    w->setHeader(fields);
                }
            } else if (fields[0] == "ColWidth") {
                fields.pop_front();
                w->setColWidths(fields);
            } else if (fields[0] == "com") { // legacy code
                w->setCommands(*line);
            } else if (fields[0] == "<com>") {
                for (line++; line != flist.end() && *line != "</com>"; line++) {
                    int col = (*line).mid(9, (*line).length() - 11).toInt();
                    QString formula;
                    for (line++; line != flist.end() && *line != "</col>"; line++)
                        formula += *line + "\n";
                    formula.truncate(formula.length() - 1);
                    w->setCommand(col, formula);
                }
            } else if (fields[0] == "ColType") { // d_file_version > 65
                fields.pop_front();
                w->setColumnTypes(fields);
            } else if (fields[0] == "Comments") { // d_file_version > 71
                fields.pop_front();
                w->setColComments(fields);
            } else if (fields[0] == "WindowLabel") { // d_file_version > 71
                const QString unescLbl = QString(fields.value(1)).replace(QLatin1String("&#9;"), QLatin1String("\t"));
                w->setWindowLabel(unescLbl);
                w->setCaptionPolicy((MyWidget::CaptionPolicy)fields.value(2).toInt());
                app->setListViewLabel(w->name(), unescLbl);
            } else // <data> or values
                break;
        }

        QElapsedTimer t;
        t.start();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        for (line++; line != flist.end() && *line != "</data>";
             line++) { // read and set table values
            QStringList fields = (*line).split("\t");
            int row = fields[0].toInt();
            for (int col = 0; col < cols; col++) {
                if (fields.count() >= col + 2) {
                    QString cell = fields[col + 1];
                    if (cell.isEmpty())
                        continue;

                    if (d_file_version < 90 && w->columnType(col) == SciDAVis::ColumnMode::Numeric)
                        w->setCell(row, col, QLocale::c().toDouble(cell.replace(",", ".")));
                    else if (d_file_version >= 0x000100 && w->columnType(col) == SciDAVis::ColumnMode::Numeric)
                        w->setCell(row, col, QLocale().toDouble(cell));
                    else
                        w->setText(row, col, cell);
                }
            }
            if (t.elapsed() > 1000) {
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                t.start();
            }
        }
        QApplication::restoreOverrideCursor();

        return w;
    } else {
        QString s = stream.readLine();
        int length = s.toInt();

        // Keep small/medium table XML in memory (avoiding 200 disk QTemporaryFiles on workbooks);
        // spill very large tables (> 256 KB) to QTemporaryFile to bound peak QString memory.
        QTemporaryFile tmp_file;
        QString tmp_string;
        if (length <= 262144) {
            tmp_string = (length > 0) ? stream.read(length) : QString();
            stream.readLine(); // skip to next newline
        } else if (tmp_file.open()) {
            QTextStream tmp(&tmp_file);
            tmp.setCodec(QTextCodec::codecForName("UTF-8"));
            int read = 0;
            while (length - read >= 1024) {
                tmp << stream.read(1024);
                read += 1024;
            }
            tmp << stream.read(length - read);
            tmp.flush();
            tmp_file.seek(0);
            stream.readLine(); // skip to next newline
        } else {
            tmp_string = (length > 0) ? stream.read(length) : QString();
            stream.readLine(); // skip to next newline
        }

        const QString geomLine = stream.readLine();
        QString endTableLine = stream.readLine(); // </table> (skip optional blank line after geometry\n)
        while (endTableLine.isEmpty() && !endTableLine.isNull() && !stream.atEnd())
            endTableLine = stream.readLine();
        if (geomLine.isNull() || endTableLine != QLatin1String("</table>")
            || (length > 0 && !tmp_file.isOpen() && tmp_string.length() < length)) {
            if (app)
                app->setProperty("_tg_load_truncated", true);
        }
        const bool geomHidden =
                geomLine.trimmed().split(QLatin1Char('\t')).contains(QLatin1String("hidden"));

        XmlStreamReader reader(tmp_string);
        if (tmp_file.isOpen())
            reader.setDevice(&tmp_file);

        while (!reader.atEnd() && !reader.isStartElement())
            reader.readNext();
        QString xmlName = reader.attributes().value(QStringLiteral("name")).toString();
        if (xmlName.isEmpty())
            xmlName = reader.attributes().value(reader.namespaceUri().toString(), QStringLiteral("name")).toString();

        static quint64 s_loading_seq = 0;
        QString tempCap = QStringLiteral("__loading_table_%1__").arg(++s_loading_seq);
        while (app->alreadyUsedName(tempCap))
            tempCap = QStringLiteral("__loading_table_%1__").arg(++s_loading_seq);

        struct ScopedCreatingHidden {
            bool prev;
            explicit ScopedCreatingHidden(bool enable) : prev(creatingHiddenTableFlag())
            {
                if (enable)
                    creatingHiddenTableFlag() = true;
            }
            ~ScopedCreatingHidden() { creatingHiddenTableFlag() = prev; }
        };
        Table *w = nullptr;
        {
            ScopedCreatingHidden hiddenGuard(geomHidden);
            w = app->newTable(tempCap, 1, 1);
        }
        // The table is renamed from tempCap to its saved name by load() and then made unique;
        // those renames are recorded (renamedTables, explorer item below) instead of reaching
        // every graph, graph undo snapshot and formula of the project once per table.
        std::optional<Table::RenamePropagationBlocker> renameBlocker(std::in_place, w);
        if (w->d_future_table->load(&reader) == false) {
            QString msg_text = reader.errorString();
            if (!isNonInteractiveContext(this))
                QMessageBox::critical(this, tr("Error reading table from project file"), msg_text);
        }
        if (reader.hasWarnings()) {
            QString msg_text =
                    tr("The following problems occured when loading the project file:\n");
            QStringList warnings = reader.warningStrings();
            foreach (QString str, warnings)
                msg_text += str + "\n";
            if (!isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Project loading partly failed"), msg_text);
        }

        sanitizeLoadedWindowName(app, w, xmlName, QStringLiteral("Table"));
        renameBlocker.reset();

        w->setObjectName(w->name());
        app->renameListViewItem(tempCap, w->name());
        w->setCaptionPolicy(w->captionPolicy());
        if (!xmlName.isEmpty() && w->name() != xmlName)
            app->renamedTables << xmlName << w->name();

        w->setBirthDate(QLocale().toString(w->d_future_table->creationTime()));

        restoreWindowGeometry(app, w, geomLine);

        if (!app->hidden(w)) {
            bool otherSavedActive = false;
            if (Folder *wf = w->folder()) {
                MyWidget *sa = qobject_cast<MyWidget *>(
                        wf->property("torigraph_saved_active_obj").value<QObject *>());
                if (sa && sa != w && !app->hidden(sa)
                    && sa->property("torigraph_saved_active").toBool()) {
                    otherSavedActive = true;
                }
            }
            if (!otherSavedActive)
                activateSubWindow(w);
        }
        return w;
    }
}

TableStatistics *ApplicationWindow::openTableStatistics(const QStringList &flist)
{
    // Needs the "caption\tbase\trow|col\tdate" header and the targets line; an empty or
    // cut-short <TableStatistics> block is reported as truncated instead of read past its end.
    if (flist.size() < 2 || flist.at(0).split("\t").size() < 4) {
        setProperty("_tg_load_truncated", true);
        return nullptr;
    }
    QStringList::const_iterator line = flist.begin();

    QStringList list = (*line++).split("\t");
    QString caption = list[0];

    QList<int> targets;
    for (int i = 1; i <= (*line).count('\t'); i++)
        targets << (*line).section('\t', i, i).toInt();

    const QString baseRef = remapRenamedTableRef(renamedTables, list[1]);
    Table *baseTbl = nullptr;
    {
        ScopedDisableRenamedLookup guard;
        baseTbl = table(baseRef);
    }
    TableStatistics *w = newTableStatistics(baseTbl,
                                            list[2] == "row" ? TableStatistics::StatRow
                                                             : TableStatistics::StatColumn,
                                            targets, caption);

    setListViewDate(caption, list[3]);
    w->setBirthDate(list[3]);

    for (line++; line != flist.end(); line++) {
        QStringList fields = (*line).split("\t");
        if (fields[0] == "geometry") {
            restoreWindowGeometry(this, w, *line);
        } else if (fields[0] == "header") {
            fields.pop_front();
            if (d_file_version >= 78)
                w->importV0x0001XXHeader(fields);
            else {
                w->setColPlotDesignation(list[4].toInt(), SciDAVis::X);
                w->setColPlotDesignation(list[6].toInt(), SciDAVis::Y);
                w->setHeader(fields);
            }
        } else if (fields[0] == "ColWidth") {
            fields.pop_front();
            w->setColWidths(fields);
        } else if (fields[0] == "com") { // legacy code
            w->setCommands(*line);
        } else if (fields[0] == "<com>") {
            for (line++; line != flist.end() && *line != "</com>"; line++) {
                int col = (*line).mid(9, (*line).length() - 11).toInt();
                QString formula;
                for (line++; line != flist.end() && *line != "</col>"; line++)
                    formula += *line + "\n";
                formula.truncate(formula.length() - 1);
                w->setCommand(col, formula);
            }
        } else if (fields[0] == "ColType") { // d_file_version > 65
            fields.pop_front();
            w->setColumnTypes(fields);
        } else if (fields[0] == "Comments") { // d_file_version > 71
            fields.pop_front();
            w->setColComments(fields);
        } else if (fields[0] == "WindowLabel") { // d_file_version > 71
            const QString unescLbl = QString(fields.value(1)).replace(QLatin1String("&#9;"), QLatin1String("\t"));
            w->setWindowLabel(unescLbl);
            w->setCaptionPolicy((MyWidget::CaptionPolicy)fields.value(2).toInt());
            setListViewLabel(w->name(), unescLbl);
        }
    }
    return w;
}

Graph *ApplicationWindow::openGraph(ApplicationWindow *app, MultiLayer *plot,
                                    const QStringList &list)
{
    Graph *ag = 0;
    int curveID = 0;
    bool hasToriAutoscaleFonts = false;
    for (int j = 0; j < (int)list.count() - 1; j++) {
        QString s = list[j];
        const QString trimmedLine = s.trimmed();
        if (ag && trimmedLine.startsWith(QLatin1String("<Tori"))) {
            if (trimmedLine.startsWith(QLatin1String("<ToriAutoscaleFonts")))
                hasToriAutoscaleFonts = true;
            ag->restorePhase5Tag(remapToriTagColumnRefs(app->renamedTables, trimmedLine));
            continue;
        }
        if (s.startsWith(QLatin1String("ggeometry\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 5) {
                ag = (Graph *)plot->addLayer(fList[1].toInt(), fList[2].toInt(), fList[3].toInt(),
                                             fList[4].toInt());
                ag->blockSignals(true);
                ag->enableAutoscaling(autoscale2DPlots);
            }
            continue;
        }
        if (!ag)
            continue;
        if (s.startsWith(QLatin1String("Background\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 2) {
                QColor c = QColor(COLORVALUE(fList[1]));
                if (fList.count() == 3)
                    c.setAlpha(fList[2].toInt());
                ag->setBackgroundColor(c);
            }
        } else if (s.startsWith(QLatin1String("Margin\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 2)
                ag->plotWidget()->setMargin(fList[1].toInt());
        } else if (s.startsWith(QLatin1String("Border\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 3)
                ag->setFrame(fList[1].toInt(), QColor(COLORVALUE(fList[2])));
        } else if (s.startsWith(QLatin1String("EnabledAxes\t"))) {
            QStringList fList = s.split("\t");
            ag->enableAxes(fList);
        } else if (s.startsWith(QLatin1String("AxesBaseline\t"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList fList = s.split("\t", Qt::SkipEmptyParts);
#else
            QStringList fList = s.split("\t", QString::SkipEmptyParts);
#endif
            ag->setAxesBaseline(fList);
        } else if (s.startsWith(QLatin1String("EnabledTicks\t"))) { // version < 0.8.6
            QStringList fList = s.split("\t");
            fList.pop_front();
            fList.replaceInStrings("-1", "3");
            ag->setMajorTicksType(fList);
            ag->setMinorTicksType(fList);
        } else if (s.startsWith(QLatin1String("MajorTicks\t"))) { // version >= 0.8.6
            QStringList fList = s.split("\t");
            fList.pop_front();
            ag->setMajorTicksType(fList);
        } else if (s.startsWith(QLatin1String("MinorTicks\t"))) { // version >= 0.8.6
            QStringList fList = s.split("\t");
            fList.pop_front();
            ag->setMinorTicksType(fList);
        } else if (s.startsWith(QLatin1String("TicksLength\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 3)
                ag->setTicksLength(fList[1].toInt(), fList[2].toInt());
        } else if (s.startsWith(QLatin1String("EnabledTickLabels\t"))) {
            QStringList fList = s.split("\t");
            fList.pop_front();
            ag->setEnabledTickLabels(fList);
        } else if (s.startsWith(QLatin1String("AxesColors\t"))) {
            QStringList fList = s.split("\t");
            fList.pop_front();
            ag->setAxesColors(fList);
        } else if (s.startsWith(QLatin1String("AxesNumberColors\t"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList fList = s.split("\t", Qt::KeepEmptyParts);
#else
            QStringList fList = s.split("\t", QString::KeepEmptyParts);
#endif
            fList.pop_front();
            ag->setAxesNumColors(fList);
        } else if (s.startsWith(QLatin1String("grid\t"))) {
            ag->plotWidget()->grid()->load(s.split("\t"));
        } else if (s.startsWith(QLatin1String("<Antialiasing>"))
                   && s.endsWith(QLatin1String("</Antialiasing>"))) {
            bool antialiasing = s.remove("<Antialiasing>").remove("</Antialiasing>").toInt();
            ag->setAntialiasing(antialiasing, false);
        } else if (s.startsWith(QLatin1String("PieCurve\t"))) {
            QStringList curve = s.split("\t");
            if (curve.size() >= 8) {
                if (!app->renamedTables.isEmpty())
                    curve[1] = remapRenamedTableRef(app->renamedTables, curve[1]);
                QPen pen = QPen(QColor(COLORVALUE(curve[3])), curve[2].toInt(),
                                Graph::getPenStyle(curve[4]));

                Table *table = nullptr;
                {
                    ScopedDisableRenamedLookup guard;
                    table = app->table(curve[1]);
                }
                if (table) {
                    int startRow = 0;
                    int endRow = table->numRows() - 1;
                    int first_color = curve[7].toInt();
                    bool visible = true;
                    if (d_file_version >= 90 && curve.size() >= 10) {
                        startRow = curve[8].toInt();
                        endRow = curve[9].toInt();
                        visible = ((curve.last() == "1") ? true : false);
                    }

                    if (d_file_version <= 89)
                        first_color = convertOldToNewColorIndex(first_color);

                    ag->plotPie(table, curve[1], pen, curve[5].toInt(), curve[6].toInt(),
                                first_color, startRow, endRow, visible);
                }
            }
        } else if (s.startsWith(QLatin1String("curve\t"))) {
            bool curve_loaded = false; // Graph::insertCurve may fail
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList curve = s.split("\t", Qt::KeepEmptyParts);
#else
            QStringList curve = s.split("\t", QString::KeepEmptyParts);
#endif
            int s_offset = 0;
            if (curve.count() > 14) {
                if (!app->renamedTables.isEmpty()) {
                    for (int idx = 1; idx < curve.size(); ++idx) {
                        if (curve[idx].contains(QLatin1Char('_')))
                            curve[idx] = remapRenamedTableRef(app->renamedTables, curve[idx]);
                    }
                }

                CurveLayout cl;
                cl.connectType = curve[4].toInt();
                cl.lCol = COLORUINT(curve[5]);
                if (d_file_version <= 89)
                    cl.lCol = convertOldToNewColorIndex(cl.lCol);
                cl.lStyle = curve[6].toInt();
                cl.lWidth = curve[7].toInt();
                cl.sSize = curve[8].toInt();
                if (d_file_version <= 78)
                    cl.sType = Graph::obsoleteSymbolStyle(curve[9].toInt());
                else
                    cl.sType = curve[9].toInt();

                cl.symCol = COLORUINT(curve[10]);
                if (d_file_version <= 89)
                    cl.symCol = convertOldToNewColorIndex(cl.symCol);
                if (curve[11] == "-1")
                    cl.symbolFill = false;
                else {
                    cl.symbolFill = true;
                    cl.fillCol = COLORUINT(curve[11]);
                }
                if (d_file_version <= 89)
                    cl.fillCol = convertOldToNewColorIndex(cl.fillCol);
                cl.filledArea = curve[12].toInt();
                cl.aCol = COLORUINT(curve[13]);
                if (d_file_version <= 89)
                    cl.aCol = convertOldToNewColorIndex(cl.aCol);
                cl.aStyle = curve[14].toInt();
                if (curve.count() < 16)
                    cl.penWidth = cl.lWidth;
                else if ((d_file_version >= 79) && (curve[3].toInt() == Graph::Box)) {
                    cl.penWidth = curve[15].toInt();
                    s_offset++;
                } else if ((d_file_version >= 78) && (curve[3].toInt() <= Graph::LineSymbols)) {
                    cl.penWidth = curve[15].toInt();
                    s_offset++;
                } else
                    cl.penWidth = cl.lWidth;
                if (d_file_version >= 0x011800) // 1.24.0
                {
                    // custom dash pattern
                    cl.lCapStyle = curve[15 + s_offset].toInt();
                    cl.lJoinStyle = curve[16 + s_offset].toInt();
                    cl.lCustomDash = curve[17 + s_offset];
                    s_offset += 3;
                }

                Table *w = nullptr;
                {
                    ScopedDisableRenamedLookup guard;
                    w = app->table(curve[2]);
                }
                if (w) {
                    int plotType = curve[3].toInt();
                    if (curve.count() > (21 + s_offset)
                        && (plotType == Graph::VectXYXY || plotType == Graph::VectXYAM)) {
                        QStringList colsList;
                        colsList << curve[2];
                        colsList << curve[20 + s_offset];
                        colsList << curve[21 + s_offset];
                        if (d_file_version < 72)
                            colsList.prepend(w->colName(curve[1].toInt()));
                        else
                            colsList.prepend(curve[1]);

                        int startRow = 0;
                        int endRow = -1;
                        if (d_file_version >= 90) {
                            startRow = curve[curve.count() - 3].toInt();
                            endRow = curve[curve.count() - 2].toInt();
                        }

                        ag->plotVectorCurve(w, colsList, plotType, startRow, endRow);
                        curve_loaded = true;

                        if (d_file_version <= 77) {
                            int temp_index = convertOldToNewColorIndex(curve[15].toInt());
                            ag->updateVectorsLayout(curveID, ColorButton::color(temp_index),
                                                    curve[16].toInt(), curve[17].toInt(),
                                                    curve[18].toInt(), curve[19].toInt(), 0,
                                                    curve[20], curve[21]);
                        } else {
                            if (plotType == Graph::VectXYXY)
                                ag->updateVectorsLayout(
                                        curveID, curve[15 + s_offset], curve[16 + s_offset].toInt(),
                                        curve[17 + s_offset].toInt(), curve[18 + s_offset].toInt(),
                                        curve[19 + s_offset].toInt(), 0);
                            else if (curve.count() > 22 + s_offset)
                                ag->updateVectorsLayout(
                                        curveID, curve[15 + s_offset], curve[16 + s_offset].toInt(),
                                        curve[17 + s_offset].toInt(), curve[18 + s_offset].toInt(),
                                        curve[19 + s_offset].toInt(), curve[22 + s_offset].toInt());
                        }
                    } else if (plotType == Graph::Box) {
                        ag->openBoxDiagram(w, curve, d_file_version);
                        curve_loaded = true;
                    } else if (plotType == Graph::Histogram && curve.count() > 19) {
                        if (d_file_version < 90)
                            curve_loaded = ag->plotHistogram(w, QStringList() << curve[2]);
                        else
                            curve_loaded = ag->plotHistogram(w, QStringList() << curve[2],
                                                             curve[curve.count() - 3].toInt(),
                                                             curve[curve.count() - 2].toInt());
                        if (curve_loaded) {
                            QwtHistogram *h = (QwtHistogram *)ag->curve(curveID);
                            if (d_file_version <= 76)
                                h->setBinning(curve[16].toInt(), curve[17].toDouble(),
                                              curve[18].toDouble(), curve[19].toDouble());
                            else if (curve.count() > 20 + s_offset)
                                h->setBinning(curve[17 + s_offset].toInt(),
                                              curve[18 + s_offset].toDouble(),
                                              curve[19 + s_offset].toDouble(),
                                              curve[20 + s_offset].toDouble());
                            h->loadData();
                        }
                    } else {
                        if (d_file_version < 72)
                            curve_loaded = ag->insertCurve(w, curve[1].toInt(), curve[2], plotType);
                        else if (d_file_version < 90)
                            curve_loaded = ag->insertCurve(w, curve[1], curve[2], plotType);
                        else {
                            int startRow = curve[curve.count() - 3].toInt();
                            int endRow = curve[curve.count() - 2].toInt();
                            curve_loaded = ag->insertCurve(w, curve[1], curve[2], plotType,
                                                           startRow, endRow);
                        }
                    }

                    if (curve_loaded
                        && (plotType == Graph::VerticalBars || plotType == Graph::HorizontalBars
                            || plotType == Graph::Histogram)) {
                        if (d_file_version <= 76 && curve.count() > 15)
                            ag->setBarsGap(curveID, curve[15].toInt(), 0);
                        else if (curve.count() > (16 + s_offset))
                            ag->setBarsGap(curveID, curve[15 + s_offset].toInt(),
                                           curve[16 + s_offset].toInt());
                    }
                    if (curve_loaded)
                        ag->updateCurveLayout(curveID, &cl);
                    if (d_file_version >= 88) {
                        QwtPlotCurve *c = ag->curve(curveID);
                        if (c && c->rtti() == QwtPlotItem::Rtti_PlotCurve) {
                            if (d_file_version < 90)
                                c->setAxis(curve[curve.count() - 2].toInt(),
                                           curve[curve.count() - 1].toInt());
                            else {
                                c->setAxis(curve[curve.count() - 5].toInt(),
                                           curve[curve.count() - 4].toInt());
                                c->setVisible(curve.last().toInt());
                            }
                        }
                    }
                }
                if (curve_loaded)
                    curveID++;
            }
        } else if (s.startsWith(QLatin1String("FunctionCurve\t"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList curve = s.split("\t", Qt::KeepEmptyParts);
#else
            QStringList curve = s.split("\t", QString::KeepEmptyParts);
#endif
            if (curve.size() < 17)
                continue;
            CurveLayout cl;
            cl.connectType = curve[6].toInt();
            cl.lCol = COLORUINT(curve[7]);
            cl.lStyle = curve[8].toInt();
            cl.lWidth = curve[9].toInt();
            cl.sSize = curve[10].toInt();
            cl.sType = curve[11].toInt();
            cl.symCol = COLORUINT(curve[12]);
            if (curve[13] == "-1")
                cl.symbolFill = false;
            else {
                cl.symbolFill = true;
                cl.fillCol = COLORUINT(curve[13]);
            }
            cl.filledArea = curve[14].toInt();
            cl.aCol = COLORUINT(curve[15]);
            cl.aStyle = curve[16].toInt();
            int current_index = 17;
            if (curve.count() < 16)
                cl.penWidth = cl.lWidth;
            else if ((d_file_version >= 79) && (curve[5].toInt() == Graph::Box)) {
                cl.penWidth = curve[17].toInt();
                current_index++;
            } else if ((d_file_version >= 78) && (curve[5].toInt() <= Graph::LineSymbols)) {
                cl.penWidth = curve[17].toInt();
                current_index++;
            } else
                cl.penWidth = cl.lWidth;

            if (d_file_version >= 0x011800) // 1.24.0
            {
                // skeep capStyle, joinStyle and custom dash pattern values
                current_index += 3;
            }

            QStringList func_spec;
            func_spec << curve[1];

            j++;
            while (j < list.size() && list[j] == "<formula>") { // d_file_version >= 0x000105
                QString formula;
                for (j++; j < list.size() && list[j] != "</formula>"; j++)
                    formula += list[j] + "\n";
                func_spec << formula;
                if (j < list.size())
                    j++;
            }
            QVector<double> sampledX, sampledY;
            if (j < list.size() && list[j] == "<sampled_xy>") {
                for (j++; j < list.size() && list[j] != "</sampled_xy>"; j++) {
                    const QStringList xy = list[j].split("\t");
                    if (xy.size() >= 2) {
                        sampledX.push_back(xy[0].toDouble());
                        sampledY.push_back(xy[1].toDouble());
                    }
                }
                if (j < list.size())
                    j++;
            }
            j--;

            bool fnLoaded = ag->insertFunctionCurve(app, func_spec, curve[2].toInt(), d_file_version);
            if (!fnLoaded && !sampledX.isEmpty()) {
                QStringList cMeta = curve[1].split(",");
                const int fType = !cMeta.isEmpty() ? cMeta[0].toInt() : int(FunctionCurve::Normal);
                const QString fName = (cMeta.size() >= 5)
                        ? cMeta.mid(1, cMeta.size() - 4).join(",")
                        : (cMeta.size() > 1 ? cMeta[1] : QStringLiteral("Fit"));
                const QString fVar = (cMeta.size() >= 5) ? cMeta[cMeta.size() - 3] : QStringLiteral("x");
                const double r0 = (cMeta.size() >= 5) ? cMeta[cMeta.size() - 2].toDouble() : sampledX.first();
                const double r1 = (cMeta.size() >= 5) ? cMeta[cMeta.size() - 1].toDouble() : sampledX.last();

                FunctionCurve *fc = new FunctionCurve(
                        app, static_cast<FunctionCurve::FunctionType>(fType), fName);
                fc->setRange(r0, r1);
                fc->setVariable(fVar);
                if (func_spec.size() > 1)
                    fc->setFormulas(func_spec.mid(1));
                fc->setData(sampledX.constData(), sampledY.constData(), sampledX.size());
                ag->insertPlotItem(fc, Graph::Line);
                ag->addLegendItem(fc->legend());
                fnLoaded = true;
            }

            if (fnLoaded) {
                ag->setCurveType(curveID, (Graph::CurveType)curve[5].toInt(), false);
                ag->updateCurveLayout(curveID, &cl);
                if (d_file_version >= 88) {
                    QwtPlotCurve *c = ag->curve(curveID);
                    if (c) {
                        if (current_index + 1 < curve.size())
                            c->setAxis(curve[current_index].toInt(),
                                       curve[current_index + 1].toInt());
                        if (d_file_version >= 90 && current_index + 2 < curve.size())
                            c->setVisible(curve.last().toInt());
                        else
                            c->setVisible(true);
                    }
                }
                if (ag->curve(curveID))
                    curveID++;
            }
        } else if (s.startsWith(QLatin1String("ErrorBars\t"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList curve = s.split("\t", Qt::SkipEmptyParts);
#else
            QStringList curve = s.split("\t", QString::SkipEmptyParts);
#endif
            if (!app->renamedTables.isEmpty() && curve.size() > 4) {
                curve[2] = remapRenamedTableRef(app->renamedTables, curve[2]);
                curve[3] = remapRenamedTableRef(app->renamedTables, curve[3]);
                curve[4] = remapRenamedTableRef(app->renamedTables, curve[4]);
            }
            bool errAdded = false;
            if (ag && curve.size() >= 11) {
                ScopedDisableRenamedLookup guard;
                Table *w = app->table(curve[3]);
                Table *errTable = app->table(curve[4]);
                if (w && errTable) {
                    errAdded = ag->addErrorBars(curve[2], curve[3], errTable, curve[4],
                                                curve[1].toInt(), curve[5].toInt(),
                                                curve[6].toInt(), QColor(COLORVALUE(curve[7])),
                                                curve[8].toInt(), curve[10].toInt(),
                                                curve[9].toInt());
                    if (errAdded) {
                        if (QwtPlotCurve *errC = ag->curve(curveID)) {
                            if (curve.size() >= 12) {
                                errC->setVisible(curve[11].toInt() != 0);
                            } else {
                                QwtPlotCurve *masterC = ag->curve(curve[3]);
                                errC->setVisible(masterC ? masterC->isVisible() : true);
                            }
                        }
                        curveID++;
                    }
                }
            }
        } else if (s == "<spectrogram>") {
            QStringList lst;
            while (s != "</spectrogram>" && j + 1 < (int)list.count()) {
                s = list[++j];
                lst << s;
            }
            if (!lst.isEmpty() && lst.last() == "</spectrogram>")
                lst.pop_back();
            if (ag) {
                const int prevCurves = ag->curves();
                ag->restoreSpectrogram(app, lst);
                if (ag->curves() > prevCurves)
                    curveID += (ag->curves() - prevCurves);
            }
        } else if (s.left(6) == "scale\t") {
            QStringList scl = s.split("\t");
            scl.pop_front();
            if (d_file_version < 88) {
                double step = scl[2].toDouble();
                if (scl[5] == "0")
                    step = 0.0;
                ag->setScale(QwtPlot::xBottom, scl[0].toDouble(), scl[1].toDouble(), step,
                             scl[3].toInt(), scl[4].toInt(), scl[6].toInt(), bool(scl[7].toInt()));
                ag->setScale(QwtPlot::xTop, scl[0].toDouble(), scl[1].toDouble(), step,
                             scl[3].toInt(), scl[4].toInt(), scl[6].toInt(), bool(scl[7].toInt()));

                step = scl[10].toDouble();
                if (scl[13] == "0")
                    step = 0.0;
                ag->setScale(QwtPlot::yLeft, scl[8].toDouble(), scl[9].toDouble(), step,
                             scl[11].toInt(), scl[12].toInt(), scl[14].toInt(),
                             bool(scl[15].toInt()));
                ag->setScale(QwtPlot::yRight, scl[8].toDouble(), scl[9].toDouble(), step,
                             scl[11].toInt(), scl[12].toInt(), scl[14].toInt(),
                             bool(scl[15].toInt()));
            } else
                ag->setScale(scl[0].toInt(), scl[1].toDouble(), scl[2].toDouble(),
                             scl[3].toDouble(), scl[4].toInt(), scl[5].toInt(), scl[6].toInt(),
                             bool(scl[7].toInt()));
        } else if (s.startsWith(QLatin1String("PlotTitle\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 4) {
                ag->setTitle(QString(fList[1]).replace(QLatin1String("&#9;"), QLatin1String("\t")));
                ag->setTitleColor(QColor(COLORVALUE(fList[2])));
                ag->setTitleAlignment(fList[3].toInt());
            }
        } else if (s.startsWith(QLatin1String("TitleFont\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 7) {
                QFont fnt = QFont(fList[1], fList[2].toInt(), fList[3].toInt(), fList[4].toInt());
                fnt.setUnderline(fList[5].toInt());
                fnt.setStrikeOut(fList[6].toInt());
                if (fList.size() >= 8 && fList[7].toDouble() > 0.0)
                    fnt.setPointSizeF(fList[7].toDouble()); // exact pt (e.g. 12.9)
                ag->setTitleFont(fnt);
            }
        } else if (s.startsWith(QLatin1String("AxesTitles\t"))) {
            QStringList legend = s.split("\t");
            legend.pop_front();
            for (int i = 0; i < 4; i++) {
                if (legend.count() > i) {
                    ag->setAxisTitle(
                            Graph::mapToQwtAxis(i),
                            QString(legend[i]).replace(QLatin1String("&#9;"), QLatin1String("\t")));
                }
            }
        } else if (s.startsWith(QLatin1String("AxesTitleColors\t"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList colors = s.split("\t", Qt::SkipEmptyParts);
#else
            QStringList colors = s.split("\t", QString::SkipEmptyParts);
#endif
            ag->setAxesTitleColor(colors);
        } else if (s.startsWith(QLatin1String("AxesTitleAlignment\t"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList align = s.split("\t", Qt::SkipEmptyParts);
#else
            QStringList align = s.split("\t", QString::SkipEmptyParts);
#endif
            ag->setAxesTitlesAlignment(align);
        } else if (s.startsWith(QLatin1String("ScaleFont")) && s.indexOf('\t') == 10) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 7) {
                QFont fnt = QFont(fList[1], fList[2].toInt(), fList[3].toInt(), fList[4].toInt());
                fnt.setUnderline(fList[5].toInt());
                fnt.setStrikeOut(fList[6].toInt());
                if (fList.size() >= 8 && fList[7].toDouble() > 0.0)
                    fnt.setPointSizeF(fList[7].toDouble()); // exact pt (e.g. 12.9)

                int axis = (fList[0].right(1)).toInt();
                ag->setAxisTitleFont(axis, fnt);
            }
        } else if (s.startsWith(QLatin1String("AxisFont")) && s.indexOf('\t') == 9) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 7) {
                QFont fnt = QFont(fList[1], fList[2].toInt(), fList[3].toInt(), fList[4].toInt());
                fnt.setUnderline(fList[5].toInt());
                fnt.setStrikeOut(fList[6].toInt());
                if (fList.size() >= 8 && fList[7].toDouble() > 0.0)
                    fnt.setPointSizeF(fList[7].toDouble()); // exact pt (e.g. 12.9)

                int axis = (fList[0].right(1)).toInt();
                ag->setAxisFont(axis, fnt);
            }
        } else if (s.startsWith(QLatin1String("AxesFormulas\t"))) {
            QStringList fList = s.split("\t");
            fList.removeAll(fList.first());
            ag->setAxesFormulas(fList);
        } else if (s.startsWith("<AxisFormula ")) {
            int pos = s.mid(18, s.length() - 20).toInt();
            QString formula;
            for (j++; j < (int)list.count() && list[j] != "</AxisFormula>"; j++)
                formula += list[j] + "\n";
            formula.truncate(formula.length() - 1);
            ag->setAxisFormula(pos, formula);
        } else if (s.startsWith(QLatin1String("LabelsFormat\t"))) {
            QStringList fList = s.split("\t");
            fList.pop_front();
            ag->setLabelsNumericFormat(fList);
        } else if (s.startsWith(QLatin1String("LabelsRotation\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 3) {
                ag->setAxisLabelRotation(QwtPlot::xBottom, fList[1].toInt());
                ag->setAxisLabelRotation(QwtPlot::xTop, fList[2].toInt());
            }
        } else if (s.startsWith(QLatin1String("DrawAxesBackbone\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 2)
                ag->loadAxesOptions(fList[1]);
        } else if (s.startsWith(QLatin1String("AxesLineWidth\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 2)
                ag->loadAxesLinewidth(fList[1].toInt());
        } else if (s.startsWith(QLatin1String("CanvasFrame\t"))) {
            QStringList list = s.split("\t");
            ag->drawCanvasFrame(list);
        } else if (s.startsWith(QLatin1String("CanvasBackground\t"))) {
            QStringList list = s.split("\t");
            if (list.size() >= 2) {
                QColor c = QColor(COLORVALUE(list[1]));
                if (list.count() == 3)
                    c.setAlpha(list[2].toInt());
                ag->setCanvasBackground(c);
            }
        } else if (s.startsWith(QLatin1String("<legend>"))
                   && s.endsWith(QLatin1String("</legend>"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList fList = s.remove("</legend>").split("\t", Qt::KeepEmptyParts);
#else
            QStringList fList = s.remove("</legend>").split("\t", QString::KeepEmptyParts);
#endif
            ag->insertLegend(fList, d_file_version);
        } else if (s.startsWith(QLatin1String("Legend\t"))) { // version <= 0.8.9
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList fList = s.split("\t", Qt::KeepEmptyParts);
#else
            QStringList fList = s.split("\t", QString::KeepEmptyParts);
#endif
            ag->insertLegend(fList, d_file_version);
        } else if (s.startsWith(QLatin1String("<text>"))
                   && s.endsWith(QLatin1String("</text>"))) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList fList = s.remove("</text>").split("\t", Qt::KeepEmptyParts);
#else
            QStringList fList = s.remove("</text>").split("\t", QString::KeepEmptyParts);
#endif
            ag->insertTextMarker(fList, d_file_version);
        } else if (s.startsWith(QLatin1String("textMarker\t"))) { // version <= 0.8.9
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList fList = s.split("\t", Qt::KeepEmptyParts);
#else
            QStringList fList = s.split("\t", QString::KeepEmptyParts);
#endif
            ag->insertTextMarker(fList, d_file_version);
        } else if (s.startsWith(QLatin1String("<line>"))
                   && s.endsWith(QLatin1String("</line>"))) {
            QStringList fList = s.remove("</line>").split("\t");
            ag->addArrow(fList, d_file_version);
        } else if (s.startsWith(QLatin1String("lineMarker\t"))) { // version <= 0.8.9
            QStringList fList = s.split("\t");
            ag->addArrow(fList, d_file_version);
        } else if ((s.startsWith(QLatin1String("<image>"))
                    && s.endsWith(QLatin1String("</image>")))
                   || s.startsWith(QLatin1String("ImageMarker\t"))) {
            QStringList fList = s.remove("</image>").split("\t");
            ag->insertImageMarker(fList, d_file_version);
        } else if (s.startsWith(QLatin1String("AxisType\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 5)
                for (int i = 0; i < 4; i++) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                    QStringList lst = fList[i + 1].split(";", Qt::SkipEmptyParts);
#else
                    QStringList lst = fList[i + 1].split(";", QString::SkipEmptyParts);
#endif
                    if (lst.size() < 2)
                        continue;
                    Graph::AxisType format = static_cast<Graph::AxisType>(lst[0].toInt());
                    switch (format) {
                    case Graph::AxisType::Day:
                        ag->setLabelsDayFormat(i, lst[1].toInt());
                        break;
                    case Graph::AxisType::Month:
                        ag->setLabelsMonthFormat(i, lst[1].toInt());
                        break;
                    case Graph::AxisType::Time:
                    case Graph::AxisType::Date:
                    case Graph::AxisType::DateTime:
                        if (lst.size() >= 3)
                            ag->setLabelsDateTimeFormat(i, format, lst[1] + ";" + lst[2]);
                        break;
                    case Graph::AxisType::Txt: {
                        const QString colRef = remapRenamedTableRef(app->renamedTables, lst[1]);
                        ScopedDisableRenamedLookup guard;
                        Table *lblTbl = app->table(colRef);
                        if (lblTbl && lblTbl->colIndex(colRef) >= 0)
                            ag->setLabelsTextFormat(i, lblTbl, colRef);
                        break;
                    }
                    case Graph::AxisType::ColHeader: {
                        const QString tblRef = remapRenamedTableRef(app->renamedTables, lst[1]);
                        ScopedDisableRenamedLookup guard;
                        ag->setLabelsColHeaderFormat(i, app->table(tblRef));
                        break;
                    }
                    default:
                        break;
                    }
                }
        } else if (d_file_version < 69 && s.startsWith(QLatin1String("AxesTickLabelsCol\t"))) {
            QStringList fList = s.split("\t");
            if (fList.size() >= 5) {
                const QList<Graph::AxisType> &axesTypes = ag->axesType();
                for (int i = 0; i < 4; i++) {
                    switch (axesTypes[i]) {
                    case Graph::AxisType::Txt: {
                        const QString colRef = remapRenamedTableRef(app->renamedTables, fList[i + 1]);
                        ScopedDisableRenamedLookup guard;
                        Table *lblTbl = app->table(colRef);
                        if (lblTbl && lblTbl->colIndex(colRef) >= 0)
                            ag->setLabelsTextFormat(i, lblTbl, colRef);
                        break;
                    }
                    case Graph::AxisType::ColHeader: {
                        const QString tblRef = remapRenamedTableRef(app->renamedTables, fList[i + 1]);
                        ScopedDisableRenamedLookup guard;
                        ag->setLabelsColHeaderFormat(i, app->table(tblRef));
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
    }
    if (!ag)
        return nullptr;
    ag->replot();
    if (ag->isPiePlot()) {
        QwtPieCurve *c = (QwtPieCurve *)ag->curve(0);
        if (c)
            c->updateBoundingRect();
    }

    ag->blockSignals(false);
    ag->setIgnoreResizeEvents(!app->autoResizeLayers);
    if (!hasToriAutoscaleFonts)
        ag->setAutoscaleFonts(app->autoScaleFonts);
    ag->setTextMarkerDefaults(app->legendFrameStyle, app->plotLegendFont, app->legendTextColor,
                              app->legendBackground);
    ag->setArrowDefaults(app->defaultArrowLineWidth, app->defaultArrowColor,
                         app->defaultArrowLineStyle, app->defaultArrowHeadLength,
                         app->defaultArrowHeadAngle, app->defaultArrowHeadFill);
    return ag;
}

Graph3D *ApplicationWindow::openSurfacePlot(ApplicationWindow *app, const QStringList &lst)
{
    if (!app || lst.size() < 3)
        return 0;

    QStringList fList = lst.value(0).split("\t");
    QString caption = fList.value(0);
    if (caption.isEmpty())
        caption = app->generateUniqueName(tr("Graph"));
    while (app->alreadyUsedName(caption))
        caption = app->generateUniqueName(tr("Graph"));
    QString date = fList.value(1);
    if (date.isEmpty())
        date = QLocale().toString(QDateTime::currentDateTime());

    // Keep empty parts so an empty plotAssociation/formula ("SurfaceFunction\t\tx0...") does not
    // shift coordinates left and overrun fList[7].
    fList = lst.value(2).split("\t");
    if (fList.size() < 8)
        return 0;
    if (!fList[1].isEmpty())
        fList[1] = remapSurfacePlotSource(app->renamedTables, fList[1]);
    const double xl = fList.value(2).toDouble();
    const double xr = fList.value(3).toDouble();
    const double yl = fList.value(4).toDouble();
    const double yr = fList.value(5).toDouble();
    const double zl = fList.value(6).toDouble();
    const double zr = fList.value(7).toDouble();
    Graph3D *plot = 0;
    {
        ScopedDisableRenamedLookup guard;
        if (fList[1].endsWith("(Y)", Qt::CaseSensitive)) // Ribbon plot
            plot = app->dataPlot3D(caption, fList[1], xl, xr, yl, yr, zl, zr);
        else if (fList[1].contains("(Z)", Qt::CaseSensitive))
            plot = app->dataPlotXYZ(caption, fList[1], xl, xr, yl, yr, zl, zr);
        else if (fList[1].startsWith("matrix<", Qt::CaseSensitive)
                 && fList[1].endsWith(">", Qt::CaseInsensitive))
            plot = app->openMatrixPlot3D(caption, fList[1], xl, xr, yl, yr, zl, zr);
        else
            plot = app->newPlot3D(caption, fList[1], xl, xr, yl, yr, zl, zr);
    }

    if (!plot)
        return 0;

    app->setListViewDate(caption, date);
    plot->setBirthDate(date);
    plot->setIgnoreFonts(true);
    restoreWindowGeometry(app, plot, lst.value(1));

    auto splitNonEmptyTabs = [&lst](int idx) -> QStringList {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        return lst.value(idx).split("\t", Qt::SkipEmptyParts);
#else
        return lst.value(idx).split("\t", QString::SkipEmptyParts);
#endif
    };

    if (lst.size() > 3)
        plot->setStyle(splitNonEmptyTabs(3));

    if (lst.size() > 4) {
        fList = splitNonEmptyTabs(4);
        if (fList.size() > 1)
            plot->setGrid(fList.value(1).toInt());
    }

    if (lst.size() > 5) {
        fList = lst.value(5).split("\t");
        plot->setTitle(fList);
    }

    if (lst.size() > 6)
        plot->setColors(splitNonEmptyTabs(6));

    if (lst.size() > 7) {
        fList = splitNonEmptyTabs(7);
        if (!fList.isEmpty())
            fList.pop_front();
        plot->setAxesLabels(fList);
    }

    if (lst.size() > 8)
        plot->setTicks(splitNonEmptyTabs(8));

    if (lst.size() > 9)
        plot->setTickLengths(splitNonEmptyTabs(9));

    if (lst.size() > 10)
        plot->setOptions(splitNonEmptyTabs(10));

    if (lst.size() > 11)
        plot->setNumbersFont(splitNonEmptyTabs(11));

    if (lst.size() > 12)
        plot->setXAxisLabelFont(splitNonEmptyTabs(12));

    if (lst.size() > 13)
        plot->setYAxisLabelFont(splitNonEmptyTabs(13));

    if (lst.size() > 14)
        plot->setZAxisLabelFont(splitNonEmptyTabs(14));

    if (lst.size() > 15) {
        fList = splitNonEmptyTabs(15);
        if (fList.size() > 3)
            plot->setRotation(fList.value(1).toDouble(), fList.value(2).toDouble(),
                              fList.value(3).toDouble());
    }

    if (lst.size() > 16) {
        fList = splitNonEmptyTabs(16);
        if (fList.size() > 1)
            plot->setZoom(fList.value(1).toDouble());
    }

    if (lst.size() > 17) {
        fList = splitNonEmptyTabs(17);
        if (fList.size() > 3)
            plot->setScale(fList.value(1).toDouble(), fList.value(2).toDouble(),
                           fList.value(3).toDouble());
    }

    if (lst.size() > 18) {
        fList = splitNonEmptyTabs(18);
        if (fList.size() > 3)
            plot->setShift(fList.value(1).toDouble(), fList.value(2).toDouble(),
                           fList.value(3).toDouble());
    }

    if (lst.size() > 19) {
        fList = splitNonEmptyTabs(19);
        if (fList.size() > 1)
            plot->setMeshLineWidth(fList.value(1).toInt());
    }

    if (d_file_version > 71 && lst.size() > 20) {
        fList = lst.value(20).split(
                "\t"); // using QString::SkipEmptyParts here causes a crash for empty window labels
        const QString unescLbl = QString(fList.value(1)).replace(QLatin1String("&#9;"), QLatin1String("\t"));
        plot->setWindowLabel(unescLbl);
        plot->setCaptionPolicy((MyWidget::CaptionPolicy)fList.value(2).toInt());
        app->setListViewLabel(plot->name(), unescLbl);
    }

    if (d_file_version >= 88 && lst.size() > 21) {
        fList = splitNonEmptyTabs(21);
        if (fList.size() > 1)
            plot->setOrtho(fList.value(1).toInt());
    }

    plot->update();
    plot->setIgnoreFonts(true);
    return plot;
}

ApplicationWindow *ApplicationWindow::importOPJ(const QString &filename [[maybe_unused]])
{
    const bool nonInteractive = isNonInteractiveContext(this);
    if (filename.endsWith(".opju", Qt::CaseInsensitive)) {
        if (!nonInteractive)
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        ApplicationWindow *app = new ApplicationWindow();
        app->setHeadlessMode(d_headless_mode || nonInteractive);
        app->applyUserSettings();
        app->setWindowTitle("ToriGraph - " + filename);
        if (!nonInteractive)
            app->showMaximized();
        app->projectname = filename;
        app->recentProjects.removeAll(filename);
        app->recentProjects.push_front(filename);
        app->updateRecentProjectsList();

        QString opjuErr;
        const bool ok = ImportOPJU::importFile(app, filename, &opjuErr);
        if (!nonInteractive)
            QApplication::restoreOverrideCursor();
        if (!ok) {
            app->blockSignals(true);
            delete app;
            if (!nonInteractive) {
                QMessageBox::warning(
                        this, tr("Origin OPJU Import Error"),
                        tr("Failed to import Origin Unicode project (.opju):\n%1")
                                .arg(opjuErr.isEmpty() ? tr("Unsupported or corrupted .opju file.")
                                                       : opjuErr));
            }
            return nullptr;
        }
        return app;
    }
#ifdef ORIGIN_IMPORT
    auto codec = getSettings().value("/General/Dialogs/LastUsedOriginLocale", "").toString();
    if (filename.endsWith(".opj", Qt::CaseInsensitive)
        || filename.endsWith(".ogg", Qt::CaseInsensitive)
        || filename.endsWith(".org", Qt::CaseInsensitive)) {
        if (!nonInteractive)
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        ApplicationWindow *app = new ApplicationWindow();
        app->setHeadlessMode(d_headless_mode || nonInteractive);
        app->applyUserSettings();
        app->setWindowTitle("ToriGraph - " + filename);
        if (!nonInteractive)
            app->showMaximized();
        app->projectname = filename;
        app->recentProjects.removeAll(filename);
        app->recentProjects.push_front(filename);
        app->updateRecentProjectsList();

        ImportOPJ importer(app, filename, codec);
        if (!nonInteractive)
            QApplication::restoreOverrideCursor();
        if (importer.error() != 0) {
            app->blockSignals(true);
            delete app;
            return nullptr;
        }
        return app;
    } else if (filename.endsWith(".ogm", Qt::CaseInsensitive)
               || filename.endsWith(".ogw", Qt::CaseInsensitive)) {
        ImportOPJ importer(this, filename, codec);
        if (importer.error() != 0)
            return nullptr;
        recentProjects.removeAll(filename);
        recentProjects.push_front(filename);
        updateRecentProjectsList();
        return this;
    } else
        return 0;
#else
    return NULL;
#endif
}

void ApplicationWindow::updateRecentProjectsList()
{
    for (int i = recentProjects.size() - 1; i >= 0; --i) {
        const QString &entry = recentProjects.at(i);
        if (entry.contains("torigraph_ole_", Qt::CaseInsensitive))
            recentProjects.removeAt(i);
    }

    if (recent)
        recent->clear();

    if (recentProjects.isEmpty())
        return;

    while ((int)recentProjects.size() > MaxRecentProjects)
        recentProjects.pop_back();

    if (!recent)
        return;

    for (int i = 0; i < (int)recentProjects.size(); i++)
        connect(recent->addAction("&" + QString::number(i + 1) + " " + recentProjects[i]),
                SIGNAL(triggered()), this, SLOT(openRecentProject()));
}

void ApplicationWindow::appendProject()
{
    if (isNonInteractiveContext(this))
        return;

    OpenProjectDialog *open_dialog = new OpenProjectDialog(this, false);
    open_dialog->setDirectory(workingDir);
    open_dialog->setExtensionWidget(0);
    if (open_dialog->exec() != QDialog::Accepted || open_dialog->selectedFiles().isEmpty())
        return;
    workingDir = open_dialog->directory().path();
    appendProject(open_dialog->selectedFiles()[0]);
}

static bool preflightScanLegacyProjectComplete(QFile *file, int fileVersion)
{
    if (!file)
        return false;
    const qint64 origPos = file->pos();
    file->seek(0);
    QTextStream t(file);
    t.setCodec(QTextCodec::codecForName("UTF-8"));

    if (t.atEnd()) {
        file->seek(origPos);
        return false;
    }
    t.readLine(); // header line already verified
    if (fileVersion < 73 && !t.atEnd())
        t.readLine();

    int folderDepth = 0;
    bool ok = true;
    QString s;
    while (!t.atEnd()) {
        s = t.readLine(1048576);
        if (s.isNull())
            break;
        if (s.left(8) == QLatin1String("<folder>")) {
            if (s.split(QLatin1Char('\t')).count() < 4) {
                ok = false;
                break;
            }
            ++folderDepth;
        } else if (s == QLatin1String("</folder>")) {
            --folderDepth;
            if (folderDepth < 0) {
                ok = false;
                break;
            }
        } else if (s == QLatin1String("<table>")) {
            if (fileVersion < 0x000200) {
                while (s != QLatin1String("</table>") && !t.atEnd()) {
                    s = t.readLine();
                    if (s.isNull())
                        break;
                }
                if (s != QLatin1String("</table>")) {
                    ok = false;
                    break;
                }
            } else {
                const QString lenLine = t.readLine();
                bool okLen = false;
                const int length = lenLine.toInt(&okLen);
                if (lenLine.isNull() || !okLen || length < 0) {
                    ok = false;
                    break;
                }
                if (length > 0) {
                    if (length <= 262144) {
                        const QString chunk = t.read(length);
                        if (chunk.length() < length) {
                            ok = false;
                            break;
                        }
                    } else {
                        int remaining = length;
                        while (remaining > 0) {
                            const int step = qMin(remaining, 1024);
                            const QString chunk = t.read(step);
                            if (chunk.length() < step) {
                                ok = false;
                                break;
                            }
                            remaining -= step;
                        }
                        if (!ok)
                            break;
                    }
                    t.readLine(); // skip trailing newline after XML payload
                } else {
                    t.readLine();
                }
                const QString geomLine = t.readLine();
                QString endTableLine = t.readLine();
                while (endTableLine.isEmpty() && !endTableLine.isNull() && !t.atEnd())
                    endTableLine = t.readLine();
                if (geomLine.isNull() || endTableLine != QLatin1String("</table>")) {
                    ok = false;
                    break;
                }
            }
        } else if (s.left(17) == QLatin1String("<TableStatistics>")) {
            while (s != QLatin1String("</TableStatistics>") && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
            }
            if (s != QLatin1String("</TableStatistics>")) {
                ok = false;
                break;
            }
        } else if (s == QLatin1String("<matrix>")) {
            while (s != QLatin1String("</matrix>") && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
            }
            if (s != QLatin1String("</matrix>")) {
                ok = false;
                break;
            }
        } else if (s == QLatin1String("<note>")) {
            int hdrCount = 0;
            for (; hdrCount < 3 && !t.atEnd(); ++hdrCount) {
                s = t.readLine();
                if (s.isNull())
                    break;
            }
            if (hdrCount < 3) {
                ok = false;
                break;
            }
            while (s != QLatin1String("</note>") && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
            }
            if (s != QLatin1String("</note>")) {
                ok = false;
                break;
            }
        } else if (s == QLatin1String("<multiLayer>")) {
            const QString hdr = t.readLine();
            if (hdr.isNull() || hdr.split(QLatin1Char('\t')).size() < 4) {
                ok = false;
                break;
            }
            while (s != QLatin1String("</multiLayer>") && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
                if (s.left(7) == QLatin1String("<graph>")) {
                    while (s != QLatin1String("</graph>") && !t.atEnd()) {
                        s = t.readLine();
                        if (s.isNull())
                            break;
                    }
                    if (s != QLatin1String("</graph>")) {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok || s != QLatin1String("</multiLayer>")) {
                ok = false;
                break;
            }
        } else if (s == QLatin1String("<SurfacePlot>")) {
            while (s != QLatin1String("</SurfacePlot>") && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
            }
            if (s != QLatin1String("</SurfacePlot>")) {
                ok = false;
                break;
            }
        } else if (s.left(5) == QLatin1String("<log>")) {
            if (!s.contains(QLatin1String("</log>"))) {
                while (s != QLatin1String("</log>") && !s.endsWith(QLatin1String("</log>"))
                       && !t.atEnd()) {
                    s = t.readLine();
                    if (s.isNull())
                        break;
                }
                if (s != QLatin1String("</log>") && !s.endsWith(QLatin1String("</log>"))) {
                    ok = false;
                    break;
                }
            }
        }
    }

    file->seek(origPos);
    return ok && (folderDepth == 0);
}

void ApplicationWindow::appendProject(const QString &fn)
{
    this->setProperty("torigraph_last_append_ok", false);
    setProperty("_tg_load_truncated", false);
    if (fn.isEmpty())
        return;
    if (guardProjectActionOutsideOleEmbedded(tr("append another project"), fn))
        return;

    struct ScopedRestoreFileVersion
    {
        int &ref;
        int savedVal;
        ~ScopedRestoreFileVersion() { ref = savedVal; }
    } restoreFileVersionGuard { d_file_version, d_file_version };

    renamedTables.clear();
    QFile *file;

    QFileInfo fi(fn);
    workingDir = fi.absolutePath();

    const bool nonInteractive = isNonInteractiveContext(this);
    if (fn.contains(".sciprj", Qt::CaseInsensitive) || fn.contains(".qti", Qt::CaseInsensitive)
        || fn.contains(".opju", Qt::CaseInsensitive) || fn.contains(".opj", Qt::CaseInsensitive)
        || fn.contains(".ogm", Qt::CaseInsensitive) || fn.contains(".ogw", Qt::CaseInsensitive)
        || fn.contains(".ogg", Qt::CaseInsensitive) || fn.contains(".org", Qt::CaseInsensitive)) {
        QFileInfo f(fn);
        if (!f.exists()) {
            if (!nonInteractive) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("The file: <b>%1</b> doesn't exist!").arg(fn));
            }
            return;
        }
    } else {
        if (!nonInteractive) {
            QMessageBox::critical(
                    this, tr("File opening error"),
                    tr("The file: <b>%1</b> is not a ToriGraph or Origin project file!").arg(fn));
        }
        return;
    }

    const bool isOriginProject = fn.contains(".opju", Qt::CaseInsensitive)
            || fn.contains(".opj", Qt::CaseInsensitive)
            || fn.contains(".ogm", Qt::CaseInsensitive)
            || fn.contains(".ogw", Qt::CaseInsensitive)
            || fn.contains(".ogg", Qt::CaseInsensitive)
            || fn.contains(".org", Qt::CaseInsensitive);

    if (fn.endsWith(".gz", Qt::CaseInsensitive) || fn.endsWith(".gz~", Qt::CaseInsensitive)) {
        file = openCompressedFile(fn);
        if (!file) {
            bool backupAccepted = false;
            if (offerBackupIfPresent(fn, &backupAccepted) && backupAccepted) {
                appendProject(fn + QStringLiteral("~"));
                return;
            }
            if (!backupAccepted && !nonInteractive && !fn.endsWith(QLatin1String("~"))
                && QFile::exists(fn + "~")) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("The file <b>%1</b> is not a valid project file.").arg(fn));
            }
            return;
        }
    } else {
        file = new QFile(fn);
        if (!file->open(QIODevice::ReadOnly)) {
            if (!nonInteractive)
                QMessageBox::critical(this, tr("File opening error"), file->errorString());
            delete file;
            return;
        }
    }

    if (!isOriginProject) {
        QTextStream hdrCheck(file);
        hdrCheck.setCodec(QTextCodec::codecForName("UTF-8"));
        const QString hdrLine = hdrCheck.readLine();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QStringList hdrParts = hdrLine.split(QRegExp("\\s"), Qt::SkipEmptyParts);
#else
        const QStringList hdrParts = hdrLine.split(QRegExp("\\s"), QString::SkipEmptyParts);
#endif
        if (hdrParts.count() < 2
            || (hdrParts[0] != "SciDAVis" && hdrParts[0] != "ToriGraph"
                && hdrParts[0] != "QtiPlot")) {
            file->close();
            delete file;
            bool backupAccepted = false;
            if (offerBackupIfPresent(fn, &backupAccepted) && backupAccepted) {
                appendProject(fn + QStringLiteral("~"));
                return;
            }
            if (!backupAccepted && !nonInteractive) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("The file <b>%1</b> is not a valid project file.").arg(fn));
            }
            return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList verParts = hdrParts[1].split(QLatin1Char('.'), Qt::SkipEmptyParts);
#else
        QStringList verParts = hdrParts[1].split(QLatin1Char('.'), QString::SkipEmptyParts);
#endif
        while (verParts.size() < 3)
            verParts << QStringLiteral("0");
        const int parsedVersion = (fn.endsWith(".qti", Qt::CaseInsensitive)
                                   || fn.endsWith(".qti.gz", Qt::CaseInsensitive)
                                   || fn.endsWith(".qti~", Qt::CaseInsensitive)
                                   || fn.endsWith(".qti.gz~", Qt::CaseInsensitive))
                ? (100 * verParts[0].toInt() + 10 * verParts[1].toInt() + verParts[2].toInt())
                : ((verParts[0].toInt() << 16) + (verParts[1].toInt() << 8) + verParts[2].toInt());
        if (!preflightScanLegacyProjectComplete(file, parsedVersion)) {
            file->close();
            delete file;
            bool backupAccepted = false;
            if (offerBackupIfPresent(fn, &backupAccepted) && backupAccepted) {
                appendProject(fn + QStringLiteral("~"));
                return;
            }
            if (!backupAccepted && !nonInteractive) {
                QMessageBox::critical(
                        this, tr("File opening error"),
                        tr("The project file <b>%1</b> is incomplete or corrupted (unexpected end "
                           "of file) and could not be appended safely.")
                                .arg(fn));
            }
            return;
        }
        file->seek(0);
    }

    recentProjects.removeAll(fn);
    recentProjects.push_front(fn);
    updateRecentProjectsList();

    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    BatchWindowUpdates batchGuard(this);

    Folder *cf = current_folder;
    FolderListItem *item = (FolderListItem *)current_folder->folderListItem();
    folders.blockSignals(true);
    blockSignals(true);

    QString baseName = fi.fileName();
    static const char *const kKnownProjSuffixes[] = {
        ".sciprj.gz~", ".sciprj.gz", ".sciprj~", ".sciprj",
        ".qti.gz~",    ".qti.gz",    ".qti~",    ".qti",
        ".opju",       ".opj",       ".ogm",     ".ogw",
        ".ogg",        ".org"
    };
    bool strippedSuffix = false;
    for (const char *sfx : kKnownProjSuffixes) {
        const QString qsfx = QLatin1String(sfx);
        if (baseName.length() > qsfx.length() && baseName.endsWith(qsfx, Qt::CaseInsensitive)) {
            baseName.chop(qsfx.length());
            strippedSuffix = true;
            break;
        }
    }
    if (!strippedSuffix)
        baseName = fi.completeBaseName();
    if (baseName.isEmpty())
        baseName = fi.baseName();

    QStringList lst = current_folder->subfolders();
    int n = lst.count(baseName);
    if (n) { // avoid identical subfolder names
        while (lst.count(baseName + QString::number(n)))
            n++;
        baseName += QString::number(n);
    }

    QUndoStack *projUndo = d_project ? d_project->undoStack() : nullptr;
    const int prevUndoIndex = projUndo ? projUndo->index() : 0;
    const QUndoCommand *prevTopCmd =
            (projUndo && prevUndoIndex > 0) ? projUndo->command(prevUndoIndex - 1) : nullptr;
    struct ScopedAppendMacro
    {
        QUndoStack *stack = nullptr;
        bool active = false;
        explicit ScopedAppendMacro(QUndoStack *s, const QString &text) : stack(s)
        {
            if (stack) {
                stack->beginMacro(text);
                active = true;
            }
        }
        void close()
        {
            if (active && stack) {
                active = false;
                stack->endMacro();
            }
        }
        ~ScopedAppendMacro() { close(); }
    } appendMacro(projUndo, tr("Append project"));

    Folder *appendedRoot = &current_folder->addChild<Folder>(baseName);
    current_folder = appendedRoot;
    FolderListItem *fli = new FolderListItem(item, current_folder);
    current_folder->setFolderListItem(fli);

    auto abortAppendCleanup = [&]() {
        // Close non-aspect views first, roll back and purge partial undo commands before
        // removing leftover folders so prior user undo history remains intact.
        QVector<Folder *> pending;
        pending.append(appendedRoot);
        while (!pending.isEmpty()) {
            Folder *node = pending.takeLast();
            if (!node)
                continue;
            const QList<MyWidget *> wins = node->windowsList();
            for (MyWidget *mw : wins) {
                if (!mw)
                    continue;
                mw->askOnCloseEvent(false);
                if (!qobject_cast<Table *>(mw) && !qobject_cast<Matrix *>(mw))
                    closeWindow(mw);
            }
            for (QObject *obj : node->children()) {
                if (Folder *sub = qobject_cast<Folder *>(obj))
                    pending.append(sub);
            }
        }

        appendMacro.close();
        if (projUndo) {
            const bool pushedMacro = (projUndo->index() > prevUndoIndex)
                    || (projUndo->index() > 0
                        && projUndo->command(projUndo->index() - 1) != prevTopCmd);
            if (pushedMacro) {
                ScopedUndoReplayGuard replayGuard(this);
                projUndo->undo();
                projUndo->push(new ObsoletePurgeCmd());
            }
        }

        const bool prevConfirm = confirmCloseFolder;
        const bool prevSuppressUndoClear = d_suppress_delete_folder_undo_clear;
        confirmCloseFolder = false;
        d_suppress_delete_folder_undo_clear = true;
        deleteFolder(appendedRoot);
        d_suppress_delete_folder_undo_clear = prevSuppressUndoClear;
        confirmCloseFolder = prevConfirm;

        current_folder = cf;
        if (cf && cf->folderListItem())
            folders.setCurrentItem(cf->folderListItem());
        folders.blockSignals(false);
        changeFolder(cf, true);
        blockSignals(false);
        renamedTables.clear();
        if (!nonInteractive)
            QApplication::restoreOverrideCursor();
    };

    if (fn.endsWith(".opju", Qt::CaseInsensitive)) {
        file->close();
        delete file;
        QString opjuErr;
        if (!ImportOPJU::importFile(this, fn, &opjuErr)) {
            abortAppendCleanup();
            if (!nonInteractive) {
                QMessageBox::warning(
                        this, tr("Origin OPJU Import Error"),
                        tr("Failed to import Origin Unicode project (.opju):\n%1")
                                .arg(opjuErr.isEmpty() ? tr("Unsupported or corrupted .opju file.")
                                                       : opjuErr));
            }
            return;
        }
    } else if (isOriginProject)
#ifdef ORIGIN_IMPORT
    {
        auto codec = getSettings().value("/General/Dialogs/LastUsedOriginLocale", "").toString();
        ImportOPJ importer(this, fn, codec);
        file->close();
        delete file;
        if (importer.error() != 0) {
            abortAppendCleanup();
            return;
        }
    }
#else
    {
        if (!nonInteractive) {
            QMessageBox::critical(
                    this, tr("File opening error"),
                    tr("ToriGraph currently does not support Origin import."));
        }
        file->close();
        delete file;
        abortAppendCleanup();
        return;
    }
#endif
    else {
        QTextStream t(file);
        t.setCodec(QTextCodec::codecForName("UTF-8"));

        QString s = t.readLine();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        lst = s.split(QRegExp("\\s"), Qt::SkipEmptyParts);
        QString version = (lst.count() >= 2) ? lst[1] : QStringLiteral("0.0.0");
        lst = version.split(".", Qt::SkipEmptyParts);
#else
        lst = s.split(QRegExp("\\s"), QString::SkipEmptyParts);
        QString version = (lst.count() >= 2) ? lst[1] : QStringLiteral("0.0.0");
        lst = version.split(".", QString::SkipEmptyParts);
#endif
        while (lst.size() < 3)
            lst << QStringLiteral("0");
        if (fn.endsWith(".qti", Qt::CaseInsensitive) || fn.endsWith(".qti.gz", Qt::CaseInsensitive))
            d_file_version = 100 * (lst[0]).toInt() + 10 * (lst[1]).toInt() + (lst[2]).toInt();
        else
            d_file_version = ((lst[0]).toInt() << 16) + ((lst[1]).toInt() << 8) + (lst[2]).toInt();

        t.readLine();
        if (d_file_version < 73)
            t.readLine();

        bool truncatedProject = false;
        // Folder made current by each genuine <folder> / </folder> line of the first pass, keyed
        // by the stream position after that line (see skipSecondPassProjectBlock / openProject).
        QHash<qint64, QPointer<Folder>> folderAtMarker;
        bool markerKeysValid = true;
        auto recordFolderMarker = [&t, &folderAtMarker, &markerKeysValid](Folder *f) {
            const qint64 key = t.pos();
            if (key < 0)
                markerKeysValid = false;
            else
                folderAtMarker.insert(key, QPointer<Folder>(f));
        };
        // process tables and matrix information
        while (!t.atEnd()) {
            s = t.readLine(1048576); // workaround for safely reading very big lines
            if (s.isNull())
                break;
            lst.clear();
            if (s.left(8) == "<folder>") {
                lst = s.split("\t");
                if (lst.count() < 4) {
                    truncatedProject = true;
                    break;
                }
                Folder &f = current_folder->addChild<Folder>(lst[1]);
                f.setBirthDate(lst[2]);
                f.setModificationDate(lst[3]);

                FolderListItem *fli = new FolderListItem(current_folder->folderListItem(), &f);
                fli->setText(0, lst[1]);
                f.setFolderListItem(fli);

                current_folder = &f;
                recordFolderMarker(&f);
            } else if (s == "<table>") {
                openTable(this, t);
                if (property("_tg_load_truncated").toBool()) {
                    truncatedProject = true;
                    break;
                }
            } else if (s.startsWith(QLatin1String("<workbook>\t"))) {
                WorkbookManager::instance(this)->restoreFromLine(s, current_folder, renamedTables);
            } else if (s.left(17) == "<TableStatistics>") {
                QStringList tsLst;
                while (s != "</TableStatistics>" && !t.atEnd()) {
                    s = t.readLine();
                    if (s.isNull())
                        break;
                    tsLst << s;
                }
                if (s != "</TableStatistics>") {
                    truncatedProject = true;
                    break;
                }
                if (!tsLst.isEmpty())
                    tsLst.pop_back();
                openTableStatistics(tsLst);
            } else if (s == "<matrix>") {
                while (s != "</matrix>" && !t.atEnd()) {
                    s = t.readLine();
                    if (s.isNull())
                        break;
                    lst << s;
                }
                if (s != "</matrix>") {
                    truncatedProject = true;
                    break;
                }
                if (!lst.isEmpty())
                    lst.pop_back();
                openMatrix(this, lst);
            } else if (s == "<note>") {
                for (int i = 0; i < 3 && !t.atEnd(); i++) {
                    s = t.readLine();
                    if (s.isNull())
                        break;
                    lst << s;
                }
                if (lst.size() < 3) {
                    truncatedProject = true;
                    break;
                }
                Note *m = openNote(this, lst);
                QStringList cont;
                while (s != "</note>" && !t.atEnd()) {
                    s = t.readLine();
                    if (s.isNull())
                        break;
                    cont << s;
                }
                if (s != "</note>") {
                    truncatedProject = true;
                    break;
                }
                if (!cont.isEmpty())
                    cont.pop_back();
                if (m)
                    m->restore(cont);
            } else if (s == "</folder>") {
                Folder *parent = (Folder *)current_folder->parent();
                if (!parent)
                    current_folder = projectFolder();
                else
                    current_folder = parent;
                recordFolderMarker(current_folder);
            } else if (isSecondPassProjectBlockStart(s)) {
                skipSecondPassProjectBlock(t, s);
            }
        }

        // process the rest
        if (!truncatedProject && !property("_tg_load_truncated").toBool()) {
            t.seek(0);

            MultiLayer *plot = 0;
            while (!t.atEnd()) {
                s = t.readLine(1048576); // workaround for safely reading very big lines
                if (s.isNull())
                    break;
                if (s.left(8) == "<folder>" || s == "</folder>") {
                    applySecondPassFolderMarker(t, s, folderAtMarker, markerKeysValid,
                                                current_folder, projectFolder());
                } else if (s == "<multiLayer>") { // process multilayers information
                    s = t.readLine();
                    QStringList graph = s.split("\t");
                    if (s.isNull() || graph.size() < 4) {
                        truncatedProject = true;
                        break;
                    }
                    QString caption = graph[0];
                    plot = multilayerPlot(caption);
                    plot->setCols(graph[1].toInt());
                    plot->setRows(graph[2].toInt());
                    setListViewDate(caption, graph[3]);
                    plot->setBirthDate(graph[3]);
                    plot->blockSignals(true);

                    restoreWindowGeometry(this, plot, t.readLine());

                    if (d_file_version > 71 && !t.atEnd()) {
                        QStringList lst = t.readLine().split("\t");
                        const QString unescLbl = QString(lst.value(1))
                                                         .replace(QLatin1String("&#9;"),
                                                                  QLatin1String("\t"))
                                                         .replace(QLatin1String("<br>"),
                                                                  QLatin1String("\n"));
                        plot->setWindowLabel(unescLbl);
                        setListViewLabel(plot->name(), unescLbl);
                        plot->setCaptionPolicy((MyWidget::CaptionPolicy)lst.value(2).toInt());
                    }

                    if (d_file_version > 83 && !t.atEnd()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        const auto skipEmpty = Qt::SkipEmptyParts;
#else
                        const auto skipEmpty = QString::SkipEmptyParts;
#endif
                        QStringList lst = t.readLine().split("\t", skipEmpty);
                        if (lst.size() >= 5)
                            plot->setMargins(lst[1].toInt(), lst[2].toInt(), lst[3].toInt(),
                                             lst[4].toInt());
                        if (!t.atEnd()) {
                            lst = t.readLine().split("\t", skipEmpty);
                            if (lst.size() >= 3)
                                plot->setSpacing(lst[1].toInt(), lst[2].toInt());
                        }
                        if (!t.atEnd()) {
                            lst = t.readLine().split("\t", skipEmpty);
                            if (lst.size() >= 3)
                                plot->setLayerCanvasSize(lst[1].toInt(), lst[2].toInt());
                        }
                        if (!t.atEnd()) {
                            lst = t.readLine().split("\t", skipEmpty);
                            if (lst.size() >= 3)
                                plot->setAlignement(lst[1].toInt(), lst[2].toInt());
                        }
                    }

                    while (s != "</multiLayer>" && !t.atEnd()) { // open layers
                        s = t.readLine();
                        if (s.isNull())
                            break;
                        if (s.left(7) == "<graph>") {
                            lst.clear();
                            while (s != "</graph>" && !t.atEnd()) {
                                s = t.readLine();
                                if (s.isNull())
                                    break;
                                lst << s;
                            }
                            if (s != "</graph>") {
                                truncatedProject = true;
                                break;
                            }
                            openGraph(this, plot, lst);
                        } else if (s.trimmed().startsWith("<Tori")) {
                            plot->restorePhase5Tag(s.trimmed());
                        }
                    }
                    plot->finalizePhase5Restore();
                    plot->blockSignals(false);
                    if (truncatedProject || s != "</multiLayer>") {
                        truncatedProject = true;
                        break;
                    }
                } else if (s == "<SurfacePlot>") { // process 3D plots information
                    lst.clear();
                    while (s != "</SurfacePlot>" && !t.atEnd()) {
                        s = t.readLine();
                        if (s.isNull())
                            break;
                        lst << s;
                    }
                    if (s != "</SurfacePlot>") {
                        truncatedProject = true;
                        break;
                    }
                    openSurfacePlot(this, lst);
                } else if (s.left(5) == QLatin1String("<log>")) {
                    // The results log is not restored when appending, but its lines must not be
                    // taken for folder markers or window blocks.
                    skipSecondPassProjectBlock(t, s);
                }
            }
        }
        file->close();
        delete file;

        if (truncatedProject || property("_tg_load_truncated").toBool()) {
            abortAppendCleanup();
            bool backupAccepted = false;
            if (offerBackupIfPresent(fn, &backupAccepted) && backupAccepted) {
                appendProject(fn + QStringLiteral("~"));
                return;
            }
            if (!backupAccepted && !nonInteractive) {
                QMessageBox::critical(
                        this, tr("File opening error"),
                        tr("The project file <b>%1</b> is incomplete or corrupted (unexpected end "
                           "of file) and could not be appended safely.")
                                .arg(fn));
            }
            return;
        }
    }

    appendMacro.close();
    if (cf && cf->folderListItem())
        folders.setCurrentItem(cf->folderListItem());
    folders.blockSignals(false);
    // keep caller's active folder after appending project
    changeFolder(cf, true);
    updateExplorerFolderPaneVisibility();
    for (MyWidget *mw : windowsList()) {
        if (!mw)
            continue;
        mw->setProperty("torigraph_saved_active", QVariant());
        if (Folder *mf = mw->folder())
            mf->setProperty("torigraph_saved_active_obj", QVariant());
        if ((mw->inherits("Table") || mw->inherits("Matrix")) && stringContainsWhitespace(mw->name())) {
            const QString oldName = mw->name();
            {
                // the rename is propagated once, by updateTableNames below
                Table::RenamePropagationBlocker renameBlocker(qobject_cast<Table *>(mw));
                sanitizeLoadedWindowName(this, mw, oldName,
                                         mw->inherits("Matrix") ? QStringLiteral("Matrix")
                                                                : QStringLiteral("Table"));
            }
            const QString newName = mw->name();
            if (newName != oldName) {
                mw->setObjectName(newName);
                renameListViewItem(oldName, newName);
                if (mw->inherits("Table"))
                    updateTableNames(oldName, newName);
                else
                    changeMatrixName(oldName, newName);
            }
        }
    }
    blockSignals(false);
    renamedTables = QStringList();
    modifiedProject();
    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
    this->setProperty("torigraph_last_append_ok", true);
}

void ApplicationWindow::rawSaveFolder(Folder *folder, QIODevice *device)
{
    QTextStream stream(device);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
    foreach (MyWidget *w, folder->windowsList()) {
        Table *t = qobject_cast<Table *>(w);
        if (t)
            t->saveToDevice(device, windowGeometryInfo(w));
        else
            stream << w->saveToString(windowGeometryInfo(w));
    }
    // D8: workbook (sheet tabs) membership; older versions ignore the line
    stream << WorkbookManager::instance(this)->saveFolderBooks(folder);
    stream.flush();
    foreach (Folder *subfolder, folder->folders()) {
        stream << "<folder>\t" + QString(subfolder->name()) + "\t" + subfolder->birthDate() + "\t"
                        + subfolder->modificationDate();
        if (subfolder == current_folder)
            stream << "\tcurrent\n";
        else
            stream << "\n"; // FIXME: Having no 5th string here is not a good idea
        stream.flush();
        rawSaveFolder(subfolder, device);
        stream << "</folder>\n";
    }
}

bool ApplicationWindow::saveFolder(Folder *folder, const QString &fn)
{
    const bool nonInteractive = isNonInteractiveContext(this);
    // file saving procedure follows
    // https://bugs.launchpad.net/ubuntu/+source/linux/+bug/317781/comments/54
    // 1. write everything to "<fn>.new" and commit it to disk
    // 2. swap it with "<fn>", keeping the previous version as "<fn>~"
    // The existing project file is never opened for writing.
    QFile f(fn + ".new");
    while (!f.open(QIODevice::WriteOnly)) {
        if (f.isOpen())
            f.close();
        if (nonInteractive)
            return false;
        // The following message is slightly misleading, since it may be that fn.new can't be opened
        // _at_all_. However, changing this would break translations, in a bugfix release.
        // TODO: rephrase message for next minor release
        switch (QMessageBox::critical(
                this, tr("File save error"),
                tr("The file: <br><b>%1</b> is opened in read-only mode").arg(fn + ".new"),
                QMessageBox::Retry | QMessageBox::Default,
                QMessageBox::Abort | QMessageBox::Escape)) {
        case QMessageBox::Abort:
            return false;
        }
    }

    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QTextStream t(&f);
    t.setCodec(QTextCodec::codecForName("UTF-8"));
    t << SciDAVis::schemaVersion() + " project file\n";
    t << "<scripting-lang>\t" + QString(scriptEnv->objectName()) + "\n";
    t << "<windows>\t" + QString::number(folder->windowCount(true)) + "\n";
    t.flush();
    rawSaveFolder(folder, &f);
    t << "<log>\n" + logInfo + "</log>";
    t.flush();

    // second part of secure file saving (see comment at the start of this method)
    // http://support.microsoft.com/kb/148505/en-us
    // http://msdn.microsoft.com/en-us/library/17618685(VS.80).aspx
    if (t.status() != QTextStream::Ok || f.error() != QFile::NoError || !flushAndCommitFile(f)) {
        const QString reason =
                f.error() != QFile::NoError ? f.errorString() : qt_error_string(errno);
        f.close();
        if (!nonInteractive) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(
                    this, tr("Error writing data to disk"),
                    tr("<html>The project could not be written to disk:<br>%1<br><br>The "
                       "incomplete file is <em>%2</em>. If there already was a version of this "
                       "project on disk, it has not been touched.</html>")
                            .arg(reason.toHtmlEscaped(),
                                 QDir::toNativeSeparators(fn + ".new").toHtmlEscaped()));
        }
        return false;
    }
    f.close();

    QString replaceErr;
    if (!replaceFileWithBackup(fn + ".new", fn, &replaceErr)) {
        if (!nonInteractive) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(
                    this, tr("Project not saved"),
                    tr("<html>The project file could not be replaced:<br>%1<br><br>The file "
                       "<em>%2</em> has not been changed. It may be in use by another program "
                       "(for example a file synchronization service). Your current data was "
                       "written to <em>%3</em>.<br><br>Please try saving again, or use "
                       "\"Save Project As\" with a different name.</html>")
                            .arg(replaceErr.toHtmlEscaped(),
                                 QDir::toNativeSeparators(fn).toHtmlEscaped(),
                                 QDir::toNativeSeparators(fn + ".new").toHtmlEscaped()));
        }
        return false;
    }

    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
    return true;
}

/*!
 * Write \a folder as a project to \a projectPath. A path ending in ".gz" is written
 * compressed. Returns false (after telling the user why, unless running non-interactively)
 * if the file on disk could not be brought up to date.
 */
bool ApplicationWindow::writeProjectFile(Folder *folder, const QString &projectPath)
{
    if (!projectPath.endsWith(QLatin1String(".gz"), Qt::CaseInsensitive))
        return saveFolder(folder, projectPath);

    // Write the uncompressed data to a scratch file next to the target, so that neither an
    // existing "<name>.sciprj" nor its backup is touched by saving "<name>.sciprj.gz".
    const QString rawTmp = projectPath.left(projectPath.length() - 3) + ".uncompressed.tmp";
    QFile::remove(rawTmp);
    if (!saveFolder(folder, rawTmp))
        return false;

    QString gzErr;
    if (!compressFileGzipUnicode(rawTmp, projectPath, &gzErr)) {
        QFile::remove(rawTmp);
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(
                    this, tr("Project not saved"),
                    tr("<html>The compressed project file <em>%1</em> could not be "
                       "written:<br>%2<br><br>If there already was a version of this project on "
                       "disk, it has not been changed.</html>")
                            .arg(QDir::toNativeSeparators(projectPath).toHtmlEscaped(),
                                 gzErr.toHtmlEscaped()));
        }
        return false;
    }
    return true;
}

void ApplicationWindow::saveAsProject()
{
    saveFolderAsProject(current_folder);
}

void ApplicationWindow::saveFolderAsProject(Folder *f)
{
    if (isNonInteractiveContext(this))
        return;

    QString filter = tr("ToriGraph project") + " (*.sciprj);;";
    filter += tr("Compressed ToriGraph project") + " (*.sciprj.gz)";

    QString selectedFilter;
    QString fn = QFileDialog::getSaveFileName(this, tr("Save project as"), workingDir, filter,
                                              &selectedFilter);
    if (!fn.isEmpty()) {
        QFileInfo fi(fn);
        workingDir = fi.absolutePath();
        QString baseName = fi.fileName();
        if (!baseName.endsWith(".sciprj") && !baseName.endsWith(".sciprj.gz")) {
            fn.append(".sciprj");
        }
        if (selectedFilter.contains(".gz") && !fn.endsWith(".gz"))
            fn.append(".gz");

        writeProjectFile(f, fn);
    }
}

MultiLayer *ApplicationWindow::openMultiLayerFromTemplateFile(const QString &fn, QString *errOut)
{
    if (fn.isEmpty())
        return nullptr;
    QFileInfo fi(fn);
    if (!fi.exists()) {
        if (errOut)
            *errOut = tr("The template file '%1' does not exist.").arg(fn);
        return nullptr;
    }
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errOut)
            *errOut = f.errorString();
        return nullptr;
    }
    QTextStream t(&f);
    t.setCodec(QTextCodec::codecForName("UTF-8"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const auto skipEmpty = Qt::SkipEmptyParts;
#else
    const auto skipEmpty = QString::SkipEmptyParts;
#endif
    const QStringList l = t.readLine().split(QRegularExpression(QStringLiteral("\\s+")), skipEmpty);
    if (l.size() < 2 || (l[0] != "SciDAVis" && l[0] != "ToriGraph" && l[0] != "QtiPlot")) {
        if (errOut)
            *errOut = tr("Not a valid ToriGraph template file.");
        return nullptr;
    }
    QStringList vl = l[1].split('.', skipEmpty);
    while (vl.size() < 3)
        vl << QStringLiteral("0");

    const int savedFileVer = d_file_version;
    auto restoreVer = [&]() { d_file_version = savedFileVer; };
    if (l[0] == "QtiPlot") {
        d_file_version = 100 * vl[0].toInt() + 10 * vl[1].toInt() + vl[2].toInt();
        if (d_file_version > 90) {
            restoreVer();
            if (errOut)
                *errOut = tr("Unsupported QtiPlot template version (> 0.9.0).");
            return nullptr;
        }
    } else {
        d_file_version = (vl[0].toInt() << 16) + (vl[1].toInt() << 8) + vl[2].toInt();
    }

    QString templateType;
    t >> templateType;
    if (templateType != "<multiLayer>") {
        restoreVer();
        if (errOut)
            *errOut = tr("Selected file is not a 2D graph (.qpt) template.");
        return nullptr;
    }

    int rows = 1, cols = 1;
    t >> rows;
    t >> cols;
    t.skipWhiteSpace();
    const QString geometry = t.readLine();

    const QString label = generateUniqueName(tr("Graph"));
    MultiLayer *ml = multilayerPlot(label);
    if (!ml) {
        restoreVer();
        return nullptr;
    }

    ml->setCols(qMax(1, cols));
    ml->setRows(qMax(1, rows));
    restoreWindowGeometry(this, ml, geometry);

    if (d_file_version > 83 && !t.atEnd()) {
        QStringList lst = t.readLine().split("\t", skipEmpty);
        if (lst.size() >= 5)
            ml->setMargins(lst[1].toInt(), lst[2].toInt(), lst[3].toInt(), lst[4].toInt());
        if (!t.atEnd()) {
            lst = t.readLine().split("\t", skipEmpty);
            if (lst.size() >= 3)
                ml->setSpacing(lst[1].toInt(), lst[2].toInt());
        }
        if (!t.atEnd()) {
            lst = t.readLine().split("\t", skipEmpty);
            if (lst.size() >= 3)
                ml->setLayerCanvasSize(lst[1].toInt(), lst[2].toInt());
        }
        if (!t.atEnd()) {
            lst = t.readLine().split("\t", skipEmpty);
            if (lst.size() >= 3)
                ml->setAlignement(lst[1].toInt(), lst[2].toInt());
        }
    }
    while (!t.atEnd()) {
        QString s = t.readLine();
        if (s.isNull())
            break;
        if (s.left(7) == "<graph>") {
            QStringList lst;
            while (s != "</graph>" && !t.atEnd()) {
                s = t.readLine();
                if (s.isNull())
                    break;
                lst << s;
            }
            openGraph(this, ml, lst);
        } else if (s.trimmed().startsWith("<Tori")) {
            ml->restorePhase5Tag(s.trimmed());
        }
    }
    ml->finalizePhase5Restore();
    restoreVer();
    if (ml->pageViewMode()) {
        ml->fitLayersToPage();
        ml->fitPageZoomToViewport();
    }
    return ml;
}

void ApplicationWindow::applyTemplateToActiveTable()
{
    Table *t = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (!t) {
        for (MyWidget *w : (current_folder ? current_folder->windowsList() : windowsList())) {
            if (Table *cand = qobject_cast<Table *>(w)) {
                t = cand;
                break;
            }
        }
    }
    if (!t) {
        showUxHint(tr("Please select or open a worksheet before applying a graph template."));
        return;
    }
    if (t->noXColumn()) {
        if (!isNonInteractiveContext(this))
            QMessageBox::warning(this, tr("Plot Error"),
                                 tr("You need at least one X column to plot this worksheet."));
        return;
    }

    QStringList yCols = t->selectedYColumns();
    if (yCols.isEmpty())
        yCols = t->YColumns();
    if (yCols.isEmpty()) {
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colPlotDesignation(c) != SciDAVis::X)
                yCols << t->colName(c);
        }
    }
    if (yCols.isEmpty()) {
        showUxHint(tr("No Y columns were found in '%1'.").arg(t->name()));
        return;
    }

    if (isNonInteractiveContext(this))
        return;
    const QString fn = QFileDialog::getOpenFileName(
            this, tr("Open 2D Graph Template (.qpt)"), templatesDir,
            tr("ToriGraph 2D Graph Template (*.qpt)"));
    if (fn.isEmpty())
        return;
    templatesDir = QFileInfo(fn).absolutePath();

    QString errMsg;
    MultiLayer *ml = openMultiLayerFromTemplateFile(fn, &errMsg);
    if (!ml) {
        QMessageBox::critical(this, tr("Template Error"),
                              errMsg.isEmpty() ? tr("Failed to open template '%1'.").arg(fn)
                                               : errMsg);
        return;
    }

    Graph *ag = ml->activeGraph();
    if (!ag && !ml->layersList().isEmpty())
        ag = ml->layersList().first();
    if (!ag)
        ag = ml->addLayer();

    const int startRow = (t->selectedRowCount(false) >= 2) ? t->firstSelectedRow(false) : 0;
    const int endRow = (t->selectedRowCount(false) >= 2) ? t->lastSelectedRow(false)
                                                         : qMax(0, t->numRows() - 1);
    const int curveStyle = (!ag->templateCurveStyles().isEmpty())
            ? ag->templateCurveStyles().first().curveType
            : defaultCurveStyle;

    while (ag->curves() > 0)
        ag->removeCurve(0);
    ag->insertCurvesList(t, yCols, curveStyle, defaultCurveLineWidth, defaultSymbolSize, startRow,
                         endRow);
    ag->applyTemplateCurveStylesToAll();
    if (!ag->hasLegend())
        ag->newLegend();
    else
        ag->reconstructLegend();
    applyWorksheetMetadataToGraph(ag, t, yCols, curveStyle);
    ag->setAutoScale();
    ag->replot();
    if (ml->pageViewMode()) {
        ml->fitLayersToPage();
        ml->fitPageZoomToViewport();
    } else {
        ml->arrangeLayers(false, false);
    }
    if (ml->undoStack())
        ml->undoStack()->clear();
    customMenu(ml);
    activateSubWindow(ml);
    emit ml->modifiedPlot();
    if (d_graph_inspector_dock)
        d_graph_inspector_dock->setTargetMultiLayer(ml, true);
    updateUndoRedoActionsForActiveWindow();
    emit modified();
}
