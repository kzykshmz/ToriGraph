/***************************************************************************
        File                 : ApplicationWindow.cpp
        Project              : SciDAVis
        Description          : SciDAVis's main window (core lifecycle, events, MDI windows, folders, explorer, docks & shell)
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

#define HOMEPAGE_URI "https://github.com/kzykshmz/ToriGraph"
#define MANUAL_URI "https://github.com/kzykshmz/ToriGraph/blob/main/README.md"
#define FORUM_URI "https://github.com/kzykshmz/ToriGraph/discussions"
#define BUGREPORT_URI "https://github.com/kzykshmz/ToriGraph/issues"
#define DOWNLOAD_URI "https://github.com/kzykshmz/ToriGraph/releases"

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

extern "C" {
void file_compress(const char *file, const char *mode);
}

namespace ApplicationWindowInternal {
bool &creatingHiddenTableFlag()
{
    static thread_local bool s_creating_hidden_table = false;
    return s_creating_hidden_table;
}

//! ApplicationWindow objects whose destructor has started. The project explorer trees (lv,
//! folders) are members and are destroyed before the MDI area that still holds the windows, and
//! before QWidget deletes the remaining children; a window lookup made while those windows go
//! away (signal, event filter, dock or tool that still holds the main window) would walk the
//! freed folder tree.
QSet<const ApplicationWindow *> &applicationWindowsBeingDestroyed()
{
    static QSet<const ApplicationWindow *> s;
    return s;
}

int &renamedLookupSuppressionDepth()
{
    static int s_renamedLookupSuppressionDepth = 0;
    return s_renamedLookupSuppressionDepth;
}
} // namespace ApplicationWindowInternal

namespace {

static void syncWorkspaceHiddenTabs(ApplicationWindow *app)
{
    if (!app)
        return;
    const QSet<MyWidget *> hiddenSet(app->hiddenWindows.begin(), app->hiddenWindows.end());
    const QSet<MyWidget *> outSet(app->outWindows.begin(), app->outWindows.end());
    const QList<QMdiSubWindow *> creationOrder =
            app->d_workspace.subWindowList(QMdiArea::CreationOrder);
    for (QMdiSubWindow *sw : creationOrder) {
        MyWidget *mw = qobject_cast<MyWidget *>(sw);
        if (!mw)
            continue;
        if (hiddenSet.contains(mw) || mw->status() == MyWidget::Hidden) {
            if (!mw->isHidden())
                mw->hide();
        }
    }
    if (app->d_workspace.viewMode() != QMdiArea::TabbedView)
        return;

    if (MyWidget *curAw = qobject_cast<MyWidget *>(app->d_workspace.activeSubWindow())) {
        if (hiddenSet.contains(curAw) || outSet.contains(curAw)
            || curAw->status() == MyWidget::Hidden) {
            MyWidget *fallback = nullptr;
            const QList<QMdiSubWindow *> hist =
                    app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
            for (int i = hist.size() - 1; i >= 0; --i) {
                MyWidget *cand = qobject_cast<MyWidget *>(hist.at(i));
                if (cand && cand != curAw && !hiddenSet.contains(cand) && !outSet.contains(cand)
                    && cand->status() != MyWidget::Hidden) {
                    fallback = cand;
                    break;
                }
            }
            app->d_workspace.setActiveSubWindow(fallback);
        }
    }

    if (QTabBar *tb = app->d_workspace.findChild<QTabBar *>()) {
        Folder *curFolder = app->currentFolder();
        for (int i = 0; i < tb->count(); ++i) {
            MyWidget *mw = (i < creationOrder.size())
                    ? qobject_cast<MyWidget *>(creationOrder.at(i))
                    : nullptr;
            const bool wrongFolder = curFolder && mw && mw->folder() && mw->folder() != curFolder;
            const bool hideTab = mw
                    && (wrongFolder || hiddenSet.contains(mw) || outSet.contains(mw)
                        || mw->status() == MyWidget::Hidden);
            tb->setTabVisible(i, !hideTab);
        }
    }
}

static void scheduleSyncWorkspaceHiddenTabs(ApplicationWindow *app)
{
    if (!app || app->isBatchWindowUpdatesActive())
        return;
    syncWorkspaceHiddenTabs(app);
    QPointer<ApplicationWindow> self(app);
    QTimer::singleShot(0, app, [self]() {
        if (self && !self->isBatchWindowUpdatesActive())
            syncWorkspaceHiddenTabs(self.data());
    });
}

class ExplorerContainerWidget : public QWidget
{
public:
    explicit ExplorerContainerWidget(QWidget *parent = nullptr) : QWidget(parent) { }
    QSize sizeHint() const override { return QSize(280, 440); }
    QSize minimumSizeHint() const override { return QSize(230, 140); }
};


void syncExplorerSelectionToWindow(FolderListView &lv, MyWidget *target)
{
    QSignalBlocker sbLv(&lv);
    if (target) {
        for (QTreeWidgetItemIterator it(&lv); *it; ++it) {
            QTreeWidgetItem *item = *it;
            if (!item || item->type() != WindowListItem::WindowType)
                continue;
            if (static_cast<WindowListItem *>(item)->window() == target && !item->isHidden()) {
                if (lv.currentItem() != item || !item->isSelected()) {
                    lv.setCurrentItem(
                            item, 0,
                            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                }
                return;
            }
        }
    }
    lv.clearSelection();
    lv.setCurrentItem(nullptr);
}

} // anonymous namespace

ApplicationWindow::ApplicationWindow()
    : scripted(ScriptingLangManager::newEnv(this)),
      //      logWindow(new QDockWidget(this)),
      //      explorerWindow(new QDockWidget(this)),
      //      results(new QTextEdit(logWindow)),
      //#ifdef SCRIPTING_CONSOLE
      //      consoleWindow(new QDockWidget(this)),
      //      console(new QTextEdit(consoleWindow)),
      //#endif
      //      d_workspace(new QMdiArea(this)),
      //      lv(new FolderListView()),
      //      folders(new FolderListView()),

      //      hiddenWindows(new QList<MyWidget*>()),
      //      outWindows(new QList<MyWidget*>()),
      lastModified(0),
      current_folder(new Folder(tr("UNTITLED"))),
      show_windows_policy(ActiveFolder),
      d_file_version(SciDAVis::version()),
      appStyle(qApp->style()->objectName()),
      appFont(QFont()),
      projectname("untitled"),
      logInfo(QString()),
      savingTimerId(0),
      copiedLayer(false),
      renamedTables(QStringList()),
      copiedMarkerType(Graph::None),
#ifdef SEARCH_FOR_UPDATES
      autoSearchUpdatesRequest(false),
#endif
      lastCopiedLayer(0),
      explorerSplitter(new QSplitter(Qt::Horizontal, &explorerWindow)),
      actionNextWindow(new QAction(IconLoader::load("go-next"), tr("&Next", "next window"), this)),
      actionPrevWindow(
              new QAction(IconLoader::load("go-previous"), tr("&Previous", "previous window"), this))

{
    applicationWindowsBeingDestroyed().remove(this); // a window destroyed at this address before
    gsl_set_error_handler_off();
    setAttribute(Qt::WA_DeleteOnClose);
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
    if (QApplication *qa = qobject_cast<QApplication *>(qApp)) {
        if (qa->styleSheet().isEmpty()
            || !qa->property("_tg_modern_theme_applied").toBool()) {
            ModernUiTheme::applyModernTheme(qa);
            qa->setProperty("_tg_modern_theme_applied", true);
        }
    }

    // Cold-start i18n fix: register extra dictionary entries and install Japanese translator
    // BEFORE any UI strings, dock titles, Project Explorer headers, actions, toolbars, or menus are built.
    registerApplicationWindowJapaneseStrings();
    ToriUx::registerUxTranslations();
    {
        auto &earlySettings = getSettings();
        earlySettings.beginGroup("/General");
        appLanguage = earlySettings
                              .value("/Language", QLocale::system().name().section('_', 0, 0))
                              .toString();
        earlySettings.endGroup();
    }
    d_builtin_ja_translator = sharedBuiltinJaTranslator();
    if (appLanguage.startsWith("ja") && d_builtin_ja_translator) {
        qApp->installTranslator(d_builtin_ja_translator);
    }
    if (current_folder)
        current_folder->setName(tr("UNTITLED"));
    if (actionNextWindow)
        actionNextWindow->setText(tr("&Next", "next window"));
    if (actionPrevWindow)
        actionPrevWindow->setText(tr("&Previous", "previous window"));

    setWindowTitle(tr("ToriGraph - untitled"));

    {
        QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (docsDir.isEmpty() || !QDir(docsDir).exists())
            docsDir = QDir::homePath();
        workingDir = docsDir;
        asciiDirPath = docsDir;
        imagesDirPath = docsDir;

        QLocale initLocale = QLocale();
        initLocale.setNumberOptions(initLocale.numberOptions() | QLocale::OmitGroupSeparator);
        QLocale::setDefault(initLocale);
    }

    // Icons
    IconLoader::init();

    initFonts();
    QPixmapCache::setCacheLimit(20 * QPixmapCache::cacheLimit());

    d_project = new Project();
    if (d_project->undoStack())
        d_project->undoStack()->setUndoLimit(undoLimit);
    connect(d_project, SIGNAL(aspectAdded(const AbstractAspect *, int)), this,
            SLOT(handleAspectAdded(const AbstractAspect *, int)));
    connect(d_project, SIGNAL(aspectAboutToBeRemoved(const AbstractAspect *, int)), this,
            SLOT(handleAspectAboutToBeRemoved(const AbstractAspect *, int)));
    connect(d_project, SIGNAL(aspectDescriptionChanged(const AbstractAspect *)), this,
            SLOT(handleAspectDescriptionChanged(const AbstractAspect *)), Qt::UniqueConnection);

    d_show_table_comments = true;
    ::future::Table::setDefaultCommentVisibility(true);

    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    explorerWindow.setWindowTitle(tr("Project Explorer"));
    explorerWindow.setObjectName(
            "explorerWindow"); // this is needed for QMainWindow::restoreState()
    explorerWindow.setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
                                   | Qt::BottomDockWidgetArea);
    explorerWindow.setMinimumWidth(220);
    explorerWindow.setMinimumHeight(140);
    addDockWidget(Qt::LeftDockWidgetArea, &explorerWindow);

    folders.setObjectName("folders");
    folders.setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    folders.setContextMenuPolicy(Qt::CustomContextMenu);

    folders.setHeaderLabels(QStringList() << tr("Folder") << QString());
    folders.setRootIsDecorated(true);
    folders.setColumnWidth(1, 0); // helps autoScroll
    folders.hideColumn(1); // helps autoScroll
#if QT_VERSION >= 0x050000
    folders.header()->setSectionsClickable(false);
    folders.header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    folders.header()->setClickable(false);
    folders.header()->setResizeMode(QHeaderView::ResizeToContents);
#endif
    folders.header()->hide();
    folders.setSelectionMode(QTreeWidget::SingleSelection);

    connect(&folders, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this,
            SLOT(folderItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
    connect(&folders, SIGNAL(itemRenamed(QTreeWidgetItem *, int, const QString &)), this,
            SLOT(renameFolder(QTreeWidgetItem *, int, const QString &)));
    connect(&folders, SIGNAL(customContextMenuRequested(const QPoint &)), this,
            SLOT(showFolderPopupMenu(const QPoint &)));
    connect(&folders, SIGNAL(dragItems(QList<QTreeWidgetItem *>)), this,
            SLOT(dragFolderItems(QList<QTreeWidgetItem *>)));
    connect(&folders, SIGNAL(dropItems(QTreeWidgetItem *)), this,
            SLOT(dropFolderItems(QTreeWidgetItem *)));
    connect(&folders, SIGNAL(renameItem(QTreeWidgetItem *, int)), this,
            SLOT(startRenameFolder(QTreeWidgetItem *, int)));
    connect(&folders, SIGNAL(addFolderItem()), this, SLOT(addFolder()));
    connect(&folders, SIGNAL(deleteSelection()), this, SLOT(deleteSelectedItems()));

    FolderListItem *fli = new FolderListItem(&folders, current_folder);
    current_folder->setFolderListItem(fli);
    folders.setCurrentItem(fli);
    fli->setExpanded(true);

    lv.setObjectName("lv");
    lv.setRootIsDecorated(false);
    lv.setContextMenuPolicy(Qt::CustomContextMenu);
    lv.setHeaderLabels(QStringList()
                       << tr("Name") << tr("Type") << tr("View") << tr("Created") << tr("Label"));
    lv.header()->setStretchLastSection(true);
    lv.setMinimumHeight(80);
    lv.setSelectionMode(QTreeWidget::ExtendedSelection);
    lv.setTextElideMode(Qt::ElideMiddle);
    lv.setWordWrap(false);
    updateExplorerFolderPaneVisibility();

    explorerSplitter->addWidget(&folders);
    explorerSplitter->addWidget(&lv);
    explorerSplitter->setStretchFactor(0, 22);
    explorerSplitter->setStretchFactor(1, 78);
    explorerSplitter->setSizes(QList<int>() << 22 << 78);

    d_explorer_container = new ExplorerContainerWidget(&explorerWindow);
    QVBoxLayout *explorerMainLayout = new QVBoxLayout(d_explorer_container);
    explorerMainLayout->setContentsMargins(3, 3, 3, 3);
    explorerMainLayout->setSpacing(3);

    d_explorer_bar_grid = new QGridLayout();
    d_explorer_bar_grid->setContentsMargins(2, 2, 2, 2);
    d_explorer_bar_grid->setHorizontalSpacing(4);
    d_explorer_bar_grid->setVerticalSpacing(4);

    d_explorer_search = new QLineEdit(d_explorer_container);
    d_explorer_search->setObjectName("explorerSearchEdit");
    d_explorer_search->setMinimumWidth(120);
    d_explorer_search->setFixedHeight(26);
    d_explorer_search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_explorer_search->setClearButtonEnabled(true);
    d_explorer_search->setPlaceholderText(tr("Search windows (name / label)..."));
    connect(d_explorer_search, SIGNAL(textChanged(const QString &)), this,
            SLOT(filterExplorerWindows()));

    d_explorer_type_filter = new QComboBox(d_explorer_container);
    d_explorer_type_filter->setObjectName("explorerTypeFilterCombo");
    d_explorer_type_filter->setMinimumWidth(88);
    d_explorer_type_filter->setFixedHeight(26);
    d_explorer_type_filter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_explorer_type_filter->addItem(tr("All Windows"), QString("all"));
    d_explorer_type_filter->addItem(tr("Tables (Workbooks)"), QString("Table"));
    d_explorer_type_filter->addItem(tr("Graphs (2D/3D)"), QString("MultiLayer"));
    d_explorer_type_filter->addItem(tr("Matrices"), QString("Matrix"));
    d_explorer_type_filter->addItem(tr("Notes / Scripts"), QString("Note"));
    d_explorer_type_filter->addItem(tr("Subfolders Only"), QString("Folder"));
    connect(d_explorer_type_filter, SIGNAL(currentIndexChanged(int)), this,
            SLOT(filterExplorerWindows()));

    d_explorer_subfolder_check = new QCheckBox(tr("Subfolders"), d_explorer_container);
    d_explorer_subfolder_check->setObjectName("explorerSubfolderCheck");
    d_explorer_subfolder_check->setFixedHeight(26);
    d_explorer_subfolder_check->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    d_explorer_subfolder_check->setToolTip(tr("Include windows from subfolders in list"));
    connect(d_explorer_subfolder_check, SIGNAL(toggled(bool)), this,
            SLOT(filterExplorerWindows()));

    d_btn_explorer_new_folder = new QToolButton(d_explorer_container);
    d_btn_explorer_new_folder->setAutoRaise(true);
    d_btn_explorer_new_folder->setIcon(IconLoader::load("new-folder"));
    d_btn_explorer_new_folder->setToolTip(tr("New Folder"));
    connect(d_btn_explorer_new_folder, SIGNAL(clicked()), this, SLOT(addFolder()));

    d_btn_explorer_new_table = new QToolButton(d_explorer_container);
    d_btn_explorer_new_table->setAutoRaise(true);
    d_btn_explorer_new_table->setIcon(IconLoader::load("new-table"));
    d_btn_explorer_new_table->setToolTip(tr("New Table"));
    connect(d_btn_explorer_new_table, SIGNAL(clicked()), this, SLOT(newTable()));

    d_btn_explorer_new_graph = new QToolButton(d_explorer_container);
    d_btn_explorer_new_graph->setAutoRaise(true);
    d_btn_explorer_new_graph->setIcon(IconLoader::load("new-graph"));
    d_btn_explorer_new_graph->setToolTip(tr("New Graph"));
    connect(d_btn_explorer_new_graph, SIGNAL(clicked()), this, SLOT(newGraph()));

    d_btn_tabbed_view = new QToolButton(d_explorer_container);
    d_btn_tabbed_view->setObjectName("explorerTabbedViewButton");
    d_btn_tabbed_view->setAutoRaise(true);
    d_btn_tabbed_view->setFixedHeight(26);
    d_btn_tabbed_view->setText(tr("Tabbed View"));
    d_btn_tabbed_view->setCheckable(true);
    d_btn_tabbed_view->setToolTip(tr("Toggle Tabbed Workspace View"));
    connect(d_btn_tabbed_view, SIGNAL(toggled(bool)), this, SLOT(setTabbedWorkspaceView(bool)));

    explorerMainLayout->addLayout(d_explorer_bar_grid);
    explorerMainLayout->addWidget(explorerSplitter, 1);

    d_explorer_container->installEventFilter(this);
    lv.installEventFilter(this);
    connect(&explorerWindow, &QDockWidget::dockLocationChanged, this,
            [this](Qt::DockWidgetArea) { updateExplorerAdaptiveLayout(false); });
    connect(&explorerWindow, &QDockWidget::topLevelChanged, this,
            [this](bool) { updateExplorerAdaptiveLayout(false); });
    connect(&explorerWindow, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (d_suppress_explorer_user_track || d_headless_mode)
            return;
        if (!this->isVisible())
            return;
        const bool isSplit = d_modern_ribbon
                && (d_modern_ribbon->workspaceLayoutMode()
                    == ModernRibbonBar::LayoutSplitTableGraph);
        if (isSplit && width() < 1480) {
            d_explorer_user_shown_in_narrow_split = visible;
            d_explorer_auto_collapsed_for_split = false;
        }
    });
    updateExplorerAdaptiveLayout();

    explorerWindow.setWidget(d_explorer_container);
    explorerWindow.hide();

    logWindow.setObjectName("logWindow"); // this is needed for QMainWindow::restoreState()
    logWindow.setWindowTitle(tr("Results Log"));
    logWindow.setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea
                              | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, &logWindow);

    results->setReadOnly(true);

    logWindow.setWidget(results);
    logWindow.hide();

#ifdef SCRIPTING_CONSOLE
    consoleWindow.setObjectName("consoleWindow"); // this is needed for QMainWindow::restoreState()
    consoleWindow.setWindowTitle(tr("Scripting Console"));
    consoleWindow.setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea
                                  | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, &consoleWindow);
    console.setReadOnly(true);
    consoleWindow.setWidget(&console);
    consoleWindow.hide();
#endif

    d_graph_inspector_dock = new GraphInspectorDock(this, this);
    addDockWidget(Qt::RightDockWidgetArea, d_graph_inspector_dock);
    connect(d_graph_inspector_dock, &GraphInspectorDock::userPinToggled, this,
            [this](bool pinned) {
                d_inspector_user_pinned = pinned;
                updateGraphInspectorResponsiveness();
            });
    connect(d_graph_inspector_dock, &GraphInspectorDock::closeRequestedByUser, this,
            [this]() {
                d_inspector_user_closed = true;
                updateGraphInspectorResponsiveness();
            });
    connect(d_graph_inspector_dock, &GraphInspectorDock::collapsedStateChanged, this,
            [this](bool) {
                syncGraphPageAndToolbarMenuState();
                if (d_modern_ribbon) {
                    d_modern_ribbon->syncCheckableStates();
                    if (d_modern_ribbon->workspaceLayoutMode()
                        == ModernRibbonBar::LayoutSplitTableGraph) {
                        d_modern_ribbon->applySplitWorkspaceGeometry();
                    }
                }
            });

    // Needs to be done after initialization of dock windows,
    // because we now use QDockWidget::toggleViewAction()
    createActions();
    initToolBars();
    initPlot3DToolBar();
    initMainMenu();

    d_workspace.setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d_workspace.setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d_workspace.setActivationOrder(QMdiArea::ActivationHistoryOrder);

    d_central_container = new QWidget(this);
    d_central_container->setObjectName(QStringLiteral("centralWorkspaceContainer"));
    QVBoxLayout *centralLayout = new QVBoxLayout(d_central_container);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(&d_workspace, 1);
    setCentralWidget(d_central_container);
    setAcceptDrops(true);

    d_ribbon_toolbar = new QToolBar(tr("Modern Ribbon"), this);
    d_ribbon_toolbar->setObjectName(QStringLiteral("ModernRibbonToolBar"));
    d_ribbon_toolbar->setMovable(false);
    d_ribbon_toolbar->setFloatable(false);
    d_ribbon_toolbar->setAllowedAreas(Qt::TopToolBarArea);
    d_ribbon_toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    if (d_ribbon_toolbar->layout()) {
        d_ribbon_toolbar->layout()->setContentsMargins(0, 0, 0, 0);
        d_ribbon_toolbar->layout()->setSpacing(0);
    }
    if (QAction *tva = d_ribbon_toolbar->toggleViewAction()) {
        tva->setVisible(false);
        tva->setEnabled(false);
    }
    if (file_tools) {
        insertToolBar(file_tools, d_ribbon_toolbar);
        insertToolBarBreak(file_tools);
    } else {
        addToolBar(Qt::TopToolBarArea, d_ribbon_toolbar);
        addToolBarBreak(Qt::TopToolBarArea);
    }

    d_modern_ribbon = new ModernRibbonBar(this, d_ribbon_toolbar);
    d_ribbon_toolbar->addWidget(d_modern_ribbon);

    readSettings();
    if (autoSave && autoSaveTime > 0)
        savingTimerId = startTimer(autoSaveTime * 60000);
    d_ribbon_toolbar->setVisible(true);
    updateExplorerAdaptiveLayout();
    createLanguagesList();
    installUxNavigation();

    d_auto_updater = new AutoUpdater(this, this);
    d_python_server = new PythonAutomationServer(this, this);

    connect(d_modern_ribbon, &ModernRibbonBar::focusHudModeChanged, this, [this](bool enabled) {
        if (actionToggleFocusHudMode && actionToggleFocusHudMode->isChecked() != enabled) {
            QSignalBlocker blocker(actionToggleFocusHudMode);
            actionToggleFocusHudMode->setChecked(enabled);
        }
        updateGraphInspectorResponsiveness();
    });
    connect(d_modern_ribbon, &ModernRibbonBar::workspaceLayoutModeChanged, this,
            [this](ModernRibbonBar::WorkspaceLayoutMode mode) {
                if (actionWorkspaceLayoutTabbed) {
                    QSignalBlocker b(actionWorkspaceLayoutTabbed);
                    actionWorkspaceLayoutTabbed->setChecked(mode == ModernRibbonBar::LayoutTabbed);
                }
                if (actionWorkspaceLayoutSplit) {
                    QSignalBlocker b(actionWorkspaceLayoutSplit);
                    actionWorkspaceLayoutSplit->setChecked(
                            mode == ModernRibbonBar::LayoutSplitTableGraph);
                }
                if (actionWorkspaceLayoutMdi) {
                    QSignalBlocker b(actionWorkspaceLayoutMdi);
                    actionWorkspaceLayoutMdi->setChecked(mode == ModernRibbonBar::LayoutFreeMdi);
                }
                updateGraphInspectorResponsiveness();
                updateExplorerAdaptiveLayout();
                windowActivated(d_workspace.activeSubWindow());
            });
    connect(d_modern_ribbon, &ModernRibbonBar::openPythonConsoleRequested, this,
            &ApplicationWindow::openPythonConsole);
    connect(d_modern_ribbon, &ModernRibbonBar::runPythonScriptRequested, this,
            &ApplicationWindow::runPythonScriptDialog);
    connect(d_modern_ribbon, &ModernRibbonBar::checkForUpdatesRequested, this,
            &ApplicationWindow::checkForUpdatesManual);

    d_python_server->setCustomCommandHook(
            [this](const QString &method, const QJsonObject &params, bool *handled) -> QJsonObject {
                if (handled)
                    *handled = false;
                if (method == QStringLiteral("app.set_focus_hud")) {
                    if (handled)
                        *handled = true;
                    const bool cur = d_modern_ribbon ? d_modern_ribbon->isFocusHudMode() : false;
                    const bool enabled = params.contains(QStringLiteral("enabled"))
                            ? params.value(QStringLiteral("enabled")).toBool()
                            : !cur;
                    if (d_modern_ribbon)
                        d_modern_ribbon->setFocusHudMode(enabled);
                    QJsonObject out;
                    out[QStringLiteral("ok")] = true;
                    out[QStringLiteral("enabled")] =
                            d_modern_ribbon ? d_modern_ribbon->isFocusHudMode() : enabled;
                    return out;
                }
                if (method == QStringLiteral("app.get_focus_hud")) {
                    if (handled)
                        *handled = true;
                    QJsonObject out;
                    out[QStringLiteral("ok")] = true;
                    out[QStringLiteral("enabled")] =
                            d_modern_ribbon ? d_modern_ribbon->isFocusHudMode() : false;
                    return out;
                }
                if (method == QStringLiteral("app.set_workspace_mode")) {
                    if (!d_modern_ribbon)
                        return QJsonObject();
                    const QString mode =
                            params.value(QStringLiteral("mode")).toString().trimmed().toLower();
                    if (!d_modern_ribbon->setWorkspaceLayoutModeByName(mode))
                        return QJsonObject();
                    if (handled)
                        *handled = true;
                    QJsonObject out;
                    out[QStringLiteral("ok")] = true;
                    out[QStringLiteral("mode")] = (mode == QStringLiteral("tabs"))
                            ? QStringLiteral("tabs")
                            : d_modern_ribbon->workspaceLayoutModeName();
                    return out;
                }
                if (method == QStringLiteral("app.check_updates") && d_auto_updater) {
                    if (handled)
                        *handled = true;
                    QString rawUrl = params.value(QStringLiteral("feed_url")).toString().trimmed();
                    if (rawUrl.isEmpty())
                        rawUrl = params.value(QStringLiteral("url")).toString().trimmed();
                    QUrl overrideUrl;
                    if (!rawUrl.isEmpty()) {
                        if (QFileInfo::exists(rawUrl))
                            overrideUrl = QUrl::fromLocalFile(QFileInfo(rawUrl).absoluteFilePath());
                        else
                            overrideUrl = QUrl::fromUserInput(rawUrl);
                    }
                    const bool prevPre = d_auto_updater->includePreReleases();
                    const bool hasPreOverride =
                            params.contains(QStringLiteral("include_prerelease"));
                    if (hasPreOverride) {
                        d_auto_updater->setIncludePreReleases(
                                params.value(QStringLiteral("include_prerelease")).toBool(false));
                    }
                    const int timeoutMs = params.value(QStringLiteral("timeout_ms")).toInt(8000);
                    QJsonObject res = d_auto_updater->checkFeedBlocking(overrideUrl, timeoutMs);
                    if (hasPreOverride)
                        d_auto_updater->setIncludePreReleases(prevPre);
                    if (params.contains(QStringLiteral("current_version"))) {
                        const QString simCur =
                                params.value(QStringLiteral("current_version")).toString().trimmed();
                        res[QStringLiteral("current_version")] = simCur;
                        res[QStringLiteral("update_available")] = res[QStringLiteral("ok")].toBool()
                                && (AutoUpdater::compareSemVer(
                                            res[QStringLiteral("latest_version")].toString(),
                                            simCur)
                                    > 0);
                    }
                    return res;
                }
                return QJsonObject();
            });

    d_modern_ribbon->setSplitLeftRatio(d_saved_split_left_ratio);
    if (!d_saved_workspace_layout_mode.isEmpty()
        && d_modern_ribbon->setWorkspaceLayoutModeByName(d_saved_workspace_layout_mode)) {
        // Restored saved workspace layout mode (tabbed / split / mdi)
    } else {
        d_modern_ribbon->setWorkspaceLayoutMode(isTabbedWorkspaceView()
                                                        ? ModernRibbonBar::LayoutTabbed
                                                        : ModernRibbonBar::LayoutFreeMdi);
    }
    d_modern_ribbon->setClassicToolbarsVisible(false);

    insertTranslatedStrings();

    actionNextWindow->setShortcut(tr("F5", "next window shortcut"));
    connect(actionNextWindow, SIGNAL(triggered()), &d_workspace, SLOT(activateNextSubWindow()));

    actionPrevWindow->setShortcut(tr("F6", "previous window shortcut"));
    connect(actionPrevWindow, SIGNAL(triggered()), &d_workspace, SLOT(activatePreviousSubWindow()));

    // Attach core shortcut actions directly to ApplicationWindow so shortcuts continue working
    // in Focus HUD (F11) when menuBar() and toolbars are hidden.
    QAction *const coreShortcutActions[] = {
        actionNewProject,      actionNewGraph,        actionNewNote,
        actionNewTable,        actionNewMatrix,       actionNewFunctionPlot,
        actionNewSurfacePlot,  actionOpen,            actionLoadImage,
        actionSaveProject,     actionSaveProjectAs,   actionLoad,
        actionUndo,            actionRedo,            actionCutSelection,
        actionCopySelection,   actionPasteSelection,  actionClearSelection,
        actionCopyGraphOle,    actionCopyPageImage,   actionCopyGraphFormat,
        actionPasteGraphFormat, actionShowExplorer,   actionAddLayer,
        actionShowLayerDialog, actionExportGraph,     actionExportAllGraphs,
        actionExportPDF,       actionPrint,           actionCloseAllWindows,
        actionShowCurvesDialog, actionAddErrorBars,   actionAddFunctionCurve,
        actionUnzoom,          actionNewLegend,       actionAddImage,
        actionShowFitDialog,   actionShowHelp,        actionCloseWindow,
        actionDeleteLayer,     actionPeakAnalyzerDialog, actionRecalculateNow,
        actionAddText,         btnArrow,              btnLine,
        btnZoomIn,             btnZoomOut,            btnCursor,
        btnSelect,             btnMovePoints,         btnRemovePoints,
        btnMaskPoints,         btnUnmaskPoints,       actionNextWindow,
        actionPrevWindow,      actionNoteExecute,     actionNoteExecuteAll,
        actionNoteEvaluate
    };
    for (QAction *act : coreShortcutActions) {
        if (act && !this->actions().contains(act))
            this->addAction(act);
    }

    connect(this, SIGNAL(modified()), this, SLOT(modifiedProject()));
    connect(&d_workspace, SIGNAL(subWindowActivated(QMdiSubWindow *)), this,
            SLOT(windowActivated(QMdiSubWindow *)));
    connect(&lv, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this,
            SLOT(folderItemDoubleClicked(QTreeWidgetItem *, int)));
    connect(&lv, SIGNAL(customContextMenuRequested(const QPoint &)), this,
            SLOT(showWindowPopupMenu(const QPoint &)));
    connect(&lv, SIGNAL(dragItems(QList<QTreeWidgetItem *>)), this,
            SLOT(dragFolderItems(QList<QTreeWidgetItem *>)));
    connect(&lv, SIGNAL(dropItems(QTreeWidgetItem *)), this,
            SLOT(dropFolderItems(QTreeWidgetItem *)));
    connect(&lv, SIGNAL(renameItem(QTreeWidgetItem *, int)), this,
            SLOT(startRenameFolder(QTreeWidgetItem *, int)));
    connect(&lv, SIGNAL(addFolderItem()), this, SLOT(addFolder()));
    connect(&lv, SIGNAL(deleteSelection()), this, SLOT(deleteSelectedItems()));
    connect(&lv, SIGNAL(itemRenamed(QTreeWidgetItem *, int, const QString &)), this,
            SLOT(renameWindow(QTreeWidgetItem *, int, const QString &)));
    connect(scriptEnv, SIGNAL(error(const QString &, const QString &, int)), this,
            SLOT(scriptError(const QString &, const QString &, int)));
    connect(scriptEnv, SIGNAL(print(const QString &)), this, SLOT(scriptPrint(const QString &)));

    // this has to be done after connecting scriptEnv
    scriptEnv->initialize();

    lv.setDragEnabled(true);
    lv.setAcceptDrops(true);
    lv.setDefaultDropAction(Qt::MoveAction);
    folders.setDragEnabled(true);
    folders.setAcceptDrops(true);
    folders.setDefaultDropAction(Qt::MoveAction);

    connect(d_project->undoStack(), &QUndoStack::canUndoChanged, this,
            [this](bool) { updateUndoRedoActionsForActiveWindow(); });
    connect(d_project->undoStack(), &QUndoStack::canRedoChanged, this,
            [this](bool) { updateUndoRedoActionsForActiveWindow(); });
    connect(d_project->undoStack(), &QUndoStack::indexChanged, this,
            [this](int) { updateUndoRedoActionsForActiveWindow(); });

    {
        PeakAnchorEventFilter *winResizeFilter = new PeakAnchorEventFilter(this);
        winResizeFilter->handler = [this](QObject *watched, QEvent *event) -> bool {
            if (watched == this && event) {
                const QEvent::Type et = event->type();
                if (et == QEvent::Resize || et == QEvent::Show
                    || et == QEvent::WindowStateChange) {
                    updateGraphInspectorResponsiveness();
                    updateEmptyGraphPanePlaceholder();
                    if (et == QEvent::Resize || et == QEvent::Show)
                        updateExplorerAdaptiveLayout();
                }
            } else if (watched == d_workspace.viewport() && event
                       && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
                updateEmptyGraphPanePlaceholder();
            }
            return false;
        };
        this->installEventFilter(winResizeFilter);
        if (d_workspace.viewport())
            d_workspace.viewport()->installEventFilter(winResizeFilter);
    }

    if (RecalcManager *rm = RecalcManager::instance(this)) {
        connect(rm, &RecalcManager::linkFailed, this,
                [this](Graph *failedGraph, const QString &msg) {
                    Q_UNUSED(failedGraph)
                    if (msg.isEmpty())
                        return;
                    updateLog(QStringLiteral("[Recalc] ") + msg + QLatin1Char('\n'));
                    if (statusBar()) {
                        const QString prefix = tr("Recalc failed: %1").arg(QString());
                        const QString shown = (!prefix.isEmpty() && msg.startsWith(prefix))
                                ? msg
                                : tr("Recalc failed: %1").arg(msg);
                        statusBar()->showMessage(shown, 12000);
                    }
                });
    }

    updateGraphInspectorResponsiveness();
    updateEmptyGraphPanePlaceholder();
    updateUndoRedoActionsForActiveWindow();
}

void ApplicationWindow::setExplorerSearchText(const QString &text)
{
    if (d_explorer_search && d_explorer_search->text() != text)
        d_explorer_search->setText(text);
    filterExplorerWindows();
}

void ApplicationWindow::setExplorerTypeFilter(int filterIndex)
{
    if (d_explorer_type_filter && d_explorer_type_filter->currentIndex() != filterIndex)
        d_explorer_type_filter->setCurrentIndex(filterIndex);
    filterExplorerWindows();
}

void ApplicationWindow::filterExplorerWindows()
{
    if (!current_folder)
        return;

    const QString searchText =
            d_explorer_search ? d_explorer_search->text().trimmed() : QString();
    const int typeFilter =
            d_explorer_type_filter ? d_explorer_type_filter->currentIndex() : 0;
    const bool includeSubfolders =
            d_explorer_subfolder_check && d_explorer_subfolder_check->isChecked();

    // Ensure subfolder items are populated in lv when includeSubfolders is active
    QSet<MyWidget *> existingWindowItems;
    for (int i = 0; i < lv.topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = lv.topLevelItem(i);
        if (it && it->type() == WindowListItem::WindowType)
            existingWindowItems.insert(static_cast<WindowListItem *>(it)->window());
    }

    if (includeSubfolders) {
        QList<MyWidget *> subWins =
                current_folder->findWindows(QString(), true, true, false, true, true);
        for (MyWidget *w : subWins) {
            if (w && !isInternalHelperWindow(w) && !existingWindowItems.contains(w)) {
                addListViewItem(w);
                existingWindowItems.insert(w);
            }
        }
    } else {
        // Remove items from subfolders that do not belong to current_folder
        const QList<MyWidget *> curWins = current_folder->windowsList();
        const QSet<MyWidget *> curWinSet(curWins.begin(), curWins.end());
        for (int i = lv.topLevelItemCount() - 1; i >= 0; --i) {
            QTreeWidgetItem *it = lv.topLevelItem(i);
            if (it && it->type() == WindowListItem::WindowType) {
                MyWidget *w = static_cast<WindowListItem *>(it)->window();
                if (w && (!curWinSet.contains(w) || isInternalHelperWindow(w)))
                    delete lv.takeTopLevelItem(i);
            }
        }
    }

    for (int i = 0; i < lv.topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = lv.topLevelItem(i);
        if (!it)
            continue;

        bool typeMatches = true;
        if (it->type() == FolderListItem::FolderType) {
            typeMatches = (typeFilter == 0 || typeFilter == 5);
        } else if (it->type() == WindowListItem::WindowType) {
            MyWidget *w = static_cast<WindowListItem *>(it)->window();
            if (!w || isInternalHelperWindow(w)) {
                typeMatches = false;
            } else {
                switch (typeFilter) {
                case 0:
                    typeMatches = true;
                    break;
                case 1:
                    typeMatches = w->inherits("Table");
                    break;
                case 2:
                    typeMatches = (w->inherits("MultiLayer") || w->inherits("Graph3D"));
                    break;
                case 3:
                    typeMatches = w->inherits("Matrix");
                    break;
                case 4:
                    typeMatches = w->inherits("Note");
                    break;
                case 5:
                    typeMatches = false;
                    break;
                default:
                    typeMatches = true;
                    break;
                }
            }
        }

        bool textMatches = true;
        if (!searchText.isEmpty()) {
            const QString nameStr = it->text(0);
            const QString typeStr = it->text(1);
            const QString labelStr = it->text(4);
            textMatches = nameStr.contains(searchText, Qt::CaseInsensitive)
                    || typeStr.contains(searchText, Qt::CaseInsensitive)
                    || labelStr.contains(searchText, Qt::CaseInsensitive);
        }

        it->setHidden(!(typeMatches && textMatches));
    }
}

void ApplicationWindow::setTabbedWorkspaceView(bool tabbed)
{
    d_workspace.setViewMode(tabbed ? QMdiArea::TabbedView : QMdiArea::SubWindowView);
    if (tabbed) {
        d_workspace.setTabsClosable(true);
        d_workspace.setTabsMovable(true);
        d_workspace.setTabShape(QTabWidget::Rounded);
    }
    if (d_btn_tabbed_view && d_btn_tabbed_view->isChecked() != tabbed) {
        d_btn_tabbed_view->blockSignals(true);
        d_btn_tabbed_view->setChecked(tabbed);
        d_btn_tabbed_view->blockSignals(false);
    }
    if (actionTabbedWorkspaceView && actionTabbedWorkspaceView->isChecked() != tabbed) {
        actionTabbedWorkspaceView->blockSignals(true);
        actionTabbedWorkspaceView->setChecked(tabbed);
        actionTabbedWorkspaceView->blockSignals(false);
    }
    if (actionWorkspaceLayoutTabbed && tabbed) {
        QSignalBlocker b(actionWorkspaceLayoutTabbed);
        actionWorkspaceLayoutTabbed->setChecked(true);
    } else if (!tabbed && actionWorkspaceLayoutMdi
               && (!d_modern_ribbon
                   || d_modern_ribbon->workspaceLayoutMode()
                           != ModernRibbonBar::LayoutSplitTableGraph)) {
        QSignalBlocker b(actionWorkspaceLayoutMdi);
        actionWorkspaceLayoutMdi->setChecked(true);
    }
    if (d_modern_ribbon)
        d_modern_ribbon->syncWorkspaceModeButtons(tabbed);
    scheduleSyncWorkspaceHiddenTabs(this);
}

void ApplicationWindow::toggleTabbedWorkspaceView()
{
    setTabbedWorkspaceView(!isTabbedWorkspaceView());
}

void ApplicationWindow::renameListViewItem(const QString &oldName, const QString &newName)
{
    QTreeWidgetItem *it = nullptr;
    const int topCnt = lv.topLevelItemCount();
    if (topCnt > 0 && lv.topLevelItem(topCnt - 1)->text(0) == oldName)
        it = lv.topLevelItem(topCnt - 1);
    else
        it = lv.findItems(oldName, Qt::MatchExactly | Qt::MatchCaseSensitive).value(0);
    if (it)
        it->setText(0, newName);
    if (d_batch_window_updates_depth > 0)
        d_batch_pending_explorer_filter = true;
    else
        filterExplorerWindows();
}

void ApplicationWindow::setListViewLabel(const QString &caption, const QString &label)
{
    QTreeWidgetItem *it = nullptr;
    const int topCnt = lv.topLevelItemCount();
    if (topCnt > 0 && lv.topLevelItem(topCnt - 1)->text(0) == caption)
        it = lv.topLevelItem(topCnt - 1);
    else
        it = lv.findItems(caption, Qt::MatchExactly | Qt::MatchCaseSensitive).value(0);
    if (it)
        it->setText(4, label);
    if (d_batch_window_updates_depth > 0)
        d_batch_pending_explorer_filter = true;
    else
        filterExplorerWindows();
}

void ApplicationWindow::setListViewDate(const QString &caption, const QString &date)
{
    QTreeWidgetItem *it = nullptr;
    const int topCnt = lv.topLevelItemCount();
    if (topCnt > 0 && lv.topLevelItem(topCnt - 1)->text(0) == caption)
        it = lv.topLevelItem(topCnt - 1);
    else
        it = lv.findItems(caption, Qt::MatchExactly | Qt::MatchCaseSensitive).value(0);
    if (it)
        it->setText(3, date);
}

void ApplicationWindow::setListView(const QString &caption, const QString &view)
{
    QTreeWidgetItem *it = nullptr;
    const int topCnt = lv.topLevelItemCount();
    if (topCnt > 0 && lv.topLevelItem(topCnt - 1)->text(0) == caption)
        it = lv.topLevelItem(topCnt - 1);
    else
        it = lv.findItems(caption, Qt::MatchExactly | Qt::MatchCaseSensitive).value(0);
    if (it)
        it->setText(2, view);
}

void ApplicationWindow::beginBatchWindowUpdates()
{
    if (d_batch_window_updates_depth++ == 0) {
        d_batch_pending_explorer_filter = false;
        d_batch_pending_lv_resize = false;
        d_batch_pending_auto_axis_refresh = false;
        d_batch_hidden_set = QSet<MyWidget *>(hiddenWindows.begin(), hiddenWindows.end());
        d_batch_widget_name.clear();
        d_batch_name_counts.clear();
        d_batch_unique_seq.clear();
        for (MyWidget *w : windowsList())
            syncBatchWindowName(w);
        for (MyWidget *hw : hiddenWindows)
            syncBatchWindowName(hw);
    }
}

void ApplicationWindow::endBatchWindowUpdates()
{
    if (d_batch_window_updates_depth <= 0)
        return;
    if (--d_batch_window_updates_depth > 0)
        return;
    d_batch_hidden_set.clear();
    d_batch_widget_name.clear();
    d_batch_name_counts.clear();
    d_batch_unique_seq.clear();
    if (d_batch_pending_explorer_filter) {
        d_batch_pending_explorer_filter = false;
        filterExplorerWindows();
    }
    if (d_batch_pending_lv_resize) {
        d_batch_pending_lv_resize = false;
        if (lv.columnCount() >= 5 && lv.isColumnHidden(3)) {
            lv.resizeColumnToContents(1);
            lv.resizeColumnToContents(2);
        }
    }
    if (d_batch_pending_auto_axis_refresh) {
        d_batch_pending_auto_axis_refresh = false;
        refreshAllGraphsAutoAxisTitles();
    }
    scheduleSyncWorkspaceHiddenTabs(this);
}

void ApplicationWindow::syncBatchWindowName(MyWidget *w)
{
    if (d_batch_window_updates_depth <= 0 || !w)
        return;
    const QString cur = w->name();
    const auto it = d_batch_widget_name.find(w);
    if (it != d_batch_widget_name.end()) {
        if (it.value() == cur)
            return;
        const int cnt = d_batch_name_counts.value(it.value(), 0) - 1;
        if (cnt <= 0)
            d_batch_name_counts.remove(it.value());
        else
            d_batch_name_counts.insert(it.value(), cnt);
    }
    if (!cur.isEmpty()) {
        d_batch_widget_name.insert(w, cur);
        d_batch_name_counts[cur] += 1;
    } else if (it != d_batch_widget_name.end()) {
        d_batch_widget_name.erase(it);
    }
}

void ApplicationWindow::removeBatchWindow(MyWidget *w)
{
    if (d_batch_window_updates_depth <= 0 || !w)
        return;
    d_batch_hidden_set.remove(w);
    const auto it = d_batch_widget_name.find(w);
    if (it != d_batch_widget_name.end()) {
        const int cnt = d_batch_name_counts.value(it.value(), 0) - 1;
        if (cnt <= 0)
            d_batch_name_counts.remove(it.value());
        else
            d_batch_name_counts.insert(it.value(), cnt);
        d_batch_widget_name.erase(it);
    }
}

bool ApplicationWindow::isBatchNameUsedByOther(const QString &candName, MyWidget *exclude) const
{
    if (candName.isEmpty())
        return false;
    int cnt = d_batch_name_counts.value(candName, 0);
    if (cnt <= 0)
        return false;
    if (exclude && d_batch_widget_name.value(exclude) == candName)
        --cnt;
    return cnt > 0;
}

QString ApplicationWindow::nextBatchUniqueName(const QString &base, MyWidget *exclude)
{
    int &idx = d_batch_unique_seq[base];
    if (idx < 1)
        idx = 1;
    QString cand = base + QString::number(idx);
    while (isBatchNameUsedByOther(cand, exclude))
        cand = base + QString::number(++idx);
    return cand;
}

MyWidget *ApplicationWindow::window(const QString &name)
{
    if (isBeingDestroyed(this))
        return nullptr; // the folder tree is (being) torn down
    QList<MyWidget *> windows = windowsList();
    for (int i = 0; i < windows.count(); i++) {
        MyWidget *widget = windows.at(i);
        if (widget && widget->name() == name)
            return widget;
    }
    return nullptr;
}

Table *ApplicationWindow::table(const QString &name)
{
    if (name.isEmpty())
        return 0;

    QString lookupName = name;
    if (renamedLookupSuppressionDepth() == 0 && !renamedTables.isEmpty()) {
        int exactIdx = -1;
        int bestPrefixIdx = -1;
        int bestPrefixLen = -1;
        for (int k = 0; k + 1 < renamedTables.size(); k += 2) {
            const QString &oldCap = renamedTables.at(k);
            if (oldCap == lookupName) {
                exactIdx = k;
            }
            if (!oldCap.isEmpty() && lookupName.startsWith(oldCap + QLatin1Char('_'))
                && oldCap.length() >= bestPrefixLen) {
                bestPrefixIdx = k;
                bestPrefixLen = oldCap.length();
            }
        }
        if (exactIdx >= 0) {
            lookupName = renamedTables.at(exactIdx + 1);
        } else if (bestPrefixIdx >= 0) {
            const QString &oldCap = renamedTables.at(bestPrefixIdx);
            lookupName = renamedTables.at(bestPrefixIdx + 1) + lookupName.mid(oldCap.length());
        }
    }

    QList<MyWidget *> lst = windowsList();
    for (QWidget *hw : hiddenWindows) {
        if (MyWidget *mw = qobject_cast<MyWidget *>(hw)) {
            if (!lst.contains(mw))
                lst.append(mw);
        }
    }

    // 1. Exact table name match (handles table names containing '_')
    for (MyWidget *w : lst) {
        if (w && w->inherits("Table")) {
            Table *t = static_cast<Table *>(w);
            if (t->name() == lookupName)
                return t;
        }
    }

    // 2. Longest TableName_ColumnName prefix match (preferring table that owns the column)
    Table *bestWithCol = nullptr;
    int bestWithColLen = -1;
    Table *bestPrefix = nullptr;
    int bestPrefixLen = -1;
    for (MyWidget *w : lst) {
        if (!w || !w->inherits("Table"))
            continue;
        Table *t = static_cast<Table *>(w);
        const QString tName = t->name();
        if (tName.isEmpty() || !lookupName.startsWith(tName + QLatin1Char('_')))
            continue;
        const int tLen = tName.length();
        const QString colPart = lookupName.mid(tLen + 1);
        if (t->column(colPart) != nullptr) {
            if (tLen > bestWithColLen) {
                bestWithCol = t;
                bestWithColLen = tLen;
            }
        }
        if (tLen > bestPrefixLen) {
            bestPrefix = t;
            bestPrefixLen = tLen;
        }
    }
    return bestWithCol ? bestWithCol : bestPrefix;
}

Matrix *ApplicationWindow::matrix(const QString &name)
{
    if (name.isEmpty())
        return 0;

    QString caption = name;
    if (renamedLookupSuppressionDepth() == 0 && !renamedTables.isEmpty()) {
        for (int k = 0; k + 1 < renamedTables.size(); k += 2) {
            if (renamedTables.at(k) == name)
                caption = renamedTables.at(k + 1);
        }
    }

    QList<MyWidget *> lst = windowsList();
    for (QWidget *hw : hiddenWindows) {
        if (MyWidget *mw = qobject_cast<MyWidget *>(hw)) {
            if (!lst.contains(mw))
                lst.append(mw);
        }
    }
    foreach (MyWidget *w, lst) {
        if (w && w->inherits("Matrix") && static_cast<Matrix *>(w)->name() == caption) {
            return (Matrix *)w;
        }
    }
    return 0;
}

void ApplicationWindow::windowActivated(QMdiSubWindow *w)
{
    if (GraphMiniToolbar *mt = GraphMiniToolbar::instance())
        mt->cancelScheduledAndHide();

    if (d_modern_ribbon)
        d_modern_ribbon->updateActiveContext(qobject_cast<MyWidget *>(w));

    MultiLayer *targetMl = qobject_cast<MultiLayer *>(w);
    if (!targetMl && w && d_modern_ribbon
        && d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
        const QList<QMdiSubWindow *> history =
                d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = history.size() - 1; i >= 0; --i) {
            MultiLayer *cand = qobject_cast<MultiLayer *>(history.at(i));
            if (!cand || hiddenWindows.contains(cand) || outWindows.contains(cand))
                continue;
            if (current_folder && cand->folder() && cand->folder() != current_folder)
                continue;
            if (cand->isVisible()) {
                targetMl = cand;
                break;
            }
        }
    }

    if (d_graph_inspector_dock)
        d_graph_inspector_dock->setTargetMultiLayer(targetMl);

    updateGraphInspectorResponsiveness();
    updateUndoRedoActionsForActiveWindow();
    updateStatusBarForWindow(d_status_info, w);

    if (!w || !w->inherits("MyWidget")) {
        if (!w) {
            customToolBars(nullptr);
            customMenu(nullptr);
            syncExplorerSelectionToWindow(lv, nullptr);
        }
        syncGraphPageAndToolbarMenuState();
        return;
    }

    customToolBars((MyWidget *)w);
    customMenu((MyWidget *)w);

    Folder *f = ((MyWidget *)w)->folder();
    if (f)
        f->setActiveWindow((MyWidget *)w);

    syncExplorerSelectionToWindow(lv, static_cast<MyWidget *>(w));
    syncGraphPageAndToolbarMenuState();
}

void ApplicationWindow::executeNotes()
{
    QList<MyWidget *> lst = projectFolder()->windowsList();
    foreach (MyWidget *widget, lst)
        if (widget->inherits("Note") && ((Note *)widget)->autoexec())
            ((Note *)widget)->executeAll();
}

void ApplicationWindow::scriptError(const QString &message, const QString &scriptName,
                                    int lineNumber)
{
    Q_UNUSED(scriptName)
    Q_UNUSED(lineNumber)
    if (isNonInteractiveContext(this))
        return;
    QMessageBox::critical(this, tr("ToriGraph") + " - " + tr("Script Error"), message);
}

void ApplicationWindow::scriptPrint(const QString &text)
{
#ifdef SCRIPTING_CONSOLE
    if (!text.isEmpty())
        console.insertPlainText(text);
#else
    printf(text.toUtf8().constData());
#endif
}

bool ApplicationWindow::setScriptingLang(const QString &lang, bool force, bool batch)
{
    if (!force && lang == scriptEnv->objectName())
        return true;
    if (lang.isEmpty())
        return false;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    ScriptingEnv *newEnv = ScriptingLangManager::newEnv(lang.toStdString(), this, batch);
    if (!newEnv) {
        QApplication::restoreOverrideCursor();
        return false;
    }

    connect(newEnv, SIGNAL(error(const QString &, const QString &, int)), this,
            SLOT(scriptError(const QString &, const QString &, int)));
    connect(newEnv, SIGNAL(print(const QString &)), this, SLOT(scriptPrint(const QString &)));
    if (!newEnv->initialize()) {
        QApplication::restoreOverrideCursor();
        return false;
    }

    // notify everyone who might be interested
    ScriptingChangeEvent sce(newEnv);
    QApplication::sendEvent(this, &sce);

    foreach (QObject *i, findChildren<QWidget *>())
        QApplication::postEvent(i, new ScriptingChangeEvent(newEnv));

    QApplication::restoreOverrideCursor();

    return true;
}

void ApplicationWindow::showScriptingLangDialog()
{
    if (isNonInteractiveContext(this))
        return;
    ScriptingLangDialog *d = new ScriptingLangDialog(scriptEnv, this);
    d->showNormal();
    d->activateWindow();
}

void ApplicationWindow::restartScriptingEnv()
{
    if (setScriptingLang(scriptEnv->objectName(), true))
        executeNotes();
    else if (!isNonInteractiveContext(this))
        QMessageBox::critical(
                this, tr("Scripting Error"),
                tr("Scripting language \"%1\" failed to initialize.").arg(scriptEnv->objectName()));
}

QString ApplicationWindow::windowGeometryInfo(MyWidget *w)
{
    QString s = "geometry\t";
    const bool hide = hidden(w) || w->status() == MyWidget::Hidden;
    if (w->status() == MyWidget::Maximized && !hide) {
        if (w == w->folder()->activeWindow())
            return s + "maximized\tactive\n";
        else
            return s + "maximized\n";
    }

    if (!w->parent())
        s += "0\t0\t500\t400\t";
    else {
        QPoint p = w->pos(); // store position
        s += QString::number(p.x()) + "\t";
        s += QString::number(p.y()) + "\t";
        s += QString::number(w->frameGeometry().width()) + "\t";
        s += QString::number(w->frameGeometry().height()) + "\t";
    }

    if (w->status() == MyWidget::Minimized && !hide)
        s += "minimized\t";

    if (w == w->folder()->activeWindow() && !hide)
        s += "active\n";
    else if (hide)
        s += "hidden\n";
    else
        s += "\n";
    return s;
}

void ApplicationWindow::restoreWindowGeometry(ApplicationWindow *app, MyWidget *w, const QString s)
{
    w->blockSignals(true);
    const QString caption = w->name();
    const QStringList lst = s.trimmed().split("\t");
    const bool isHidden = lst.contains("hidden");

    if (lst.contains("minimized")) {
        if (lst.count() > 4)
            w->setGeometry(lst[1].toInt(), lst[2].toInt(), lst[3].toInt(), lst[4].toInt());
        w->setStatus(MyWidget::Minimized);
        app->setListView(caption, tr("Minimized"));
    } else if (lst.contains("maximized") && !isHidden) {
        w->setStatus(MyWidget::Maximized);
        app->setListView(caption, tr("Maximized"));
    } else {
        if (lst.count() > 4)
            w->setGeometry(lst[1].toInt(), lst[2].toInt(), lst[3].toInt(), lst[4].toInt());
        w->setStatus(MyWidget::Normal);
    }

    if (isHidden)
        app->hideWindow(w);

    if (Folder *f = w->folder()) {
        if (lst.contains("active") && !isHidden) {
            if (MyWidget *prev = qobject_cast<MyWidget *>(
                        f->property("torigraph_saved_active_obj").value<QObject *>())) {
                if (prev != w)
                    prev->setProperty("torigraph_saved_active", QVariant());
            }
            w->setProperty("torigraph_saved_active", true);
            f->setProperty("torigraph_saved_active_obj", QVariant::fromValue<QObject *>(w));
            f->setActiveWindow(w);
        } else {
            MyWidget *savedActive = qobject_cast<MyWidget *>(
                    f->property("torigraph_saved_active_obj").value<QObject *>());
            if (savedActive
                && (app->hidden(savedActive)
                    || !savedActive->property("torigraph_saved_active").toBool())) {
                savedActive = nullptr;
            }
            if (savedActive) {
                f->setActiveWindow(savedActive);
            } else if (isHidden && f->activeWindow() == w) {
                MyWidget *fallback = nullptr;
                for (MyWidget *cand : f->windowsList()) {
                    if (cand && cand != w && !app->hidden(cand)
                        && !app->outWindows.contains(cand)) {
                        fallback = cand;
                        break;
                    }
                }
                f->setActiveWindow(fallback);
            }
        }
    }

    w->blockSignals(false);
}

Folder *ApplicationWindow::projectFolder()
{
    return ((FolderListItem *)folders.topLevelItem(0))->folder();
}

void ApplicationWindow::renameActiveWindow()
{
    if (isNonInteractiveContext(this))
        return;
    MyWidget *m = (MyWidget *)d_workspace.activeSubWindow();
    if (!m)
        return;

    RenameWindowDialog *rwd = new RenameWindowDialog(this);
    rwd->setAttribute(Qt::WA_DeleteOnClose);
    rwd->setWidget(m);
    rwd->exec();
}

void ApplicationWindow::renameWindow(QTreeWidgetItem *item, int, const QString &text)
{
    if (auto wli = dynamic_cast<WindowListItem *>(item))
        if (auto w = wli->window())
            if (text != w->name())
                renameWindow(w, text);
}

bool ApplicationWindow::renameWindow(MyWidget *w, const QString &text)
{
    if (!w)
        return false;

    const bool nonInteractive = isNonInteractiveContext(this);
    QString name = w->name();

    QString newName = text;
    newName.replace("-", "_");
    if (newName.isEmpty()) {
        if (!nonInteractive)
            QMessageBox::critical(this, tr("Error"), tr("Please enter a valid name!"));
        return false;
    } else if (newName.contains(QRegularExpression(
                       QStringLiteral("[^\\p{L}\\p{N}_]"),
                       QRegularExpression::UseUnicodePropertiesOption))) {
        if (!nonInteractive) {
            QMessageBox::critical(
                    this, tr("Error"),
                    tr("The name you chose is not valid: only letters and digits are allowed!") + "<p>"
                            + tr("Please choose another name!"));
        }
        return false;
    }

    newName.replace("_", "-");

    while (alreadyUsedName(newName)) {
        if (!nonInteractive) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Name <b>%1</b> already exists!").arg(newName) + "<p>"
                                          + tr("Please choose another name!") + "<p>"
                                          + tr("Warning: for internal consistency reasons the "
                                               "underscore character is replaced with a minus sign."));
        }
        return false;
    }

    if (w->inherits("Table")) {
        QStringList labels = ((Table *)w)->colNames();
        if (labels.contains(newName)) {
            if (!nonInteractive) {
                QMessageBox::critical(
                        this, tr("Error"),
                        tr("The table name must be different from the names of its columns!") + "<p>"
                                + tr("Please choose another name!"));
            }
            return false;
        }

        updateTableNames(name, newName);
    } else if (w->inherits("Matrix"))
        changeMatrixName(name, newName);

    w->setName(newName);
    syncBatchWindowName(w);
    w->setCaptionPolicy(w->captionPolicy());
    renameListViewItem(name, newName);
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == w)
        d_graph_inspector_dock->refreshAll();
    return true;
}

void ApplicationWindow::updateLog(const QString &result)
{
    if (!result.isEmpty()) {
        logInfo += result;
        showResults(true);
        emit modified();
    }
}

void ApplicationWindow::showResults(bool ok)
{
    if (ok) {
        if (!logInfo.isEmpty())
            results->setText(logInfo);
        else
            results->setText(tr("Sorry, there are no results to display!"));

        logWindow.show();
        if (!logWindow.isFloating() && dockWidgetArea(&logWindow) == Qt::BottomDockWidgetArea) {
            const int cappedH = qBound(96, height() / 5, 160);
            resizeDocks({ &logWindow }, { cappedH }, Qt::Vertical);
        }
        QTextCursor cur = results->textCursor();
        cur.movePosition(QTextCursor::End);
        results->setTextCursor(cur);
    } else
        logWindow.hide();
}

void ApplicationWindow::showResults(const QString &s, bool ok)
{
    logInfo += s;
    if (!logInfo.isEmpty())
        results->setText(logInfo);
    showResults(ok);
}

MyWidget *ApplicationWindow::clone()
{
    MyWidget *w = (MyWidget *)d_workspace.activeSubWindow();
    if (!w) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Duplicate window error"),
                                  tr("There are no windows available in this project!"));
        }
        return 0;
    }

    return clone(w);
}

MyWidget *ApplicationWindow::clone(MyWidget *w)
{
    if (!w)
        return 0;

    MyWidget *nw = 0;
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    if (w->inherits("MultiLayer")) {
        nw = multilayerPlot(generateUniqueName(tr("Graph")));
        ((MultiLayer *)nw)->copy(this, (MultiLayer *)w);
    } else if (w->inherits("Table")) {
        Table *t = (Table *)w;
        QString caption = generateUniqueName(tr("Table"));
        nw = newTable(caption, t->numRows(), t->numCols());
        ((Table *)nw)->copy(t);
    } else if (w->inherits("Graph3D")) {
        Graph3D *g = (Graph3D *)w;
        if (!g->hasData()) {
            QApplication::restoreOverrideCursor();
            if (!isNonInteractiveContext(this)) {
                QMessageBox::warning(this, tr("Duplicate error"),
                                     tr("Empty 3D surface plots cannot be duplicated!"));
            }
            return 0;
        }

        QString caption = generateUniqueName(tr("Graph"));
        QString s = g->formula();
        if (g->userFunction())
            nw = newPlot3D(caption, s, g->xStart(), g->xStop(), g->yStart(), g->yStop(),
                           g->zStart(), g->zStop());
        else if (s.endsWith("(Z)"))
            nw = dataPlotXYZ(caption, s, g->xStart(), g->xStop(), g->yStart(), g->yStop(),
                             g->zStart(), g->zStop());
        else if (s.endsWith("(Y)")) // Ribbon plot
            nw = dataPlot3D(caption, s, g->xStart(), g->xStop(), g->yStart(), g->yStop(),
                            g->zStart(), g->zStop());
        else
            nw = openMatrixPlot3D(caption, s, g->xStart(), g->xStop(), g->yStart(), g->yStop(),
                                  g->zStart(), g->zStop());

        if (!nw)
            return 0;

        ((Graph3D *)nw)->copy(g);
        customToolBars((MyWidget *)nw);
    } else if (w->inherits("Matrix")) {
        nw = newMatrix(((Matrix *)w)->numRows(), ((Matrix *)w)->numCols());
        ((Matrix *)nw)->copy((Matrix *)w);
    } else if (w->inherits("Note")) {
        nw = newNote();
        if (nw)
            ((Note *)nw)->setText(((Note *)w)->text());
    }

    if (nw) {
        if (w->inherits("MultiLayer")) {
            if (w->status() == MyWidget::Maximized)
                nw->showMaximized();
        } else if (w->inherits("Graph3D")) {
            ((Graph3D *)nw)->setIgnoreFonts(true);
            if (w->status() == MyWidget::Maximized) {
                w->showNormal();
                w->resize(500, 400);
                nw->resize(w->size());
                nw->showMaximized();
            } else
                nw->resize(w->size());
            ((Graph3D *)nw)->setIgnoreFonts(false);
        } else {
            nw->resize(w->size());
            nw->showNormal();
        }

        nw->setWindowLabel(w->windowLabel());
        nw->setCaptionPolicy(w->captionPolicy());
        setListViewLabel(nw->name(), w->windowLabel());
    }
    QApplication::restoreOverrideCursor();
    return nw;
}

MyWidget *ApplicationWindow::activeWindow() const
{
    if (isBeingDestroyed(this))
        return nullptr;
    if (QMdiSubWindow *sw = d_workspace.activeSubWindow()) {
        if (MyWidget *mw = qobject_cast<MyWidget *>(sw))
            return mw;
    }
    if (QMdiSubWindow *csw = d_workspace.currentSubWindow()) {
        if (MyWidget *mw = qobject_cast<MyWidget *>(csw))
            return mw;
    }
    auto isLiveRegisteredWindow = [this](MyWidget *cand) -> bool {
        if (!cand)
            return false;
        if (d_workspace.subWindowList().contains(cand))
            return true;
        if (current_folder && current_folder->windowsList().contains(cand))
            return true;
        if (hiddenWindows.contains(cand) || outWindows.contains(cand))
            return true;
        return false;
    };
    if (current_folder) {
        MyWidget *faw = current_folder->activeWindow();
        if (isLiveRegisteredWindow(faw))
            return faw;
    }
    if (isLiveRegisteredWindow(lastModified))
        return lastModified;
    return nullptr;
}

void ApplicationWindow::undo()
{
    if (!isNonInteractiveContext(this)) {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (QLineEdit *le = qobject_cast<QLineEdit *>(fw)) {
                if (!le->isReadOnly() && (le->isUndoAvailable() || le->isModified())) {
                    le->undo();
                    return;
                }
            } else if (QTextEdit *te = qobject_cast<QTextEdit *>(fw)) {
                if (!te->isReadOnly() && te->document() && te->document()->isUndoAvailable()) {
                    te->undo();
                    return;
                }
            } else if (QPlainTextEdit *pte = qobject_cast<QPlainTextEdit *>(fw)) {
                if (!pte->isReadOnly() && pte->document() && pte->document()->isUndoAvailable()) {
                    pte->undo();
                    return;
                }
            }
        }
    }

    MyWidget *aw = activeWindow();
    const bool nonInteractive = isNonInteractiveContext(this);
    ScopedUndoReplayGuard replayGuard(this);
    if (MultiLayer *ml = qobject_cast<MultiLayer *>(aw)) {
        if (ml->undoStack() && ml->undoStack()->canUndo()) {
            if (!nonInteractive)
                QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            ml->undoStack()->undo();
            if (!nonInteractive)
                QApplication::restoreOverrideCursor();
        }
    } else if (!qobject_cast<Graph3D *>(aw) && !qobject_cast<Note *>(aw)
               && d_project && d_project->undoStack() && d_project->undoStack()->canUndo()) {
        const QString cmdText = d_project->undoStack()->undoText();
        QPointer<MyWidget> prevLastMod(lastModified);
        lastModified = nullptr;
        if (!nonInteractive)
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        d_project->undoStack()->undo();
        if (!nonInteractive)
            QApplication::restoreOverrideCursor();
        Table *targetTbl = qobject_cast<Table *>(lastModified);
        if (!lastModified && prevLastMod)
            lastModified = prevLastMod.data();
        if (!targetTbl) {
            const QString ownerPrefix =
                    cmdText.section(QLatin1Char(':'), 0, 0).section(QLatin1Char('/'), 0, 0).trimmed();
            if (!ownerPrefix.isEmpty()) {
                Table *cand = table(ownerPrefix);
                if (cand && cand->name() == ownerPrefix)
                    targetTbl = cand;
            }
        }
        if (targetTbl && targetTbl != aw && targetTbl->d_future_table
            && targetTbl->d_future_table->parentAspect() != nullptr) {
            if (WorkbookManager *wm = WorkbookManager::existing(this);
                wm && !wm->bookOf(targetTbl).isEmpty()) {
                wm->setCurrentSheet(targetTbl, true);
            } else if (!hidden(targetTbl) && targetTbl->folder() == current_folder) {
                activateSubWindow(targetTbl);
            }
            if (statusBar()) {
                statusBar()->showMessage(
                        tr("Undid \"%1\" in %2").arg(cmdText, targetTbl->name()), 8000);
            }
        }
    }
    updateUndoRedoActionsForActiveWindow();
}

void ApplicationWindow::redo()
{
    if (!isNonInteractiveContext(this)) {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (QLineEdit *le = qobject_cast<QLineEdit *>(fw)) {
                if (!le->isReadOnly() && le->isRedoAvailable()) {
                    le->redo();
                    return;
                }
            } else if (QTextEdit *te = qobject_cast<QTextEdit *>(fw)) {
                if (!te->isReadOnly() && te->document() && te->document()->isRedoAvailable()) {
                    te->redo();
                    return;
                }
            } else if (QPlainTextEdit *pte = qobject_cast<QPlainTextEdit *>(fw)) {
                if (!pte->isReadOnly() && pte->document() && pte->document()->isRedoAvailable()) {
                    pte->redo();
                    return;
                }
            }
        }
    }

    MyWidget *aw = activeWindow();
    const bool nonInteractive = isNonInteractiveContext(this);
    ScopedUndoReplayGuard replayGuard(this);
    if (MultiLayer *ml = qobject_cast<MultiLayer *>(aw)) {
        if (ml->undoStack() && ml->undoStack()->canRedo()) {
            if (!nonInteractive)
                QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            ml->undoStack()->redo();
            if (!nonInteractive)
                QApplication::restoreOverrideCursor();
        }
    } else if (!qobject_cast<Graph3D *>(aw) && !qobject_cast<Note *>(aw)
               && d_project && d_project->undoStack() && d_project->undoStack()->canRedo()) {
        const QString cmdText = d_project->undoStack()->redoText();
        QPointer<MyWidget> prevLastMod(lastModified);
        lastModified = nullptr;
        if (!nonInteractive)
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        d_project->undoStack()->redo();
        if (!nonInteractive)
            QApplication::restoreOverrideCursor();
        Table *targetTbl = qobject_cast<Table *>(lastModified);
        if (!lastModified && prevLastMod)
            lastModified = prevLastMod.data();
        if (!targetTbl) {
            const QString ownerPrefix =
                    cmdText.section(QLatin1Char(':'), 0, 0).section(QLatin1Char('/'), 0, 0).trimmed();
            if (!ownerPrefix.isEmpty()) {
                Table *cand = table(ownerPrefix);
                if (cand && cand->name() == ownerPrefix)
                    targetTbl = cand;
            }
        }
        if (targetTbl && targetTbl != aw && targetTbl->d_future_table
            && targetTbl->d_future_table->parentAspect() != nullptr) {
            if (WorkbookManager *wm = WorkbookManager::existing(this);
                wm && !wm->bookOf(targetTbl).isEmpty()) {
                wm->setCurrentSheet(targetTbl, true);
            } else if (!hidden(targetTbl) && targetTbl->folder() == current_folder) {
                activateSubWindow(targetTbl);
            }
            if (statusBar()) {
                statusBar()->showMessage(
                        tr("Redid \"%1\" in %2").arg(cmdText, targetTbl->name()), 8000);
            }
        }
    }
    updateUndoRedoActionsForActiveWindow();
}

bool ApplicationWindow::hidden(MyWidget *window)
{
    if (d_batch_window_updates_depth > 0)
        return d_batch_hidden_set.contains(window) || outWindows.contains(window);
    if (hiddenWindows.contains(window) || outWindows.contains(window))
        return true;

    return false;
}

void ApplicationWindow::updateWindowStatus(MyWidget *w)
{
    setListView(w->name(), w->aspect());
    if (w->status() == MyWidget::Maximized) {
        QList<MyWidget *> windows = current_folder->windowsList();
        foreach (MyWidget *oldMaxWindow, windows) {
            if (oldMaxWindow != w && oldMaxWindow->status() == MyWidget::Maximized)
                oldMaxWindow->setStatus(MyWidget::Normal);
        }
    }
}

void ApplicationWindow::hideActiveWindow()
{
    MyWidget *w = (MyWidget *)d_workspace.activeSubWindow();
    if (!w)
        return;

    hideWindow(w);
}

void ApplicationWindow::hideWindow(MyWidget *w)
{
    if (!w)
        return;
    if (d_batch_window_updates_depth > 0) {
        if (!d_batch_hidden_set.contains(w)) {
            d_batch_hidden_set.insert(w);
            hiddenWindows.append(w);
        }
    } else if (!hiddenWindows.contains(w)) {
        hiddenWindows.append(w);
    }
    w->setHidden();

    MyWidget *fallback = nullptr;
    Folder *f = w->folder();
    const bool needFallback =
            f && (f->activeWindow() == w || d_workspace.activeSubWindow() == w);
    if (needFallback) {
        for (MyWidget *cand : f->windowsList()) {
            if (cand && cand != w && !hidden(cand) && !outWindows.contains(cand)) {
                fallback = cand;
                break;
            }
        }
        if (f->activeWindow() == w)
            f->setActiveWindow(fallback);
    }

    if (d_workspace.activeSubWindow() == w) {
        if (f && f == current_folder && fallback && show_windows_policy != HideAll) {
            d_workspace.setActiveSubWindow(fallback);
            if (d_modern_ribbon
                && d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
                d_modern_ribbon->applySplitWorkspaceGeometry();
            }
        } else {
            d_workspace.setActiveSubWindow(nullptr);
            customMenu(nullptr);
            customToolBars(nullptr);
            if (d_modern_ribbon) {
                d_modern_ribbon->updateActiveContext(nullptr);
                if (d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
                    d_modern_ribbon->applySplitWorkspaceGeometry();
                }
            }
        }
    }
    scheduleSyncWorkspaceHiddenTabs(this);
    emit modified();
}

static void runWindowGeometryDialog(ApplicationWindow *app, MyWidget *w)
{
    if (!app || !w)
        return;
    QPointer<ImageDialog> id(new ImageDialog(app));
    app->connect(id.data(), SIGNAL(setGeometry(int, int, int, int)), app,
                 SLOT(setWindowGeometry(int, int, int, int)));
    id->setWindowTitle(ApplicationWindow::tr("Window Geometry"));
    id->setOrigin(w->pos());
    id->setSize(w->size());
    id->exec();
    delete id.data();
}

void ApplicationWindow::resizeActiveWindow()
{
    if (isNonInteractiveContext(this))
        return;
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w)
        return;

    runWindowGeometryDialog(this, w);
}

void ApplicationWindow::resizeWindow()
{
    if (isNonInteractiveContext(this))
        return;
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w)
        return;

    d_workspace.setActiveSubWindow(w);
    runWindowGeometryDialog(this, w);
}

void ApplicationWindow::setWindowGeometry(int x, int y, int w, int h)
{
    if (QMdiSubWindow *sw = d_workspace.activeSubWindow())
        sw->setGeometry(x, y, w, h);
}

void ApplicationWindow::activateSubWindow()
{
    if (!d_headless_mode && !d_ole_embedded && !m_batch) {
        setWindowState(Qt::WindowActive);
        raise();
        show();
    }
    WindowListItem *it = (WindowListItem *)lv.currentItem();
    if (it)
        activateSubWindow(it->window());
}

void ApplicationWindow::activateSubWindow(MyWidget *w)
{
    if (!w)
        return;

    if (isTabbedWorkspaceView())
        w->setMaximized();
    else
        w->setNormal();
    d_workspace.setActiveSubWindow(w);

    updateWindowLists(w);
    emit modified();
}

void ApplicationWindow::maximizeWindow()
{
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w)
        return;

    updateWindowLists(w);
    w->setMaximized();
    emit modified();
}

void ApplicationWindow::minimizeWindow()
{
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w)
        return;

    updateWindowLists(w);
    w->setMinimized();
    emit modified();
}

void ApplicationWindow::updateWindowLists(MyWidget *w)
{
    if (!w)
        return;

    if (hiddenWindows.contains(w)) {
        hiddenWindows.takeAt(hiddenWindows.indexOf(w));
        d_batch_hidden_set.remove(w);
    } else if (outWindows.contains(w)) {
        outWindows.takeAt(outWindows.indexOf(w));
        d_workspace.addSubWindow(w);
        w->setAttribute(Qt::WA_DeleteOnClose);
    }
    scheduleSyncWorkspaceHiddenTabs(this);
}

void ApplicationWindow::closeActiveWindow()
{
    QWidget *w = (QWidget *)d_workspace.activeSubWindow();
    if (w)
        w->close();
}

void ApplicationWindow::removeWindowFromLists(MyWidget *w)
{
    if (!w)
        return;

    QString caption = w->name();
    if (w->inherits("Table")) {
        Table *m = (Table *)w;
        for (int i = 0; i < m->numCols(); i++) {
            QString name = m->colName(i);
            removeCurves(name);
        }
        QList<MyWidget *> windows = windowsList();
        for (QMdiSubWindow *sw : d_workspace.subWindowList()) {
            if (MyWidget *mw = qobject_cast<MyWidget *>(sw)) {
                if (!windows.contains(mw))
                    windows.append(mw);
            }
        }
        for (MyWidget *win : windows) {
            if (win && win->inherits("Graph3D") && static_cast<Graph3D *>(win)->getTable() == m)
                static_cast<Graph3D *>(win)->clearData();
        }
        if (w == lastModified) {
            actionUndo->setEnabled(false);
            actionRedo->setEnabled(false);
        }
    } else if (w->inherits("MultiLayer")) {
        MultiLayer *ml = (MultiLayer *)w;
        invalidateModelessGraphDialogs(ml);
        if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
            d_graph_inspector_dock->setTargetMultiLayer(nullptr);
        for (QWidget *gw : ml->graphPtrs()) {
            if (gw)
                RecalcManager::instance(this)->removeLinksForGraph(static_cast<Graph *>(gw));
        }
        Graph *g = ml->activeGraph();
        if (g)
            btnPointer->setChecked(true);
    } else if (w->inherits("Matrix"))
        remove3DMatrixPlots((Matrix *)w);

    if (hiddenWindows.contains(w))
        hiddenWindows.takeAt(hiddenWindows.indexOf(w));
    else if (outWindows.contains(w))
        outWindows.takeAt(outWindows.indexOf(w));
    removeBatchWindow(w);

    if (lastModified == w)
        lastModified = nullptr;
    clearLastCopiedLayerIfReferencing(w);
    if (w->folder() && w->folder()->activeWindow() == w)
        w->folder()->setActiveWindow(nullptr);
    if (current_folder && current_folder->activeWindow() == w)
        current_folder->setActiveWindow(nullptr);

    updateUndoRedoActionsForActiveWindow();
    updateGraphInspectorResponsiveness();
}

void ApplicationWindow::closeWindow(MyWidget *window)
{
    if (!window)
        return;

    static QSet<MyWidget *> s_closing_windows;
    if (s_closing_windows.contains(window))
        return;

    struct ScopedClosingGuard {
        QSet<MyWidget *> &setRef;
        MyWidget *key;
        ScopedClosingGuard(QSet<MyWidget *> &s, MyWidget *k) : setRef(s), key(k)
        {
            setRef.insert(key);
        }
        ~ScopedClosingGuard() { setRef.remove(key); }
    } closingGuard(s_closing_windows, window);

    QPointer<MyWidget> guardedWin(window);

    removeWindowFromLists(window);
    if (window->folder())
        window->folder()->removeWindow(window);

    // update list view in project explorer
    const QList<QTreeWidgetItem *> items =
            lv.findItems(window->name(), Qt::MatchExactly | Qt::MatchCaseSensitive);
    if (!items.isEmpty())
        delete items.at(0);

    window->hide();
    d_workspace.removeSubWindow(window);

    const bool undoReplayActive = Table::isUndoRedoReplaying();
    const bool calledFromAspectRemoval = d_in_aspect_about_to_be_removed;

    if (Table *tbl = qobject_cast<Table *>(window)) {
        const bool canUndoViaAspect = tbl->d_future_table
                && (undoReplayActive || calledFromAspectRemoval
                    || tbl->d_future_table->parentAspect() != nullptr);
        if (WorkbookManager *wm = WorkbookManager::existing(this))
            wm->onTableClosed(tbl, canUndoViaAspect);
        window->setProperty("_tg_hidden_for_focus_mdi", QVariant());
        if (ModernRibbonBar *rib = modernRibbonBar()) {
            if (FocusTableDrawer *dr = rib->focusTableDrawer())
                dr->onTableClosed(tbl);
        }
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->setParent(nullptr);
        if (!undoReplayActive && !calledFromAspectRemoval) {
            if (tbl->d_future_table && tbl->d_future_table->parentAspect() != nullptr) {
                tbl->d_future_table->remove();
            } else if (guardedWin) {
                guardedWin->deleteLater();
            }
        }
        updateUndoRedoActionsForActiveWindow();
    } else if (Matrix *mat = qobject_cast<Matrix *>(window)) {
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->setParent(nullptr);
        if (!undoReplayActive && !calledFromAspectRemoval) {
            if (mat->d_future_matrix && mat->d_future_matrix->parentAspect() != nullptr) {
                mat->d_future_matrix->remove();
            } else if (guardedWin) {
                guardedWin->deleteLater();
            }
        }
        updateUndoRedoActionsForActiveWindow();
    } else {
        window->deleteLater();
    }
    if (d_workspace.subWindowList().isEmpty() && actionPasteSelection)
        actionPasteSelection->setEnabled(true);
    emit modified();
}

void ApplicationWindow::about()
{
    if (isNonInteractiveContext(nullptr))
        return;
    SciDAVis::about();
}

void ApplicationWindow::newProject()
{
    if (guardProjectActionOutsideOleEmbedded(tr("start a new project")))
        return;
    saveSettings(); // the recent projects must be saved

    ApplicationWindow *ed = new ApplicationWindow();
    ed->applyUserSettings();
    ed->newTable();

    if (this->isMaximized())
        ed->showMaximized();
    else
        ed->show();

    ed->savedProject();

    if (!d_ole_embedded)
        this->close();
}

void ApplicationWindow::savedProject()
{
    actionSaveProject->setEnabled(false);
    saved = true;
    d_project->undoStack()->clear();
    removeRecoveryFile();
}

/*!
 * Like savedProject(), but for the case that the user just saved the project being worked
 * on: the undo history is kept (only its clean state is updated), so that edits made before
 * saving can still be undone afterwards.
 */
void ApplicationWindow::markProjectSaved()
{
    actionSaveProject->setEnabled(false);
    saved = true;
    d_project->undoStack()->setClean();
    removeRecoveryFile();
}

void ApplicationWindow::modifiedProject()
{
    actionSaveProject->setEnabled(true);
    saved = false;
    updateStatusBarForWindow(d_status_info, d_workspace.activeSubWindow());
}

void ApplicationWindow::modifiedProject(MyWidget *w)
{
    modifiedProject();

    lastModified = w;
    if (w && !w->property("_tg_lastmod_hook").toBool()) {
        w->setProperty("_tg_lastmod_hook", true);
        connect(w, &QObject::destroyed, this, [this, w]() {
            if (lastModified == w)
                lastModified = nullptr;
        });
    }
    updateUndoRedoActionsForActiveWindow();
}

void ApplicationWindow::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == savingTimerId)
        writeRecoveryFile();
    else
        QWidget::timerEvent(e);
}

void ApplicationWindow::dropEvent(QDropEvent *e)
{
    if (!e || !e->mimeData())
        return;

    if (e->mimeData()->hasFormat(QLatin1String(MultiLayer::kColumnMimeType))) {
        if (handleColumnMimeDrop(e->mimeData(), nullptr, nullptr, -1, false))
            e->acceptProposedAction();
        return;
    }
    if (!e->mimeData()->hasUrls())
        return;

    // Every dropped data file goes through AsciiTableImportFilter with fixed defaults: the
    // drop never inherits what was last chosen in the import dialog (ignored lines, separator,
    // ...). The filter keeps rows with missing cells (as empty cells), reads every .ras scan and
    // reports what it skipped; the report is shown in the status bar.
    QStringList importSummaries;
    QStringList unsupportedFiles;
    bool excelDropped = false;

    // A fresh project starts with one empty table; the first dropped data file replaces it
    // (as importASCII() does for File > Import).
    Table *blankInitialTable = nullptr;
    if (projectname == QLatin1String("untitled")) {
        const QList<MyWidget *> allWins = windowsList();
        if (allWins.size() == 1) {
            if (Table *cand = qobject_cast<Table *>(allWins.first())) {
                bool untouched = cand->d_future_table && cand->windowLabel().isEmpty()
                        && dependingPlots(cand->name()).isEmpty() && cand->columnCount() <= 2;
                for (int c = 0; untouched && c < cand->columnCount(); ++c) {
                    Column *col = cand->column(c);
                    if (!col || !col->longName().isEmpty() || !col->units().isEmpty()
                        || !col->comment().isEmpty() || !col->formulaIntervals().isEmpty()) {
                        untouched = false;
                        break;
                    }
                    for (int r = 0; r < col->rowCount(); ++r) {
                        const bool empty = (col->columnMode() == SciDAVis::ColumnMode::Numeric)
                                ? col->isInvalid(r)
                                : col->textAt(r).isEmpty();
                        if (!empty) {
                            untouched = false;
                            break;
                        }
                    }
                }
                if (untouched)
                    blankInitialTable = cand;
            }
        }
    }

    auto importRigakuRasOrSpectrum = [this, &importSummaries, &blankInitialTable](
                                             const QString &path, bool isRigakuRas,
                                             bool autoPlotLine) -> bool {
        Q_UNUSED(isRigakuRas)
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return false;
        const QByteArray raw = f.readAll();
        f.close();

        const QFileInfo fi(path);
        const QString baseTitle =
                fi.completeBaseName().isEmpty() ? tr("Table") : fi.completeBaseName();
        AsciiTableImportFilter::ImportReport report;
        std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                raw, baseTitle, path, &report));
        if (!ft || ft->columnCount() < 1 || ft->rowCount() < 1)
            return false;
        bool anyNumeric = false;
        for (int c = 0; c < ft->columnCount() && !anyNumeric; ++c)
            anyNumeric = ft->column(c)
                    && ft->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric;
        // A plain text table (sample list, labels only ...) is still a table: for the usual
        // text-table extensions it is imported as text columns, like File > Import does. Other
        // files (unknown extensions, .ras/.xy ...) must contain numbers to count as data.
        const QString dropSuffix = fi.suffix().toLower();
        const bool textTableExt = dropSuffix == QLatin1String("csv")
                || dropSuffix == QLatin1String("tsv") || dropSuffix == QLatin1String("tab")
                || dropSuffix == QLatin1String("dat") || dropSuffix == QLatin1String("txt");
        if (!anyNumeric && !textTableExt)
            return false;

        if (blankInitialTable) {
            blankInitialTable->askOnCloseEvent(false);
            if (blankInitialTable->d_future_table)
                blankInitialTable->d_future_table->remove();
            blankInitialTable = nullptr;
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
        Table *t = newTable(generateUniqueName(baseTitle), path, cols);
        if (!t) {
            qDeleteAll(cols);
            return false;
        }
        if (t->d_future_table) {
            t->d_future_table->setImportSource(path, QString(), report.separator, report.encoding,
                                               0, -1, t->numCols());
        }

        if (autoPlotLine && anyNumeric && t->numCols() >= 2) {
            QStringList yCols;
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->colPlotDesignation(c) == SciDAVis::Y
                    && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric)
                    yCols << t->colName(c);
            }
            if (yCols.isEmpty())
                yCols << t->colName(1);
            multilayerPlot(t, yCols, Graph::Line);
        }
        importSummaries << fi.fileName() + QStringLiteral(": ")
                        + AsciiTableImportFilter::describeImportReport(report);
        return true;
    };

    auto importVaspDoscarDrop = [this](const QString &path) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream ts(&f);
        QString header6;
        for (int i = 0; i < 6 && !ts.atEnd(); ++i)
            header6 = ts.readLine().trimmed();
        const QStringList hParts = header6.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (hParts.size() < 4)
            return false;
        bool okN = false, okEf = false;
        const int nedos = hParts.at(2).toInt(&okN);
        const double eFermi = hParts.at(3).toDouble(&okEf);
        if (!okN || !okEf || nedos < 2 || nedos > 200000)
            return false;

        QVector<QVector<double>> rows;
        rows.reserve(nedos);
        int nCols = 0;
        for (int i = 0; i < nedos && !ts.atEnd(); ++i) {
            const QStringList parts = ts.readLine().trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() < 2)
                break;
            QVector<double> vals;
            vals.reserve(parts.size());
            bool allOk = true;
            for (const QString &p : parts) {
                bool ok = false;
                const double v = p.toDouble(&ok);
                if (!ok) {
                    allOk = false;
                    break;
                }
                vals.append(v);
            }
            if (!allOk)
                break;
            nCols = qMax(nCols, vals.size());
            rows.append(std::move(vals));
        }
        f.close();
        if (rows.size() < 2 || nCols < 2)
            return false;

        Table *t = newTable(rows.size(), nCols, generateUniqueName(QStringLiteral("DOSCAR")), path);
        if (!t)
            return false;
        t->setColName(0, QStringLiteral("Energy"));
        t->setColUnits(0, QStringLiteral("eV"));
        t->setColName(1, QStringLiteral("TDOS"));
        for (int c = 2; c < nCols; ++c)
            t->setColName(c, QStringLiteral("DOS_%1").arg(c));
        for (int c = 0; c < nCols; ++c) {
            if (Column *colPtr = t->column(c)) {
                QVector<double> v(rows.size(), 0.0);
                for (int r = 0; r < rows.size(); ++r)
                    v[r] = (c < rows.at(r).size()) ? rows.at(r).at(c) : 0.0;
                colPtr->replaceValues(0, v);
            }
        }
        CurveMathTools::plotVaspDos(this, t, t->colName(0), QStringList() << t->colName(1),
                                    QStringList(), eFermi, false);
        return true;
    };

    auto importVaspEigenvalDrop = [this](const QString &path) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream ts(&f);
        QString line6;
        for (int i = 0; i < 6 && !ts.atEnd(); ++i)
            line6 = ts.readLine().trimmed();
        const QStringList hParts = line6.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (hParts.size() < 3)
            return false;
        bool okK = false, okB = false;
        const int nkpts = hParts.at(1).toInt(&okK);
        const int nbands = hParts.at(2).toInt(&okB);
        if (!okK || !okB || nkpts < 2 || nbands < 1 || nkpts > 50000 || nbands > 4096)
            return false;

        QVector<double> kDist;
        QVector<QVector<double>> bandEnergies(nbands);
        double prevKx = 0.0, prevKy = 0.0, prevKz = 0.0, cumDist = 0.0;
        for (int ik = 0; ik < nkpts && !ts.atEnd(); ++ik) {
            QString kLine;
            while (!ts.atEnd()) {
                kLine = ts.readLine().trimmed();
                if (!kLine.isEmpty())
                    break;
            }
            const QStringList kp = kLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (kp.size() < 4)
                break;
            const double kx = kp.at(0).toDouble();
            const double ky = kp.at(1).toDouble();
            const double kz = kp.at(2).toDouble();
            if (ik > 0) {
                const double dx = kx - prevKx, dy = ky - prevKy, dz = kz - prevKz;
                cumDist += std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            prevKx = kx;
            prevKy = ky;
            prevKz = kz;
            kDist.append(cumDist);

            for (int ib = 0; ib < nbands && !ts.atEnd(); ++ib) {
                const QStringList bp = ts.readLine().trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
                bandEnergies[ib].append(bp.size() >= 2 ? bp.at(1).toDouble() : 0.0);
            }
        }
        f.close();
        if (kDist.size() < 2)
            return false;

        Table *t = newTable(kDist.size(), 1 + nbands,
                            generateUniqueName(QStringLiteral("EIGENVAL")), path);
        if (!t)
            return false;
        t->setColName(0, QStringLiteral("k_dist"));
        if (Column *kc = t->column(0))
            kc->replaceValues(0, kDist);
        QStringList bandColNames;
        for (int ib = 0; ib < nbands; ++ib) {
            const QString bn = QStringLiteral("Band_%1").arg(ib + 1);
            t->setColName(ib + 1, bn);
            if (Column *bc = t->column(ib + 1))
                bc->replaceValues(0, bandEnergies[ib]);
            bandColNames.append(t->colName(ib + 1));
        }
        CurveMathTools::plotVaspBands(this, t, t->colName(0), bandColNames);
        return true;
    };

    const QList<QUrl> urls = e->mimeData()->urls();
    bool handledAny = false;

    for (const QUrl &url : urls) {
        const QString fileName = url.toLocalFile();
        if (fileName.isEmpty())
            continue;
        const QFileInfo fileInfo(fileName);
        if (!fileInfo.exists() || !fileInfo.isFile())
            continue;

        const QString lowerFileName = fileInfo.fileName().toLower();
        const QString ext = fileInfo.suffix().toLower();

        if (lowerFileName == QLatin1String("doscar")
            || lowerFileName.startsWith(QLatin1String("doscar."))
            || lowerFileName.endsWith(QLatin1String("_doscar"))) {
            if (importVaspDoscarDrop(fileName)) {
                handledAny = true;
                continue;
            }
        }
        if (lowerFileName == QLatin1String("eigenval")
            || lowerFileName.startsWith(QLatin1String("eigenval."))
            || lowerFileName.endsWith(QLatin1String("_eigenval"))) {
            if (importVaspEigenvalDrop(fileName)) {
                handledAny = true;
                continue;
            }
        }

        const bool isCompoundProject =
                lowerFileName.endsWith(QLatin1String(".sciprj.gz"))
                || lowerFileName.endsWith(QLatin1String(".sciprj.gz~"))
                || lowerFileName.endsWith(QLatin1String(".qti.gz"))
                || lowerFileName.endsWith(QLatin1String(".tgprj.gz"));

        if (isCompoundProject || ext == QLatin1String("sciprj") || ext == QLatin1String("sciprj~")
            || ext == QLatin1String("tgprj") || ext == QLatin1String("opj")
            || ext == QLatin1String("opju") || ext == QLatin1String("qti")
            || ext == QLatin1String("ogm") || ext == QLatin1String("ogw")
            || ext == QLatin1String("ogg") || ext == QLatin1String("org")) {
            if (!guardProjectActionOutsideOleEmbedded(tr("open a project"), fileName))
                open(fileName);
            handledAny = true;
        } else if (ext == QLatin1String("ras")) {
            bool isSunRaster = false;
            QFile magicFile(fileName);
            if (magicFile.open(QIODevice::ReadOnly)) {
                const QByteArray hdr = magicFile.read(4);
                magicFile.close();
                if (hdr.size() == 4
                    && static_cast<unsigned char>(hdr.at(0)) == 0x59
                    && static_cast<unsigned char>(hdr.at(1)) == 0xA6
                    && static_cast<unsigned char>(hdr.at(2)) == 0x6A
                    && static_cast<unsigned char>(hdr.at(3)) == 0x95) {
                    isSunRaster = true;
                }
            }
            if (isSunRaster) {
                loadImage(fileName);
                handledAny = true;
            } else if (importRigakuRasOrSpectrum(fileName, true, true)) {
                handledAny = true;
            } else {
                unsupportedFiles << fileInfo.fileName();
            }
        } else if (ext == QLatin1String("xy") || ext == QLatin1String("xye")
                   || ext == QLatin1String("chi") || ext == QLatin1String("uxd")) {
            if (importRigakuRasOrSpectrum(fileName, false, true))
                handledAny = true;
            else
                unsupportedFiles << fileInfo.fileName();
        } else if (ext == QLatin1String("asc") || ext == QLatin1String("mpt")
                   || ext == QLatin1String("dta") || ext == QLatin1String("csv")
                   || ext == QLatin1String("tsv") || ext == QLatin1String("tab")
                   || ext == QLatin1String("dat") || ext == QLatin1String("txt")) {
            if (importRigakuRasOrSpectrum(fileName, false, false))
                handledAny = true;
            else
                unsupportedFiles << fileInfo.fileName();
        } else if (ext == QLatin1String("bmp") || ext == QLatin1String("bw")
                   || ext == QLatin1String("eps") || ext == QLatin1String("epsf")
                   || ext == QLatin1String("epsi") || ext == QLatin1String("exr")
                   || ext == QLatin1String("kra") || ext == QLatin1String("ora")
                   || ext == QLatin1String("pcx") || ext == QLatin1String("psd")
                   || ext == QLatin1String("rgb") || ext == QLatin1String("rgba")
                   || ext == QLatin1String("sgi") || ext == QLatin1String("tga")
                   || ext == QLatin1String("xcf") || ext == QLatin1String("dds")
                   || ext == QLatin1String("gif") || ext == QLatin1String("ico")
                   || ext == QLatin1String("jp2") || ext == QLatin1String("jpeg")
                   || ext == QLatin1String("jpg") || ext == QLatin1String("mng")
                   || ext == QLatin1String("pbm") || ext == QLatin1String("pgm")
                   || ext == QLatin1String("pic") || ext == QLatin1String("png")
                   || ext == QLatin1String("ppm") || ext == QLatin1String("svg")
                   || ext == QLatin1String("svgz") || ext == QLatin1String("tif")
                   || ext == QLatin1String("tiff") || ext == QLatin1String("webp")
                   || ext == QLatin1String("xbm") || ext == QLatin1String("xpm")
                   || ext == QLatin1String("xv")) {
            loadImage(fileName);
            handledAny = true;
        } else if (ext == QLatin1String("xlsx") || ext == QLatin1String("xlsm")) {
            // D1: read directly; a damaged / password-protected workbook shows its reason
            QString xlsxSummary;
            if (XlsxImport::importDroppedFile(this, fileName, &xlsxSummary)) {
                importSummaries << xlsxSummary;
                blankInitialTable = nullptr; // replaced by the first sheet, if it was blank
                handledAny = true;
            }
        } else if (ext == QLatin1String("xls") || ext == QLatin1String("ods")) {
            excelDropped = true;
            unsupportedFiles << fileInfo.fileName();
        } else if (importRigakuRasOrSpectrum(fileName, false, false)) {
            // unknown extension (.log, .out, no extension ...) that turned out to be a text table
            handledAny = true;
        } else {
            unsupportedFiles << fileInfo.fileName();
        }
    }

    if (!importSummaries.isEmpty()) {
        const QString summary = importSummaries.join(QStringLiteral(" / "));
        setProperty("torigraph_last_drop_import_summary", summary);
        statusBar()->showMessage(tr("Imported %1").arg(summary), 15000);
    }
    if (!unsupportedFiles.isEmpty()) {
        QString msg;
        if (excelDropped) {
            XlsxImport::registerTranslations();
            msg = tr("Old Excel (.xls) and OpenDocument (.ods) files cannot be read directly.\n"
                     "Save them as an Excel Workbook (.xlsx) or as CSV and drop that file, or "
                     "copy the cells in Excel (Ctrl+C) and paste them into a worksheet here "
                     "(Ctrl+V).");
        } else {
            msg = tr("These files could not be read as data, graph or project files.\n"
                     "Text tables (CSV, TXT, DAT), Rigaku .ras, images and project files can be "
                     "dropped. For other formats, save the data as CSV first.");
        }
        msg += QStringLiteral("\n\n") + unsupportedFiles.join(QStringLiteral("\n"));
        setProperty("torigraph_last_drop_unsupported_message", msg);
        if (!isNonInteractiveContext(this))
            QMessageBox::information(this, tr("File Not Imported"), msg);
        else
            statusBar()->showMessage(msg.section(QLatin1Char('\n'), 0, 0), 15000);
    }

    if (handledAny)
        e->acceptProposedAction();
}

void ApplicationWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (!e || !e->mimeData())
        return;
    if (e->mimeData()->hasFormat(QLatin1String(MultiLayer::kColumnMimeType))) {
        e->acceptProposedAction();
        return;
    }
    if (e->source()) {
        e->ignore();
        return;
    }
    (e->mimeData()->hasUrls()) ? e->acceptProposedAction() : e->ignore();
}

void ApplicationWindow::closeEvent(QCloseEvent *ce)
{
    if (d_ole_embedded) {
        QPointer<ApplicationWindow> self(this);
        setProperty("_toriLastOleSaveOk", QVariant(true));
        setProperty("_toriLastOleSaveError", QVariant());
        if (auto onClose = d_ole_on_close)
            onClose();
        if (!self)
            return;
        const QVariant okProp = property("_toriLastOleSaveOk");
        const bool oleOk = okProp.isValid() ? okProp.toBool() : true;
        if (!oleOk) {
            const QString err = property("_toriLastOleSaveError").toString();
            modifiedProject();
            if (!isNonInteractiveContext(this)) {
                const int choice = QMessageBox::warning(
                        this, tr("Could not save embedded PowerPoint object"),
                        tr("Failed to update the embedded PowerPoint object before closing%1.\n\n"
                           "Keep ToriGraph open so you can save a backup copy with File -> Save "
                           "Project As, or close anyway without updating PowerPoint?")
                                .arg(err.isEmpty() ? QString() : QStringLiteral(": ") + err),
                        tr("Keep Open"), tr("Close Without Updating"), QString(), 0, 0);
                if (!self)
                    return;
                if (choice == 0) {
                    ce->ignore();
                    return;
                }
            } else {
                ce->ignore();
                return;
            }
        }
        d_ole_on_close = nullptr;
        if (!d_headless_mode && !m_batch && !isNonInteractiveContext(this))
            saveSettings();
        ce->accept();
        return;
    }

    if (d_headless_mode || m_batch || isNonInteractiveContext(this)) {
        releaseProjectFileLock();
        untrackOpenedProjectFile();
        ce->accept();
        return;
    }

    if (!saved) {
        QString s = tr("Save changes to project: <p><b> %1 </b> ?").arg(projectname);
        // Verb buttons instead of Yes/No: the answer must be readable without the question.
        switch (QMessageBox::information(this, tr("ToriGraph"), s, tr("&Save"), tr("Do&n't Save"),
                                         tr("Cancel"), 0, 2)) {
        case 0:
            if (!saveProject()) {
                ce->ignore();
                break;
            }
            saveSettings(); // the recent projects must be saved
            ce->accept();
            break;

        case 1:
        default:
            saveSettings(); // the recent projects must be saved
            ce->accept();
            break;

        case 2:
            ce->ignore();
            break;
        }
    } else {
        saveSettings(); // the recent projects must be saved
        ce->accept();
    }

    // Regular close (saved, or the user chose not to save): the recovery copy is obsolete.
    if (ce->isAccepted()) {
        removeRecoveryFile();
        releaseProjectFileLock();
        untrackOpenedProjectFile();
    }
}

void ApplicationWindow::customEvent(QEvent *e)
{
    if (e->type() == SCRIPTING_CHANGE_EVENT) {
        scriptingChangeEvent((ScriptingChangeEvent *)e);
        // If the event is triggered by setScriptingLang(), the connections are already made
        // (for messages emitted during initialization). However, it's good programming practice not
        // to assume a particular call path for an event; which means that we don't know for sure
        // at this point whether scriptEnv is connected or not.
        scriptEnv->disconnect(this);
        connect(scriptEnv, SIGNAL(error(const QString &, const QString &, int)), this,
                SLOT(scriptError(const QString &, const QString &, int)));
        connect(scriptEnv, SIGNAL(print(const QString &)), this,
                SLOT(scriptPrint(const QString &)));
    }
}

void ApplicationWindow::deleteSelectedItems()
{
    if (folders.hasFocus()
        && folders.currentItem()
                != folders.topLevelItem(
                        0)) { // we never allow the user to delete the project folder item
        deleteFolder();
        return;
    }

    QVector<QPointer<Folder>> targetFolders;
    QVector<QPointer<MyWidget>> targetWindows;
    for (QTreeWidgetItemIterator it(&lv); *it; ++it) {
        QTreeWidgetItem *item = *it;
        if (!item || !item->isSelected())
            continue;
        if (item->type() == FolderListItem::FolderType) {
            Folder *f = static_cast<FolderListItem *>(item)->folder();
            if (f && f != projectFolder() && !targetFolders.contains(f))
                targetFolders.append(QPointer<Folder>(f));
        } else if (item->type() == WindowListItem::WindowType) {
            MyWidget *w = static_cast<WindowListItem *>(item)->window();
            if (w && !targetWindows.contains(w))
                targetWindows.append(QPointer<MyWidget>(w));
        }
    }

    QSignalBlocker folderSigBlocker(&folders);
    for (const QPointer<Folder> &fp : targetFolders) {
        Folder *const rawFolder = fp.data();
        if (!rawFolder)
            continue;
        if (deleteFolder(rawFolder)) {
            QList<QTreeWidgetItem *> staleFolderItems;
            for (QTreeWidgetItemIterator it(&lv); *it; ++it) {
                if ((*it)->type() == FolderListItem::FolderType
                    && static_cast<FolderListItem *>(*it)->folder() == rawFolder) {
                    staleFolderItems.append(*it);
                }
            }
            qDeleteAll(staleFolderItems);
        }
    }
    for (const QPointer<MyWidget> &wp : targetWindows) {
        if (wp && windowsList().contains(wp.data()))
            wp->close();
    }
}

void ApplicationWindow::showTable(const QString &curve)
{
    Table *w = table(curve);
    if (!w)
        return;

    const bool keepMax = d_workspace.activeSubWindow()
            && d_workspace.activeSubWindow()->isMaximized();
    updateWindowLists(w);
    int colIndex = w->colIndex(curve);
    w->deselectAll();
    if (colIndex >= 0 && w->d_future_table && w->d_future_table->rowCount() > 0)
        w->setCellsSelected(0, colIndex, w->d_future_table->rowCount() - 1, colIndex);
    if (keepMax || w->status() == MyWidget::Maximized)
        w->showMaximized();
    else {
        if (w->isMinimized() || w->isHidden())
            w->showNormal();
        activateSubWindow(w);
        w->raise();
        w->setFocus();
    }
    const QList<QTreeWidgetItem *> items =
            lv.findItems(w->name(), Qt::MatchExactly | Qt::MatchCaseSensitive);
    if (!items.isEmpty() && items.at(0))
        items.at(0)->setText(2, w->isMaximized() ? tr("Maximized") : tr("Normal"));
    emit modified();
}

QStringList ApplicationWindow::depending3DPlots(Matrix *m)
{
    QList<MyWidget *> windows = windowsList();
    QStringList plots;
    for (int i = 0; i < (int)windows.count(); i++) {
        MyWidget *w = windows.at(i);
        if (w && w->inherits("Graph3D") && ((Graph3D *)w)->matrix() == m)
            plots << w->name();
    }
    return plots;
}

// TODO: Implement this in an elegant way
QStringList ApplicationWindow::dependingPlots(const QString &name)
{
    QList<MyWidget *> windows = windowsList();
    QStringList plots;
    if (name.isEmpty())
        return plots;

    const QString tblPrefix = name + QLatin1Char('_');

    for (int i = 0; i < windows.count(); i++) {
        MyWidget *w = windows.at(i);
        if (!w)
            continue;
        if (w->inherits("MultiLayer")) {
            QWidgetList lst = ((MultiLayer *)w)->graphPtrs();
            foreach (QWidget *widget, lst) {
                Graph *g = static_cast<Graph *>(widget);
                if (!g)
                    continue;
                const QStringList onPlot = g->curvesList();
                for (const QString &curveName : onPlot) {
                    if (!curveName.startsWith(tblPrefix))
                        continue;
                    Table *rt = table(curveName);
                    if ((!rt || rt->name() == name) && !plots.contains(w->name())) {
                        plots << w->name();
                        break;
                    }
                }
            }
        } else if (w->inherits("Graph3D")) {
            Graph3D *g3 = static_cast<Graph3D *>(w);
            if (g3->userFunction() || g3->matrix())
                continue;
            Table *tbl = g3->getTable();
            bool usesTable = false;
            if (tbl) {
                usesTable = (tbl->name() == name);
            } else {
                QString xSpec, ySpec, zSpec;
                bool isXYZ = false;
                if (splitGraph3DTableAssoc(g3->formula(), xSpec, ySpec, zSpec, isXYZ)) {
                    Table *rt = table(xSpec);
                    usesTable = rt ? (rt->name() == name) : xSpec.startsWith(tblPrefix);
                }
            }
            if (usesTable && !plots.contains(w->name()))
                plots << w->name();
        }
    }
    return plots;
}

QStringList ApplicationWindow::multilayerDependencies(MyWidget *w)
{
    QStringList tables;
    MultiLayer *g = qobject_cast<MultiLayer *>(w);
    if (!g)
        return tables;
    QWidgetList graphsList = g->graphPtrs();
    for (int i = 0; i < graphsList.count(); i++) {
        Graph *ag = qobject_cast<Graph *>(graphsList.at(i));
        if (!ag)
            continue;
        const QStringList onPlot = ag->curvesList();
        for (int j = 0; j < onPlot.count(); j++) {
            if (Table *t = table(onPlot[j])) {
                if (!tables.contains(t->name()))
                    tables << t->name();
            }
        }
    }
    return tables;
}

void ApplicationWindow::chooseHelpFolder()
{
    if (isNonInteractiveContext(this))
        return;
    const bool preferEn =
            appLanguage.trimmed().startsWith(QLatin1String("en"), Qt::CaseInsensitive);
    auto pickGuideInDir = [preferEn](const QString &dir) -> QString {
        if (preferEn) {
            const QString enPath =
                    dir + QDir::toNativeSeparators(QStringLiteral("/index.en.html"));
            if (QFileInfo::exists(enPath))
                return enPath;
        }
        const QString jaPath = dir + QDir::toNativeSeparators(QStringLiteral("/index.html"));
        if (QFileInfo::exists(jaPath))
            return jaPath;
        return QString();
    };
// TODO: move all paths & location handling to anothor class
#if defined(Q_OS_WIN)
    const QString defaultManualDir =
            qApp->applicationDirPath() + QDir::toNativeSeparators(QStringLiteral("/manual"));
#else
    const QString defaultManualDir =
            QDir::toNativeSeparators(QStringLiteral("/usr/share/doc/scidavis/manual"));
#endif
    const QString locateDefaultHelp = pickGuideInDir(defaultManualDir);
    if (!locateDefaultHelp.isEmpty()) {
        helpFilePath = locateDefaultHelp;
    } else {
        const QString dir = QFileDialog::getExistingDirectory(
                this, tr("Choose the location of the ToriGraph help folder!"),
                qApp->applicationDirPath());

        if (!dir.isEmpty()) {
            const QString picked = pickGuideInDir(dir);
            // TODO: Probably some kind of validity check to make sure that the
            // index.html file belongs to sciDavis
            if (picked.isEmpty()) {
                QMessageBox::information(
                        this, tr("index.html File Not Found!"),
                        tr("There is no file called <b>index.html</b> in this folder."
                           "<br>Please choose another folder!"));
            } else {
                helpFilePath = picked;
            }
        }
    }
}

void ApplicationWindow::showHelp()
{
    if (isNonInteractiveContext(this))
        return;
    const QUrl url = quickGuideUrl();
    if (!url.isLocalFile()) {
        const bool isEn =
                appLanguage.trimmed().startsWith(QLatin1String("en"), Qt::CaseInsensitive);
        statusBar()->showMessage(
                isEn ? tr("The built-in guide (manual/index.en.html) was not found; opening the "
                          "English README on GitHub instead.")
                     : tr("The built-in guide (manual/index.html) was not found; opening the "
                          "Japanese README on GitHub instead."),
                10000);
    }
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::information(this, tr("Help"),
                                 tr("The guide could not be opened automatically. Open this "
                                    "address in a web browser:")
                                         + QStringLiteral("<p><a href=\"%1\">%1</a></p>")
                                                   .arg(url.toString().toHtmlEscaped()));
    }
}

void ApplicationWindow::clearLogInfo()
{
    if (!logInfo.isEmpty()) {
        logInfo = "";
        results->setText(logInfo);
        emit modified();
    }
}

QList<MyWidget *> ApplicationWindow::windowsList()
{
    QList<MyWidget *> lst = QList<MyWidget *>();
    if (isBeingDestroyed(this))
        return lst; // the folder tree is (being) torn down

    Folder *project_folder = projectFolder();
    FolderListItem *item = project_folder->folderListItem();
    int initial_depth = item->depth();
    QTreeWidgetItemIterator it(item);
    while (item && item->depth() >= initial_depth) {
        QList<MyWidget *> folderWindows = item->folder()->windowsList();
        foreach (MyWidget *w, folderWindows) {
            if (w && !lst.contains(w))
                lst.append(w);
        }
        it++;
        item = (FolderListItem *)(*it);
    }

    for (MyWidget *w : hiddenWindows) {
        if (w && !lst.contains(w))
            lst.append(w);
    }
    for (MyWidget *w : outWindows) {
        if (w && !lst.contains(w))
            lst.append(w);
    }

    return lst;
}

#ifdef DOWNLOAD_LINKS
void ApplicationWindow::downloadManual()
{
    if (isNonInteractiveContext(this))
        return;
    QDesktopServices::openUrl(QUrl(MANUAL_URI));
}
#endif // defined DOWNLOAD_LINKS

void ApplicationWindow::showHomePage()
{
    if (isNonInteractiveContext(this))
        return;
    QDesktopServices::openUrl(QUrl(HOMEPAGE_URI));
}

void ApplicationWindow::showForums()
{
    if (isNonInteractiveContext(this))
        return;
    QDesktopServices::openUrl(QUrl(FORUM_URI));
}

void ApplicationWindow::showBugTracker()
{
    if (isNonInteractiveContext(this))
        return;
    QDesktopServices::openUrl(QUrl(BUGREPORT_URI));
}

void ApplicationWindow::parseCommandLineArguments(const QStringList &args)
{
    int num_args = args.count();
    if (num_args == 0)
        return;

    QString str;
    bool exec = false;
    int scriptArg = 0;
    //	foreach(str, args){
    for (int i = 0; i < num_args; ++i) {
        str = args[i];
        if ((str == "-a" || str == "--about") || (str == "-m" || str == "--manual")) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("<b> %1 </b>: This command line option must be used without "
                                         "other arguments!")
                                              .arg(str));
            }
        } else if (str == "-v" || str == "--version") {
            QString s = SciDAVis::versionString() + SciDAVis::extraVersion() + "\n";
            s += QObject::tr("Released") + ": " + SciDAVis::releaseDateString() + "\n";
            s += SciDAVis::copyrightString() + "\n";

#ifdef Q_OS_WIN
            if (isNonInteractiveContext(this)) {
                std::cout << s.toStdString();
            } else {
                hide();
                QMessageBox::information(this, tr("ToriGraph") + " - " + tr("Version"), s);
            }
#else
            std::cout << s.toStdString();
#endif
            ::exit(0);
        } else if (str == "-h" || str == "--help") {
            QString s = "\n" + tr("Usage") + ": ";
            s += "ToriGraph [" + tr("options") + "] [" + tr("file") + "_" + tr("name") + "]\n\n";
            s += tr("Valid options are") + ":\n";
            s += "-a " + tr("or") + " --about: " + tr("show about dialog and exit") + "\n";
            s += "-h " + tr("or") + " --help: " + tr("show command line options") + "\n";
            s += "-l=XX " + tr("or") + " --lang=XX: " + tr("start ToriGraph in language")
                    + " XX ('en', 'fr', 'de', ...)\n";
            s += "-m " + tr("or")
                    + " --manual: " + tr("show ToriGraph manual in a standalone window") + "\n";
            s += "-v " + tr("or") + " --version: " + tr("print ToriGraph version and release date")
                    + "\n";
            s += "-x " + tr("or") + " --execute: " + tr("execute the script file given as argument")
                    + "\n";
            s += "--headless: " + tr("run without visible GUI window for Python automation") + "\n";
            s += "--rpc-port <port>: " + tr("bind localhost JSON-RPC 2.0 server to <port> (0 = ephemeral)") + "\n";
            s += "--rpc-port-file <path>: " + tr("write bound JSON-RPC endpoint URL atomically to <path>") + "\n";
            s += "--rpc-pipe <name>: " + tr("start local named pipe JSON-RPC 2.0 server") + "\n";
            s += "--run-python <script.py>: " + tr("execute Python automation script against this ToriGraph instance")
                    + "\n\n";
#ifdef ORIGIN_IMPORT
            s += "'" + tr("file") + "_" + tr("name") + "' "
                    + tr("can be any .sciprj, .sciprj.gz, .qti, qti.gz, .opj, .ogm, .ogw, .ogg, "
                         ".org, .py or ASCII file")
                    + "\n";
#else
            s += "'" + tr("file") + "_" + tr("name") + "' "
                    + tr("can be any .sciprj, .sciprj.gz, .qti, qti.gz, .py or ASCII file") + "\n";
#endif
#ifdef Q_OS_WIN
            if (isNonInteractiveContext(this)) {
                std::cout << s.toStdString();
            } else {
                hide();
                QMessageBox::information(this, tr("ToriGraph - Help"), s);
            }
#else
            std::cout << s.toStdString();
#endif
            ::exit(0);
        } else if (str.startsWith("--lang=") || str.startsWith("-l=")) {
            QString locale = str.mid(str.indexOf('=') + 1);
            if (locales.contains(locale))
                switchToLanguage(locale);

            if (!locales.contains(locale) && !isNonInteractiveContext(this))
                QMessageBox::critical(
                        this, tr("Error"),
                        tr("<b> %1 </b>: Wrong locale option or no translation available!")
                                .arg(locale));
        } else if (str.startsWith("--execute") || str.startsWith("-x"))
            exec = true;
        else if (str.startsWith("-") || str.startsWith("--")) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("<b> %1 </b> unknown command line option!").arg(str) + "\n"
                                              + tr("Type %1 to see the list of the valid options.")
                                                        .arg("'scidavis -h'"));
            }
        }
        if (str.startsWith("-"))
            scriptArg = i; // save last flag
    }

    if (scriptArg < num_args - 1)
        scriptArg++;
    QString file_name = args[scriptArg]; // last argument

    QStringList scriptArgs;
    for (auto i = scriptArg + 1; i < num_args; ++i)
        scriptArgs << args[i];

    if (file_name.startsWith("-"))
        return; // no file name given

    if (!file_name.isEmpty()) {
        QFileInfo fi(file_name);
        if (fi.isDir()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(
                        this, tr("File opening error"),
                        tr("<b>%1</b> is a directory, please specify a file name!").arg(file_name));
            }
            return;
        } else if (!fi.isReadable()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("You don't have the permission to open this file: <b>%1</b>")
                                              .arg(file_name));
            }
            return;
        } else if (!fi.exists()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("File opening error"),
                                      tr("The file: <b>%1</b> doesn't exist!").arg(file_name));
            }
            return;
        }

        workingDir = fi.absolutePath();
        saveSettings(); // the recent projects must be saved

        ApplicationWindow *a;
        if (exec)
            a = loadScript(file_name, scriptArgs, exec);
        else
            a = open(file_name, scriptArgs);

        if (a && a != this) {
            a->workingDir = workingDir;
            close();
        }
    }
}

QStringList ApplicationWindow::matrixNames()
{
    QStringList names;
    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (w->inherits("Matrix"))
            names << static_cast<Matrix *>(w)->name();
    }
    return names;
}

bool ApplicationWindow::alreadyUsedName(const QString &label)
{
    if (d_batch_window_updates_depth > 0)
        return isBatchNameUsedByOther(label, nullptr);
    QList<MyWidget *> windows = windowsList();
    bool used = false;
    foreach (MyWidget *widget, windows) {
        if (widget && widget->name() == label) {
            used = true;
            break;
        }
    }
    return used;
}

bool ApplicationWindow::projectHasMatrices()
{
    QList<MyWidget *> windows = windowsList();
    bool has = false;
    foreach (MyWidget *w, windows) {
        if (w->inherits("Matrix")) {
            has = true;
            break;
        }
    }
    return has;
}

bool ApplicationWindow::projectHas2DPlots()
{
    QList<MyWidget *> windows = windowsList();
    bool hasPlots = false;
    foreach (MyWidget *w, windows) {
        if (w->inherits("MultiLayer")) {
            hasPlots = true;
            break;
        }
    }
    return hasPlots;
}

bool ApplicationWindow::projectHas3DPlots()
{
    QList<MyWidget *> windows = windowsList();
    bool has3DPlots = false;
    foreach (MyWidget *w, windows) {
        if (w->inherits("Graph3D")) {
            has3DPlots = true;
            break;
        }
    }
    return has3DPlots;
}

void ApplicationWindow::setShowWindowsPolicy(bool checked)
{
    Q_UNUSED(checked)

    QAction *act = qobject_cast<QAction *>(sender());
    if (!act)
        return;
    int p = act->data().toInt();

    if (show_windows_policy == (ShowWindowsPolicy)p)
        return;

    show_windows_policy = (ShowWindowsPolicy)p;
    if (show_windows_policy == HideAll) {
        QList<MyWidget *> lst = windowsList();
        foreach (MyWidget *widget, lst) {
            if (!widget)
                continue;
            if (!hiddenWindows.contains(widget)) {
                hiddenWindows.append(widget);
                if (d_batch_window_updates_depth > 0)
                    d_batch_hidden_set.insert(widget);
            }
            widget->hide();
            setListView(widget->name(), tr("Hidden"));
        }
    } else
        showAllFolderWindows();
}

void ApplicationWindow::showFindDialogue()
{
    if (isNonInteractiveContext(this))
        return;

    FindDialog *fd = new FindDialog(this);
    fd->setAttribute(Qt::WA_DeleteOnClose);
    fd->exec();
}

void ApplicationWindow::startRenameFolder()
{
    FolderListItem *fi = current_folder->folderListItem();
    if (!fi)
        return;

    disconnect(&folders, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this,
               SLOT(folderItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
    fi->setFlags(fi->flags() | Qt::ItemIsEditable);
    fi->treeWidget()->editItem(fi, 0);
}

void ApplicationWindow::startRenameFolder(QTreeWidgetItem *item, int column)
{
    if (!item || item == folders.topLevelItem(0))
        return;

    if (item->treeWidget() == &lv && item->type() == FolderListItem::FolderType) {
        disconnect(
                &folders,
                SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *, QTreeWidgetItem *)),
                this, SLOT(folderItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
        current_folder = ((FolderListItem *)item)->folder();
        FolderListItem *it = current_folder->folderListItem();
        it->treeWidget()->setCurrentItem(it, 0);
        it->setFlags(it->flags() | Qt::ItemIsEditable);
        it->treeWidget()->editItem(it, column);
    } else {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->treeWidget()->editItem(item, 0);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }
}

void ApplicationWindow::renameFolder(QTreeWidgetItem *it, int col, const QString &text)
{
    Q_UNUSED(col)

    if (!it)
        return;

    if (it->text(0) == text)
        return;

    Folder *parent = (Folder *)current_folder->parent();
    if (!parent) // the parent folder is the project folder (it always exists)
        parent = projectFolder();

    if (text.isEmpty()) {
        if (!isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"), tr("Please enter a valid name!"));
        it->setFlags(it->flags() | Qt::ItemIsEditable);
        return;
    }

    QStringList lst = parent->subfolders();
    lst.removeAll(current_folder->name());
    while (lst.contains(text)) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Name already exists!") + "\n"
                                          + tr("Please choose another name!"));
        }

        it->setFlags(it->flags() | Qt::ItemIsEditable);
        it->setText(0, current_folder->name());
        it->treeWidget()->editItem(it, 0);
        return;
    }

    current_folder->setName(text);
    it->setText(0, text);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    connect(&folders, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this,
            SLOT(folderItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
    folders.scrollToItem(parent->folderListItem(), QAbstractItemView::EnsureVisible);
    folders.setCurrentItem(parent->folderListItem()); // update the list views
}

void ApplicationWindow::showAllFolderWindows()
{
    QList<MyWidget *> lst = current_folder->windowsList();
    foreach (MyWidget *w, lst) { // force show all windows in current folder
        if (w) {
            updateWindowLists(w);
            switch (w->status()) {
            case MyWidget::Hidden:
                w->showNormal();
                break;

            case MyWidget::Normal:
                w->showNormal();
                break;

            case MyWidget::Minimized:
                w->showMinimized();
                break;

            case MyWidget::Maximized:
                w->showMaximized();
                break;
            }
        }
    }

    if ((current_folder->children()).isEmpty())
        return;

    FolderListItem *fi = current_folder->folderListItem();
    FolderListItem *item = (FolderListItem *)fi->child(0);
    int initial_depth = item->depth();
    QTreeWidgetItemIterator it(item);
    while (item && item->depth() >= initial_depth) { // show/hide windows in all subfolders
        lst = ((Folder *)item->folder())->windowsList();
        foreach (MyWidget *w, lst) {
            if (w && show_windows_policy == SubFolders) {
                updateWindowLists(w);
                switch (w->status()) {
                case MyWidget::Hidden:
                    w->showNormal();
                    break;

                case MyWidget::Normal:
                    w->showNormal();
                    break;

                case MyWidget::Minimized:
                    w->showMinimized();
                    break;

                case MyWidget::Maximized:
                    w->showMaximized();
                    break;
                }
            } else
                w->hide();
        }

        it++;
        item = (FolderListItem *)(*it);
    }
}

void ApplicationWindow::hideAllFolderWindows()
{
    QList<MyWidget *> lst = current_folder->windowsList();
    foreach (MyWidget *w, lst)
        hideWindow(w);

    if ((current_folder->children()).isEmpty())
        return;

    if (show_windows_policy == SubFolders) {
        FolderListItem *fi = current_folder->folderListItem();
        FolderListItem *item = (FolderListItem *)fi->child(0);
        int initial_depth = item->depth();
        QTreeWidgetItemIterator it(item);
        while (item && item->depth() >= initial_depth) {
            lst = item->folder()->windowsList();
            foreach (MyWidget *w, lst)
                hideWindow(w);

            it++;
            item = (FolderListItem *)(*it);
        }
    }
}

void ApplicationWindow::projectProperties()
{
    if (isNonInteractiveContext(this))
        return;

    QString s = QString(current_folder->name()) + "\n\n";
    s += "\n\n\n";
    s += tr("Type") + ": " + tr("Project") + "\n\n";
    if (projectname != "untitled") {
        s += tr("Path") + ": " + projectname + "\n\n";

        QFileInfo fi(projectname);
        s += tr("Size") + ": " + QString::number(fi.size()) + " " + tr("bytes") + "\n\n";
    }

    s += tr("Contents") + ": " + QString::number(windowsList().count()) + " " + tr("windows");

    s += ", " + QString::number(current_folder->subfolders().count()) + " " + tr("folders")
            + "\n\n";
    s += "\n\n\n";

    if (projectname != "untitled") {
        QFileInfo fi(projectname);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
        s += tr("Created") + ": " + QLocale().toString(fi.created()) + "\n\n";
#else
        s += tr("Created") + ": " + QLocale().toString(fi.birthTime()) + "\n\n";
#endif
        s += tr("Modified") + ": " + QLocale().toString(fi.lastModified()) + "\n\n";
    } else
        s += tr("Created") + ": " + current_folder->birthDate() + "\n\n";

    QMessageBox *mbox = new QMessageBox(tr("Properties"), s, QMessageBox::NoIcon, QMessageBox::Ok,
                                        QMessageBox::NoButton, QMessageBox::NoButton, this);

    mbox->setIconPixmap(IconLoader::duotonePixmap("settings", 32));
    mbox->show();
}

void ApplicationWindow::folderProperties()
{
    if (isNonInteractiveContext(this))
        return;

    if (!current_folder->parent()) {
        projectProperties();
        return;
    }

    QString s = current_folder->name() + "\n\n";
    s += "\n\n\n";
    s += tr("Type") + ": " + tr("Folder") + "\n\n";
    s += tr("Path") + ": " + current_folder->path() + "\n\n";
    s += tr("Contents") + ": " + QString::number(current_folder->windowsList().count()) + " "
            + tr("windows");
    s += ", " + QString::number(current_folder->subfolders().count()) + " " + tr("folders")
            + "\n\n";
    // s += "\n\n\n";
    s += tr("Created") + ": " + current_folder->birthDate() + "\n\n";
    // s += tr("Modified") + ": " + current_folder->modificationDate() + "\n\n";

    QMessageBox *mbox = new QMessageBox(tr("Properties"), s, QMessageBox::NoIcon, QMessageBox::Ok,
                                        QMessageBox::NoButton, QMessageBox::NoButton, this);

    mbox->setIconPixmap(IconLoader::duotonePixmap("folder-open", 32));
    mbox->show();
}

void ApplicationWindow::addFolder()
{
    QStringList lst = current_folder->subfolders();
    QString name = tr("New Folder");
    lst = lst.filter(name);
    if (!lst.isEmpty())
        name += " (" + QString::number(lst.size() + 1) + ")";

    Folder &f = current_folder->addChild<Folder>(name);
    addFolderListViewItem(&f);

    FolderListItem *fi = new FolderListItem(current_folder->folderListItem(), &f);
    f.setFolderListItem(fi);
    fi->setFlags(fi->flags() | Qt::ItemIsEditable);
    fi->treeWidget()->setCurrentItem(fi, 0);
    if (!isNonInteractiveContext(this))
        fi->treeWidget()->editItem(fi, 0);
    fi->treeWidget()->resizeColumnToContents(0);
    updateExplorerFolderPaneVisibility();
}

bool ApplicationWindow::deleteFolder(Folder *f)
{
    if (!f || f == projectFolder())
        return false;

    if (confirmCloseFolder && !isNonInteractiveContext(this)
        && QMessageBox::question(
                this, tr("Delete folder?"),
                tr("Delete folder '%1' and all the windows it contains?\nThis operation cannot be "
                   "undone.")
                        .arg(f->name()),
                tr("&Delete"), tr("&Cancel"), QString(), 1, 1)
                != 0) {
        return false;
    }

    // Collect the subtree in bottom-up (post-order) BEFORE deleting any Folder or FolderListItem,
    // avoiding QTreeWidgetItemIterator use-after-free when parent items are deleted.
    QList<QPointer<Folder>> doomedFolders;
    QSet<Folder *> doomedSet;
    std::function<void(Folder *)> collectPostOrder = [&](Folder *node) {
        if (!node || doomedSet.contains(node))
            return;
        doomedSet.insert(node);
        for (Folder *child : node->folders())
            collectPostOrder(child);
        doomedFolders.append(QPointer<Folder>(node));
    };
    collectPostOrder(f);

    if (current_folder && doomedSet.contains(current_folder)) {
        Folder *fallback = qobject_cast<Folder *>(f->parent());
        if (!fallback || doomedSet.contains(fallback))
            fallback = projectFolder();
        current_folder = fallback;
    }

    for (const QPointer<Folder> &fp : doomedFolders) {
        Folder *df = fp.data();
        if (!df)
            continue;
        d_folder_active_windows.remove(df);
        const QList<MyWidget *> wins = df->windowsList();
        for (MyWidget *w : wins) {
            if (!w)
                continue;
            w->askOnCloseEvent(false);
            closeWindow(w);
        }
    }

    // Remove any folder entries referencing the deleted subtree from lv.
    for (int i = lv.topLevelItemCount() - 1; i >= 0; --i) {
        QTreeWidgetItem *it = lv.topLevelItem(i);
        if (it && it->type() == FolderListItem::FolderType) {
            Folder *lf = static_cast<FolderListItem *>(it)->folder();
            if (doomedSet.contains(lf))
                delete lv.takeTopLevelItem(i);
        }
    }

    FolderListItem *rootFi = f->folderListItem();
    for (const QPointer<Folder> &fp : doomedFolders) {
        if (fp)
            fp->setFolderListItem(nullptr);
    }
    if (rootFi) {
        QSignalBlocker sbFolders(&folders);
        if (QTreeWidgetItem *pItem = rootFi->parent())
            pItem->removeChild(rootFi);
        else if (const int topIdx = folders.indexOfTopLevelItem(rootFi); topIdx >= 0)
            folders.takeTopLevelItem(topIdx);
        delete rootFi;
    }

    for (const QPointer<Folder> &fp : doomedFolders) {
        if (fp)
            delete fp.data();
    }

    if (!d_suppress_delete_folder_undo_clear && d_project && d_project->undoStack())
        d_project->undoStack()->clear();

    updateExplorerFolderPaneVisibility();
    updateEmptyGraphPanePlaceholder();
    return true;
}

void ApplicationWindow::deleteFolder()
{
    Folder *parent = (Folder *)current_folder->parent();
    if (!parent)
        parent = projectFolder();

    folders.blockSignals(true);

    if (deleteFolder(current_folder)) {
        current_folder = parent;
        folders.setCurrentItem(parent->folderListItem());
        changeFolder(parent, true);
    }

    folders.blockSignals(false);
    folders.setFocus();
}

void ApplicationWindow::folderItemDoubleClicked(QTreeWidgetItem *it, int column)
{
    Q_UNUSED(column)
    if (!it)
        return;

    if (it->type() == FolderListItem::FolderType) {
        FolderListItem *item = ((FolderListItem *)it)->folder()->folderListItem();
        folders.setCurrentItem(item);
    } else if (it->type() == WindowListItem::WindowType) {
        MyWidget *w = ((WindowListItem *)it)->window();
        if (!w)
            return;
        if (d_workspace.activeSubWindow() != w)
            activateSubWindow(w);
        else {
            if (!w->isMaximized())
                w->setMaximized();
            else
                w->setNormal();
        }
    }
}

void ApplicationWindow::folderItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)

    if (!current)
        return;

    if (current == previous)
        return;

    current->setExpanded(true);
    changeFolder(((FolderListItem *)current)->folder());
    folders.scrollToItem(folders.currentItem(), QAbstractItemView::EnsureVisible);
    folders.setFocus();
}

void ApplicationWindow::hideFolderWindows(Folder *f)
{
    QList<MyWidget *> lst = f->windowsList();
    foreach (MyWidget *w, lst)
        w->hide();

    if ((f->children()).isEmpty())
        return;

    FolderListItem *fi = f->folderListItem();
    FolderListItem *item = (FolderListItem *)fi->child(0);
    if (!item)
        return;
    int initial_depth = item->depth();
    QTreeWidgetItemIterator it(item);
    while (item && item->depth() >= initial_depth) {
        lst = item->folder()->windowsList();
        foreach (MyWidget *w, lst)
            w->hide();
        it++;
        item = (FolderListItem *)(*it);
    }
}

bool ApplicationWindow::changeFolder(Folder *newFolder, bool force)
{
    if (!newFolder)
        return false;
    if (current_folder == newFolder && !force)
        return false;

    const bool isSplitView = d_modern_ribbon
            && d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph;

    deactivateFolders();
    newFolder->folderListItem()->setActive(true);

    Folder *oldFolder = current_folder;
    MyWidget::Status old_active_window_state = MyWidget::Normal;
    MyWidget *old_active_window = oldFolder ? oldFolder->activeWindow() : nullptr;
    if (old_active_window)
        old_active_window_state = old_active_window->status();

    MyWidget::Status active_window_state = MyWidget::Normal;
    MyWidget *active_window = newFolder->activeWindow();
    if (!active_window)
        active_window = d_folder_active_windows.value(newFolder).data();
    if (active_window && active_window->folder() != newFolder)
        active_window = nullptr;
    if (active_window)
        active_window_state = active_window->status();

    if (oldFolder)
        d_folder_active_windows[oldFolder] = old_active_window;

    d_workspace.blockSignals(true);
    if (oldFolder)
        hideFolderWindows(oldFolder);
    current_folder = newFolder;

    lv.clear();

    QObjectList folderLst = newFolder->children();
    if (!folderLst.isEmpty()) {
        foreach (QObject *f, folderLst)
            addFolderListViewItem(static_cast<Folder *>(f));
    }

    const QSet<MyWidget *> hiddenSet(hiddenWindows.begin(), hiddenWindows.end());
    const QSet<MyWidget *> outSet(outWindows.begin(), outWindows.end());

    QList<MyWidget *> lst = newFolder->windowsList();
    foreach (MyWidget *w, lst) {
        w->blockSignals(true);
        if (!hiddenSet.contains(w) && !outSet.contains(w)
            && show_windows_policy != HideAll) {
            // show only windows in the current folder which are not hidden by the user
            if (isSplitView) {
                if (w->isMaximized() || w->isMinimized()) {
                    w->setWindowState(
                            w->windowState() & ~(Qt::WindowMaximized | Qt::WindowMinimized));
                }
                w->setStatus(MyWidget::Normal);
                w->showNormal();
            } else if (w->status() == MyWidget::Normal || w->status() == MyWidget::Maximized
                       || w->status() == MyWidget::Hidden) {
                w->showNormal();
            } else if (w->status() == MyWidget::Minimized) {
                w->showMinimized();
            }
        } else {
            w->hide();
            w->setStatus(MyWidget::Hidden);
        }

        addListViewItem(w);
    }

    if (!(newFolder->children()).isEmpty()) {
        FolderListItem *fi = newFolder->folderListItem();
        FolderListItem *item = fi ? (FolderListItem *)fi->child(0) : nullptr;
        if (item) {
            int initial_depth = item->depth();
            QTreeWidgetItemIterator it(item);
            while (item && item->depth() >= initial_depth) { // show/hide windows in subfolders
                lst = ((Folder *)item->folder())->windowsList();
                foreach (MyWidget *w, lst) {
                    if (!hiddenSet.contains(w) && !outSet.contains(w)) {
                        if (show_windows_policy == SubFolders) {
                            if (isSplitView) {
                                if (w->isMaximized() || w->isMinimized()) {
                                    w->setWindowState(w->windowState()
                                                      & ~(Qt::WindowMaximized | Qt::WindowMinimized));
                                }
                                w->setStatus(MyWidget::Normal);
                                w->showNormal();
                            } else if (w->status() == MyWidget::Normal
                                       || w->status() == MyWidget::Maximized
                                       || w->status() == MyWidget::Hidden) {
                                w->showNormal();
                            } else if (w->status() == MyWidget::Minimized) {
                                w->showMinimized();
                            }
                        } else if (w->isVisible())
                            w->hide();
                    } else if (w->isVisible()) {
                        w->hide();
                    }
                }
                it++;
                item = (FolderListItem *)(*it);
            }
        }
    }

    d_workspace.blockSignals(false);
    foreach (MyWidget *w, newFolder->windowsList())
        w->blockSignals(false);

    if ((!active_window || hiddenSet.contains(active_window)
         || outSet.contains(active_window))
        && show_windows_policy != HideAll) {
        active_window = nullptr;
        for (MyWidget *cand : newFolder->windowsList()) {
            if (cand && !hiddenSet.contains(cand) && !outSet.contains(cand)) {
                active_window = cand;
                active_window_state = cand->status();
                break;
            }
        }
    }

    if (active_window && !hiddenSet.contains(active_window)
        && !outSet.contains(active_window) && show_windows_policy != HideAll) {
        d_workspace.setActiveSubWindow(active_window);
        if (isSplitView) {
            if (active_window->isMaximized() || active_window->isMinimized()) {
                active_window->setWindowState(active_window->windowState()
                                              & ~(Qt::WindowMaximized | Qt::WindowMinimized));
            }
            active_window->setStatus(MyWidget::Normal);
            active_window->showNormal();
        } else if (active_window_state == MyWidget::Minimized) {
            active_window->showMinimized(); // d_workspace.setActiveWindow() makes minimized windows
                                            // to be shown normally
        } else if (active_window_state == MyWidget::Maximized) {
            if (active_window->inherits("Graph3D"))
                ((Graph3D *)active_window)->setIgnoreFonts(true);
            active_window->showMaximized();
            if (active_window->inherits("Graph3D"))
                ((Graph3D *)active_window)->setIgnoreFonts(false);
        }
        current_folder->setActiveWindow(active_window);
        customMenu(active_window);
        customToolBars(active_window);
        if (d_modern_ribbon) {
            d_modern_ribbon->updateActiveContext(active_window);
            if (isSplitView)
                d_modern_ribbon->applySplitWorkspaceGeometry();
        }
    } else {
        d_workspace.setActiveSubWindow(nullptr);
        current_folder->setActiveWindow(nullptr);
        customMenu(nullptr);
        customToolBars(nullptr);
        if (d_modern_ribbon) {
            d_modern_ribbon->updateActiveContext(nullptr);
            if (isSplitView)
                d_modern_ribbon->applySplitWorkspaceGeometry();
        }
    }

    if (d_modern_ribbon && d_modern_ribbon->isFocusHudMode()
        && d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutFreeMdi) {
        for (MyWidget *mw : newFolder->windowsList()) {
            if (!mw)
                continue;
            const bool keepVisible =
                    (mw == active_window)
                    && (qobject_cast<MultiLayer *>(mw) || qobject_cast<Graph3D *>(mw));
            if (!keepVisible && !mw->isHidden())
                mw->hide();
        }
    }

    if (GraphMiniToolbar *mt = GraphMiniToolbar::instance())
        mt->cancelScheduledAndHide();
    if (d_graph_inspector_dock) {
        MultiLayer *mlTarget = qobject_cast<MultiLayer *>(active_window);
        if (!mlTarget && isSplitView) {
            for (MyWidget *cand : newFolder->windowsList()) {
                if (MultiLayer *candMl = qobject_cast<MultiLayer *>(cand)) {
                    if (!hiddenSet.contains(candMl) && !outSet.contains(candMl)
                        && candMl->isVisible()) {
                        mlTarget = candMl;
                        break;
                    }
                }
            }
        }
        d_graph_inspector_dock->setTargetMultiLayer(mlTarget);
    }
    updateGraphInspectorResponsiveness();
    updateUndoRedoActionsForActiveWindow();
    updateStatusBarForWindow(d_status_info, active_window);
    updateEmptyGraphPanePlaceholder();

    if (old_active_window && oldFolder && oldFolder != newFolder) {
        old_active_window->setStatus(old_active_window_state);
        oldFolder->setActiveWindow(old_active_window);
    }
    scheduleSyncWorkspaceHiddenTabs(this);

    filterExplorerWindows();
    syncExplorerSelectionToWindow(lv, active_window);
    return true;
}

void ApplicationWindow::deactivateFolders()
{
    QTreeWidgetItemIterator it(&folders);
    while (*it) {
        ((FolderListItem *)(*it))->setActive(false);
        it++;
    }
}

void ApplicationWindow::addListViewItem(MyWidget *w)
{
    if (!w || isInternalHelperWindow(w))
        return;

    WindowListItem *it = new WindowListItem(&lv, w);
    if (w->inherits("Matrix")) {
        it->setIcon(0, IconLoader::duotoneIcon("new-matrix"));
        it->setText(1, tr("Matrix"));
    } else if (w->inherits("Table")) {
        it->setIcon(0, IconLoader::duotoneIcon("new-table"));
        it->setText(1, tr("Table"));
    } else if (w->inherits("Note")) {
        it->setIcon(0, IconLoader::duotoneIcon("new-note"));
        it->setText(1, tr("Note"));
    } else if (w->inherits("MultiLayer")) {
        it->setIcon(0, IconLoader::duotoneIcon("new-graph"));
        it->setText(1, tr("Graph"));
    } else if (w->inherits("Graph3D")) {
        it->setIcon(0, IconLoader::duotoneIcon("plot-3d-ribbon"));
        it->setText(1, tr("3D Graph"));
    }

    it->setText(0, w->name());
    it->setText(2, w->aspect());
    it->setText(3, w->birthDate());
    it->setText(4, w->windowLabel());
    syncBatchWindowName(w);
    if (WorkbookManager *wb = WorkbookManager::existing(this))
        wb->decorateListItem(it, w); // D8: "Sheet of <book>"
    QString tip = w->name();
    if (!it->text(1).isEmpty())
        tip += QStringLiteral("\n") + tr("Type") + QStringLiteral(": ") + it->text(1);
    if (!w->aspect().isEmpty())
        tip += QStringLiteral("  (") + w->aspect() + QStringLiteral(")");
    if (!w->windowLabel().isEmpty())
        tip += QStringLiteral("\n") + tr("Label") + QStringLiteral(": ") + w->windowLabel();
    it->setToolTip(0, tip);
    it->setToolTip(1, tip);
    if (lv.columnCount() >= 5 && lv.isColumnHidden(3)) {
        if (d_batch_window_updates_depth > 0) {
            d_batch_pending_lv_resize = true;
        } else if (!lv.isColumnHidden(1)) {
            lv.resizeColumnToContents(1);
            lv.resizeColumnToContents(2);
        }
    }
}

void ApplicationWindow::windowProperties()
{
    if (isNonInteractiveContext(this))
        return;

    QTreeWidgetItem *cur = lv.currentItem();
    if (!cur || cur->type() != WindowListItem::WindowType)
        return;
    WindowListItem *it = static_cast<WindowListItem *>(cur);
    MyWidget *w = it->window();
    if (!w)
        return;

    QMessageBox *mbox =
            new QMessageBox(tr("Properties"), QString(), QMessageBox::NoIcon, QMessageBox::Ok,
                            QMessageBox::NoButton, QMessageBox::NoButton, this);

    QString s = QString(w->name()) + "\n\n";
    s += "\n\n\n";

    s += tr("Label") + ": " + ((MyWidget *)w)->windowLabel() + "\n\n";

    if (w->inherits("Matrix")) {
        mbox->setIconPixmap(IconLoader::duotonePixmap("new-matrix", 32));
        s += tr("Type") + ": " + tr("Matrix") + "\n\n";
    } else if (w->inherits("Table")) {
        mbox->setIconPixmap(IconLoader::duotonePixmap("new-table", 32));
        s += tr("Type") + ": " + tr("Table") + "\n\n";
    } else if (w->inherits("Note")) {
        mbox->setIconPixmap(IconLoader::duotonePixmap("new-note", 32));
        s += tr("Type") + ": " + tr("Note") + "\n\n";
    } else if (w->inherits("MultiLayer")) {
        mbox->setIconPixmap(IconLoader::duotonePixmap("new-graph", 32));
        s += tr("Type") + ": " + tr("Graph") + "\n\n";
    } else if (w->inherits("Graph3D")) {
        mbox->setIconPixmap(IconLoader::duotonePixmap("plot-3d-ribbon", 32));
        s += tr("Type") + ": " + tr("3D Graph") + "\n\n";
    }
    s += tr("Path") + ": " + current_folder->path() + "\n\n";
    s += tr("Created") + ": " + w->birthDate() + "\n\n";
    s += tr("Status") + ": " + it->text(2) + "\n\n";
    mbox->setText(s);
    mbox->show();
}

void ApplicationWindow::addFolderListViewItem(Folder *f)
{
    if (!f)
        return;

    FolderListItem *it = new FolderListItem(&lv, f);
    it->setActive(false);
    it->setText(0, f->name());
    it->setText(1, tr("Folder"));
    it->setText(3, f->birthDate());
}

void ApplicationWindow::find(const QString &s, bool windowNames, bool labels, bool folderNames,
                             bool caseSensitive, bool partialMatch, bool subfolders)
{
    if (windowNames || labels) {
        MyWidget *w =
                current_folder->findWindow(s, windowNames, labels, caseSensitive, partialMatch);
        if (w) {
            activateSubWindow(w);
            return;
        }

        if (subfolders) {
            FolderListItem *item = (FolderListItem *)folders.currentItem()->child(0);
            QTreeWidgetItemIterator it(item);
            while (item) {
                Folder *f = item->folder();
                MyWidget *w = f->findWindow(s, windowNames, labels, caseSensitive, partialMatch);
                if (w) {
                    folders.setCurrentItem(f->folderListItem());
                    activateSubWindow(w);
                    return;
                }
                it++;
                item = (FolderListItem *)(*it);
            }
        }
    }

    if (folderNames) {
        Folder *f = current_folder->findSubfolder(s, caseSensitive, partialMatch);
        if (f) {
            folders.setCurrentItem(f->folderListItem());
            return;
        }

        if (subfolders) {
            FolderListItem *item = (FolderListItem *)folders.currentItem()->child(0);
            QTreeWidgetItemIterator it(item);
            while (item) {
                Folder *f = item->folder()->findSubfolder(s, caseSensitive, partialMatch);
                if (f) {
                    folders.setCurrentItem(f->folderListItem());
                    return;
                }

                it++;
                item = (FolderListItem *)(*it);
            }
        }
    }

    if (!isNonInteractiveContext(this)) {
        QMessageBox::warning(this, tr("No match found"),
                             tr("Sorry, no match found for string: '%1'").arg(s));
    }
}

void ApplicationWindow::dropFolderItems(QTreeWidgetItem *dest)
{
    if (!dest || draggedItems.isEmpty())
        return;

    Folder *dest_f = ((FolderListItem *)dest)->folder();

    QTreeWidgetItem *it;
    QStringList subfolders = dest_f->subfolders();

    foreach (it, draggedItems) {
        if (it->type() == FolderListItem::FolderType) {
            Folder *f = ((FolderListItem *)it)->folder();
            FolderListItem *src = f->folderListItem();
            if (dest_f == f) {
                if (!isNonInteractiveContext(this))
                    QMessageBox::critical(this, tr("Error"), tr("Cannot move an object to itself!"));
                return;
            }

            if (((FolderListItem *)dest)->isChildOf(src)) {
                if (!isNonInteractiveContext(this)) {
                    QMessageBox::critical(this, tr("Error"),
                                          tr("Cannot move a parent folder into a child folder!"));
                }
                draggedItems.clear();
                folders.setCurrentItem(current_folder->folderListItem());
                return;
            }

            Folder *parent = (Folder *)f->parent();
            if (!parent)
                parent = projectFolder();
            if (dest_f == parent)
                return;

            if (subfolders.contains(f->name())) {
                if (!isNonInteractiveContext(this)) {
                    QMessageBox::critical(this, tr("ToriGraph") + " - " + tr("Skipped moving folder"),
                                          tr("The destination folder already contains a folder called "
                                             "'%1'! Folder skipped!")
                                                  .arg(f->name()));
                }
            } else
                moveFolder(src, (FolderListItem *)dest);
        } else {
            if (dest_f == current_folder)
                return;

            MyWidget *w = ((WindowListItem *)it)->window();
            if (w) {
                if (WorkbookManager *wb = WorkbookManager::existing(this))
                    if (Table *sheet = qobject_cast<Table *>(w))
                        if (!wb->bookOf(sheet).isEmpty())
                            wb->detachSheet(sheet, false); // D8
                current_folder->removeWindow(w);
                w->hide();
                dest_f->addWindow(w);
                delete it;
            }
        }
    }

    draggedItems.clear();
    current_folder = dest_f;
    folders.setCurrentItem(dest_f->folderListItem());
    changeFolder(dest_f, true);
    folders.setFocus();
    modifiedProject();
}

void ApplicationWindow::moveFolder(FolderListItem *src, FolderListItem *dest)
{
    if (!src || !dest || src == dest)
        return;

    Folder *destRoot = dest->folder();
    Folder *srcRoot = src->folder();
    if (!destRoot || !srcRoot || destRoot == srcRoot)
        return;

    // Guard against moving a folder into itself or one of its descendants.
    for (QObject *p = destRoot; p; p = p->parent()) {
        if (p == srcRoot)
            return;
    }

    QSignalBlocker sbFolders(&folders);
    QSet<Folder *> movedOldFolders;

    std::function<void(Folder *, Folder *, FolderListItem *)> cloneSubtree =
            [&](Folder *oldFolder, Folder *destParentFolder, FolderListItem *destParentItem) {
                if (!oldFolder || !destParentFolder || !destParentItem)
                    return;

                movedOldFolders.insert(oldFolder);

                Folder *newFolder = &destParentFolder->addChild<Folder>(oldFolder->name());
                newFolder->setBirthDate(oldFolder->birthDate());
                newFolder->setModificationDate(oldFolder->modificationDate());

                FolderListItem *newItem = new FolderListItem(destParentItem, newFolder);
                newItem->setText(0, oldFolder->name());
                newFolder->setFolderListItem(newItem);

                MyWidget *oldActive = oldFolder->activeWindow();
                const QList<MyWidget *> wins = oldFolder->windowsList();
                for (MyWidget *w : wins) {
                    if (!w)
                        continue;
                    oldFolder->removeWindow(w);
                    w->hide();
                    newFolder->addWindow(w);
                }
                if (oldActive)
                    newFolder->setActiveWindow(oldActive);
                if (d_folder_active_windows.contains(oldFolder))
                    d_folder_active_windows.insert(newFolder,
                                                   d_folder_active_windows.take(oldFolder));
                if (current_folder == oldFolder)
                    current_folder = newFolder;

                QList<Folder *> subFolders;
                for (QObject *obj : oldFolder->children()) {
                    if (Folder *sub = qobject_cast<Folder *>(obj))
                        subFolders.append(sub);
                }
                for (Folder *sub : subFolders)
                    cloneSubtree(sub, newFolder, newItem);

                oldFolder->setFolderListItem(nullptr);
            };

    cloneSubtree(srcRoot, destRoot, dest);

    for (int i = lv.topLevelItemCount() - 1; i >= 0; --i) {
        QTreeWidgetItem *it = lv.topLevelItem(i);
        if (it && it->type() == FolderListItem::FolderType) {
            Folder *lf = static_cast<FolderListItem *>(it)->folder();
            if (movedOldFolders.contains(lf))
                delete lv.takeTopLevelItem(i);
        }
    }

    delete src;
    delete srcRoot;
}

#ifdef SEARCH_FOR_UPDATES
void ApplicationWindow::searchForUpdates()
{
    const bool silentStartupCheck = autoSearchUpdatesRequest;
    autoSearchUpdatesRequest = false;

    if (silentStartupCheck && (d_ole_embedded || m_batch || d_headless_mode))
        return;

    if (!d_auto_updater)
        d_auto_updater = new AutoUpdater(this, this);

    d_auto_updater->checkForUpdates(silentStartupCheck);
}

void ApplicationWindow::receivedVersionFile(QNetworkReply *netreply)
{
    if (netreply)
        netreply->deleteLater();
    autoSearchUpdatesRequest = false;
}

#endif // defined SEARCH_FOR_UPDATES
void ApplicationWindow::setOleEmbeddedMode(bool embedded)
{
    d_ole_embedded = embedded;
    if (GraphMiniToolbar *mt = GraphMiniToolbar::instance())
        mt->cancelScheduledAndHide();
    if (embedded && d_python_server)
        d_python_server->stop();
    if (actionNewProject)
        actionNewProject->setEnabled(!embedded);
    if (actionOpen)
        actionOpen->setEnabled(!embedded);
    if (recent)
        recent->setEnabled(!embedded);
    if (actionToggleFocusHudMode)
        actionToggleFocusHudMode->setEnabled(true);
    if (actionCommandPalette)
        actionCommandPalette->setEnabled(true);
    if (actionToggleGraphInspector)
        actionToggleGraphInspector->setEnabled(true);
    const bool showRibbon = !d_modern_ribbon || !d_modern_ribbon->isFocusHudMode();
    if (d_ribbon_toolbar)
        d_ribbon_toolbar->setVisible(showRibbon);
    if (d_modern_ribbon && showRibbon)
        d_modern_ribbon->show();
    updateGraphInspectorResponsiveness();
}

void ApplicationWindow::setHeadlessMode(bool headless)
{
    d_headless_mode = headless;
}

bool ApplicationWindow::isNonInteractiveContext(const QObject *context)
{
    if (qApp && qApp->property("torigraph_noninteractive").toBool())
        return true;

    const QObject *cur = context;
    while (cur) {
        if (const ApplicationWindow *app = qobject_cast<const ApplicationWindow *>(cur))
            return app->isNonInteractive();
        if (const QWidget *w = qobject_cast<const QWidget *>(cur)) {
            if (const ApplicationWindow *winApp =
                        qobject_cast<const ApplicationWindow *>(w->window())) {
                return winApp->isNonInteractive();
            }
        }
        cur = cur->parent();
    }

    bool foundAnyApp = false;
    bool allNonInteractive = true;
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (const ApplicationWindow *app = qobject_cast<const ApplicationWindow *>(tlw)) {
            foundAnyApp = true;
            if (!app->isNonInteractive()) {
                allNonInteractive = false;
                break;
            }
        }
    }
    return foundAnyApp && allNonInteractive;
}

void ApplicationWindow::checkForUpdatesManual()
{
#ifdef SEARCH_FOR_UPDATES
    if (isNonInteractiveContext(this))
        return;
    autoSearchUpdatesRequest = false;
    if (!d_auto_updater)
        d_auto_updater = new AutoUpdater(this, this);
    d_auto_updater->checkForUpdates(false);
#endif
}

void ApplicationWindow::openPythonConsole()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_python_server)
        d_python_server = new PythonAutomationServer(this, this);
    const bool wasPipeListening = d_python_server->isLocalPipeListening();
    const bool wasTcpListening = d_python_server->isTcpListening();
    if (!wasPipeListening && !wasTcpListening)
        d_python_server->startLocalPipe();
    PythonConsoleDialog *dlg = PythonConsoleDialog::showDialog(this, d_python_server);
    if (dlg && !wasPipeListening && !wasTcpListening
        && !dlg->property("_tg_stop_pipe_hooked").toBool()) {
        dlg->setProperty("_tg_stop_pipe_hooked", true);
        connect(dlg, &QDialog::finished, this, [this](int) {
            if (d_python_server && !d_python_server->isTcpListening())
                d_python_server->stop();
        });
    }
}

void ApplicationWindow::runPythonScriptDialog()
{
    if (isNonInteractiveContext(this))
        return;
    const QString path = QFileDialog::getOpenFileName(
            this, tr("Open Python Automation Script"), workingDir,
            tr("Python Scripts (*.py);;All Files (*)"));
    if (path.isEmpty())
        return;
    workingDir = QFileInfo(path).absolutePath();
    runPythonScriptFile(path);
}

void ApplicationWindow::runPythonScriptFile(const QString &scriptPath)
{
    if (isNonInteractiveContext(this))
        return;
    if (scriptPath.trimmed().isEmpty())
        return;
    if (!d_python_server)
        d_python_server = new PythonAutomationServer(this, this);
    if (!d_python_server->isLocalPipeListening() && !d_python_server->isTcpListening())
        d_python_server->startLocalPipe();
    PythonConsoleDialog *dlg = PythonConsoleDialog::showDialog(this, d_python_server);
    if (dlg)
        dlg->loadAndRunScriptFile(QFileInfo(scriptPath).absoluteFilePath());
}

void ApplicationWindow::toggleFocusHudMode()
{
    if (d_modern_ribbon)
        d_modern_ribbon->toggleFocusHudMode();
}

void ApplicationWindow::openCommandPalette()
{
    if (isNonInteractiveContext(this))
        return;
    if (d_modern_ribbon)
        d_modern_ribbon->openCommandPalette();
}

void ApplicationWindow::setWorkspaceLayoutTabbed()
{
    if (d_modern_ribbon)
        d_modern_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
    else
        setTabbedWorkspaceView(true);
}

void ApplicationWindow::setWorkspaceLayoutSplit()
{
    if (d_modern_ribbon)
        d_modern_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
}

void ApplicationWindow::setWorkspaceLayoutMdi()
{
    if (d_modern_ribbon)
        d_modern_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
    else
        setTabbedWorkspaceView(false);
}

QString ApplicationWindow::generateUniqueName(const QString &name, bool increment)
{
    int index = 0;
    QList<MyWidget *> windows = windowsList();
    QStringList lst;

    for (int i = 0; i < windows.count(); i++) {
        MyWidget *widget = windows.at(i);
        if (!widget)
            continue;
        lst << widget->name();
        if (widget->name().startsWith(name))
            index++;
    }

    QString newName = name;
    if (increment) // force return of a different name
        newName += QString::number(++index);
    else {
        if (index > 0)
            newName += QString::number(index);
    }

    while (lst.contains(newName))
        newName = name + QString::number(++index);
    return newName;
}

ApplicationWindow::~ApplicationWindow()
{
    // Window lookups answer "none" from here on (see applicationWindowsBeingDestroyed()). The
    // entry stays until a new ApplicationWindow is constructed at the same address: QWidget and
    // QObject still delete children after the members are gone.
    applicationWindowsBeingDestroyed().insert(this);
    if (d_project) {
        d_project->blockSignals(true);
        disconnect(d_project, nullptr, this, nullptr);
        if (d_project->undoStack())
            disconnect(d_project->undoStack(), nullptr, this, nullptr);
    }
    {
        QSet<MultiLayer *> allMls;
        for (MultiLayer *ml : findChildren<MultiLayer *>()) {
            if (ml)
                allMls.insert(ml);
        }
        for (QMdiSubWindow *sw : d_workspace.subWindowList()) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(sw))
                allMls.insert(ml);
        }
        for (MyWidget *hw : hiddenWindows) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(hw))
                allMls.insert(ml);
        }
        for (MyWidget *ow : outWindows) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(ow))
                allMls.insert(ml);
        }
        for (MultiLayer *ml : allMls) {
            if (ml && ml->undoStack())
                disconnect(ml->undoStack(), nullptr, this, nullptr);
        }
    }
    if (WorkbookManager *wb = WorkbookManager::existing(this))
        wb->shutdown(); // D8: stop reacting while the windows are torn down
    explorerWindow.disconnect(this);
    if (d_explorer_container) {
        d_explorer_container->removeEventFilter(this);
        d_explorer_container = nullptr;
        d_explorer_bar_grid = nullptr;
    }
    lv.removeEventFilter(this);
    folders.setParent(nullptr);
    lv.setParent(nullptr);

    if (GraphMiniToolbar *mt = GraphMiniToolbar::instance())
        mt->cancelScheduledAndHide();

    if (d_graph_inspector_dock) {
        d_graph_inspector_dock->setTargetMultiLayer(nullptr);
        d_graph_inspector_dock->hide();
    }

    if (d_python_server) {
        d_python_server->stop();
        delete d_python_server;
        d_python_server = nullptr;
    }

    if (d_auto_updater) {
        delete d_auto_updater;
        d_auto_updater = nullptr;
    }

    if (d_modern_ribbon) {
        d_modern_ribbon->prepareForAppShutdown();
        delete d_modern_ribbon;
        d_modern_ribbon = nullptr;
    }

    d_workspace.blockSignals(true);
    disconnect(&d_workspace, nullptr, this, nullptr);
    folders.blockSignals(true);
    disconnect(&folders, nullptr, this, nullptr);
    lv.blockSignals(true);
    disconnect(&lv, nullptr, this, nullptr);
    lv.clear();

    // Destroy workspace / hidden / detached subwindows (non-data views before Table/Matrix)
    // while d_project is still alive so views detach cleanly without double-freeing
    // undo-retained aspects, then delete d_project to reclaim all project & undo memory.
    {
        QList<QPointer<MyWidget>> nonDataWins;
        QList<QPointer<MyWidget>> dataWins;
        QSet<MyWidget *> seenWins;
        auto collectWin = [&](MyWidget *w) {
            if (!w || seenWins.contains(w))
                return;
            seenWins.insert(w);
            if (qobject_cast<Table *>(w) || qobject_cast<Matrix *>(w))
                dataWins.append(QPointer<MyWidget>(w));
            else
                nonDataWins.append(QPointer<MyWidget>(w));
        };
        for (QMdiSubWindow *sw : d_workspace.subWindowList())
            collectWin(qobject_cast<MyWidget *>(sw));
        for (MyWidget *hw : hiddenWindows)
            collectWin(hw);
        for (MyWidget *ow : outWindows)
            collectWin(ow);
        for (MyWidget *cw : findChildren<MyWidget *>())
            collectWin(cw);

        hiddenWindows.clear();
        outWindows.clear();
        d_folder_active_windows.clear();

        auto destroyWin = [&](const QPointer<MyWidget> &gp) {
            MyWidget *w = gp.data();
            if (!w)
                return;
            w->askOnCloseEvent(false);
            w->hide();
            if (Folder *wf = w->folder())
                wf->removeWindow(w);
            d_workspace.removeSubWindow(w);
            w->setAttribute(Qt::WA_DeleteOnClose, false);
            w->setParent(nullptr);
            delete w;
        };
        for (const QPointer<MyWidget> &gp : nonDataWins)
            destroyWin(gp);
        if (lastCopiedLayer) {
            delete lastCopiedLayer;
            lastCopiedLayer = nullptr;
        }
        for (const QPointer<MyWidget> &gp : dataWins)
            destroyWin(gp);
    }

    if (d_central_container) {
        if (QLayout *lay = d_central_container->layout())
            lay->removeWidget(&d_workspace);
        d_workspace.setParent(nullptr);
        if (centralWidget() == d_central_container)
            takeCentralWidget();
        delete d_central_container;
        d_central_container = nullptr;
    }

    if (d_project) {
        delete d_project;
        d_project = nullptr;
    }

    releaseProjectFileLock();
    untrackOpenedProjectFile();

#ifndef Q_OS_WIN
    QApplication::clipboard()->clear(QClipboard::Clipboard);
#endif
}

QString ApplicationWindow::versionString()
{
    return SciDAVis::versionString();
}

void ApplicationWindow::cascade()
{
    QList<QMdiSubWindow *> windows = d_workspace.subWindowList(QMdiArea::StackingOrder);

    const int xoffset = 13;
    const int yoffset = 20;

    int x = 0;
    int y = 0;

    foreach (QMdiSubWindow *sw, windows) {
        MyWidget *mw = qobject_cast<MyWidget *>(sw);
        if (!mw || hidden(mw) || mw->status() == MyWidget::Hidden)
            continue;
        if (current_folder && mw->folder() != current_folder)
            continue;
        mw->activateWindow();
        mw->showNormal();
        mw->setStatus(MyWidget::Normal);
        updateWindowStatus(mw);

        mw->setGeometry(x, y, mw->width(), mw->height());
        mw->raise();
        x += xoffset;
        y += yoffset;
    }
    modifiedProject();
}

ApplicationWindow *ApplicationWindow::loadScript(const QString &fn, const QStringList &args,
                                                 bool execute)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    ApplicationWindow *app = new ApplicationWindow();
    app->setHeadlessMode(d_headless_mode);
    app->applyUserSettings();
    bool isPython = fn.endsWith(".py", Qt::CaseInsensitive);
    app->m_batch = true;
    if (isPython)
        app->setScriptingLangForBatch("Python");
    else
        app->setScriptingLangForBatch("muParser");
    if (!d_headless_mode && !d_ole_embedded)
        app->showMaximized();
    Note *script_note = app->newNote(fn);
    if (isPython) {
        // copy any arguments into the sys.argv array
        script_note->insert("import sys\n");
        QString prologue = "sys.argv=['" + fn + "'";
        for (auto &a : args)
            prologue += ",'" + a + "'";
        prologue += "]\n";
        script_note->insert(prologue);
    }
    script_note->importASCII(fn);
    QApplication::restoreOverrideCursor();
    if (execute) {
        // we need to disable the redirect of stdio, as this is batch processing
        Script *script = nullptr;
        if (auto scriptEdit = script_note->findChild<ScriptEdit *>())
            if ((script = scriptEdit->findChild<Script *>()))
                script->batchMode = true;
        if (!script_note->executeAll())
            exit(1);
        if (script)
            script->batchMode = false;
    }
    app->m_batch = false;
    return app;
}

bool ApplicationWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event && watched == d_explorer_container && event->type() == QEvent::Show) {
        if (!d_suppress_explorer_user_track && !d_headless_mode && this->isVisible()) {
            const bool isSplit = d_modern_ribbon
                    && (d_modern_ribbon->workspaceLayoutMode()
                        == ModernRibbonBar::LayoutSplitTableGraph);
            if (isSplit && width() < 1480) {
                d_explorer_user_shown_in_narrow_split = true;
                d_explorer_auto_collapsed_for_split = false;
            }
        }
        if (!explorerWindow.isFloating()
            && (dockWidgetArea(&explorerWindow) == Qt::LeftDockWidgetArea
                || dockWidgetArea(&explorerWindow) == Qt::RightDockWidgetArea)
            && explorerWindow.width() > 300) {
            resizeDocks({ &explorerWindow }, { 255 }, Qt::Horizontal);
        }
    }
    if (event && (watched == d_explorer_container || watched == &lv)) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
            updateExplorerAdaptiveLayout(false);
    }
    if (event && !d_restoring_window_geometry && !d_ole_embedded) {
        const QEvent::Type et = event->type();
        if (et == QEvent::Move || et == QEvent::Resize || et == QEvent::WindowStateChange
            || et == QEvent::Close) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(watched)) {
                captureLastGraphGeometry(ml, et == QEvent::Close);
                if (et == QEvent::Close)
                    captureLastGraphSettingsForExit(ml);
            } else if (Table *tbl = qobject_cast<Table *>(watched)) {
                captureLastTableGeometry(tbl, et == QEvent::Close);
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void ApplicationWindow::updateExplorerFolderPaneVisibility()
{
    Folder *root = projectFolder();
    const bool hasSubfolders = root && !root->folders().isEmpty();
    if (folders.isHidden() == hasSubfolders)
        folders.setVisible(hasSubfolders);
}

void ApplicationWindow::updateExplorerAdaptiveLayout(bool allowAutoCollapse)
{
    updateExplorerFolderPaneVisibility();

    if (allowAutoCollapse) {
        const bool isSplitLayout = d_modern_ribbon
                && (d_modern_ribbon->workspaceLayoutMode()
                    == ModernRibbonBar::LayoutSplitTableGraph);
        if (isSplitLayout && width() < 1480 && !d_explorer_user_shown_in_narrow_split
            && !d_headless_mode && !d_ole_embedded && !explorerWindow.isHidden()) {
            d_suppress_explorer_user_track = true;
            explorerWindow.hide();
            d_suppress_explorer_user_track = false;
            d_explorer_auto_collapsed_for_split = true;
        } else if ((!isSplitLayout || width() >= 1480) && d_explorer_auto_collapsed_for_split
                   && explorerWindow.isHidden()) {
            d_suppress_explorer_user_track = true;
            explorerWindow.show();
            d_suppress_explorer_user_track = false;
            d_explorer_auto_collapsed_for_split = false;
        }
    }

    if (!d_explorer_container || !d_explorer_bar_grid)
        return;

    const Qt::DockWidgetArea area = dockWidgetArea(&explorerWindow);
    const int contW = d_explorer_container->width();
    const bool sideDocked = !explorerWindow.isFloating()
            && (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea);
    const bool verticalMode = sideDocked || (contW > 0 && contW < 420);

    const QVariant prevModeVar = d_explorer_container->property("_verticalBarMode");
    if (!prevModeVar.isValid() || prevModeVar.toBool() != verticalMode) {
        d_explorer_container->setProperty("_verticalBarMode", verticalMode);

        while (QLayoutItem *it = d_explorer_bar_grid->takeAt(0))
            delete it;
        for (int c = 0; c <= 6; ++c)
            d_explorer_bar_grid->setColumnStretch(c, 0);

        if (verticalMode) {
            if (d_explorer_search) {
                d_explorer_search->setMinimumWidth(90);
                d_explorer_bar_grid->addWidget(d_explorer_search, 0, 0, 1, 2);
            }
            if (d_btn_explorer_new_folder) {
                d_btn_explorer_new_folder->setFixedSize(26, 26);
                d_explorer_bar_grid->addWidget(d_btn_explorer_new_folder, 0, 2);
            }
            if (d_btn_explorer_new_table) {
                d_btn_explorer_new_table->setFixedSize(26, 26);
                d_explorer_bar_grid->addWidget(d_btn_explorer_new_table, 0, 3);
            }
            if (d_btn_explorer_new_graph) {
                d_btn_explorer_new_graph->setFixedSize(26, 26);
                d_explorer_bar_grid->addWidget(d_btn_explorer_new_graph, 0, 4);
            }
            if (d_explorer_type_filter) {
                d_explorer_type_filter->setMinimumWidth(84);
                d_explorer_type_filter->setSizeAdjustPolicy(
                        QComboBox::AdjustToMinimumContentsLengthWithIcon);
                d_explorer_bar_grid->addWidget(d_explorer_type_filter, 1, 0);
            }
            if (d_explorer_subfolder_check)
                d_explorer_bar_grid->addWidget(d_explorer_subfolder_check, 1, 1, 1, 4,
                                               Qt::AlignLeft | Qt::AlignVCenter);
            if (d_btn_tabbed_view)
                d_btn_tabbed_view->hide();
            d_explorer_bar_grid->setColumnStretch(0, 1);

            if (explorerSplitter) {
                const bool orientChanged = (explorerSplitter->orientation() != Qt::Vertical);
                if (orientChanged)
                    explorerSplitter->setOrientation(Qt::Vertical);
                explorerSplitter->setStretchFactor(0, 22);
                explorerSplitter->setStretchFactor(1, 78);
                if (orientChanged || !explorerSplitter->property("_verticalRatioInit").toBool()) {
                    explorerSplitter->setProperty("_verticalRatioInit", true);
                    const int totalH = qMax(300, explorerSplitter->height());
                    explorerSplitter->setSizes(QList<int>() << qRound(totalH * 0.22)
                                                            << qRound(totalH * 0.78));
                }
            }
        } else {
            if (d_explorer_search) {
                d_explorer_search->setMinimumWidth(140);
                d_explorer_bar_grid->addWidget(d_explorer_search, 0, 0);
            }
            if (d_explorer_type_filter) {
                d_explorer_type_filter->setMinimumWidth(110);
                d_explorer_type_filter->setSizeAdjustPolicy(
                        QComboBox::AdjustToMinimumContentsLengthWithIcon);
                d_explorer_bar_grid->addWidget(d_explorer_type_filter, 0, 1);
            }
            if (d_explorer_subfolder_check)
                d_explorer_bar_grid->addWidget(d_explorer_subfolder_check, 0, 2,
                                               Qt::AlignLeft | Qt::AlignVCenter);
            if (d_btn_explorer_new_folder) {
                d_btn_explorer_new_folder->setFixedSize(26, 26);
                d_explorer_bar_grid->addWidget(d_btn_explorer_new_folder, 0, 3);
            }
            if (d_btn_explorer_new_table) {
                d_btn_explorer_new_table->setFixedSize(26, 26);
                d_explorer_bar_grid->addWidget(d_btn_explorer_new_table, 0, 4);
            }
            if (d_btn_explorer_new_graph) {
                d_btn_explorer_new_graph->setFixedSize(26, 26);
                d_explorer_bar_grid->addWidget(d_btn_explorer_new_graph, 0, 5);
            }
            if (d_btn_tabbed_view) {
                d_explorer_bar_grid->addWidget(d_btn_tabbed_view, 0, 6);
                d_btn_tabbed_view->show();
            }
            d_explorer_bar_grid->setColumnStretch(0, 2);
            d_explorer_bar_grid->setColumnStretch(1, 1);

            if (explorerSplitter && explorerSplitter->orientation() != Qt::Horizontal) {
                explorerSplitter->setOrientation(Qt::Horizontal);
                const int totalW = qMax(500, explorerSplitter->width());
                explorerSplitter->setSizes(QList<int>() << qRound(totalW * 0.30)
                                                        << qRound(totalW * 0.70));
            }
        }
    }

    if (lv.columnCount() >= 5) {
        const bool compactDock = verticalMode && (contW <= 0 || contW < 360);
        // Column 0 already carries the compact duotone type icon; hide the wide Type text column
        // (e.g. "Sheet of <workbook>") in narrow side docks and middle-elide names so sibling
        // workbook sheets remain distinguishable instead of squeezed.
        lv.setColumnHidden(1, compactDock);
        lv.setColumnHidden(2, compactDock);
        lv.setColumnHidden(3, verticalMode);
        lv.setColumnHidden(4, compactDock);
        if (QHeaderView *hdr = lv.header()) {
            if (compactDock) {
                hdr->setStretchLastSection(false);
                hdr->setSectionResizeMode(0, QHeaderView::Stretch);
            } else {
                hdr->setSectionResizeMode(0, QHeaderView::Interactive);
                hdr->setSectionResizeMode(1, QHeaderView::Interactive);
                hdr->setSectionResizeMode(2, QHeaderView::ResizeToContents);
                hdr->setStretchLastSection(true);
            }
        }
        if (!compactDock) {
            lv.resizeColumnToContents(1);
            if (lv.columnWidth(1) > 130)
                lv.setColumnWidth(1, 130);
            if (verticalMode) {
                lv.resizeColumnToContents(2);
                if (lv.columnWidth(0) < 110)
                    lv.setColumnWidth(0, 140);
            }
        }
    }
}

void ApplicationWindow::setStatusBarText(const QString &text)
{
    d_status_info->setText(text);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ApplicationWindow::copyStatusBarText()
{
    QApplication::clipboard()->setText(d_status_info->text());
}

void ApplicationWindow::showStatusBarContextMenu(const QPoint &pos)
{
    if (isNonInteractiveContext(this))
        return;
    QMenu cm(this);
    cm.addAction(actionCopyStatusBarText);
    cm.exec(d_status_info->mapToGlobal(pos));
}

void ApplicationWindow::setActiveWindowFromAction()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action)
        activateSubWindow(qobject_cast<MyWidget *>(window(action->text())));
}

void ApplicationWindow::handleAspectAdded(const AbstractAspect *parent, int index)
{
    AbstractAspect *aspect = parent->child(index);
    ::future::Matrix *matrix = qobject_cast<::future::Matrix *>(aspect);
    if (matrix) {
        if (Matrix *mtxView = static_cast<Matrix *>(matrix->view()))
            initMatrix(mtxView);
        return;
    }
    ::future::Table *table = qobject_cast<::future::Table *>(aspect);
    if (table) {
        if (Table *tblView = static_cast<Table *>(table->view())) {
            initTable(tblView);
            if (WorkbookManager *wm = WorkbookManager::existing(this))
                wm->onTableReadded(tblView);
            if (ModernRibbonBar *rib = modernRibbonBar()) {
                if (FocusTableDrawer *dr = rib->focusTableDrawer()) {
                    if (!dr->isHidden())
                        dr->refreshTableList(false);
                }
            }
        }
        return;
    }
}

void ApplicationWindow::handleAspectAboutToBeRemoved(const AbstractAspect *parent, int index)
{
    struct ScopedAspectRemovalFlag {
        bool &flag;
        explicit ScopedAspectRemovalFlag(bool &f) : flag(f) { flag = true; }
        ~ScopedAspectRemovalFlag() { flag = false; }
    } removalFlag(d_in_aspect_about_to_be_removed);

    AbstractAspect *aspect = parent->child(index);
    ::future::Matrix *matrix = qobject_cast<::future::Matrix *>(aspect);
    if (matrix) {
        closeWindow(static_cast<Matrix *>(matrix->view()));
        return;
    }
    ::future::Table *table = qobject_cast<::future::Table *>(aspect);
    if (table) {
        closeWindow(static_cast<Table *>(table->view()));
        return;
    }
}

void ApplicationWindow::handleAspectDescriptionChanged(const AbstractAspect *aspect)
{
    if (!aspect)
        return;
    if (qobject_cast<const Column *>(aspect) || qobject_cast<const ::future::Table *>(aspect)) {
        if (d_batch_window_updates_depth > 0)
            d_batch_pending_auto_axis_refresh = true;
        else
            refreshAllGraphsAutoAxisTitles();
    }
}

void ApplicationWindow::showHistory()
{
    if (isNonInteractiveContext(this))
        return;
    QUndoStack *targetStack = d_project ? d_project->undoStack() : nullptr;
    if (MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow())) {
        if (ml->undoStack())
            targetStack = ml->undoStack();
    }
    if (!targetStack)
        return;
    QPointer<ApplicationWindow> self(this);
    QPointer<QUndoStack> stackGuard(targetStack);
    QPointer<QDialog> dialog(new QDialog(this));
    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QDialogButtonBox *button_box = new QDialogButtonBox(
            QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, dialog);
    QObject::connect(button_box, &QDialogButtonBox::accepted, dialog.data(), &QDialog::accept);
    QObject::connect(button_box, &QDialogButtonBox::rejected, dialog.data(), &QDialog::reject);
    QObject::connect(targetStack, &QObject::destroyed, dialog.data(), &QDialog::reject);

    const int index = targetStack->index();
    QUndoView *undo_view = new QUndoView(targetStack, dialog);

    layout->addWidget(undo_view);
    layout->addWidget(button_box);

    dialog->setWindowTitle(tr("Undo/Redo History"));
    ScopedUndoReplayGuard replayGuard(self.data());
    const int rc = dialog->exec();
    delete dialog.data();
    if (rc == QDialog::Accepted || !self || !stackGuard)
        return;

    stackGuard->setIndex(index);
}

QStringList ApplicationWindow::tableWindows()
{
    QList<AbstractAspect *> tables = d_project->descendantsThatInherit("future::Table");
    QStringList result;
    foreach (AbstractAspect *aspect, tables)
        result.append(aspect->name());
    return result;
}

// ============================================================================
// Slice 2: OriginPro GraphInspectorDock, Per-MultiLayer Undo/Redo & Column Drop
// ============================================================================
void ApplicationWindow::toggleGraphInspectorDock()
{
    if (!d_graph_inspector_dock)
        return;
    if (d_modern_ribbon && d_modern_ribbon->isFocusHudMode())
        return;

    if (d_graph_inspector_dock->isHidden() || d_graph_inspector_dock->isCollapsedRail()) {
        d_inspector_user_closed = false;
        d_inspector_user_pinned = true;
        d_graph_inspector_dock->setUserPinned(true);
        d_graph_inspector_dock->show();
        d_graph_inspector_dock->setCollapsedRail(false);
    } else {
        d_inspector_user_pinned = false;
        d_graph_inspector_dock->setUserPinned(false);
        d_graph_inspector_dock->setCollapsedRail(true);
    }

    if (d_modern_ribbon
        && d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
        d_modern_ribbon->applySplitWorkspaceGeometry();
    }
    syncGraphPageAndToolbarMenuState();
}

void ApplicationWindow::updateGraphInspectorResponsiveness()
{
    if (!d_graph_inspector_dock)
        return;

    const bool focusMode = (d_modern_ribbon && d_modern_ribbon->isFocusHudMode());
    if (focusMode || d_headless_mode) {
        if (!d_graph_inspector_dock->isHidden())
            d_graph_inspector_dock->hide();
        syncGraphPageAndToolbarMenuState();
        return;
    }

    if (d_inspector_user_closed) {
        if (!d_graph_inspector_dock->isHidden())
            d_graph_inspector_dock->hide();
        syncGraphPageAndToolbarMenuState();
        return;
    }

    if (d_graph_inspector_dock->isHidden())
        d_graph_inspector_dock->show();

    if (d_inspector_user_pinned) {
        if (d_graph_inspector_dock->isCollapsedRail())
            d_graph_inspector_dock->setCollapsedRail(false);
        syncGraphPageAndToolbarMenuState();
        return;
    }

    QMdiSubWindow *activeSw = d_workspace.activeSubWindow();
    const bool isSplitLayout = d_modern_ribbon
            && (d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph);
    const bool activeIsGraph = (qobject_cast<MultiLayer *>(activeSw) != nullptr);
    const bool hasSplitGraphTarget =
            isSplitLayout && (d_graph_inspector_dock->targetMultiLayer() != nullptr);

    // Hysteresis-free width measurement invariant to current dock expansion state
    const int dockCurW = (!d_graph_inspector_dock->isHidden()) ? d_graph_inspector_dock->width() : 0;
    const int totalAvailW = (d_central_container && d_central_container->width() > 100)
            ? (d_central_container->width() + dockCurW)
            : width();

    bool shouldCollapse = false;
    if (!activeIsGraph && !hasSplitGraphTarget) {
        // Keep Quick Start expanded down to 920px so 1366x768 @125% laptop users see the
        // onboarding card; collapse only on very narrow windows (< 920px).
        shouldCollapse = (width() < 920);
    } else if (width() < 1080) {
        shouldCollapse = true;
    } else if (isSplitLayout && totalAvailW < 1680) {
        shouldCollapse = true;
    }

    if (d_graph_inspector_dock->isCollapsedRail() != shouldCollapse) {
        d_graph_inspector_dock->setCollapsedRail(shouldCollapse);
        if (isSplitLayout && d_modern_ribbon)
            d_modern_ribbon->applySplitWorkspaceGeometry();
    }
    syncGraphPageAndToolbarMenuState();
    updateEmptyGraphPanePlaceholder();
}

void ApplicationWindow::updateEmptyGraphPanePlaceholder()
{
    if (d_headless_mode || d_ole_embedded || isNonInteractiveContext(this)) {
        if (d_empty_graph_pane)
            d_empty_graph_pane->hide();
        return;
    }

    QWidget *vp = d_workspace.viewport();
    const bool isSplit = d_modern_ribbon
            && d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
            && !(d_modern_ribbon->isFocusHudMode());
    if (!vp || !isSplit || !vp->isVisible() || vp->width() < 360 || vp->height() < 180) {
        if (d_empty_graph_pane)
            d_empty_graph_pane->hide();
        return;
    }

    const QSet<MyWidget *> hiddenSet(hiddenWindows.begin(), hiddenWindows.end());
    const QSet<MyWidget *> outSet(outWindows.begin(), outWindows.end());
    bool hasVisibleGraph = false;
    const QList<MyWidget *> folderWins =
            current_folder ? current_folder->windowsList() : windowsList();
    for (MyWidget *w : folderWins) {
        if (!w || hiddenSet.contains(w) || outSet.contains(w) || w->status() == MyWidget::Hidden
            || w->isHidden()) {
            continue;
        }
        if (qobject_cast<MultiLayer *>(w) || qobject_cast<Graph3D *>(w)) {
            hasVisibleGraph = true;
            break;
        }
    }

    if (hasVisibleGraph) {
        if (d_empty_graph_pane)
            d_empty_graph_pane->hide();
        return;
    }

    if (!d_empty_graph_pane) {
        QFrame *card = new QFrame(vp);
        card->setObjectName(QStringLiteral("emptyGraphPaneCard"));
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet(QStringLiteral(
                "QFrame#emptyGraphPaneCard { background: rgba(255, 255, 255, 232); "
                "border: 1px dashed #cbd5e1; border-radius: 10px; } "
                "QLabel#emptyGraphTitle { font-size: 14px; font-weight: 600; color: #0f172a; } "
                "QLabel#emptyGraphHint { font-size: 12px; color: #475569; }"));

        QVBoxLayout *lay = new QVBoxLayout(card);
        lay->setContentsMargins(20, 20, 20, 20);
        lay->setSpacing(10);
        lay->addStretch(1);

        QLabel *titleLbl = new QLabel(tr("Graph Canvas"), card);
        titleLbl->setObjectName(QStringLiteral("emptyGraphTitle"));
        titleLbl->setAlignment(Qt::AlignCenter);
        lay->addWidget(titleLbl);

        QLabel *hintLbl = new QLabel(
                tr("No graph in this folder yet. Create a plot from your worksheet or try a "
                   "built-in sample."),
                card);
        hintLbl->setObjectName(QStringLiteral("emptyGraphHint"));
        hintLbl->setWordWrap(true);
        hintLbl->setAlignment(Qt::AlignCenter);
        lay->addWidget(hintLbl);

        QGridLayout *btnGrid = new QGridLayout();
        btnGrid->setSpacing(8);
        QPushButton *newGraphBtn = new QPushButton(tr("New 2D Graph"), card);
        newGraphBtn->setProperty("accentButton", true);
        connect(newGraphBtn, &QPushButton::clicked, this, [this]() { newGraph(); });
        QPushButton *sampleBtn = new QPushButton(tr("Open Sample..."), card);
        connect(sampleBtn, &QPushButton::clicked, this, &ApplicationWindow::showSampleGalleryDialog);
        QPushButton *importBtn = new QPushButton(tr("Import CSV / Excel..."), card);
        connect(importBtn, &QPushButton::clicked, this, [this]() { open(); });
        QPushButton *guideBtn = new QPushButton(tr("Quick Guide (F1)"), card);
        connect(guideBtn, &QPushButton::clicked, this, &ApplicationWindow::showHelp);

        btnGrid->addWidget(newGraphBtn, 0, 0);
        btnGrid->addWidget(sampleBtn, 0, 1);
        btnGrid->addWidget(importBtn, 1, 0);
        btnGrid->addWidget(guideBtn, 1, 1);
        lay->addLayout(btnGrid);
        lay->addStretch(1);
        d_empty_graph_pane = card;
    }

    const double ratio = d_modern_ribbon ? d_modern_ribbon->splitLeftRatio() : 0.40;
    const int leftW = qBound(120, qRound(vp->width() * ratio), qMax(120, vp->width() - 180));
    const int paneW = vp->width() - leftW;
    const int cardW = qMin(360, qMax(220, paneW - 36));
    const int cardH = qMin(250, qMax(180, vp->height() - 36));
    const int x = leftW + qMax(12, (paneW - cardW) / 2);
    const int y = qMax(12, (vp->height() - cardH) / 2);
    d_empty_graph_pane->setGeometry(x, y, cardW, cardH);
    d_empty_graph_pane->raise();
    d_empty_graph_pane->show();
}

static QString cleanMenuTextForTip(const QString &t)
{
    QString out;
    for (int i = 0; i < t.size(); ++i) {
        if (t.at(i) == QLatin1Char('&')) {
            if (i + 1 < t.size() && t.at(i + 1) == QLatin1Char('&'))
                out.append(t.at(++i));
            continue;
        }
        out.append(t.at(i));
    }
    return out.trimmed();
}

void ApplicationWindow::updateUndoRedoActionsForActiveWindow()
{
    if (isBeingDestroyed(this) || !actionUndo || !actionRedo)
        return;

    bool canU = false;
    bool canR = false;
    QString undoText = tr("&Undo");
    QString redoText = tr("&Redo");

    MyWidget *aw = activeWindow();
    const bool is3DOrNote = (qobject_cast<Graph3D *>(aw) != nullptr || qobject_cast<Note *>(aw) != nullptr);
    if (MultiLayer *ml = qobject_cast<MultiLayer *>(aw)) {
        if (QUndoStack *us = ml->undoStack()) {
            canU = us->canUndo();
            canR = us->canRedo();
            if (canU && !us->undoText().trimmed().isEmpty())
                undoText = tr("&Undo %1").arg(us->undoText().trimmed());
            if (canR && !us->redoText().trimmed().isEmpty())
                redoText = tr("&Redo %1").arg(us->redoText().trimmed());
        }
    } else if (!is3DOrNote && d_project && d_project->undoStack()) {
        QUndoStack *us = d_project->undoStack();
        canU = us->canUndo();
        canR = us->canRedo();
        if (canU && !us->undoText().trimmed().isEmpty())
            undoText = tr("&Undo %1").arg(us->undoText().trimmed());
        if (canR && !us->redoText().trimmed().isEmpty())
            redoText = tr("&Redo %1").arg(us->redoText().trimmed());
    }

    // C11: name the graph window whose history Ctrl+Z walks (graphs keep their own history,
    // worksheets share the project history whose command text already begins with "<Table>:").
    if (qobject_cast<MultiLayer *>(aw) && (canU || canR)) {
        const QString target = QStringLiteral("  [%1]").arg(aw->name());
        if (canU)
            undoText += target;
        if (canR)
            redoText += target;
    }
    const QString scopeTip = qobject_cast<MultiLayer *>(aw)
            ? tr("Undo history of this graph window (each graph has its own).")
            : (qobject_cast<Graph3D *>(aw)
                       ? tr("3D graphs do not record Undo/Redo history.")
                       : tr("Undo history of the worksheets (shared by all tables and matrices)."));
    actionUndo->setToolTip(cleanMenuTextForTip(undoText) + QStringLiteral(" (Ctrl+Z)\n") + scopeTip);
    actionRedo->setToolTip(cleanMenuTextForTip(redoText) + QStringLiteral(" (Ctrl+Y)\n") + scopeTip);

    actionUndo->setEnabled(canU);
    actionRedo->setEnabled(canR);
    actionUndo->setText(undoText);
    actionRedo->setText(redoText);
}
