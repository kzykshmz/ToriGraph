/***************************************************************************
        File                 : ApplicationWindow_DataAndExport.cpp
        Project              : SciDAVis
        Description          : SciDAVis's main window (tables, matrices, notes, ASCII/image import, export, print, clipboard & OLE)
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
#include "core/datatypes/DateTime2StringFilter.h"
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

void ApplicationWindow::updateTableNames(const QString &oldName, const QString &newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;

    const QString oldPrefix = oldName + QLatin1Char('_');
    const QString newPrefix = newName + QLatin1Char('_');

    QList<MyWidget *> windows = windowsList();

    // Column names of the renamed table (called after the rename, or before it): graph undo
    // snapshots then rewrite only `oldName_<column of this table>`, not the columns of another
    // table whose name starts with `oldName_` (Table1 vs Table1_Sheet1).
    Table *renamedTable = nullptr;
    for (const QString &candidate : { newName, oldName }) {
        for (MyWidget *w : windows) {
            Table *t = qobject_cast<Table *>(w);
            if (t && t->name() == candidate) {
                renamedTable = t;
                break;
            }
        }
        if (renamedTable)
            break;
    }
    const QStringList renamedTableCols = renamedTable ? renamedTable->colNames() : QStringList();

    foreach (MyWidget *w, windows) {
        if (!w)
            continue;
        if (w->inherits("MultiLayer")) {
            MultiLayer *ml = static_cast<MultiLayer *>(w);
            QWidgetList gr_lst = ml->graphPtrs();
            foreach (QWidget *widget, gr_lst) {
                if (widget)
                    ((Graph *)widget)->updateCurveNames(oldName, newName);
            }
            ml->remapUndoSnapshotsForTableRename(oldName, newName,
                                                 renamedTable ? &renamedTableCols : nullptr);
        } else if (w->inherits("Graph3D")) {
            Graph3D *g3 = static_cast<Graph3D *>(w);
            if (g3->userFunction() || g3->matrix())
                continue;
            Table *tbl = g3->getTable();
            if (tbl && tbl->name() != oldName && tbl->name() != newName)
                continue;

            QString xSpec, ySpec, zSpec;
            bool isXYZ = false;
            if (!splitGraph3DTableAssoc(g3->formula(), xSpec, ySpec, zSpec, isXYZ))
                continue;

            if (!tbl) {
                Table *resolved = table(xSpec);
                if (resolved && resolved->name() != oldName && resolved->name() != newName)
                    continue;
            }

            auto remapTablePrefix = [&](QString &spec) -> bool {
                if (!spec.startsWith(oldPrefix))
                    return false;
                if (!tbl) {
                    Table *rt = table(spec);
                    if (rt && rt->name() != oldName && rt->name() != newName)
                        return false;
                }
                spec = newPrefix + spec.mid(oldPrefix.length());
                return true;
            };

            bool changed = false;
            changed = remapTablePrefix(xSpec) || changed;
            changed = remapTablePrefix(ySpec) || changed;
            if (isXYZ)
                changed = remapTablePrefix(zSpec) || changed;

            if (changed)
                g3->setPlotAssociation(joinGraph3DTableAssoc(xSpec, ySpec, zSpec, isXYZ));
        }
    }
    refreshAllGraphsAutoAxisTitles();
    RecalcManager::instance(this)->notifyTableRenamed(oldName, newName);
}

void ApplicationWindow::updateColNames(const QString &oldName, const QString &newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName) {
        refreshAllGraphsAutoAxisTitles();
        return;
    }

    if (Table *owner = table(newName)) {
        const QString pfx = owner->name() + QLatin1Char('_');
        if (oldName.startsWith(pfx) && newName.startsWith(pfx)) {
            RecalcManager::instance(this)->notifyColumnRenamed(
                    owner->name(), oldName.mid(pfx.length()), newName.mid(pfx.length()));
        }
    } else if (Table *ownerOld = table(oldName)) {
        const QString pfx = ownerOld->name() + QLatin1Char('_');
        if (oldName.startsWith(pfx) && newName.startsWith(pfx)) {
            RecalcManager::instance(this)->notifyColumnRenamed(
                    ownerOld->name(), oldName.mid(pfx.length()), newName.mid(pfx.length()));
        }
    }

    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (!w)
            continue;
        if (w->inherits("MultiLayer")) {
            MultiLayer *ml = static_cast<MultiLayer *>(w);
            QWidgetList gr_lst = ml->graphPtrs();
            foreach (QWidget *widget, gr_lst) {
                if (widget)
                    ((Graph *)widget)->updateCurveNames(oldName, newName, false);
            }
            ml->remapUndoSnapshotsForColRename(oldName, newName);
        } else if (w->inherits("Graph3D")) {
            Graph3D *g3 = static_cast<Graph3D *>(w);
            if (g3->userFunction() || g3->matrix())
                continue;
            Table *tbl = g3->getTable();
            QString bareOldLabel;
            if (tbl) {
                const QString tblPrefix = tbl->name() + QLatin1Char('_');
                if (!oldName.startsWith(tblPrefix))
                    continue;
                bareOldLabel = oldName.mid(tblPrefix.length());
            }

            QString xSpec, ySpec, zSpec;
            bool isXYZ = false;
            if (!splitGraph3DTableAssoc(g3->formula(), xSpec, ySpec, zSpec, isXYZ))
                continue;

            auto remapColExact = [&](QString &spec) -> bool {
                if (spec == oldName
                    || (!bareOldLabel.isEmpty() && spec == bareOldLabel)) {
                    spec = newName;
                    return true;
                }
                return false;
            };

            bool changed = false;
            changed = remapColExact(xSpec) || changed;
            changed = remapColExact(ySpec) || changed;
            if (isXYZ)
                changed = remapColExact(zSpec) || changed;

            if (changed)
                g3->setPlotAssociation(joinGraph3DTableAssoc(xSpec, ySpec, zSpec, isXYZ));
        }
    }
    refreshAllGraphsAutoAxisTitles();
}

void ApplicationWindow::changeMatrixName(const QString &oldName, const QString &newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;

    const QString oldMatrixAssoc = QStringLiteral("matrix<") + oldName + QLatin1Char('>');
    const QString newMatrixAssoc = QStringLiteral("matrix<") + newName + QLatin1Char('>');

    QList<MyWidget *> lst = windowsList();
    foreach (MyWidget *w, lst) {
        if (!w)
            continue;
        if (w->inherits("Graph3D")) {
            Graph3D *g3 = static_cast<Graph3D *>(w);
            if (g3->userFunction())
                continue;
            Matrix *m = g3->matrix();
            if (m && m->name() != oldName && m->name() != newName)
                continue;
            if (g3->formula() == oldMatrixAssoc)
                g3->setPlotAssociation(newMatrixAssoc);
        } else if (w->inherits("MultiLayer")) {
            QWidgetList graphsList = ((MultiLayer *)w)->graphPtrs();
            foreach (QWidget *gr_widget, graphsList) {
                Graph *g = static_cast<Graph *>(gr_widget);
                if (!g)
                    continue;
                auto updateSpectrogramTitle = [&](QwtPlotItem *it) {
                    if (!it || it->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
                        return;
                    Spectrogram *sp = static_cast<Spectrogram *>(it);
                    Matrix *sm = sp->matrix();
                    if (sm && sm->name() != oldName && sm->name() != newName)
                        return;
                    if (sp->title().text() == oldName)
                        sp->setTitle(newName);
                };
                const int itemCount = qMax(g->curves(), g->plotItemsList().count());
                for (int i = 0; i < itemCount; ++i)
                    updateSpectrogramTitle(g->plotItem(i));
                if (Plot *pw = g->plotWidget()) {
                    const QwtPlotItemList &rawItems = pw->itemList();
                    for (QwtPlotItem *it : rawItems)
                        updateSpectrogramTitle(it);
                }
            }
            static_cast<MultiLayer *>(w)->remapUndoSnapshotsForMatrixRename(oldName, newName);
        }
    }
}

Matrix *ApplicationWindow::importImage()
{
    if (isNonInteractiveContext(this))
        return 0;
    QList<QByteArray> list = QImageReader::supportedImageFormats();
    QString filter = tr("Images") + " (", aux1, aux2;
    for (int i = 0; i < (int)list.count(); i++) {
        aux1 = " *." + list[i] + " ";
        aux2 += " *." + list[i] + ";;";
        filter += aux1;
    }
    filter += ");;" + aux2;

    QString fn =
            QFileDialog::getOpenFileName(this, tr("Import image from file"), imagesDirPath, filter);
    if (!fn.isEmpty()) {
        QFileInfo fi(fn);
        imagesDirPath = fi.absolutePath();
        return importImage(fn);
    } else
        return 0;
}

void ApplicationWindow::loadImage()
{
    if (isNonInteractiveContext(this))
        return;
    QList<QByteArray> list = QImageReader::supportedImageFormats();
    QString filter = tr("Images") + " (", aux1, aux2;
    for (int i = 0; i < (int)list.count(); i++) {
        aux1 = " *." + list[i] + " ";
        aux2 += " *." + list[i] + ";;";
        filter += aux1;
    }
    filter += ");;" + aux2;

    QString fn =
            QFileDialog::getOpenFileName(this, tr("Load image from file"), imagesDirPath, filter);
    if (!fn.isEmpty()) {
        loadImage(fn);
        QFileInfo fi(fn);
        imagesDirPath = fi.absolutePath();
    }
}

void ApplicationWindow::loadImage(const QString &fn)
{
    const bool nonInteractive = isNonInteractiveContext(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    MultiLayer *plot = multilayerPlot(generateUniqueName(tr("Graph")));
    plot->setWindowLabel(fn);
    plot->setCaptionPolicy(MyWidget::Both);
    setListViewLabel(plot->name(), fn);

    if (isTabbedWorkspaceView())
        plot->showMaximized();
    else
        plot->showNormal();
    const QSize cvSize = plot->canvasWidget() ? plot->canvasWidget()->size() : plot->size();
    Graph *g = plot->addLayer(0, 0, cvSize.width(), cvSize.height());

    g->setTitle("");
    QVector<bool> axesOn(4);
    for (int j = 0; j < 4; j++)
        axesOn[j] = false;
    g->enableAxes(axesOn);
    g->removeLegend();
    g->setIgnoreResizeEvents(false);
    g->addImage(fn);
    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
}

void ApplicationWindow::customizeTables(const QColor &bgColor, const QColor &textColor,
                                        const QColor &headerColor, const QFont &textFont,
                                        const QFont &headerFont, bool showComments)
{
    tableBkgdColor = bgColor;
    tableTextColor = textColor;
    tableHeaderColor = headerColor;
    tableTextFont = textFont;
    tableHeaderFont = headerFont;
    d_show_table_comments = showComments;

    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (w->inherits("Table"))
            customTable((Table *)w);
    }
}

void ApplicationWindow::customTable(Table *w)
{
    QPalette cg;
    cg.setColor(QPalette::Base, QColor(tableBkgdColor));
    cg.setColor(QPalette::Text, QColor(tableTextColor));
    w->setPalette(cg);

    w->setHeaderColor(tableHeaderColor);
    w->setTextFont(tableTextFont);
    w->setHeaderFont(tableHeaderFont);
    w->showComments(d_show_table_comments);
}

void ApplicationWindow::newWrksheetPlot(const QString &name, const QString &label,
                                        QList<Column *> columns)
{
    Table *w = newTable(name, label, columns);
    MultiLayer *plot = multilayerPlot(w, QStringList(QString(w->name()) + "_intensity"), 0);
    Graph *g = (Graph *)plot->activeGraph();
    if (g) {
        g->setTitle("");
        g->setXAxisTitle(tr("Position, x / px"));
        g->setYAxisTitle(tr("Intensity, I / a.u."));
    }
}

/*
 *used when importing an ASCII file
 */
Table *ApplicationWindow::newTable(const QString &fname, const QString &sep, int lines,
                                   bool renameCols, bool stripSpaces, bool simplifySpaces,
                                   bool convertToNumeric, QLocale numericLocale)
{
    Table *w = new Table(scriptEnv, fname, sep, lines, renameCols, stripSpaces, simplifySpaces,
                         convertToNumeric, numericLocale, fname, &d_workspace);
    if (w) {
        if (!w->importSucceeded()) {
            const QString err = w->lastImportError();
            if (!err.isEmpty())
                setProperty("torigraph_last_import_error", err);
            w->askOnCloseEvent(false);
            delete w;
            return nullptr;
        }
        w->setName(generateUniqueName(tr("Table")));
        w->setAutoRecalcFormulas(RecalcManager::instance(this)->recalcMode()
                                 == RecalcManager::Auto);
        d_project->addChild(w->d_future_table);
    }
    return w;
}

/*
 *creates a new empty table
 */
Table *ApplicationWindow::newTable()
{
    Table *w = new Table(scriptEnv, 30, 2, "", &d_workspace, 0);
    w->setName(generateUniqueName(tr("Table")));
    w->setAutoRecalcFormulas(RecalcManager::instance(this)->recalcMode() == RecalcManager::Auto);
    d_project->addChild(w->d_future_table);
    attachEmptyTableHint(w);
    return w;
}

/*
 *used when opening a project file
 */
Table *ApplicationWindow::newTable(const QString &caption, int r, int c)
{
    Table *w = new Table(scriptEnv, r, c, "", &d_workspace, 0);
    w->setName(caption);
    w->setAutoRecalcFormulas(RecalcManager::instance(this)->recalcMode() == RecalcManager::Auto);
    d_project->addChild(w->d_future_table);
    if (w->name() != caption) // the table was renamed
    {
        renamedTables << caption << w->name();

        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Renamed Window"),
                                 tr("The table '%1' already exists. It has been renamed '%2'.")
                                         .arg(caption)
                                         .arg(w->name()));
        }
    }
    return w;
}

Table *ApplicationWindow::newTable(int r, int c, const QString &name, const QString &legend)
{
    Table *w = new Table(scriptEnv, r, c, legend, &d_workspace, 0);
    w->setName(name);
    w->setAutoRecalcFormulas(RecalcManager::instance(this)->recalcMode() == RecalcManager::Auto);
    d_project->addChild(w->d_future_table);
    attachEmptyTableHint(w);
    return w;
}

Table *ApplicationWindow::newTable(const QString &name, const QString &legend,
                                   QList<Column *> columns)
{
    for (Column *col : columns) {
        if (col && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
            if (auto *out = dynamic_cast<Double2StringFilter *>(col->outputFilter())) {
                if (out->numericFormat() == 'f' && out->numDigits() == 14) {
                    out->setNumericFormat('g');
                    out->setNumDigits(6);
                }
            }
        }
    }
    Table *w = new Table(scriptEnv, 0, 0, legend, &d_workspace, 0);
    w->d_future_table->appendColumns(columns);
    w->setName(name);
    w->setAutoRecalcFormulas(RecalcManager::instance(this)->recalcMode() == RecalcManager::Auto);
    d_project->addChild(w->d_future_table);
    return w;
}

Table *ApplicationWindow::newHiddenTable(const QString &name, const QString &label,
                                         QList<Column *> columns)
{
    struct ScopedCreatingHidden {
        bool prev;
        ScopedCreatingHidden() : prev(creatingHiddenTableFlag()) { creatingHiddenTableFlag() = true; }
        ~ScopedCreatingHidden() { creatingHiddenTableFlag() = prev; }
    } guard;
    auto w = newTable(name, label, columns);
    hideWindow(w);
    return w;
}

Table *ApplicationWindow::newHiddenTable(int r, int c, const QString &name, const QString &legend)
{
    struct ScopedCreatingHidden {
        bool prev;
        ScopedCreatingHidden() : prev(creatingHiddenTableFlag()) { creatingHiddenTableFlag() = true; }
        ~ScopedCreatingHidden() { creatingHiddenTableFlag() = prev; }
    } guard;
    Table *w = newTable(r, c, name, legend);
    hideWindow(w);
    return w;
}

void ApplicationWindow::initTable(Table *w)
{
    d_workspace.addSubWindow(w);
    w->setWindowIcon(IconLoader::load("new-table"));
    Folder *targetFolder = current_folder;
    if (w->d_future_table) {
        const QVariant fv = w->d_future_table->property("_tg_restore_folder");
        if (fv.isValid()) {
            w->d_future_table->setProperty("_tg_restore_folder", QVariant());
            if (Folder *saved = qobject_cast<Folder *>(fv.value<QPointer<QObject>>().data()))
                targetFolder = saved;
        }
    }
    targetFolder->addWindow(w);
    w->setFolder(targetFolder);
    if (targetFolder == current_folder)
        addListViewItem(w);
    if (creatingHiddenTableFlag())
        w->setHidden();
    else if (targetFolder != current_folder) {
        w->setStatus(MyWidget::Normal);
        w->hide();
    } else if (isTabbedWorkspaceView())
        w->showMaximized();
    else if (d_has_last_table_geometry
             && !(d_modern_ribbon
                  && d_modern_ribbon->workspaceLayoutMode()
                          == ModernRibbonBar::LayoutSplitTableGraph)) {
        applyLastTableWindowGeometry(w);
    } else {
        const QSize vp = d_workspace.viewport() ? d_workspace.viewport()->size() : QSize();
        if (vp.width() > 240 && vp.height() > 240) {
            const int targetW = qMax(680, int(vp.width() * 0.75));
            const int targetH = qMax(500, int(vp.height() * 0.80));
            w->resize(qMin(targetW, qMax(400, vp.width() - 16)),
                      qMin(targetH, qMax(300, vp.height() - 16)));
        }
        w->showNormal();
        if (vp.width() <= 240 || vp.height() <= 240) {
            QPointer<Table> safeTbl(w);
            QTimer::singleShot(0, this, [this, safeTbl]() {
                if (!safeTbl || isTabbedWorkspaceView() || safeTbl->isMaximized()
                    || safeTbl->isMinimized())
                    return;
                const QSize vp2 = d_workspace.viewport() ? d_workspace.viewport()->size() : QSize();
                if (vp2.width() > 240 && vp2.height() > 240) {
                    const int targetW = qMax(680, int(vp2.width() * 0.75));
                    const int targetH = qMax(500, int(vp2.height() * 0.80));
                    safeTbl->resize(qMin(targetW, qMax(400, vp2.width() - 16)),
                                    qMin(targetH, qMax(300, vp2.height() - 16)));
                }
            });
        }
    }

    connectTable(w);
    customTable(w);

    w->d_future_table->setPlotMenu(plot2D);

    emit modified();
}

/*
 * !creates a new table with type statistics on target columns/rows of table base
 */
TableStatistics *ApplicationWindow::newTableStatistics(Table *base, int type, QList<int> target,
                                                       const QString &caption)
{
    if (!base)
        return nullptr;
    TableStatistics *s =
            new TableStatistics(scriptEnv, &d_workspace, base, (TableStatistics::Type)type, target);
    if (!caption.isEmpty())
        s->setName(caption);

    d_project->addChild(s->d_future_table);
    connect(base, SIGNAL(modifiedData(Table *, const QString &)), s,
            SLOT(update(Table *, const QString &)));
    connect(base, SIGNAL(changedColHeader(const QString &, const QString &)), s,
            SLOT(renameCol(const QString &, const QString &)));
    connect(base, SIGNAL(removedCol(const QString &)), s, SLOT(removeCol(const QString &)));
    connect(base->d_future_table, SIGNAL(aspectAboutToBeRemoved(const AbstractAspect *)), this,
            SLOT(removeDependentTableStatistics(const AbstractAspect *)));
    return s;
}

void ApplicationWindow::removeDependentTableStatistics(const AbstractAspect *aspect)
{
    ::future::Table *future_table =
            qobject_cast<::future::Table *>(const_cast<AbstractAspect *>(aspect));
    if (!future_table || !d_project)
        return;
    Table *table = qobject_cast<Table *>(future_table->view());
    if (!table)
        return;
    QList<QPointer<::future::Table>> toRemove;
    const QList<MyWidget *> windows = windowsList();
    for (MyWidget *win : windows) {
        TableStatistics *table_stat = qobject_cast<TableStatistics *>(win);
        if (table_stat && table_stat->base() == table && table_stat->d_future_table
            && table_stat->d_future_table->parentAspect() == d_project) {
            toRemove.append(table_stat->d_future_table);
        }
    }
    if (toRemove.isEmpty())
        return;
    // Suppress undo stack pushes so removing dependent TableStatistics inside
    // aspectAboutToBeRemoved (during push, undo, or redo) never calls QUndoStack::push() re-entrantly.
    ScopedProjectUndoSuppressor undoSuppress(d_project);
    for (const QPointer<::future::Table> &statTable : toRemove) {
        if (statTable && statTable->parentAspect() == d_project)
            d_project->removeChild(statTable.data());
    }
}

/*
 *creates a new empty note window
 */
Note *ApplicationWindow::newNote(const QString &caption)
{
    Note *m = new Note(scriptEnv, "", &d_workspace);
    if (caption.isEmpty())
        initNote(m, generateUniqueName(tr("Notes")));
    else
        initNote(m, caption);
    if (isTabbedWorkspaceView())
        m->showMaximized();
    else
        m->showNormal();
    return m;
}

void ApplicationWindow::initNote(Note *m, const QString &caption)
{
    QString name = caption;
    while (name.isEmpty() || alreadyUsedName(name))
        name = generateUniqueName(tr("Notes"));

    d_workspace.addSubWindow(m);
    m->setWindowTitle(name);
    m->setName(name);
    m->setWindowIcon(IconLoader::load("new-note"));
    m->askOnCloseEvent(confirmCloseNotes);
    m->setFolder(current_folder);

    current_folder->addWindow(m);
    addListViewItem(m);

    connect(m, SIGNAL(modifiedWindow(MyWidget *)), this, SLOT(modifiedProject(MyWidget *)));
    connect(m, SIGNAL(closedWindow(MyWidget *)), this, SLOT(closeWindow(MyWidget *)));
    connect(m, SIGNAL(hiddenWindow(MyWidget *)), this, SLOT(hideWindow(MyWidget *)));
    connect(m, SIGNAL(statusChanged(MyWidget *)), this, SLOT(updateWindowStatus(MyWidget *)));
    connect(m, SIGNAL(showTitleBarMenu()), this, SLOT(showWindowTitleBarMenu()));

    emit modified();
}

Matrix *ApplicationWindow::newMatrix(int rows, int columns)
{
    Matrix *m = new Matrix(scriptEnv, rows, columns, "", 0, 0);
    QString caption = generateUniqueName(tr("Matrix"));
    while (alreadyUsedName(caption)) {
        caption = generateUniqueName(tr("Matrix"));
    }
    m->setName(caption);
    d_project->addChild(m->d_future_matrix);
    return m;
}

Matrix *ApplicationWindow::newMatrix(const QString &caption, int r, int c)
{
    Matrix *w = new Matrix(scriptEnv, r, c, "", 0, 0);
    QString name = caption;
    while (alreadyUsedName(name)) {
        name = generateUniqueName(caption);
    }
    w->setName(name);
    d_project->addChild(w->d_future_matrix);
    if (w->name() != caption) // the matrix was renamed
        renamedTables << caption << w->name();

    return w;
}

void ApplicationWindow::matrixDeterminant()
{
    Matrix *m = qobject_cast<Matrix *>(d_workspace.activeSubWindow());
    if (!m)
        return;

    QDateTime dt = QDateTime::currentDateTime();
    QString info = QLocale().toString(dt);
    info += "\n" + tr("Determinant of ") + QString(m->name()) + ":\t";
    info += "det = " + QString::number(m->determinant()) + "\n";
    info += "-------------------------------------------------------------\n";

    logInfo += info;

    showResults(true);
}

void ApplicationWindow::invertMatrix()
{
    Matrix *m = qobject_cast<Matrix *>(d_workspace.activeSubWindow());
    if (!m)
        return;

    m->invert();
}

Table *ApplicationWindow::convertMatrixToTable()
{
    Matrix *m = qobject_cast<Matrix *>(d_workspace.activeSubWindow());
    if (!m)
        return 0;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    int rows = m->numRows();
    int cols = m->numCols();

    Table *w = new Table(scriptEnv, rows, cols, "", &d_workspace, 0);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            w->setCell(i, j, m->cell(i, j));
    }

    w->setName(generateUniqueName(tr("Table")));
    d_project->addChild(w->d_future_table);
    w->setWindowLabel(m->windowLabel());
    w->setCaptionPolicy(m->captionPolicy());
    w->resize(m->size());
    if (isTabbedWorkspaceView())
        w->showMaximized();
    else
        w->showNormal();

    QApplication::restoreOverrideCursor();

    return w;
}

void ApplicationWindow::initMatrix(Matrix *m)
{
    if (!m)
        return;
    d_workspace.addSubWindow(m);
    m->setWindowIcon(IconLoader::load("new-matrix"));
    m->askOnCloseEvent(confirmCloseMatrix);
    m->setNumericFormat(d_default_numeric_format, d_decimal_digits);
    Folder *targetFolder = current_folder;
    if (m->d_future_matrix) {
        const QVariant fv = m->d_future_matrix->property("_tg_restore_folder");
        if (fv.isValid()) {
            m->d_future_matrix->setProperty("_tg_restore_folder", QVariant());
            if (Folder *saved = qobject_cast<Folder *>(fv.value<QPointer<QObject>>().data()))
                targetFolder = saved;
        }
    }

    targetFolder->addWindow(m);
    m->setFolder(targetFolder);
    if (targetFolder == current_folder)
        addListViewItem(m);
    if (targetFolder != current_folder) {
        m->setStatus(MyWidget::Normal);
        m->hide();
    } else if (isTabbedWorkspaceView())
        m->showMaximized();
    else
        m->showNormal();

    connect(m, SIGNAL(showTitleBarMenu()), this, SLOT(showWindowTitleBarMenu()),
            Qt::UniqueConnection);
    connect(m, SIGNAL(closedWindow(MyWidget *)), this, SLOT(closeWindow(MyWidget *)),
            Qt::UniqueConnection);
    connect(m, SIGNAL(modifiedWindow(MyWidget *)), this, SLOT(modifiedProject(MyWidget *)),
            Qt::UniqueConnection);
    connect(m, SIGNAL(modifiedWindow(MyWidget *)), this, SLOT(updateMatrixPlots(MyWidget *)),
            Qt::UniqueConnection);
    connect(m, SIGNAL(hiddenWindow(MyWidget *)), this, SLOT(hideWindow(MyWidget *)),
            Qt::UniqueConnection);
    connect(m, SIGNAL(statusChanged(MyWidget *)), this, SLOT(updateWindowStatus(MyWidget *)),
            Qt::UniqueConnection);
    connect(m, SIGNAL(showContextMenu()), this, SLOT(showWindowContextMenu()),
            Qt::UniqueConnection);
    emit modified();
}

Matrix *ApplicationWindow::convertTableToMatrix()
{
    Table *m = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (!m)
        return 0;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    int rows = m->numRows();
    int cols = m->numCols();

    Matrix *w = new Matrix(scriptEnv, rows, cols, "", 0, 0);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            w->setText(i, j, m->text(i, j));
    }

    QString caption = generateUniqueName(m->name());
    w->setName(caption);
    d_project->addChild(w->d_future_matrix);

    w->setCaptionPolicy(m->captionPolicy());
    w->resize(m->size());
    if (isTabbedWorkspaceView())
        w->showMaximized();
    else
        w->showNormal();

    QApplication::restoreOverrideCursor();
    return w;
}

ApplicationWindow *ApplicationWindow::plotFile(const QString &fn)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    ApplicationWindow *app = new ApplicationWindow();
    app->setHeadlessMode(d_headless_mode);
    app->applyUserSettings();
    if (!d_headless_mode)
        app->showMaximized();

    Table *t = app->newTable(fn, app->columnSeparator, 0, true, app->strip_spaces,
                             app->simplify_spaces, app->d_convert_to_numeric,
                             app->d_ASCII_import_locale);
    if (!t) {
        QApplication::restoreOverrideCursor();
        app->savedProject();
        delete app;
        return nullptr;
    }
    t->setCaptionPolicy(MyWidget::Both);
    app->multilayerPlot(t, t->YColumns(), Graph::LineSymbols);
    QApplication::restoreOverrideCursor();
    return app;
}

void ApplicationWindow::importASCII()
{
    if (isNonInteractiveContext(this))
        return;
    QScopedPointer<ImportASCIIDialog> import_dialog(new ImportASCIIDialog(
            d_workspace.currentSubWindow() && d_workspace.currentSubWindow()->inherits("Table"),
            this, d_extended_import_ASCII_dialog));
    import_dialog->setDirectory(asciiDirPath);
    import_dialog->selectNameFilter(d_ASCII_file_filter);
    if (import_dialog->exec() != QDialog::Accepted)
        return;

    asciiDirPath = import_dialog->directory().path();
    d_ASCII_file_filter = import_dialog->selectedNameFilter();
    if (import_dialog->rememberOptions()) {
        columnSeparator = import_dialog->columnSeparator();
        ignoredLines = import_dialog->ignoredLines();
        renameColumns = import_dialog->renameColumns();
        strip_spaces = import_dialog->stripSpaces();
        simplify_spaces = import_dialog->simplifySpaces();
        d_ASCII_import_locale = import_dialog->decimalSeparators();
        d_convert_to_numeric = import_dialog->convertToNumeric();
        saveSettings();
    }

    QLocale save_locale = QLocale();
    QLocale::setDefault(import_dialog->decimalSeparators());
    importASCII(import_dialog->selectedFiles(), import_dialog->importMode(),
                import_dialog->columnSeparator(), import_dialog->ignoredLines(),
                import_dialog->renameColumns(), import_dialog->stripSpaces(),
                import_dialog->simplifySpaces(), import_dialog->convertToNumeric(),
                import_dialog->decimalSeparators());
    QLocale::setDefault(save_locale);
}

// Renames columns 0..targetNames.size()-1 of `table` to targetNames (empty entry = keep the
// name) through temporary names first, so that swapped headers ([A,B] -> [B,A]) do not collide
// with a sibling and get uniquified to "B 1". Same scheme as Table::reimportFromSource(). The
// columns are matched by name, so formula references keep the names they use. Returns the names
// the columns had before, for Table::propagateRelabeledColumnNames() once the table has its final
// columns (names that are gone then were real renames).
static QStringList renameImportedColumnsViaTemp(Table *table, const QVector<QString> &targetNames)
{
    future::Table *ft = table ? table->d_future_table : nullptr;
    if (!ft)
        return QStringList();
    const int n = qMin(targetNames.size(), ft->columnCount());
    const QStringList oldNames = table->colNamesInRange(0, n);
    Table::RenamePropagationBlocker renameBlocker(table);
    int tmpIdx = 0;
    for (int i = 0; i < n; ++i) {
        Column *dst = ft->column(i);
        if (!dst || targetNames[i].isEmpty() || dst->name() == targetNames[i])
            continue;
        QString tmpName;
        do {
            tmpName = QStringLiteral("__tg_import_tmp_%1").arg(tmpIdx++);
        } while (ft->column(tmpName, false) != nullptr || targetNames.contains(tmpName));
        dst->setName(tmpName);
    }
    for (int i = 0; i < n; ++i) {
        Column *dst = ft->column(i);
        if (!dst || targetNames[i].isEmpty() || dst->name() == targetNames[i])
            continue;
        dst->setName(targetNames[i]);
    }
    return oldNames;
}

void ApplicationWindow::importASCII(const QStringList &files, int import_mode,
                                    const QString &local_column_separator, int local_ignored_lines,
                                    bool local_rename_columns, bool local_strip_spaces,
                                    bool local_simplify_spaces, bool local_convert_to_numeric,
                                    QLocale local_numeric_locale)
{
    if (files.isEmpty())
        return;

    // this is very much a special case, and thus is handled completely in its own block
    if (import_mode == ImportASCIIDialog::NewTables) {
        int dx = 0, dy = 0;
        QStringList sorted_files = files;
        sorted_files.sort();

        Table *emptyInitialTable = nullptr;
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
                                if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                                    if (!col->isInvalid(r)) {
                                        untouched = false;
                                        break;
                                    }
                                } else if (!col->textAt(r).isEmpty()) {
                                    untouched = false;
                                    break;
                                }
                            }
                        }
                        if (untouched)
                            emptyInitialTable = cand;
                    }
                }
            }
        }
        Table *firstNewTable = nullptr;
        for (int i = 0; i < sorted_files.size(); i++) {
            Table *w = newTable(sorted_files[i], local_column_separator, local_ignored_lines,
                                local_rename_columns, local_strip_spaces, local_simplify_spaces,
                                local_convert_to_numeric, local_numeric_locale);
            if (!w) {
                const QString storedErr = property("torigraph_last_import_error").toString();
                const QString msg = storedErr.isEmpty()
                        ? tr("Could not import \"%1\". Check file permissions, separator, and text "
                             "format.")
                                  .arg(QFileInfo(sorted_files[i]).fileName())
                        : storedErr;
                setProperty("torigraph_last_import_error", msg);
                if (!isNonInteractiveContext(this))
                    QMessageBox::warning(this, tr("ASCII Import Failed"), msg);
                continue;
            }
            if (!firstNewTable) {
                firstNewTable = w;
                if (emptyInitialTable && emptyInitialTable != w) {
                    emptyInitialTable->askOnCloseEvent(false);
                    if (emptyInitialTable->d_future_table)
                        emptyInitialTable->d_future_table->remove();
                    emptyInitialTable = nullptr;
                }
            }
            const QFileInfo impFi(sorted_files[i]);
            QString baseTitle =
                    impFi.completeBaseName().isEmpty() ? tr("Table") : impFi.completeBaseName();
            baseTitle.replace(QRegExp(QStringLiteral("\\s+")), QStringLiteral("_"));
            const QString uniqName = generateUniqueName(baseTitle);
            const QString prevName = w->objectName();
            w->setName(uniqName);
            renameListViewItem(prevName, uniqName);
            w->setWindowLabel(sorted_files[i]);
            w->setCaptionPolicy(MyWidget::Both);
            setListViewLabel(w->name(), sorted_files[i]);
            if (i == 0) {
                dx = w->verticalHeaderWidth();
                dy = w->frameGeometry().height() - w->height();
                w->move(QPoint(0, 0));
                offerPlotAfterImport(w);
            } else
                w->move(QPoint(i * dx, i * dy));
        }
        modifiedProject();
        return;
    }

    Table *table = qobject_cast<Table *>(d_workspace.currentSubWindow());
    if (!table)
        return;

    // Files already copied into the worksheet. If a later file is refused (type mismatch),
    // the import stops there, but what was imported so far must still mark the project as
    // modified (save prompt, auto-save) and refresh the graphs that use the table.
    QStringList importedFiles;
    const QString importMacroLabel = (import_mode == ImportASCIIDialog::NewColumns)
            ? tr("Import ASCII (New Columns)")
            : (import_mode == ImportASCIIDialog::NewRows)
            ? tr("Import ASCII (New Rows)")
            : (import_mode == ImportASCIIDialog::Overwrite) ? tr("Import ASCII (Overwrite)")
                                                            : tr("Import ASCII");
    struct DeferredTableMacroGuard
    {
        future::Table *tbl = nullptr;
        bool active = false;
        void ensureBegin(future::Table *t, const QString &text)
        {
            if (active || !t)
                return;
            tbl = t;
            tbl->beginMacro(text);
            active = true;
        }
        void finish()
        {
            if (active && tbl)
                tbl->endMacro();
            active = false;
            tbl = nullptr;
        }
        ~DeferredTableMacroGuard() { finish(); }
    } importMacro;

    auto stopAfterPartialImport = [this, table, &importedFiles, &importMacro]() {
        if (importedFiles.isEmpty()) {
            importMacro.finish();
            return;
        }
        table->setWindowLabel(importedFiles.join("; "));
        importMacro.finish();
        table->notifyChanges();
        modifiedProject(table);
        modifiedProject();
    };

    foreach (QString file, files) {
        // The file is read into a bare future::Table (same filter and options as a new
        // worksheet). A temporary worksheet window was used before; deleting that window
        // crashed after the data had been copied.
        std::unique_ptr<future::Table> tempOwner;
        QString openErr;
        {
            AsciiTableImportFilter filter;
            filter.set_ignored_lines(local_ignored_lines);
            filter.set_separator(local_column_separator);
            filter.set_first_row_names_columns(local_rename_columns);
            filter.set_trim_whitespace(local_strip_spaces);
            filter.set_simplify_whitespace(local_simplify_spaces);
            filter.set_convert_to_numeric(local_convert_to_numeric);
            filter.set_numeric_locale(local_numeric_locale);
            QFile in(file);
            if (!in.open(QIODevice::ReadOnly))
                openErr = in.errorString();
            else
                tempOwner.reset(static_cast<future::Table *>(filter.importAspect(in)));
        }
        future::Table *temp = tempOwner.get();
        // An empty file (0 bytes, blanks or '#' comments only) parses into a table without
        // columns; overwriting with it would delete the imported columns of the worksheet.
        if (!temp || temp->columnCount() == 0) {
            // A file refused for its size (a few lines with a huge number of fields) says why.
            const QString refusal =
                    openErr.isEmpty() ? AsciiTableImportFilter::lastImportReport().refusal
                                      : QString();
            const QString msg = !openErr.isEmpty()
                    ? tr("Could not open \"%1\": %2").arg(QFileInfo(file).fileName(), openErr)
                    : !refusal.isEmpty()
                    ? QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), refusal)
                    : tr("Could not parse any columns from \"%1\".").arg(QFileInfo(file).fileName());
            setProperty("torigraph_last_import_error", msg);
            if (!isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("ASCII Import Failed"), msg);
            continue;
        }

        auto computeTableImportedSpan = [](Table *t) -> int {
            if (!t || !t->d_future_table)
                return t ? t->columnCount() : 0;
            const int totalCols = t->columnCount();
            int span = t->d_future_table->importSourceColumnCount();
            if (span < 0)
                span = totalCols;
            span = qBound(0, span, totalCols);
            while (span > 0) {
                Column *c = t->column(span - 1);
                if (!c || c->formulaIntervals().isEmpty())
                    break;
                --span;
            }
            return span;
        };

        // Check per-column data type compatibility for append/overwrite (allowing Text columns in
        // numeric-converting imports so "Sample Name (Text) + Value (Numeric)" tables work cleanly).
        QVector<int> overwriteEmptyColsToConvert;
        if (import_mode == ImportASCIIDialog::NewRows
            || import_mode == ImportASCIIDialog::Overwrite) {
            const int checkColSpan = (import_mode == ImportASCIIDialog::Overwrite)
                    ? qMin(temp->columnCount(), computeTableImportedSpan(table))
                    : qMin(temp->columnCount(), table->columnCount());
            if (import_mode == ImportASCIIDialog::Overwrite) {
                for (int col = 0; col < checkColSpan; col++) {
                    Column *src_col = temp->column(col);
                    Column *dst_col = table->column(col);
                    if (!src_col || !dst_col || src_col->dataType() == dst_col->dataType())
                        continue;
                    bool dstAllEmpty = true;
                    for (int r = 0; r < dst_col->rowCount(); ++r) {
                        bool emptyCell = true;
                        switch (dst_col->dataType()) {
                        case SciDAVis::TypeDouble:
                            emptyCell = dst_col->isInvalid(r);
                            break;
                        case SciDAVis::TypeQDateTime:
                            emptyCell = dst_col->isInvalid(r) || !dst_col->dateTimeAt(r).isValid();
                            break;
                        case SciDAVis::TypeQString:
                            emptyCell = dst_col->isInvalid(r) || dst_col->textAt(r).isEmpty();
                            break;
                        }
                        if (!emptyCell) {
                            dstAllEmpty = false;
                            break;
                        }
                    }
                    if (dstAllEmpty)
                        overwriteEmptyColsToConvert.append(col);
                }
            }

            // Column::copy() refuses to copy between types: check before touching the table.
            for (int col = 0; col < checkColSpan; col++) {
                Column *src_col = temp->column(col);
                Column *dst_col = table->column(col);
                if (src_col && dst_col && src_col->dataType() != dst_col->dataType()
                    && !overwriteEmptyColsToConvert.contains(col)) {
                    const QString msg =
                            tr("Column \"%1\" of the file has a different data type (%2) than "
                               "column \"%3\" of the worksheet (%4). Nothing was imported.")
                                    .arg(src_col->name(),
                                         src_col->columnMode() == SciDAVis::ColumnMode::Numeric
                                                 ? tr("numeric")
                                                 : tr("text/date"),
                                         dst_col->name(),
                                         dst_col->columnMode() == SciDAVis::ColumnMode::Numeric
                                                 ? tr("numeric")
                                                 : tr("text/date"));
                    setProperty("torigraph_last_import_error", msg);
                    if (!isNonInteractiveContext(this))
                        QMessageBox::critical(this, tr("ASCII Import Failed"), msg);
                    stopAfterPartialImport();
                    return;
                }
            }
        }

        // Long Name / units travel with the data: overwrite replaces them, append only fills
        // in what the worksheet column does not have yet.
        auto copyColumnMetadata = [](Column *src_col, Column *dst_col, bool overwrite) {
            if (!src_col->longName().isEmpty() && (overwrite || dst_col->longName().isEmpty()))
                dst_col->setLongName(src_col->longName());
            if (!src_col->units().isEmpty() && (overwrite || dst_col->units().isEmpty()))
                dst_col->setUnits(src_col->units());
        };
        QStringList failedColumns;

        // copy or move data from temp to table
        switch (import_mode) {
        case ImportASCIIDialog::NewColumns: {
            importMacro.ensureBegin(table->d_future_table, importMacroLabel);

            for (int col = 0; col < temp->columnCount(); ++col) {
                Column *src = temp->column(col);
                if (!src)
                    continue;
                Column *new_col = new Column(src->name(), src->columnMode());
                new_col->copy(src);
                new_col->setPlotDesignation(src->plotDesignation());
                new_col->setLongName(src->longName());
                new_col->setUnits(src->units());
                new_col->setComment(src->comment());
                for (const Interval<int> &iv : src->formulaIntervals())
                    new_col->setFormula(iv, src->formula(iv.start()));
                if (auto *srcOut = dynamic_cast<Double2StringFilter *>(src->outputFilter())) {
                    if (auto *dstOut = dynamic_cast<Double2StringFilter *>(new_col->outputFilter())) {
                        dstOut->setNumericFormat(srcOut->numericFormat());
                        dstOut->setNumDigits(srcOut->numDigits());
                    }
                }
                if (auto *srcDt = dynamic_cast<DateTime2StringFilter *>(src->outputFilter())) {
                    if (auto *dstDt = dynamic_cast<DateTime2StringFilter *>(new_col->outputFilter()))
                        dstDt->setFormat(srcDt->format());
                }
                table->d_future_table->addChild(new_col);
            }
            break;
        }
        case ImportASCIIDialog::NewRows: {
            importMacro.ensureBegin(table->d_future_table, importMacroLabel);
            // every name the table had before the import (before the columns added below)
            const QStringList namesBeforeImport = table->colNames();
            const int existing_columns = table->columnCount();
            int missing_columns = temp->columnCount() - existing_columns;
            for (int col = 0; col < missing_columns; col++) {
                // same type as the imported column, so that the copy below cannot fail
                Column *new_col =
                        new Column(tr("new_by_import") + QString::number(col + 1),
                                   temp->column(existing_columns + col)->columnMode());
                new_col->setPlotDesignation(SciDAVis::Y);
                table->d_future_table->addChild(new_col);
            }
            Q_ASSERT(table->columnCount() >= temp->columnCount());
            int start_row = table->rowCount();
            table->d_future_table->setRowCount(table->rowCount() + temp->rowCount());
            QVector<QString> targetNames(temp->columnCount());
            for (int col = 0; col < temp->columnCount(); col++) {
                Column *src_col = temp->column(col);
                Column *dst_col = table->column(col);
                if (!dst_col->copy(src_col, 0, start_row, src_col->rowCount())) {
                    failedColumns << dst_col->name();
                    continue;
                }
                copyColumnMetadata(src_col, dst_col, false);
                if (local_rename_columns)
                    targetNames[col] = src_col->name();
            }
            table->propagateRelabeledColumnNames(renameImportedColumnsViaTemp(table, targetNames),
                                                 namesBeforeImport);
            break;
        }
        case ImportASCIIDialog::Overwrite: {
            importMacro.ensureBegin(table->d_future_table, importMacroLabel);

            for (int col : overwriteEmptyColsToConvert) {
                Column *src_col = temp->column(col);
                Column *dst_col = table->column(col);
                if (src_col && dst_col)
                    dst_col->setColumnMode(src_col->columnMode());
            }

            // every name the table had before the import, the removed columns included
            const QStringList namesBeforeImport = table->colNames();
            const int prevRows = table->rowCount();
            const int newRows = temp->rowCount();
            const int importedSpan = computeTableImportedSpan(table);
            const int common = qMin(importedSpan, temp->columnCount());

            if (prevRows != newRows)
                table->d_future_table->setRowCount(newRows);
            QVector<QString> targetNames(common);
            for (int col = 0; col < common; col++) {
                Column *src_col = temp->column(col);
                Column *dst_col = table->column(col);
                dst_col->clearFormulas();
                if (!dst_col->copy(src_col, 0, 0, newRows)) {
                    failedColumns << dst_col->name();
                    continue;
                }
                copyColumnMetadata(src_col, dst_col, true);
                if (local_rename_columns)
                    targetNames[col] = src_col->name();
            }
            // Drop the surplus imported columns before renaming: a header that moved left
            // (e.g. [A,B,C] overwritten by [C,A]) would otherwise collide with the old column
            // "C" that is about to be removed and come out as "C 1".
            if (importedSpan > temp->columnCount())
                table->d_future_table->removeColumns(temp->columnCount(),
                                                     importedSpan - temp->columnCount());
            const QStringList oldNames = renameImportedColumnsViaTemp(table, targetNames);
            if (temp->columnCount() > common) {
                QList<Column *> extraCols;
                extraCols.reserve(temp->columnCount() - common);
                for (int col = common; col < temp->columnCount(); ++col) {
                    Column *src = temp->column(col);
                    if (!src)
                        continue;
                    Column *cp = new Column(src->name(), src->columnMode());
                    cp->copy(src);
                    cp->setPlotDesignation(src->plotDesignation());
                    cp->setLongName(src->longName());
                    cp->setUnits(src->units());
                    cp->setComment(src->comment());
                    if (auto *srcOut = dynamic_cast<Double2StringFilter *>(src->outputFilter())) {
                        if (auto *dstOut = dynamic_cast<Double2StringFilter *>(cp->outputFilter())) {
                            dstOut->setNumericFormat(srcOut->numericFormat());
                            dstOut->setNumDigits(srcOut->numDigits());
                        }
                    }
                    extraCols.append(cp);
                }
                if (!extraCols.isEmpty())
                    table->d_future_table->insertColumns(common, extraCols);
            }
            // after the extra columns: a name one of them carries is still in the table
            table->propagateRelabeledColumnNames(oldNames, namesBeforeImport);
            if (table->d_future_table) {
                table->d_future_table->setImportSource(
                        file, QString(), local_column_separator,
                        AsciiTableImportFilter::dialogDefaultEncoding(), local_ignored_lines,
                        AsciiTableImportFilter::dialogDefaultUnitsRow(), temp->columnCount(),
                        future::Table::numericLocaleNameForImport(local_numeric_locale));
            }
            for (int c = temp->columnCount(); c < table->columnCount(); ++c) {
                Column *uc = table->column(c);
                if (!uc)
                    continue;
                const QList<Interval<int>> fxIntervals = uc->formulaIntervals();
                if (fxIntervals.isEmpty())
                    continue;
                if (fxIntervals.size() == 1 && fxIntervals.at(0).start() == 0
                    && (prevRows <= 0 || fxIntervals.at(0).end() >= prevRows - 1) && newRows > 0
                    && fxIntervals.at(0).end() != newRows - 1) {
                    uc->setFormula(Interval<int>(0, newRows - 1), uc->formula(0));
                }
                if (table->autoRecalcFormulas())
                    table->recalculate(c, false);
            }
            break;
        }
        }
        if (!failedColumns.isEmpty()) {
            const QString msg = tr("These columns could not be imported from %1: %2")
                                        .arg(QFileInfo(file).fileName(),
                                             failedColumns.join(QStringLiteral(", ")));
            setProperty("torigraph_last_import_error", msg);
            if (!isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("ASCII Import"), msg);
        }
        importedFiles << file;
    }

    if (importedFiles.isEmpty()) {
        importMacro.finish();
        return;
    }

    table->setWindowLabel(importedFiles.join("; "));
    importMacro.finish();
    table->notifyChanges();
    modifiedProject(table);
    modifiedProject();
    offerPlotAfterImport(table);
}

void ApplicationWindow::reimportActiveTable()
{
    Table *tbl = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (!tbl || !tbl->hasImportSource()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::information(
                    this, tr("Re-import Data"),
                    tr("The active worksheet does not have a recorded import source file."));
        }
        return;
    }

    const bool ok = tbl->reimportFromSource();
    if (statusBar()) {
        statusBar()->showMessage(ok ? tr("Re-imported source data into %1").arg(tbl->name())
                                    : tr("Failed to re-import data into %1").arg(tbl->name()),
                                 8000);
    }
    if (ok) {
        tbl->notifyChanges();
        modifiedProject(tbl);
        modifiedProject();
    } else if (!isNonInteractiveContext(this)) {
        QMessageBox::warning(
                this, tr("Re-import Failed"),
                tr("Could not re-import data for worksheet \"%1\". Verify that the original file "
                   "still exists and matches the column structure.")
                        .arg(tbl->name()));
    }
}

namespace {

bool isRecognizedImageWriterFormat(const QString &extLower)
{
    if (extLower.isEmpty())
        return false;
    const QList<QByteArray> list = QImageWriter::supportedImageFormats();
    for (const QByteArray &fmt : list) {
        if (extLower == QString::fromLatin1(fmt).toLower())
            return true;
    }
    return false;
}

bool isRecognizedExportSuffix(const QString &extLower)
{
    if (extLower.isEmpty())
        return false;
    if (extLower == QLatin1String("emf") || extLower == QLatin1String("svg")
        || extLower == QLatin1String("pdf") || extLower == QLatin1String("eps")
        || extLower == QLatin1String("ps") || extLower == QLatin1String("tif")
        || extLower == QLatin1String("tiff") || extLower == QLatin1String("png")
        || extLower == QLatin1String("jpg") || extLower == QLatin1String("jpeg")
        || extLower == QLatin1String("bmp")) {
        return true;
    }
    return isRecognizedImageWriterFormat(extLower);
}

QString exportSuffixFromFilter(const QString &selectedFilter)
{
    QString norm = ImageExportDialog::normalizedFilter(selectedFilter).trimmed();
    norm.remove(QLatin1Char('*'));
    if (norm.isEmpty())
        norm = QStringLiteral(".png");
    if (!norm.startsWith(QLatin1Char('.')))
        norm.prepend(QLatin1Char('.'));
    return norm.toLower();
}

QString ensureExportSuffix(const QString &rawFileName, const QString &selectedFilter,
                           bool *outAppended = nullptr)
{
    if (outAppended)
        *outAppended = false;
    QString out = rawFileName;
    const QString curExt = QFileInfo(out).suffix().toLower();
    if (!isRecognizedExportSuffix(curExt)) {
        const QString sfx = exportSuffixFromFilter(selectedFilter);
        if (!sfx.isEmpty()) {
            out.append(sfx);
            if (outAppended)
                *outAppended = true;
        }
    }
    return out;
}

bool probeWritableNoTruncate(const QString &filePath)
{
    const QFileInfo fi(filePath);
    if (fi.filePath().trimmed().isEmpty() || fi.isDir())
        return false;
    if (fi.exists()) {
        QFile existing(filePath);
        if (!existing.open(QIODevice::ReadWrite))
            return false;
        existing.close();
        return true;
    }
    QString cur = QDir::cleanPath(fi.absolutePath());
    QString probeDirPath;
    for (int guard = 0; guard < 256 && !cur.isEmpty(); ++guard) {
        const QFileInfo curFi(cur);
        if (curFi.exists()) {
            if (!curFi.isDir())
                return false;
            probeDirPath = cur;
            break;
        }
        const QString next = QDir::cleanPath(QFileInfo(cur).path());
        if (next == cur || next.isEmpty())
            return false;
        cur = next;
    }
    if (probeDirPath.isEmpty())
        return false;
    QTemporaryFile probe(QDir(probeDirPath).filePath(QStringLiteral(".torigraph_export_probe_XXXXXX")));
    probe.setAutoRemove(true);
    if (!probe.open())
        return false;
    probe.close();
    return true;
}

QString makeShortExportTempPath(const QFileInfo &dstInfo, const QString &extLower)
{
    QString cleanExt = extLower.trimmed().toLower();
    while (cleanExt.startsWith(QLatin1Char('.')))
        cleanExt.remove(0, 1);
    if (cleanExt.isEmpty())
        cleanExt = QFileInfo(dstInfo.filePath()).suffix().trimmed().toLower();
    const QDir dir = dstInfo.dir();
    for (int attempt = 0; attempt < 16; ++attempt) {
        const QString hex8 =
                QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toLower();
        const QString fileName = cleanExt.isEmpty()
                ? QStringLiteral(".tgexp_%1.tmp").arg(hex8)
                : QStringLiteral(".tgexp_%1.%2").arg(hex8, cleanExt);
        const QString candidate = dir.filePath(fileName);
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    const QString hex8 = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toLower();
    return dir.filePath(cleanExt.isEmpty()
                                ? QStringLiteral(".tgexp_%1.tmp").arg(hex8)
                                : QStringLiteral(".tgexp_%1.%2").arg(hex8, cleanExt));
}

template <typename WriterFn>
bool executeSafeExportWrite(const QString &finalTargetPath, const QString &extLower,
                            WriterFn &&writerFn)
{
    return ApplicationWindow::safeExportWriteTransaction(
            finalTargetPath, extLower, std::forward<WriterFn>(writerFn));
}

} // namespace

QString ApplicationWindow::resolveImageExportFileName(const QString &rawFileName,
                                                      const QString &selectedFilter,
                                                      bool *outAppended)
{
    return ensureExportSuffix(rawFileName, selectedFilter, outAppended);
}

bool ApplicationWindow::probeExportWritableNoTruncate(const QString &filePath)
{
    return probeWritableNoTruncate(filePath);
}

bool ApplicationWindow::commitExportTempFile(const QString &tmpPath,
                                             const QString &finalTargetPath)
{
    const QFileInfo tmpInfo(tmpPath);
    if (!tmpInfo.exists() || !tmpInfo.isFile() || tmpInfo.size() <= 0) {
        if (tmpInfo.exists())
            QFile::remove(tmpPath);
        return false;
    }
    const QFileInfo dstInfo(finalTargetPath);
    if (finalTargetPath.trimmed().isEmpty() || dstInfo.isDir()) {
        QFile::remove(tmpPath);
        return false;
    }
    if (dstInfo.exists() && !tmpInfo.canonicalFilePath().isEmpty()
        && tmpInfo.canonicalFilePath() == dstInfo.canonicalFilePath()) {
        return true;
    }

    if (!QFileInfo::exists(finalTargetPath)) {
#ifdef Q_OS_WIN
        const std::wstring wTmp = QDir::toNativeSeparators(tmpPath).toStdWString();
        const std::wstring wDst = QDir::toNativeSeparators(finalTargetPath).toStdWString();
        if (MoveFileExW(wTmp.c_str(), wDst.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
#endif
        if (QFile::rename(tmpPath, finalTargetPath))
            return true;
        if (!QFileInfo::exists(finalTargetPath)) {
            QFile::remove(tmpPath);
            return false;
        }
    }

    QString bakPath = finalTargetPath + QStringLiteral(".tg_bak");
    if (QFileInfo::exists(bakPath)) {
        const QString hex8 =
                QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toLower();
        bakPath = finalTargetPath + QStringLiteral(".tg_bak_") + hex8;
    }

#ifdef Q_OS_WIN
    {
        const std::wstring wTmp = QDir::toNativeSeparators(tmpPath).toStdWString();
        const std::wstring wDst = QDir::toNativeSeparators(finalTargetPath).toStdWString();
        const std::wstring wBak = QDir::toNativeSeparators(bakPath).toStdWString();
        for (int attempt = 0; attempt < 5; ++attempt) {
            if (MoveFileExW(wTmp.c_str(), wDst.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                return true;
            }
            if (ReplaceFileW(wDst.c_str(), wTmp.c_str(), wBak.c_str(),
                             REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
                DeleteFileW(wBak.c_str());
                return true;
            }
            const DWORD err = GetLastError();
            if (!QFileInfo::exists(finalTargetPath) && QFileInfo::exists(bakPath))
                QFile::rename(bakPath, finalTargetPath);
            if (err != ERROR_SHARING_VIOLATION && err != ERROR_LOCK_VIOLATION
                && err != ERROR_ACCESS_DENIED) {
                break;
            }
            Sleep(20);
        }
    }
#endif

    // Safe two-step backup rename fallback: never remove finalTargetPath before replacement!
    if (!QFile::rename(finalTargetPath, bakPath)) {
        QFile::remove(tmpPath);
        return false;
    }
    if (!QFile::rename(tmpPath, finalTargetPath)) {
        const bool restored = QFile::rename(bakPath, finalTargetPath);
        if (restored || QFileInfo::exists(finalTargetPath))
            QFile::remove(tmpPath);
        return false;
    }
    QFile::remove(bakPath);
    return true;
}

bool ApplicationWindow::safeExportWriteTransaction(
        const QString &finalTargetPath, const QString &extLower,
        const std::function<bool(const QString &writePath)> &writerFn)
{
    if (!writerFn || finalTargetPath.trimmed().isEmpty())
        return false;
    const QFileInfo dstInfo(finalTargetPath);
    if (dstInfo.isDir() || !dstInfo.dir().exists())
        return false;

    const QString tmpPath = makeShortExportTempPath(dstInfo, extLower);
    const bool handled = writerFn(tmpPath);
    const QFileInfo tmpInfo(tmpPath);
    if (!handled || !tmpInfo.exists() || !tmpInfo.isFile() || tmpInfo.size() <= 0) {
        if (tmpInfo.exists())
            QFile::remove(tmpPath);
        return false;
    }
    return commitExportTempFile(tmpPath, finalTargetPath);
}

static QString sanitizeExportStem(const QString &rawName)
{
    QString s = rawName.trimmed();
    static const QRegularExpression rxBad(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]+"));
    s.replace(rxBad, QStringLiteral("_"));
    while (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s.isEmpty() ? QStringLiteral("Graph") : s;
}

static bool exportGraph3DImageWithDpi(Graph3D *plot3D, const QString &writePath, int quality,
                                      bool transparent, int dpi, QString *errMsg)
{
    if (!plot3D)
        return false;
    const QVariant prevDpi = plot3D->property("export_dpi");
    if (dpi > 0)
        plot3D->setProperty("export_dpi", dpi);
    const bool ok = plot3D->exportImageChecked(writePath, quality, transparent, errMsg);
    plot3D->setProperty("export_dpi", prevDpi);
    return ok;
}

void ApplicationWindow::exportGraph()
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !(w->inherits("MultiLayer") || w->inherits("Graph3D"))) {
        showUxHint(tr("Graph export works on a graph window. Click the graph you want to "
                      "export first."));
        return;
    }

    MultiLayer *plot2D = qobject_cast<MultiLayer *>(w);
    Graph3D *plot3D = qobject_cast<Graph3D *>(w);
    if (plot2D) {
        if (plot2D->isEmpty()) {
            QMessageBox::critical(
                    this, tr("Export Error"),
                    tr("<h4>There are no plot layers available in this window!</h4>"));
            return;
        }
    } else if (!plot3D) {
        return;
    }

    QPointer<MultiLayer> safePlot2D(plot2D);
    QPointer<Graph3D> safePlot3D(plot3D);
    QScopedPointer<ImageExportDialog> ied(
            new ImageExportDialog(this, safePlot2D != nullptr, d_extended_export_dialog));
    ied->setDirectory(workingDir);
    ied->selectFile(sanitizeExportStem(safePlot2D ? QString(safePlot2D->name())
                                                  : QString(safePlot3D->name())));
    ied->selectFilter(d_image_export_filter);
    if (ied->exec() != QDialog::Accepted)
        return;
    if (!safePlot2D && !safePlot3D)
        return;
    workingDir = ied->directory().path();
    d_image_export_filter = ied->selectedNameFilter();
    if (ied->selectedFiles().isEmpty())
        return;

    const QString selected_filter = ied->selectedNameFilter();
    bool suffixAppended = false;
    QString file_name =
            ensureExportSuffix(ied->selectedFiles()[0], selected_filter, &suffixAppended);
    QString ext = QFileInfo(file_name).suffix().toLower();
    if (!isRecognizedExportSuffix(ext)) {
        QMessageBox::critical(
                this, tr("Export Error"),
                tr("Unsupported image or vector format: <b>%1</b>").arg(file_name));
        return;
    }

#if QT_VERSION >= 0x050000
    if (safePlot2D && (ext == QLatin1String("eps") || ext == QLatin1String("ps"))) {
        QMessageBox::warning(
                this, tr("Warning"),
                tr("Output in postscript format is not available for Qt5, using PDF"));
        const QFileInfo fi(file_name);
        file_name = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".pdf"));
        ext = QStringLiteral("pdf");
        suffixAppended = true;
    }
#endif

    if (suffixAppended && QFile::exists(file_name)) {
        const auto ans = QMessageBox::question(
                this, tr("Overwrite file?"),
                tr("A file called: <p><b>%1</b><p>already exists. Do you want to overwrite it?")
                        .arg(file_name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return;
    }
    if (!safePlot2D && !safePlot3D)
        return;

    if (!probeWritableNoTruncate(file_name)) {
        QMessageBox::critical(this, tr("Export Error"),
                              tr("Could not write to file: <br><h4> %1 </h4><p>Please verify that "
                                 "you have the right to write to this location!")
                                      .arg(file_name));
        return;
    }

    QString errMsg;
    const bool writeOk = executeSafeExportWrite(file_name, ext, [&](const QString &writePath) {
        if (ext == QLatin1String("emf")) {
            if (safePlot2D)
                return safePlot2D->exportToFileChecked(writePath, &errMsg);
            if (safePlot3D)
                return safePlot3D->exportToFileChecked(writePath, &errMsg);
        } else if (ext == QLatin1String("eps") || ext == QLatin1String("pdf")
                   || ext == QLatin1String("ps")) {
            if (safePlot3D)
                return safePlot3D->exportVectorChecked(writePath, ext, &errMsg);
            if (safePlot2D)
                return safePlot2D->exportVectorChecked(writePath, ied->resolution(), ied->color(),
                                                       ied->keepAspect(), ied->pageSize(),
                                                       ied->pageOrientation(), &errMsg);
        } else if (ext == QLatin1String("svg")) {
            if (safePlot2D)
                return safePlot2D->exportSVGChecked(writePath, &errMsg);
            if (safePlot3D)
                return safePlot3D->exportVectorChecked(writePath, QStringLiteral("svg"), &errMsg);
        } else if (isRecognizedImageWriterFormat(ext)) {
            if (safePlot2D)
                return safePlot2D->exportImageChecked(writePath, ied->quality(),
                                                      ied->transparency(), ied->resolution(),
                                                      &errMsg);
            if (safePlot3D)
                return exportGraph3DImageWithDpi(safePlot3D, writePath, ied->quality(),
                                                 ied->transparency(), ied->resolution(), &errMsg);
        }
        return false;
    });

    if (!writeOk) {
        QMessageBox::critical(this, tr("Export Error"),
                              errMsg.isEmpty()
                                      ? tr("Could not write to file: <br><h4> %1 </h4><p>Please "
                                           "verify that you have the right to write to this "
                                           "location!")
                                                .arg(file_name)
                                      : tr("Could not write to file: <br><h4> %1 </h4><p>%2")
                                                .arg(file_name, errMsg.toHtmlEscaped()));
    } else if (statusBar()) {
        statusBar()->showMessage(
                tr("Exported %1").arg(QDir::toNativeSeparators(file_name)), 4000);
    }
}

void ApplicationWindow::exportLayer()
{
    if (isNonInteractiveContext(this))
        return;
    QPointer<MultiLayer> ml(qobject_cast<MultiLayer *>(d_workspace.activeSubWindow()));
    if (!ml) {
        showUxHint(tr("Graph export works on a graph window. Click the graph you want to "
                      "export first."));
        return;
    }

    QPointer<Graph> g(ml->activeGraph());
    if (!g)
        return;

    QScopedPointer<ImageExportDialog> ied(
            new ImageExportDialog(this, g != nullptr, d_extended_export_dialog));
    ied->setDirectory(workingDir);
    ied->selectFile(sanitizeExportStem(
            QString("%1_L%2").arg(ml->name()).arg(ml->layersList().indexOf(g) + 1)));
    ied->selectFilter(d_image_export_filter);
    if (ied->exec() != QDialog::Accepted)
        return;
    if (!g)
        return;
    workingDir = ied->directory().path();
    d_image_export_filter = ied->selectedNameFilter();
    if (ied->selectedFiles().isEmpty())
        return;

    const QString selected_filter = ied->selectedNameFilter();
    bool suffixAppended = false;
    QString file_name =
            ensureExportSuffix(ied->selectedFiles()[0], selected_filter, &suffixAppended);
    QString ext = QFileInfo(file_name).suffix().toLower();
    if (!isRecognizedExportSuffix(ext)) {
        QMessageBox::critical(
                this, tr("Export Error"),
                tr("Unsupported image or vector format: <b>%1</b>").arg(file_name));
        return;
    }

#if QT_VERSION >= 0x050000
    if (ext == QLatin1String("eps") || ext == QLatin1String("ps")) {
        QMessageBox::warning(
                this, tr("Warning"),
                tr("Output in postscript format is not available for Qt5, using PDF"));
        const QFileInfo fi(file_name);
        file_name = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".pdf"));
        ext = QStringLiteral("pdf");
        suffixAppended = true;
    }
#endif

    if (suffixAppended && QFile::exists(file_name)) {
        const auto ans = QMessageBox::question(
                this, tr("Overwrite file?"),
                tr("A file called: <p><b>%1</b><p>already exists. Do you want to overwrite it?")
                        .arg(file_name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return;
    }
    if (!g)
        return;

    if (!probeWritableNoTruncate(file_name)) {
        QMessageBox::critical(this, tr("Export Error"),
                              tr("Could not write to file: <br><h4> %1 </h4><p>Please verify that "
                                 "you have the right to write to this location!")
                                      .arg(file_name));
        return;
    }

    QString errMsg;
    const bool writeOk = executeSafeExportWrite(file_name, ext, [&](const QString &writePath) {
        if (!g)
            return false;
        if (ext == QLatin1String("emf"))
            return g->exportToFileChecked(writePath, &errMsg);
        if (ext == QLatin1String("eps") || ext == QLatin1String("pdf")
            || ext == QLatin1String("ps")) {
            return g->exportVectorChecked(writePath, ied->resolution(), ied->color(),
                                          ied->keepAspect(), ied->pageSize(),
                                          ied->pageOrientation(), &errMsg);
        }
        if (ext == QLatin1String("svg"))
            return g->exportSVGChecked(writePath, &errMsg);
        if (isRecognizedImageWriterFormat(ext)) {
            return g->exportImageChecked(writePath, ied->quality(), ied->transparency(),
                                         ied->resolution(), &errMsg);
        }
        return false;
    });

    if (!writeOk) {
        QMessageBox::critical(this, tr("Export Error"),
                              errMsg.isEmpty()
                                      ? tr("Could not write to file: <br><h4> %1 </h4><p>Please "
                                           "verify that you have the right to write to this "
                                           "location!")
                                                .arg(file_name)
                                      : tr("Could not write to file: <br><h4> %1 </h4><p>%2")
                                                .arg(file_name, errMsg.toHtmlEscaped()));
    } else if (statusBar()) {
        statusBar()->showMessage(
                tr("Exported %1").arg(QDir::toNativeSeparators(file_name)), 4000);
    }
}

void ApplicationWindow::exportAllGraphs()
{
    if (isNonInteractiveContext(this))
        return;

    QList<QPointer<MyWidget>> exportTargets;
    for (MyWidget *w : windowsList()) {
        if (!w)
            continue;
        if (MultiLayer *m = qobject_cast<MultiLayer *>(w)) {
            if (!m->isEmpty())
                exportTargets << QPointer<MyWidget>(w);
        } else if (qobject_cast<Graph3D *>(w)) {
            exportTargets << QPointer<MyWidget>(w);
        }
    }
    if (exportTargets.isEmpty()) {
        QMessageBox::information(this, tr("Export All Graphs"),
                                 tr("There are no graphs available to export in this project."));
        return;
    }

    QScopedPointer<ImageExportDialog> ied(
            new ImageExportDialog(this, true, d_extended_export_dialog));
    ied->setWindowTitle(tr("Choose a directory to export the graphs to"));
    QStringList tmp = ied->nameFilters();
    ied->setFileMode(QFileDialog::Directory);
    ied->setNameFilters(tmp);
    ied->setLabelText(QFileDialog::FileType, tr("Output format:"));
    ied->setLabelText(QFileDialog::FileName, tr("Directory:"));

    ied->setDirectory(workingDir);
    ied->selectFilter(d_image_export_filter);

    if (ied->exec() != QDialog::Accepted)
        return;
    workingDir = ied->directory().path();
    d_image_export_filter = ied->selectedNameFilter();
    if (ied->selectedFiles().isEmpty())
        return;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    const QString output_dir = ied->selectedFiles()[0];
    const QString file_suffix = exportSuffixFromFilter(ied->selectedNameFilter());
    const QString ext = QString(file_suffix).remove('.').trimmed().toLower();
    if (!isRecognizedExportSuffix(ext)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(
                this, tr("Export Error"),
                tr("Unsupported image or vector format: <b>%1</b>").arg(file_suffix));
        return;
    }

    bool confirm_overwrite = true;
    int exportedCount = 0;
    QStringList failedMessages;

    for (const QPointer<MyWidget> &wGuard : exportTargets) {
        if (!wGuard)
            continue;
        QPointer<MultiLayer> plot2D(qobject_cast<MultiLayer *>(wGuard.data()));
        QPointer<Graph3D> plot3D(qobject_cast<Graph3D *>(wGuard.data()));

        QString effSuffix = file_suffix;
        QString eff_ext = ext;
#if QT_VERSION >= 0x050000
        if (plot2D && (eff_ext == QLatin1String("eps") || eff_ext == QLatin1String("ps"))) {
            effSuffix = QStringLiteral(".pdf");
            eff_ext = QStringLiteral("pdf");
        }
#endif
        const QString file_name =
                QDir(output_dir).filePath(sanitizeExportStem(wGuard->objectName()) + effSuffix);
        if (QFile::exists(file_name) && confirm_overwrite) {
            QApplication::restoreOverrideCursor();
            const int choice = QMessageBox::question(
                    this, tr("Overwrite file?"),
                    tr("A file called: <p><b>%1</b><p>already exists. Do you want to overwrite it?")
                            .arg(file_name),
                    tr("&Yes"), tr("&All"), tr("&Cancel"), 0, 1);
            if (choice == 1)
                confirm_overwrite = false;
            else if (choice == 2)
                return;
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            if (!plot2D && !plot3D)
                continue;
        }

        if (!probeWritableNoTruncate(file_name)) {
            failedMessages << QString("%1: not writable").arg(file_name);
            continue;
        }

        QString errMsg;
        const bool writeOk =
                executeSafeExportWrite(file_name, eff_ext, [&](const QString &writePath) {
                    if (eff_ext == QLatin1String("emf")) {
                        if (plot2D)
                            return plot2D->exportToFileChecked(writePath, &errMsg);
                        if (plot3D)
                            return plot3D->exportToFileChecked(writePath, &errMsg);
                    } else if (eff_ext == QLatin1String("eps") || eff_ext == QLatin1String("pdf")
                               || eff_ext == QLatin1String("ps")) {
                        if (plot3D)
                            return plot3D->exportVectorChecked(writePath, eff_ext, &errMsg);
                        if (plot2D)
                            return plot2D->exportVectorChecked(
                                    writePath, ied->resolution(), ied->color(), ied->keepAspect(),
                                    ied->pageSize(), ied->pageOrientation(), &errMsg);
                    } else if (eff_ext == QLatin1String("svg")) {
                        if (plot2D)
                            return plot2D->exportSVGChecked(writePath, &errMsg);
                        if (plot3D)
                            return plot3D->exportVectorChecked(writePath, QStringLiteral("svg"),
                                                               &errMsg);
                    } else if (isRecognizedImageWriterFormat(eff_ext)) {
                        if (plot2D)
                            return plot2D->exportImageChecked(writePath, ied->quality(),
                                                              ied->transparency(),
                                                              ied->resolution(), &errMsg);
                        if (plot3D)
                            return exportGraph3DImageWithDpi(plot3D, writePath, ied->quality(),
                                                             ied->transparency(), ied->resolution(),
                                                             &errMsg);
                    }
                    return false;
                });
        if (writeOk) {
            ++exportedCount;
        } else {
            failedMessages << (errMsg.isEmpty() ? file_name
                                                : QString("%1 (%2)").arg(file_name, errMsg));
        }
    }

    QApplication::restoreOverrideCursor();
    if (!failedMessages.isEmpty()) {
        QMessageBox::warning(
                this, tr("Export Warnings"),
                tr("Exported %1 graph(s), %2 failed:\n%3")
                        .arg(exportedCount)
                        .arg(failedMessages.size())
                        .arg(failedMessages.join(QLatin1Char('\n'))));
    } else if (statusBar()) {
        statusBar()->showMessage(
                tr("Exported %1 graph(s) to %2")
                        .arg(exportedCount)
                        .arg(QDir::toNativeSeparators(output_dir)),
                4000);
    }
}

void ApplicationWindow::saveNoteAs()
{
    if (isNonInteractiveContext(this))
        return;
    Note *w = qobject_cast<Note *>(d_workspace.activeSubWindow());
    if (!w)
        return;
    w->exportASCII();
}

// TODO: string list -> Column * list
QStringList ApplicationWindow::columnsList(SciDAVis::PlotDesignation plotType)
{
    QList<MyWidget *> windows = windowsList();
    QStringList list;
    foreach (MyWidget *w, windows) {
        if (!w->inherits("Table"))
            continue;

        Table *t = (Table *)w;
        for (int i = 0; i < t->numCols(); i++) {
            if (t->colPlotDesignation(i) == plotType)
                list << QString(t->name()) + "_" + t->colLabel(i);
        }
    }

    return list;
}

// TODO: string list -> Column * list
QStringList ApplicationWindow::columnsList()
{
    QList<MyWidget *> windows = windowsList();
    QStringList list;
    foreach (MyWidget *w, windows) {
        if (!w->inherits("Table"))
            continue;

        Table *t = (Table *)w;
        for (int i = 0; i < t->numCols(); i++) {
            list << QString(t->name()) + "_" + t->colLabel(i);
        }
    }

    return list;
}

QList<MyWidget *> *ApplicationWindow::tableList()
{
    QList<MyWidget *> *lst = new QList<MyWidget *>();
    foreach (MyWidget *w, windowsList()) {
        if (w->inherits("Table"))
            lst->append(w);
    }
    return lst;
}

void ApplicationWindow::showExportASCIIDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Table *table = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (table) {
        ExportDialog *ed = new ExportDialog(this, Qt::WindowContextHelpButtonHint);
        ed->setAttribute(Qt::WA_DeleteOnClose);
        connect(ed, SIGNAL(exportTable(const QString &, const QString &, bool, bool)), this,
                SLOT(exportASCII(const QString &, const QString &, bool, bool)));
        connect(ed, SIGNAL(exportAllTables(const QString &, bool, bool)), this,
                SLOT(exportAllTables(const QString &, bool, bool)));

        ed->setTableNames(tableWindows());
        ed->setActiveTableName(table->name());
        ed->setColumnSeparator(columnSeparator);
        ed->exec();
    }
}

void ApplicationWindow::exportAllTables(const QString &sep, bool colNames, bool expSelection)
{
    if (isNonInteractiveContext(this))
        return;
    QString dir = QFileDialog::getExistingDirectory(
            this, tr("Choose a directory to export the tables to"), workingDir,
            QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty()) {
        workingDir = dir;
        QString failedName, error;
        if (!exportTablesToDirectory(dir, sep, colNames, expSelection, &failedName, &error)
            && !failedName.isEmpty() && !isNonInteractiveContext(this))
            statusBar()->showMessage(tr("Export stopped at table %1: %2").arg(failedName, error),
                                     15000);
    }
}

bool ApplicationWindow::exportTablesToDirectory(const QString &dir, const QString &sep,
                                                bool colNames, bool expSelection,
                                                QString *failedTable, QString *error)
{
    if (failedTable)
        failedTable->clear();
    if (error)
        error->clear();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    auto &settings = getSettings();
    Table::AsciiExportOptions opts;
    opts.utf8Bom = settings.value("/General/ExportASCII/UTF8BOM", true).toBool();
    opts.longNameAndUnits = settings.value("/General/ExportASCII/LongNameUnits", true).toBool();
    // comma/semicolon separated files get .csv (Excel opens them directly), others .txt
    const QString trimmedSep = sep.trimmed();
    const QString ext = (trimmedSep == QLatin1String(",") || trimmedSep == QLatin1String(";"))
            ? QStringLiteral(".csv")
            : QStringLiteral(".txt");

    bool confirmOverwrite = true;
    QList<QPointer<Table>> tables;
    for (MyWidget *w : windowsList()) {
        if (auto *t = qobject_cast<Table *>(w))
            tables.append(t);
    }
    for (const QPointer<Table> &t : tables) {
        if (!t)
            continue;
        // "Selection only" is remembered in the settings, so with "All tables" it is usually
        // on while most tables have nothing selected. Such a table is exported whole instead
        // of failing with "no cells are selected" and stopping the export of the rest.
        bool tableSelection = false;
        if (expSelection) {
            const QList<int> selCols = t->selectedColumnIndices();
            const int topRow = t->firstSelectedRow();
            tableSelection = !selCols.isEmpty() && topRow >= 0 && t->lastSelectedRow() >= topRow;
        }
        QString safeName = t->name();
        safeName.replace(QRegExp(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
        QString fileName = dir + "/" + safeName + ext;
        bool success = true;
        if (QFile::exists(fileName) && confirmOverwrite && !isNonInteractiveContext(this)) {
            QApplication::restoreOverrideCursor();
            switch (QMessageBox::question(this, tr("Overwrite file?"),
                                          tr("A file called: <p><b>%1</b><p>already exists. "
                                             "Do you want to overwrite it?")
                                                  .arg(fileName),
                                          tr("&Yes"), tr("&All"), tr("&Cancel"), 0, 1)) {
            case 0:
                if (!t) {
                    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
                    continue;
                }
                success = t->exportASCII(fileName, sep, colNames, tableSelection, opts);
                break;

            case 1:
                confirmOverwrite = false;
                if (!t) {
                    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
                    continue;
                }
                success = t->exportASCII(fileName, sep, colNames, tableSelection, opts);
                break;

            default:
                return false;
            }
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        } else {
            if (!t)
                continue;
            success = t->exportASCII(fileName, sep, colNames, tableSelection, opts);
        }

        if (!success) {
            QApplication::restoreOverrideCursor();
            if (failedTable)
                *failedTable = t ? t->name() : QString();
            if (error)
                *error = t ? t->lastExportError() : QString();
            return false;
        }
    }
    QApplication::restoreOverrideCursor();
    return true;
}

void ApplicationWindow::exportASCII(const QString &tableName, const QString &sep, bool colNames,
                                    bool expSelection)
{
    if (isNonInteractiveContext(this))
        return;
    QPointer<Table> t = table(tableName);
    if (!t)
        return;

    if (expSelection && t->selectedColumns().isEmpty()) {
        QMessageBox::warning(
                this, tr("Export Error"),
                tr("No cells or columns are selected to export. Select columns or cells in \"%1\" "
                   "or uncheck \"Export selection\".")
                        .arg(t->name()));
        return;
    }

    const QString trimmedSep = sep.trimmed();
    const bool preferCsv = (trimmedSep == QLatin1String(",") || trimmedSep == QLatin1String(";"));
    const QString defaultExt = preferCsv ? QStringLiteral(".csv") : QStringLiteral(".txt");
    const QString filter = preferCsv
            ? QStringLiteral("*.csv;;*.txt;;*.dat;;*.DAT")
            : QStringLiteral("*.txt;;*.csv;;*.dat;;*.DAT");
    QString safeDefaultName = t->name();
    safeDefaultName.replace(QRegExp(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString suggestedPath = asciiDirPath + QLatin1Char('/') + safeDefaultName + defaultExt;

    QString selectedFilter;
    QString fname = QFileDialog::getSaveFileName(this, tr("Choose a filename to save under"),
                                                 suggestedPath, filter, &selectedFilter);
    if (!fname.isEmpty() && t) {
        QString filterExt = selectedFilter.trimmed();
        filterExt.remove(QLatin1Char('*'));
        if (!filterExt.startsWith(QLatin1Char('.')))
            filterExt = defaultExt;
        if (!fname.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)
            && !fname.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive)
            && !fname.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive)) {
            fname.append(filterExt);
        }

        QFileInfo fi(fname);
        asciiDirPath = fi.absolutePath();

        auto &settings = getSettings();
        Table::AsciiExportOptions opts;
        opts.utf8Bom = settings.value("/General/ExportASCII/UTF8BOM", true).toBool();
        opts.longNameAndUnits = settings.value("/General/ExportASCII/LongNameUnits", true).toBool();

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        const bool ok = t->exportASCII(fname, sep, colNames, expSelection, opts);
        QApplication::restoreOverrideCursor();
        if (ok) {
            statusBar()->showMessage(tr("Exported %1").arg(QDir::toNativeSeparators(fname)), 8000);
        } else {
            QMessageBox::warning(this, tr("Export Error"),
                                 tr("Failed to export \"%1\": %2")
                                         .arg(t->name(), t->lastExportError()));
        }
    }
}

void ApplicationWindow::exportPDF()
{
    if (isNonInteractiveContext(this))
        return;
    QPointer<QWidget> w = d_workspace.activeSubWindow();
    if (!w)
        return;

    if (w->inherits("MultiLayer") && ((MultiLayer *)w.data())->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"));
        return;
    }

    // B9: graphs go through the image export dialog (size/dpi summary, PDF preselected).
    if (w->inherits("MultiLayer")) {
        // PDF is only preselected for this one export: File > Export Graph keeps offering the
        // format the user normally exports to (unless they switched to another one here).
        const QString prevFilter = d_image_export_filter;
        d_image_export_filter = QStringLiteral("*.pdf");
        exportGraph();
        if (d_image_export_filter == QLatin1String("*.pdf"))
            d_image_export_filter = prevFilter;
        return;
    }

    QPointer<MyWidget> mw = qobject_cast<MyWidget *>(w.data());
    const QString suggestedPath = QDir(workingDir).filePath(
            sanitizeExportStem(mw ? QString(mw->name()) : QStringLiteral("export"))
            + QStringLiteral(".pdf"));
    QString rawFname = QFileDialog::getSaveFileName(this, tr("Choose a filename to save under"),
                                                    suggestedPath, "*.pdf");
    if (!rawFname.isEmpty() && w) {
        bool suffixAppended = false;
        QString fname = ensureExportSuffix(rawFname, QStringLiteral(".pdf"), &suffixAppended);
        if (QFileInfo(fname).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0) {
            fname += QStringLiteral(".pdf");
            suffixAppended = true;
        }

        const QFileInfo fi(fname);
        workingDir = fi.absolutePath();

        if (suffixAppended && QFile::exists(fname)) {
            const auto ans = QMessageBox::question(
                    this, tr("Overwrite file?"),
                    tr("A file called: <p><b>%1</b><p>already exists. Do you want to overwrite it?")
                            .arg(fname),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ans != QMessageBox::Yes || !w)
                return;
        }

        if (!probeWritableNoTruncate(fname)) {
            QMessageBox::critical(this, tr("Export Error"),
                                  tr("Could not write to file: <h4>%1</h4><p>Please verify that "
                                     "you have the right to write to this location or that the "
                                     "file is not being used by another application!")
                                          .arg(fname));
            return;
        }

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        QString errMsg;
        const bool writeOk = executeSafeExportWrite(
                fname, QStringLiteral("pdf"), [&](const QString &writePath) {
                    if (!w)
                        return false;
                    if (Graph3D *g3 = qobject_cast<Graph3D *>(w.data()))
                        return g3->exportVectorChecked(writePath, QStringLiteral("pdf"), &errMsg);
                    if (mw) {
                        mw->exportPDF(writePath);
                        return QFileInfo(writePath).exists() && QFileInfo(writePath).size() > 0;
                    }
                    return false;
                });
        QApplication::restoreOverrideCursor();

        if (!writeOk) {
            QMessageBox::critical(this, tr("Export Error"),
                                  errMsg.isEmpty()
                                          ? tr("Could not write to file: <h4>%1</h4><p>Please "
                                               "verify that you have the right to write to this "
                                               "location or that the file is not being used by "
                                               "another application!")
                                                    .arg(fname)
                                          : tr("Could not write to file: <h4>%1</h4><p>%2")
                                                    .arg(fname, errMsg.toHtmlEscaped()));
        } else if (statusBar()) {
            statusBar()->showMessage(tr("Exported %1").arg(QDir::toNativeSeparators(fname)), 4000);
        }
    }
}

void ApplicationWindow::print(MyWidget *w)
{
    if (isNonInteractiveContext(this) || !w)
        return;
    if (w->inherits("MultiLayer") && ((MultiLayer *)w)->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"));
        return;
    }

    w->print();
}

// print active window
void ApplicationWindow::print()
{
    MyWidget *w = (MyWidget *)(d_workspace.activeSubWindow());
    if (!w)
        return;

    print(w);
}

// print window from project explorer
void ApplicationWindow::printWindow()
{
    if (isNonInteractiveContext(this))
        return;
    WindowListItem *it = dynamic_cast<WindowListItem *>(lv.currentItem());
    if (!it)
        return;
    MyWidget *w = it->window();
    if (!w)
        return;

    print(w);
}

void ApplicationWindow::printAllPlots()
{
    if (isNonInteractiveContext(this))
        return;
    QPrinter printer;
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setColorMode(QPrinter::Color);
    printer.setFullPage(true);

    QList<QPointer<MultiLayer>> plotWindows;
    for (MyWidget *w : windowsList()) {
        if (auto *ml = qobject_cast<MultiLayer *>(w))
            plotWindows.append(ml);
    }

    const int plots = plotWindows.size();

    QPrintDialog dialog(&printer, this);
    dialog.setMinMax(0, plots);
    if (dialog.exec()) {
        QPainter paint(&printer);

        printer.setFromTo(0, plots);

        for (const QPointer<MultiLayer> &ml : plotWindows) {
            if (ml && printer.newPage())
                ml->printAllLayers(&paint);
        }
        paint.end();
    }
}

void ApplicationWindow::addColToTable()
{
    Table *m = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (m)
        m->addCol();
}

void ApplicationWindow::clearSelection()
{
    if (lv.hasFocus()) {
        deleteSelectedItems();
        return;
    }

    QWidget *m = (QWidget *)d_workspace.activeSubWindow();
    if (!m)
        return;

    if (m->inherits("Table"))
        ((Table *)m)->clearSelection();
    else if (m->inherits("Matrix"))
        ((Matrix *)m)->clearSelection();
    else if (m->inherits("MultiLayer")) {
        MultiLayer *plot = static_cast<MultiLayer *>(m);
        Graph *g = plot->activeGraph();
        if (!g)
            return;

        if (g->titleSelected()) {
            g->removeTitle();
        } else if (g->markerSelected()) {
            const bool isPrimaryLeg = (g->selectedMarkerPtr() == g->legend());
            ModalMultiLayerUndoHelper undoHelper(
                    plot, isPrimaryLeg ? tr("Remove Legend") : tr("Remove Annotation"));
            g->removeMarker();
            undoHelper.commitIfChanged();
        } else {
            const Graph::GraphSelection sel = g->currentSelection();
            if (sel.kind == Graph::GraphSelection::Curve && sel.primaryId >= 0
                && sel.primaryId < g->curves()) {
                const QString before = plot->captureUndoSnapshot();
                invalidateModelessGraphDialogs(plot);
                g->removeCurve(sel.primaryId);
                g->setCurrentSelection(Graph::GraphSelection(), true);
                g->updatePlot();
                plot->pushUndoSnapshot(tr("Remove Curve"), before);
            } else if (sel.kind == Graph::GraphSelection::AxisTitle && sel.primaryId >= 0
                       && sel.primaryId < 4) {
                const QString before = plot->captureUndoSnapshot();
                g->setAxisTitleAuto(sel.primaryId, false);
                g->setAxisTitle(sel.primaryId, QString());
                g->setCurrentSelection(Graph::GraphSelection(), true);
                g->replot();
                plot->pushUndoSnapshot(tr("Clear Axis Title"), before);
            } else if (sel.kind == Graph::GraphSelection::PlotTitle) {
                g->removeTitle();
                g->setCurrentSelection(Graph::GraphSelection(), true);
                g->replot();
            } else if (sel.kind == Graph::GraphSelection::ReferenceLine && sel.primaryId >= 0
                       && sel.primaryId < g->referenceLines().size()) {
                g->removeReferenceLine(sel.primaryId);
                g->setCurrentSelection(Graph::GraphSelection(), true);
                g->replot();
            }
        }
    } else if (m->inherits("Note"))
        ((Note *)m)->textWidget()->clear();
    emit modified();
}

/*!
 * Copy a 3D graph for PowerPoint. When the editable object cannot be made, a plain bitmap copy
 * is tried first and only then is the outcome reported, so the message says what is really on
 * the clipboard (it used to say "nothing was placed" right before the bitmap was copied).
 */
static void copyGraph3DForPowerPoint(ApplicationWindow *app, Graph3D *g3d, bool showDialog)
{
    if (!g3d)
        return;
#ifdef Q_OS_WIN
    QImage fb;
    if (g3d->sp) {
        g3d->sp->makeCurrent();
        fb = g3d->sp->grabFramebuffer();
    }
    SciDAVisOle::OleCopyResult res = SciDAVisOle::copyImageForPowerPoint(app, fb, g3d->name());
    if (!res.ok && !res.staticFallbackOnClipboard) {
        QClipboard *cb = QApplication::clipboard();
        if (cb)
            cb->clear(QClipboard::Clipboard);
        g3d->copyImage();
        const QMimeData *md = cb ? cb->mimeData(QClipboard::Clipboard) : nullptr;
        if (md && (md->hasImage() || !md->formats().isEmpty())) {
            res.staticFallbackOnClipboard = true;
            res.pngDpi = 0; // bitmap only (see reportCopyResult)
        }
    }
    SciDAVisOle::reportCopyResult(app, res, showDialog);
#else
    Q_UNUSED(app)
    Q_UNUSED(showDialog)
    g3d->copyImage();
#endif
}

void ApplicationWindow::copySelection()
{
    if (results->hasFocus()) {
        results->copy();
        return;
    }
    if (!isNonInteractiveContext(this)) {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (QLineEdit *le = qobject_cast<QLineEdit *>(fw)) {
                if (le->hasSelectedText()) {
                    le->copy();
                    return;
                }
            } else if (QTextEdit *te = qobject_cast<QTextEdit *>(fw)) {
                if (te->textCursor().hasSelection()) {
                    te->copy();
                    return;
                }
            } else if (QPlainTextEdit *pte = qobject_cast<QPlainTextEdit *>(fw)) {
                if (pte->textCursor().hasSelection()) {
                    pte->copy();
                    return;
                }
            }
        }
    }

    QWidget *m = d_workspace.activeSubWindow();
    if (!m)
        return;

    if (Table *tbl = qobject_cast<Table *>(m)) {
        tbl->copySelection();
    } else if (Matrix *mat = qobject_cast<Matrix *>(m)) {
        mat->copySelection();
    } else if (MultiLayer *plot = qobject_cast<MultiLayer *>(m)) {
        if (plot->layers() == 0)
            return;

        Graph *g = plot->activeGraph();
        if (!g)
            return;
        if (g->markerSelected()) {
            copyMarker();
        } else {
            // Even when PlotTitle or AxisTitle is selected (outside an active inline text editor),
            // Ctrl+C copies the graph as an editable PowerPoint OLE object.
            copyActiveLayer();
        }
    } else if (Graph3D *g3d = qobject_cast<Graph3D *>(m)) {
        copyGraph3DForPowerPoint(this, g3d, false);
    } else if (Note *note = qobject_cast<Note *>(m)) {
        note->textWidget()->copy();
    }
}

void ApplicationWindow::cutSelection()
{
    QWidget *m = d_workspace.activeSubWindow();
    if (!m)
        return;

    if (Table *tbl = qobject_cast<Table *>(m)) {
        tbl->cutSelection();
    } else if (Matrix *mat = qobject_cast<Matrix *>(m)) {
        mat->cutSelection();
    } else if (MultiLayer *plot = qobject_cast<MultiLayer *>(m)) {
        if (plot->layers() == 0)
            return;

        Graph *g = plot->activeGraph();
        if (!g)
            return;
        const Graph::GraphSelection sel = g->currentSelection();
        if (g->markerSelected()) {
            const bool isPrimaryLeg = (g->selectedMarkerPtr() == g->legend());
            ModalMultiLayerUndoHelper undoHelper(
                    plot, isPrimaryLeg ? tr("Remove Legend") : tr("Cut Annotation"));
            copyMarker();
            g->removeMarker();
            undoHelper.commitIfChanged();
        } else if (g->titleSelected() || sel.kind == Graph::GraphSelection::PlotTitle) {
            const QString titleText =
                    g->plotWidget() ? g->plotWidget()->title().text() : QString();
            if (!titleText.isEmpty() && QApplication::clipboard())
                QApplication::clipboard()->setText(titleText);
            g->removeTitle();
            g->setCurrentSelection(Graph::GraphSelection(), true);
            g->replot();
        } else if (sel.kind == Graph::GraphSelection::AxisTitle && sel.primaryId >= 0
                   && sel.primaryId < 4) {
            const QString axisText = g->axisTitle(sel.primaryId);
            if (!axisText.isEmpty() && QApplication::clipboard())
                QApplication::clipboard()->setText(axisText);
            const QString before = plot->captureUndoSnapshot();
            g->setAxisTitleAuto(sel.primaryId, false);
            g->setAxisTitle(sel.primaryId, QString());
            g->setCurrentSelection(Graph::GraphSelection(), true);
            g->replot();
            plot->pushUndoSnapshot(tr("Cut Axis Title"), before);
        } else if (sel.kind == Graph::GraphSelection::ReferenceLine && sel.primaryId >= 0
                   && sel.primaryId < g->referenceLines().size()) {
            g->removeReferenceLine(sel.primaryId);
            g->setCurrentSelection(Graph::GraphSelection(), true);
            g->replot();
            if (statusBar())
                statusBar()->showMessage(
                        tr("Removed selected reference line (press Ctrl+Z to undo). Layer kept."),
                        8000);
        } else if (sel.kind == Graph::GraphSelection::Curve && sel.primaryId >= 0
                   && sel.primaryId < g->curves()) {
            const QString before = plot->captureUndoSnapshot();
            invalidateModelessGraphDialogs(plot);
            g->removeCurve(sel.primaryId);
            g->setCurrentSelection(Graph::GraphSelection(), true);
            g->updatePlot();
            plot->pushUndoSnapshot(tr("Remove Curve"), before);
            if (statusBar())
                statusBar()->showMessage(
                        tr("Removed selected curve (press Ctrl+Z to undo). Layer kept."), 8000);
        } else if (sel.kind == Graph::GraphSelection::Axis
                   || sel.kind == Graph::GraphSelection::ErrorBars
                   || sel.kind == Graph::GraphSelection::Spectrogram) {
            if (statusBar())
                statusBar()->showMessage(
                        tr("Axes, error bars, and colormaps cannot be cut directly; click the "
                           "layer background to cut the layer, or use Graph -> Add/Remove Curve."),
                        8000);
            return;
        } else {
            invalidateModelessGraphDialogs(plot);
            ModalMultiLayerUndoHelper undoHelper(plot, tr("Cut Layer"));
            copyActiveLayer();
            plot->removeLayer();
            undoHelper.commitIfChanged();
        }
    } else if (Note *note = qobject_cast<Note *>(m)) {
        note->textWidget()->cut();
    }

    emit modified();
}

void ApplicationWindow::copyMarker()
{
    QWidget *m = d_workspace.activeSubWindow();
    MultiLayer *plot = qobject_cast<MultiLayer *>(m);
    if (!plot)
        return;
    Graph *g = plot->activeGraph();
    if (g && g->markerSelected()) {
        g->copyMarker();
        copiedMarkerType = g->copiedMarkerType();
        QRect rect = g->copiedMarkerRect();
        auxMrkStart = rect.topLeft();
        auxMrkEnd = rect.bottomRight();

        if (copiedMarkerType == Graph::Text) {
            Legend *leg = dynamic_cast<Legend *>(g->selectedMarkerPtr());
            if (leg) {
                auxMrkText = leg->text();
                auxMrkColor = leg->textColor();
                auxMrkFont = leg->font();
                auxMrkBkg = leg->frameStyle();
                auxMrkBkgColor = leg->backgroundColor();
            }
        } else if (copiedMarkerType == Graph::Arrow) {
            ArrowMarker *am = dynamic_cast<ArrowMarker *>(g->selectedMarkerPtr());
            if (am) {
                auxMrkWidth = am->width();
                auxMrkColor = am->color();
                auxMrkStyle = am->style();
                startArrowOn = am->hasStartArrow();
                endArrowOn = am->hasEndArrow();
                arrowHeadLength = am->headLength();
                arrowHeadAngle = am->headAngle();
                fillArrowHead = am->filledArrowHead();
            }
        } else if (copiedMarkerType == Graph::Image) {
            ImageMarker *im = dynamic_cast<ImageMarker *>(g->selectedMarkerPtr());
            if (im)
                auxMrkFileName = im->fileName();
        }
    }
    copiedLayer = false;
#ifdef Q_OS_WIN
    const SciDAVisOle::OleCopyResult res = SciDAVisOle::copyGraphForPowerPoint(this, plot, nullptr);
    SciDAVisOle::reportCopyResult(this, res, false);
    if ((res.ok || res.staticFallbackOnClipboard) && !res.registrationProblem && statusBar()) {
        statusBar()->showMessage(
                tr("Copied annotation (Ctrl+V in ToriGraph duplicates the annotation; Ctrl+V in "
                   "PowerPoint inserts the whole graph)."),
                10000);
    }
#endif
}

void ApplicationWindow::pasteSelection()
{
    QWidget *m = (QWidget *)d_workspace.activeSubWindow();
    if (!m) {
        static bool s_inSmartPaste = false;
        if (!s_inSmartPaste && d_modern_ribbon) {
            QScopedValueRollback<bool> guard(s_inSmartPaste, true);
            if (QToolButton *pasteBtn =
                        d_modern_ribbon->findChild<QToolButton *>(QStringLiteral("RibbonHomePasteBtn"));
                pasteBtn && pasteBtn->isEnabled()) {
                pasteBtn->click();
                return;
            }
            if (QPushButton *welcomePasteBtn =
                        findChild<QPushButton *>(QStringLiteral("WelcomeHubPasteBtn"));
                welcomePasteBtn && welcomePasteBtn->isEnabled()) {
                welcomePasteBtn->click();
                return;
            }
        }
        return;
    }

    if (m->inherits("Table"))
        ((Table *)m)->pasteSelection();
    else if (m->inherits("Matrix"))
        ((Matrix *)m)->pasteSelection();
    else if (m->inherits("Note"))
        ((Note *)m)->textWidget()->paste();
    else if (m->inherits("MultiLayer")) {
        MultiLayer *plot = (MultiLayer *)m;
        if (!plot)
            return;
        const QString before = plot->captureUndoSnapshot();
        const int prevCount = plot->undoStack() ? plot->undoStack()->count() : 0;
        if (copiedLayer) {
            if (!lastCopiedLayer)
                return;
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

            Graph *g = plot->addLayer();
            setPreferences(g);
            g->copy(this, lastCopiedLayer);
            QWidget *cv = plot->canvasWidget();
            QPoint pos =
                    cv ? cv->mapFromGlobal(QCursor::pos()) : plot->mapFromGlobal(QCursor::pos());
            const int lw = lastCopiedLayer->width();
            const int lh = lastCopiedLayer->height();
            int gx = pos.x();
            int gy = pos.y() - 20;
            if (cv && cv->width() > 0 && cv->height() > 0) {
                const QRect cr = cv->rect();
                gx = qBound(cr.left(), gx, qMax(cr.left(), cr.right() - lw + 1));
                gy = qBound(cr.top(), gy, qMax(cr.top(), cr.bottom() - lh + 1));
            } else {
                gx = qMax(0, gx);
                gy = qMax(0, gy);
            }
            plot->setGraphGeometry(gx, gy, lw, lh);
            plot->pushUndoSnapshot(tr("Paste Layer"), before);

            QApplication::restoreOverrideCursor();
        } else {
            if (plot->layers() == 0)
                return;

            Graph *g = (Graph *)plot->activeGraph();
            if (!g)
                return;

            g->setCopiedMarkerType(copiedMarkerType);
            g->setCopiedMarkerEnds(auxMrkStart, auxMrkEnd);

            if (copiedMarkerType == Graph::Text)
                g->setCopiedTextOptions(auxMrkBkg, auxMrkText, auxMrkFont, auxMrkColor,
                                        auxMrkBkgColor);
            if (copiedMarkerType == Graph::Arrow)
                g->setCopiedArrowOptions(auxMrkWidth, auxMrkStyle, auxMrkColor, startArrowOn,
                                         endArrowOn, arrowHeadLength, arrowHeadAngle,
                                         fillArrowHead);
            if (copiedMarkerType == Graph::Image)
                g->setCopiedImageName(auxMrkFileName);
            g->pasteMarker();
            if (!plot->undoStack() || plot->undoStack()->count() == prevCount)
                plot->pushUndoSnapshot(tr("Paste Selection"), before);
        }
    }
    emit modified();
}

Matrix *ApplicationWindow::importImage(const QString &fileName)
{
    QImage image(fileName);
    if (image.isNull())
        return NULL;

    Matrix *m = Matrix::fromImage(image, scriptEnv);
    if (!m) {
        if (!isNonInteractiveContext(this))
            QMessageBox::information(0, tr("Error importing image"),
                                     tr("Import of image '%1' failed").arg(fileName));
        return NULL;
    }
    QString caption = generateUniqueName(tr("Matrix"));
    m->setName(caption);
    d_project->addChild(m->d_future_matrix);
    return m;
}

void ApplicationWindow::clearLastCopiedLayerIfReferencing(MyWidget *w)
{
    if (!lastCopiedLayer) {
        copiedLayer = false;
        d_last_copied_layer_source_ml.clear();
        return;
    }
    bool shouldClear = (w == nullptr);
    if (!shouldClear && w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        if (d_last_copied_layer_source_ml == ml || ml->graphPtrs().contains(lastCopiedLayer))
            shouldClear = true;
    }
    if (!shouldClear && (w->inherits("Table") || w->inherits("Matrix"))) {
        if (Plot *plt = lastCopiedLayer->plotWidget()) {
            const QwtPlotItemList &items = plt->itemList();
            for (QwtPlotItem *item : items) {
                if (const DataCurve *dc = dynamic_cast<const DataCurve *>(item)) {
                    if (dc->table() == w) {
                        shouldClear = true;
                        break;
                    }
                } else if (QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(item)) {
                    if (er->masterCurve() && er->masterCurve()->table() == w) {
                        shouldClear = true;
                        break;
                    }
                } else if (const Spectrogram *sp = dynamic_cast<const Spectrogram *>(item)) {
                    if (sp->matrix() == w) {
                        shouldClear = true;
                        break;
                    }
                }
            }
        }
    }
    if (shouldClear) {
        delete lastCopiedLayer;
        lastCopiedLayer = nullptr;
        copiedLayer = false;
        d_last_copied_layer_source_ml.clear();
    }
}

void ApplicationWindow::copyActiveLayer()
{
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    copiedLayer = true;
    d_last_copied_layer_source_ml = ml;

    delete lastCopiedLayer;
    lastCopiedLayer = new Graph(0, 0, Qt::Widget);
    lastCopiedLayer->setAttribute(Qt::WA_DeleteOnClose);
    lastCopiedLayer->setGeometry(0, 0, g->width(), g->height());
    lastCopiedLayer->copy(this, g);
#ifdef Q_OS_WIN
    // Ctrl+C on a graph: report in the status bar only (no dialog for a routine copy).
    const SciDAVisOle::OleCopyResult res = SciDAVisOle::copyGraphForPowerPoint(this, ml, nullptr);
    SciDAVisOle::reportCopyResult(this, res, false);
    if (res.ok || res.staticFallbackOnClipboard)
        return;
#endif
    g->copyImage();
}

void ApplicationWindow::connectTable(Table *w)
{
    connect(w, SIGNAL(showTitleBarMenu()), this, SLOT(showWindowTitleBarMenu()));
    connect(w, SIGNAL(statusChanged(MyWidget *)), this, SLOT(updateWindowStatus(MyWidget *)));
    connect(w, SIGNAL(hiddenWindow(MyWidget *)), this, SLOT(hideWindow(MyWidget *)));
    connect(w, SIGNAL(closedWindow(MyWidget *)), this, SLOT(closeWindow(MyWidget *)));
    // the curves of a column that goes are kept with it (not deleted) and come back with it
    {
        QPointer<Table> colGuard(w);
        connect(w, &Table::aboutToRemoveCol, this, [this, colGuard](const QString &name) {
            if (colGuard)
                takeCurvesOfRemovedColumn(colGuard.data(), name);
        });
    }
    connect(w, SIGNAL(modifiedData(Table *, const QString &)), this,
            SLOT(updateCurves(Table *, const QString &)));
    connect(w, SIGNAL(modifiedWindow(MyWidget *)), this, SLOT(modifiedProject(MyWidget *)));
    connect(w, SIGNAL(changedColHeader(const QString &, const QString &)), this,
            SLOT(updateColNames(const QString &, const QString &)));
    if (w && w->d_future_table) {
        connect(w->d_future_table, SIGNAL(aspectDescriptionChanged(const AbstractAspect *)), this,
                SLOT(handleAspectDescriptionChanged(const AbstractAspect *)), Qt::UniqueConnection);
        QPointer<Table> tableGuard(w);
        connect(w->d_future_table, &future::Table::columnsRemovalStarting, this,
                [this, tableGuard](int first, int count) {
                    if (tableGuard)
                        removeCurvesOfColumnsWithUndo(tableGuard.data(), first, count);
                });
        connect(w->d_future_table, &future::Table::columnsInserted, this,
                [this, tableGuard](int, int) {
                    if (tableGuard)
                        restoreCurvesOfReaddedTable(tableGuard.data());
                });
    }
    connect(w, &TableView::selectionStatsChanged, this, [this, w](const QString &summary) {
        if (!d_status_info || d_workspace.activeSubWindow() != w)
            return;
        if (summary.trimmed().isEmpty())
            updateStatusBarForWindow(d_status_info, w);
        else
            d_status_info->setText(summary);
    });

#ifdef LEGACY_CODE_0_2_x
    connect(w->d_future_table, SIGNAL(requestRowStatistics()), this, SLOT(showRowStatistics()));
    connect(w->d_future_table, SIGNAL(requestColumnStatistics()), this, SLOT(showColStatistics()));
#endif
    w->askOnCloseEvent(confirmCloseTable);
    w->removeEventFilter(this);
    w->installEventFilter(this);
}

void ApplicationWindow::copyGraphAsOleObject()
{
    QWidget *actW = d_workspace.activeSubWindow();
    if (!actW || (!actW->inherits("Graph3D") && !actW->inherits("MultiLayer"))) {
        // A table, matrix or note is active (or nothing): say so instead of doing nothing.
        const QString msg = ToriGraphOleUi::selectGraphFirstMessage();
        if (statusBar())
            statusBar()->showMessage(msg, 8000);
        if (qApp)
            qApp->setProperty("torigraph_last_ole_copy_message", msg);
        if (!isNonInteractiveContext(this))
            QMessageBox::information(this, ToriGraphOleUi::copyForPowerPointLabel(), msg);
        return;
    }

    if (actW->inherits("Graph3D")) {
        copyGraph3DForPowerPoint(this, static_cast<Graph3D *>(actW), true);
        return;
    }

    MultiLayer *ml = static_cast<MultiLayer *>(actW);
#ifdef Q_OS_WIN
    // On failure the reason has been shown; a static picture was offered where possible.
    const SciDAVisOle::OleCopyResult res = SciDAVisOle::copyGraphForPowerPoint(this, ml, nullptr);
    SciDAVisOle::reportCopyResult(this, res, true);
#else
    ml->copyAllLayers();
#endif
}

void ApplicationWindow::repairOleRegistration()
{
#ifdef Q_OS_WIN
    SciDAVisOle::repairRegistrationInteractive(this);
#endif
}

void ApplicationWindow::setOleEmbedWholeProject(bool wholeProject)
{
#ifdef Q_OS_WIN
    SciDAVisOle::setEmbedScopeSetting(wholeProject ? SciDAVisOle::EmbedScope::WholeProject
                                                   : SciDAVisOle::EmbedScope::GraphOnly);
#else
    Q_UNUSED(wholeProject);
#endif
}

void ApplicationWindow::clearTable()
{
    Table *t = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (!t)
        return;

    if (isNonInteractiveContext(this)) {
        t->clear();
        return;
    }

    if (QMessageBox::question(this, tr("Warning"),
                              tr("This will clear the contents of all the data associated with the "
                                 "table. Are you sure?"),
                              tr("&Yes"), tr("&No"), QString(), 0, 1))
        return;
    else
        t->clear();
}

void ApplicationWindow::copyPageAsHighDpiImage()
{
    QWidget *w = d_workspace.activeSubWindow();
    if (!w)
        w = d_workspace.currentSubWindow();
    if (!w)
        w = activeWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(w);
    const QImage img = ml->copyPageAsImage(300);
    if (!img.isNull()) {
        const QString msg = tr("Copied %1 as 300 DPI PNG (%2 x %3 px) to clipboard.")
                                    .arg(ml->name())
                                    .arg(img.width())
                                    .arg(img.height());
        setStatusBarText(msg);
        if (statusBar())
            statusBar()->showMessage(msg, 4000);
    }
}

void ApplicationWindow::exportActiveGraphToPython()
{
    MyWidget *targetWin = activeWindow();
    if (!targetWin)
        targetWin = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!targetWin) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::information(
                    this, tr("Export Python Script"),
                    tr("Please open or select a Graph, Worksheet, or Matrix window first."));
        }
        return;
    }

    const QString scriptCode = PythonAutomationServer::generatePythonScriptForWindow(
            this, targetWin, true, QStringLiteral("inline"));
    if (scriptCode.trimmed().isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Export Python Script"),
                                 tr("Could not generate a Python script for the active window."));
        }
        return;
    }

    if (QClipboard *cb = QApplication::clipboard())
        cb->setText(scriptCode);

    const int nLines = scriptCode.count(QLatin1Char('\n')) + 1;
    if (statusBar()) {
        statusBar()->showMessage(
                tr("Generated Python script (%1 lines) and copied to clipboard.").arg(nLines),
                4000);
    }

    if (isNonInteractiveContext(this))
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Export Reproducible Python Script — %1").arg(targetWin->name()));
    dlg.resize(740, 540);

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->setContentsMargins(14, 14, 14, 12);
    vbox->setSpacing(10);

    QLabel *infoLbl = new QLabel(
            tr("Standalone reproducible ToriGraph SDK Python script for '%1' (copied to clipboard):")
                    .arg(targetWin->name()),
            &dlg);
    infoLbl->setWordWrap(true);
    vbox->addWidget(infoLbl);

    QPlainTextEdit *editor = new QPlainTextEdit(&dlg);
    editor->setReadOnly(true);
    editor->setPlainText(scriptCode);
    QFont codeFont(QStringLiteral("Consolas"), 9);
    codeFont.setStyleHint(QFont::Monospace);
    editor->setFont(codeFont);
    vbox->addWidget(editor, 1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    QPushButton *btnCopy = new QPushButton(tr("Copy to Clipboard"), &dlg);
    QPushButton *btnSave = new QPushButton(tr("Save .py File..."), &dlg);
    QPushButton *btnClose = new QPushButton(tr("Close"), &dlg);
    btnSave->setDefault(true);

    btnRow->addWidget(btnCopy);
    btnRow->addWidget(btnSave);
    btnRow->addStretch(1);
    btnRow->addWidget(btnClose);
    vbox->addLayout(btnRow);

    connect(btnCopy, &QPushButton::clicked, &dlg, [scriptCode, this]() {
        if (QClipboard *cb = QApplication::clipboard())
            cb->setText(scriptCode);
        if (statusBar())
            statusBar()->showMessage(tr("Copied Python script to clipboard."), 3000);
    });

    const QString targetName = targetWin->name();
    connect(btnSave, &QPushButton::clicked, &dlg, [&dlg, scriptCode, targetName, this]() {
        const QString defaultPath =
                workingDir + QLatin1Char('/') + targetName + QStringLiteral("_reproduce.py");
        const QString savePath = QFileDialog::getSaveFileName(
                &dlg, tr("Save Reproducible Python Script"), defaultPath,
                tr("Python Scripts (*.py);;All Files (*)"));
        if (savePath.isEmpty())
            return;
        QFile outFile(savePath);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            outFile.write(scriptCode.toUtf8());
            outFile.close();
            if (statusBar())
                statusBar()->showMessage(tr("Saved Python script to %1").arg(savePath), 4000);
        } else {
            QMessageBox::warning(&dlg, tr("Export Python Script"),
                                 tr("Failed to write file:\n%1").arg(savePath));
        }
    });

    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}
