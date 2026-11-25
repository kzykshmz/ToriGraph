/***************************************************************************
    File                 : ApplicationWindow.h
    Project              : SciDAVis
    Description          : SciDAVis's main window
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
#ifndef APPLICATION_H
#define APPLICATION_H

#include <QMainWindow>
#include <functional>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#ifdef SEARCH_FOR_UPDATES
#include <QNetworkAccessManager>
#include <QNetworkReply>
#endif
#include <QFile>
#include <QSplitter>
#include <QDesktopServices>
#include <QBuffer>
#include <QLocale>
#include <QMenu>
#include <QDockWidget>
#include <QHash>
#include <QMdiArea>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <QDateTime>

#ifdef _MSC_VER
#define NOMINMAX
#endif
#include "Folder.h"
#include "Table.h"
#include "ScriptingEnv.h"
#include "Script.h"
#include "SciDAVisObject.h"

class QPixmap;
class QDialog;
class QCloseEvent;
class QDropEvent;
class QTimerEvent;
class QTimer;
class QDragEnterEvent;
class QTranslator;
class QAction;
class QActionGroup;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QTranslator;
class QToolButton;
class QShortcut;
class QMenu;
class QToolBar;
class QStatusBar;
class QSignalMapper;
class QLockFile;
class QFileSystemWatcher;
class IconLoader;

class Matrix;
class Table;
class Graph;
class ScalePicker;
class Graph3D;
class Note;
class MultiLayer;
class FunctionDialog;
class Plot3DDialog;
class MyWidget;
class TableStatistics;
class CurveRangeDialog;
class Project;
class AbstractAspect;
class AxesDialog;
class ModernRibbonBar;
class GraphInspectorDock;
class AutoUpdater;
class PythonAutomationServer;
class QMimeData;

#ifndef TS_PATH
#define TS_PATH (qApp->applicationDirPath() + "/translations")
#endif

/**
 * \brief SciDAVis's main window.
 *
 * This class contains the main part of the user interface as well as the central project management
 * facilities.
 *
 * It manages all MyWidget MDI Windows in a project, knows about their organization in Folder
 * objects and contains the parts of the project explorer not implemented in Folder, FolderListItem
 * or FolderListView.
 *
 * Furthermore, it is responsible for displaying most MDI Windows' context menus and opening all
 * sorts of dialogs.
 *
 * \section future_plans Future Plans
 * Split out the project management part into a new Project class.
 * If MyWidget maintains a reference to its parent Project, it should be possible to have its
 * subclasses display their own context menus and dialogs. This is necessary for implementing new
 * plot types or even completely new MyWidget subclasses in plug-ins. It will also make
 * ApplicationWindow more manageable by removing those parts not directly related to the main
 * window.
 *
 * Project would also take care of basic project file reading/writing (using Qt's XML framework),
 * but delegate most of the work to MyWidget and its subclasses. This is necessary for providing
 * save/restore of classes implemented in plug-ins. Support for foreign formats on the other hand
 * could go into import/export classes (which could also be implemented in plug-ins). Those would
 * interface directly with Project and the MyWidgets it manages. Thus, in addition to supporting
 * QtXML-based save/restore, Project, MyWidget and subclasses will also have to provide generalized
 * save/restore methods/constructors.
 *
 * Maybe split out the project explorer into a new ProjectExplorer class, depending on how much code
 * is left in ApplicationWindow after the above reorganizations. Think about whether a Model/View
 * approach can be used for Project/ProjectExplorer.
 */
class ApplicationWindow : public SciDAVisObject<QMainWindow>, public scripted
{
    Q_OBJECT
public:
    ApplicationWindow();
    ApplicationWindow(const QStringList &l);
    ~ApplicationWindow();

    enum ShowWindowsPolicy { HideAll, ActiveFolder, SubFolders };

    QTranslator *appTranslator, *qtTranslator;
    QDockWidget logWindow;
    QDockWidget explorerWindow;
    // TODO - convert to a statically owned member, but SIP currently requires it to be pointer
    QTextEdit *results { new QTextEdit(&logWindow) };
#ifdef SCRIPTING_CONSOLE
    QDockWidget consoleWindow;
    QTextEdit console;
#endif
    QMdiArea d_workspace;
    QToolBar *file_tools, *graph_tools, *table_tools, *plot_tools, *graph_3D_tools, *edit_tools,
            *matrix_plot_tools;
    FolderListView lv;
    FolderListView folders;
    QToolButton *btnResults;
    QList<MyWidget *> hiddenWindows;
    QList<MyWidget *> outWindows;
    MyWidget *lastModified;

public:
    /*! Generates a new unique name starting with string /param name.
      You can force the output to be a name different from /param name,
      even if 'name' is not used in the project, by setting /param increment = true (the default)
    */
    QString generateUniqueName(const QString &name, bool increment = true);

    bool batchMode() const { return m_batch; } ///< running a python batch script
    static QSettings &getSettings();

    // propagates locale changed notification to all widgets
    // better to have it protected, but it is need to be called
    // from ConfigDialog.
    void localeChanged();

    ModernRibbonBar *modernRibbonBar() const { return d_modern_ribbon; }
    QToolBar *modernRibbonToolBar() const { return d_ribbon_toolbar; }
    QMenu *createPopupMenu() override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateExplorerAdaptiveLayout(bool allowAutoCollapse = true);
    GraphInspectorDock *graphInspectorDock() const { return d_graph_inspector_dock; }
    void toggleGraphInspectorDock();
    void updateGraphInspectorResponsiveness();
    void updateUndoRedoActionsForActiveWindow();
    void invalidateModelessGraphDialogs(MultiLayer *ml);
    void invalidateModelessGraphDialogs(MultiLayer *ml, QDialog *excludeDlg);
    bool handleColumnMimeDrop(const QMimeData *mimeData, MultiLayer *targetMl = nullptr,
                              Graph *targetGraph = nullptr, int targetAxisId = -1,
                              bool replaceExisting = false);
    AutoUpdater *autoUpdater() const { return d_auto_updater; }
    PythonAutomationServer *pythonAutomationServer() const { return d_python_server; }
    Project *project() const { return d_project; }
    MyWidget *activeWindow() const;

    void setHeadlessMode(bool headless);
    bool isHeadlessMode() const { return d_headless_mode; }

    enum GraphFormatScope {
        FormatFonts = 0x01,
        FormatColorsAndSymbols = 0x02,
        FormatFrameAndTicks = 0x04,
        FormatPageDimensions = 0x08,
        FormatAxisScales = 0x10,
        FormatBreaksAndRefLines = 0x20,
        FormatDefaultSafe = FormatFonts | FormatColorsAndSymbols | FormatFrameAndTicks
                | FormatPageDimensions,
        FormatAll = 0x3F
    };

public slots:
    void selectPlotType(int type);
    MultiLayer *addTableSelectionToActiveOrRecentGraph(Table *srcTable = nullptr, int style = -1);
    void openPythonConsole();
    void runPythonScriptDialog();
    void runPythonScriptFile(const QString &scriptPath);
    void checkForUpdatesManual();
    void toggleFocusHudMode();
    void openCommandPalette();
    void setWorkspaceLayoutTabbed();
    void setWorkspaceLayoutSplit();
    void setWorkspaceLayoutMdi();

    //! Copy the status bar text to the clipboard
    void copyStatusBarText();
    //! Show the context menu for the status bar
    void showStatusBarContextMenu(const QPoint &pos);
    //! Set status bar text
    void setStatusBarText(const QString &);
    //! \name Projects and Project Files
    //@{
    void open();
    /// args are any argument to be passed to fn if a script
    ApplicationWindow *open(const QString &fn, const QStringList &args = QStringList());
    //! Returns temporary file ready for reading uncompressed content
    /**
     * Close and delete after you're done with it.
     */
    QFile *openCompressedFile(const QString &fn);
    ApplicationWindow *openProject(const QString &fn);
    ///* load project file \a into this
    ///* @return true if project load successful
    bool loadProject(const QString &fn);
    ApplicationWindow *importOPJ(const QString &filename);
    void showHistory();

    /**
     * \brief Create a new project from a data file.
     *
     * \param fn is read as a data file with the default column separator (as set by the user)
     * and inserted as a table into a new, empty project.
     * This table is then plotted with the Graph::LineSymbols style.
     */
    ApplicationWindow *plotFile(const QString &fn);

    /**
     * \brief Create a new project from a script file.
     *
     * \param fn is read as a Python script file and loaded in the command script window.
     * \param execute specifies if the script should be executed after opening.
     */
    /// args are any argument to be passed to the script
    ApplicationWindow *loadScript(const QString &fn, const QStringList &args = QStringList(),
                                  bool execute = false);

    QList<MyWidget *> windowsList();
    void updateWindowLists(MyWidget *w);
    /*!
      Arranges all the visible project windows in a cascade pattern.
    */
    void cascade();

    //! Ask for a file name and save; returns true only when the project was really written
    bool saveProjectAs();
    bool saveProject();

    //! Set the project status to modifed
    void modifiedProject();
    //! Set the project status to saved (not modified) and drop the undo history
    void savedProject();
    //! Set the project status to saved after the user saved it; keeps the undo history
    void markProjectSaved();
    //! Offer to restore auto-saved copies left behind by a session that ended unexpectedly
    void checkCrashRecovery();
    //! Set the project status to modified and save 'w' as the last modified widget
    void modifiedProject(MyWidget *w);
    //@}

    //! \name Settings
    //@{
    void readSettings(bool restoreWindowLayout = true);
    void saveSettings();
    void applyUserSettings(bool resetScriptingLang = true);
    void setSaveSettings(bool autoSaving, int min);
    void changeAppStyle(const QString &s);
    void changeAppFont(const QFont &f);
    void updateAppFonts();
    void setAppColors(const QColor &wc, const QColor &pc, const QColor &tpc);
    //@}

    //! \name Multilayer Plots
    //@{
    MultiLayer *multilayerPlot(int c, int r, int style);
    MultiLayer *multilayerPlot(Table *w, const QStringList &colList, int style, int startRow = 0,
                               int endRow = -1, const QString &xColName = QString());
    //! used when restoring a plot from a project file
    MultiLayer *multilayerPlot(const QString &caption);
    //! used by the plot wizard
    MultiLayer *multilayerPlot(const QStringList &colList);
    void connectMultilayerPlot(MultiLayer *g);
    void addLayer();
    void deleteLayer();

    //! Creates a new spectrogram graph
    MultiLayer *plotSpectrogram(Matrix *m, Graph::CurveType type);
    void plotGrayScale();
    MultiLayer *plotGrayScale(Matrix *m);
    void plotContour();
    MultiLayer *plotContour(Matrix *m);
    void plotColorMap();
    MultiLayer *plotColorMap(Matrix *m);

    //! Rearrange the layersin order to fit to the size of the plot window
    void autoArrangeLayers();
    void initMultilayerPlot(MultiLayer *g, const QString &name);
    void initBareMultilayerPlot(MultiLayer *g, const QString &name);
    void polishGraph(Graph *g, int style);
    void plot2VerticalLayers();
    void plot2HorizontalLayers();
    void plot4Layers();
    void plotStackedLayers();
    void plotStackedHistograms();
    //@}

    //! \name 3D Data Plots
    //@{
    Graph3D *openMatrixPlot3D(const QString &caption, const QString &matrix_name, double xl,
                              double xr, double yl, double yr, double zl, double zr);
    Graph3D *dataPlot3D(Table *table, const QString &colName);
    Graph3D *dataPlotXYZ(Table *table, const QString &zColName, int type);
    //! X, Y and Z by column index (one column may take several roles).
    Graph3D *dataPlotXYZ(Table *table, int xCol, int yCol, int zCol, int type);
    Graph3D *dataPlot3D(const QString &caption, const QString &formula, double xl, double xr,
                        double yl, double yr, double zl, double zr);
    Graph3D *dataPlotXYZ(const QString &caption, const QString &formula, double xl, double xr,
                         double yl, double yr, double zl, double zr);
    //@}

    //! \name Surface Plots
    //@{
    Graph3D *newPlot3D();
    Graph3D *newPlot3D(const QString &formula, double xl, double xr, double yl, double yr,
                       double zl, double zr);
    Graph3D *newPlot3D(const QString &caption, const QString &formula, double xl, double xr,
                       double yl, double yr, double zl, double zr);
    void connectSurfacePlot(Graph3D *plot);
    void newSurfacePlot();
    void editSurfacePlot();
    void remove3DMatrixPlots(Matrix *m);
    void updateMatrixPlots(MyWidget *);
    void add3DData();
    void change3DData();
    void change3DData(const QString &colName);
    void change3DMatrix();
    void change3DMatrix(const QString &matrix_name);
    void insertNew3DData(const QString &colName);
    void add3DMatrixPlot();
    void insert3DMatrixPlot(const QString &matrix_name);
    void initPlot3D(Graph3D *plot);
    void customPlot3D(Graph3D *plot);
    void setPlot3DOptions();

    void plot3DWireframe();
    void plot3DHiddenLine();
    void plot3DPolygons();
    void plot3DWireSurface();

    void plot3DMatrix(int style);

    void plot3DRibbon();
    void plot3DScatter();
    void plot3DTrajectory();
    void plot3DBars();
    //@}

    //! \name User-defined Functions
    //@{
    bool newFunctionPlot(int type, QStringList &formulas, const QString &var, QList<double> &ranges,
                         int points);

    FunctionDialog *functionDialog();
    void showFunctionDialog();
    void showFunctionDialog(Graph *g, int curve);
    void addFunctionCurve();
    void clearFitFunctionsList();
    void saveFitFunctionsList(const QStringList &l);
    void clearSurfaceFunctionsList();
    void clearLogInfo();
    void clearParamFunctionsList();
    void clearPolarFunctionsList();
    void updateFunctionLists(int type, QStringList &formulas);
    void updateSurfaceFuncList(const QString &s);
    //@}

    //! \name Matrices
    //@{
    //! Creates a new empty matrix
    Matrix *newMatrix(int rows = 32, int columns = 32);
    //! To be used when opening a project file only!
    Matrix *newMatrix(const QString &caption, int r, int c);
    Matrix *matrix(const QString &name);
    Matrix *convertTableToMatrix();
    void initMatrix(Matrix *m);
    void invertMatrix();
    void matrixDeterminant();
    //@}

    //! \name Tables
    //@{
    //! Creates an empty table
    Table *newTable();
    //! Used when importing an ASCII file
    Table *newTable(const QString &fname, const QString &sep, int lines, bool renameCols,
                    bool stripSpaces, bool simplifySpaces, bool convertToNumeric,
                    QLocale numericLocale);
    //! Used when loading a table from a project file
    Table *newTable(const QString &caption, int r, int c);
    Table *newTable(int r, int c, const QString &name = QString(),
                    const QString &legend = QString());
    Table *newTable(const QString &name, const QString &legend, QList<Column *> columns);
    /**
     * \brief Create a Table which is initially hidden; used to return the result of an analysis
     * operation.
     *
     * \param name window name (compare MyWidget::MyWidget)
     * \param label window label (compare MyWidget::MyWidget)
     * \param r number of rows
     * \param c number of columns
     * \param text tab/newline - seperated initial content; may be empty
     */
    Table *newHiddenTable(const QString &name, const QString &label, QList<Column *> columns);
    Table *newHiddenTable(int r, int c, const QString &name, const QString &legend = QString());
    Table *table(const QString &name);
    Table *convertMatrixToTable();
    QList<MyWidget *> *tableList();

    void connectTable(Table *w);
    void newWrksheetPlot(const QString &name, const QString &label, QList<Column *> columns);
    void initTable(Table *w);
    void customTable(Table *w);
    void customizeTables(const QColor &bgColor, const QColor &textColor, const QColor &headerColor,
                         const QFont &textFont, const QFont &headerFont, bool showComments);

    void importASCII();
    void importASCII(const QStringList &files, int import_mode,
                     const QString &local_column_separator, int local_ignored_lines,
                     bool local_rename_columns, bool local_strip_spaces, bool local_simplify_spaces,
                     bool local_convert_to_numeric, QLocale local_numeric_locale);
    void exportAllTables(const QString &sep, bool colNames, bool expSelection);
    //! Writes every table to `dir` (the non-dialog part of exportAllTables). With expSelection,
    //! a table that has a cell selection exports that selection and a table without one exports
    //! the whole table. Returns false when a table could not be written (`failedTable`/`error`
    //! name it) or the user cancelled an overwrite prompt.
    bool exportTablesToDirectory(const QString &dir, const QString &sep, bool colNames,
                                 bool expSelection, QString *failedTable = nullptr,
                                 QString *error = nullptr);
    void exportASCII(const QString &tableName, const QString &sep, bool colNames,
                     bool expSelection);

    TableStatistics *newTableStatistics(Table *base, int type, QList<int>,
                                        const QString &caption = {});
    //@}

    //! \name Graphs
    //@{
    void setPreferences(Graph *g);
    void setGraphDefaultSettings(bool autoscale, bool scaleFonts, bool resizeLayers,
                                 bool antialiasing);
    void setLegendDefaultSettings(int frame, const QFont &font, const QColor &textCol,
                                  const QColor &backgroundCol);
    void setArrowDefaultSettings(const QPen &pen, int headLength, int headAngle, bool fillHead);
    void plotPie();
    void plotVectXYXY();
    void plotVectXYAM();
    void plotDoubleY();
    void plotYOffsetStack();
    void plotViolin();
    void plotPareto();
    void plotQQ();
    void plotTernary();
    void showColorPaletteEditorDialog();
    void applyColorPaletteByAction();
    void applyAspectRatioByAction();
    void setGraphKeepAspectRatio(bool keep);
    //@}

    //! \name Image Analysis
    //@{
    void intensityTable();
    void pixelLineProfile();
    void loadImage();
    void loadImage(const QString &fn);
    Matrix *importImage();
    Matrix *importImage(const QString &fn);
    //@}

    //! \name Export and Print
    //@{
    void exportLayer();
    void exportGraph();
    void exportAllGraphs();
    void exportPDF();
    void print();
    void print(MyWidget *w);
    void printAllPlots();
    //@}

    QStringList columnsList(SciDAVis::PlotDesignation plotType);
    QStringList columnsList();

    void undo();
    void redo();

    //! \name MDI Windows
    //@{
    MyWidget *clone();
    MyWidget *clone(MyWidget *);
    void renameActiveWindow();

    //!  Called when the user presses F2 and an item is selected in lv.
    void renameWindow(QTreeWidgetItem *item, int, const QString &s);

    //!  Checks weather the new window name is valid and modifies the name.
    bool renameWindow(MyWidget *w, const QString &text);

    void maximizeWindow();
    void minimizeWindow();
    //! Changes the geometry of the active MDI window
    void setWindowGeometry(int x, int y, int w, int h);

    void updateWindowStatus(MyWidget *);

    bool hidden(MyWidget *window);
    void closeActiveWindow();
    void closeWindow(MyWidget *window);

    //!  Does all the cleaning work before actually deleting a window!
    void removeWindowFromLists(MyWidget *w);

    void hideWindow(MyWidget *window);
    void hideActiveWindow();
    void activateSubWindow();
    void activateSubWindow(MyWidget *);
    void printWindow();
    //@}

    //! Show about dialog
    static void about();
    //! Return a version string ("SciDAVis x.y.z")
    static QString versionString();
    static int qtVersion() { return QT_VERSION; }
    void windowsMenuAboutToShow();
    void windowsMenuActivated(bool);
    void removeCurves(const QString &name);
    //! Removes the 2D curves that use columns first..first+count-1 of \a t, which are about to
    //! be removed, and records it in the removal's undo step: Undo puts those curves back into
    //! their layers once the columns are back, keeping later edits of the graphs.
    void removeCurvesOfColumnsWithUndo(Table *t, int first, int count);
    QStringList dependingPlots(const QString &caption);
    QStringList depending3DPlots(Matrix *m);
    QStringList multilayerDependencies(MyWidget *w);

    void saveAsTemplate();
    void openTemplate();

    QString windowGeometryInfo(MyWidget *w);
    void restoreWindowGeometry(ApplicationWindow *app, MyWidget *w, const QString s);

    void resizeActiveWindow();
    void resizeWindow();

    //! \name List View in Project Explorer
    //@{
    void setListView(const QString &caption, const QString &view);
    void renameListViewItem(const QString &oldName, const QString &newName);
    void setListViewDate(const QString &caption, const QString &date);
    void setListViewLabel(const QString &caption, const QString &label);
    void filterExplorerWindows();
    void setExplorerSearchText(const QString &text);
    void setExplorerTypeFilter(int index);
    void setTabbedWorkspaceView(bool tabbed);
    void toggleTabbedWorkspaceView();
    bool isTabbedWorkspaceView() const { return d_workspace.viewMode() == QMdiArea::TabbedView; }
    //@}

    void updateColNames(const QString &oldName, const QString &newName);
    void updateTableNames(const QString &oldName, const QString &newName);
    void changeMatrixName(const QString &oldName, const QString &newName);
    void updateCurves(Table *t, const QString &name);

    void showTable(const QString &curve);

    void addColToTable();
    void cutSelection();
    void copySelection();
    void copyMarker();
    void pasteSelection();
    void clearSelection();
    void copyActiveLayer();

    void newProject();

    //! Creates a new empty multilayer plot
    MultiLayer *newGraph(const QString &caption = tr("Graph"));

    //! \name Reading from a Project File
    //@{
    Matrix *openMatrix(ApplicationWindow *app, const QStringList &flist);
    Table *openTable(ApplicationWindow *app, QTextStream &stream);
    TableStatistics *openTableStatistics(const QStringList &flist);
    Graph3D *openSurfacePlot(ApplicationWindow *app, const QStringList &lst);
    Graph *openGraph(ApplicationWindow *app, MultiLayer *plot, const QStringList &list);

    void openRecentProject();
    //@}

    //! \name Initialization
    //@{
    void insertTranslatedStrings();
    void translateActionsStrings();
    void initFonts();
    void createActions();
    void initMainMenu();
    void initPlotMenu();
    void initTableAnalysisMenu();
    void initPlotDataMenu();
    void initToolBars();
    void initPlot3DToolBar();
    void disableActions();
    void customToolBars(MyWidget *w);
    void customMenu(MyWidget *w);
    void windowActivated(QMdiSubWindow *w);
    //@}

    //! \name Table Tools
    //@{
    void correlate();
    void autoCorrelate();
    void convolute();
    void deconvolute();
    void clearTable();
    //@}

    //! \name Plot Tools
    //@{
    void newLegend();
    void addTimeStamp();
    void drawLine();
    void drawArrow();
    void addText();
    void disableAddText();
    void addImage();
    void zoomIn();
    void zoomOut();
    void setAutoScale();
    void showRangeSelectors();
    void showCursor();
    void showScreenReader();
    void pickPointerCursor();
    void pickDataTool(QAction *action);

    void updateLog(const QString &result);
    //@}

    //! \name Fitting
    //@{
    void deleteFitTables();
    void fitLinear();
    void fitSigmoidal();
    void fitGauss();
    void fitLorentz();
    void fitMultiPeak(int profile);
    void fitMultiPeakGauss();
    void fitMultiPeakLorentz();
    void fitMultiPeakGaussAmp();
    void fitMultiPeakPseudoVoigt();
    void fitMultiPeakPearsonVII();
    void showPeakFitDialog(int defaultProfile = 0);
    void showPeakAnalyzerDialog(int defaultProfile = 0);
    void fitAutoPeakGauss();
    void fitAutoPeakLorentz();
    void fitAutoPeakPseudoVoigt();
    void runAutoPeakFit(int profile, int numPeaks = 0, bool autoCount = true,
                        bool plotPeakCurves = true, bool createSummary = true,
                        bool createResiduals = false, const QString &curveTitle = QString(),
                        double prominence = 0.08, double voigtMu = 0.5, double pearsonM = 2.0,
                        int baselineMode = 0, double asymAlpha = 1.0, double tailTau = 1.0,
                        bool plotBaseline = false, bool integratePeaks = false);
    void copyGraphAsOleObject();
    //! Re-register the PowerPoint/Word OLE server for this executable (Help menu)
    void repairOleRegistration();
    //! Choose whether "Copy for PowerPoint" embeds the whole project or only the graph
    void setOleEmbedWholeProject(bool wholeProject);
    //@}

    //! \name Calculus
    //@{
    void differentiate();
    void analysis(const QString &whichFit);
    void analyzeCurve(Graph *g, const QString &whichFit, const QString &curveTitle);
    void showDataSetDialog(const QString &whichFit);
    //@}

    void addErrorBars();
    void defineErrorBars(const QString &name, int type, const QString &percent, int direction);
    void defineErrorBars(const QString &curveName, const QString &errColumnName, int direction);
    void movePoints();
    void removePoints();
    void maskDataPointsTool();
    void unmaskDataPointsTool();
    void maskRegionGadget();
    void unmaskRegionGadget();
    void clearAllMasksOnGraph();

    //! \name OriginPro Interactive Gadgets
    //@{
    void attachVerticalCursorGadget();
    void attachRegionStatsGadget();
    void attachQuickFitGadget();
    void attachQuickPeaksGadget();
    void attachQuickSigmoidalGadget();
    void attachIntegrateGadget();
    void attachInterpolateGadget();
    void attachFFTGadget();
    void attachRiseTimeGadget();
    void attachClusterGadget();
    void clearActiveGadget();
    //@}

    //! \name OriginPro Curve Math, Fitting, Signal Processing, Statistics & Recalculation
    //@{
    void showSimpleCurveMathDialog();
    void showSubtractStraightLineDialog();
    void showAverageMultipleCurvesDialog();
    void showNormalizeCurvesDialog();
    void showCurveIntersectionsDialog();
    void calculatePolygonLoopArea();

    void fitApparentLinear();
    void showYorkLinearFitDialog();
    void showGlobalCurveFitDialog();
    void showImplicitODRFitDialog();
    void showModelComparisonDialog();
    void showSurfaceFit3DDialog();
    void showBatchPeakAnalysisDialog();
    void show2DPeakAnalysisDialog();

    void showSmoothLOWESSDialog();
    void showSmoothWhittakerDialog();
    void showSmoothMedianDialog();
    void showIirDigitalFilterDialog();
    void showStftSpectrogramDialog();
    void showHilbertEnvelopeDialog();
    void showWaveletTransformDialog();
    void showWelchPsdCoherenceDialog();
    void runMatrixFFT2D();
    void runMatrixIFFT2D();

    void showConfidenceEllipseDialog();
    void showGroupComparisonDialog();
    void showROCCurveDialog();
    void showKaplanMeierDialog();
    void showPCABiplotDialog();

    void setRecalcModeAuto();
    void setRecalcModeManual();
    void setRecalcModeDisabled();
    void syncRecalcMenuState();
    void recalculatePendingLinksNow();
    void repeatLinearFitForAllCurves();
    void repeatNormalizationForAllCurves();
    void showManageDialogThemes();

    // Phase 4: OriginPro Page/Layer/Frame UX, Gadgets, Robust/Piecewise/MLR Fit, Signal, Stats & Digitizer
    void showPageSetupDialog();
    void fitPageToLayers();
    void fitLayersToPage();
    void togglePageViewMode();
    void addLinkedTopRightLayer();
    void addZoomInsetLayer();
    void applyFrameBoxInward();
    void applyFrameBoxPlain();
    void applyFrameOpenL();
    void applyFrameZeroCrossed();
    void toggleGraphMiniToolbar(bool enabled);
    void syncGraphPageAndToolbarMenuState();

    void attachDifferentiateGadget();
    void attachIntersectGadget();
    void attachIntegrate2DGadget();

    void showPiecewiseLinearFitDialog();
    void showRobustLinearFitDialog();
    void showCompareDatasetsFitDialog();
    void showMultipleLinearRegressionDialog();

    void showSavGolSmoothDerivativeDialog();
    void showPeakEnvelopeAnalysisDialog();
    void showDecimateResampleDialog();

    void showNormalityTestDialog();
    void showBlandAltmanPlotDialog();
    void showTwoWayANOVAPlotDialog();
    void showGraphDigitizerDialog();

    // Phase 5: OriginPro Format Copy/Paste, Batch Plotting, Style Presets, Delta Cursor, Waterfall & Domain Analysis
    void copyGraphFormat();
    void pasteGraphFormat();
    void pasteGraphFormatWithScope(int scopeFlags);
    void pasteGraphFormatFontsOnly();
    void pasteGraphFormatColorsOnly();
    void pasteGraphFormatAxisScalesOnly();
    void pasteGraphFormatToFolder();
    void mergeOpenGraphsDialog();
    MultiLayer *batchCloneGraphToTable(MultiLayer *srcMl, Table *targetTable);
    void showBatchPlotDialog();
    void applyStyleISIJJournal();
    void applyStylePresentationSlide();
    void showStylePresetManagerDialog();

    void attachDeltaCursorGadget();
    void showLogisticRegressionDialog();
    void showTensileYieldDialog();
    void showArrheniusPlotDialog();
    void showTaucPlotDialog();
    void showFillBetweenCurvesDialog();
    void showStackOffsetsDialog();
    void showWaterfallStackOffsetDialog();
    void resetActiveGraphStackOffsets();
    void showWeibullPlotDialog();
    void showOutlierTestDialog();
    void showNonparametricTestsDialog();
    void showPairedComparisonPlotDialog();
    void showCorrelationMatrixDialog();
    void showClusteringDialog();

    // Round 2: High-DPI Page Copy, Auto-Axis-Title Sync, 2D Profile Gadget & Domain Analysis
    void refreshAllGraphsAutoAxisTitles();
    void copyPageAsHighDpiImage();
    void attachImageProfileGadget();
    void showMatrixFilter2DDialog();
    void showFourierSelfDeconvolveDialog();
    void showEISAnalyzerDialog();
    void showPassingBablokDemingDialog();
    void showSampleGalleryDialog();
    void showKissingerDialog();
    void showWorkHardeningDialog();
    void showXRDAnalyzerDialog();
    void showVaspPlotterDialog();
    void exportActiveGraphToPython();
    void toggleLanguage();
    void plotStackedBars();
    void plotPolarData();
    void showApplyStylePresetToMultipleGraphsDialog();
    void applyTemplateToActiveTable();
    void reimportActiveTable();
    void quickLinearFitSelection();
    void applyStylePresetById(const QString &presetId);
    //@}

    //! \name Event Handlers
    //@{
    void closeEvent(QCloseEvent *);
    void timerEvent(QTimerEvent *e);
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    void customEvent(QEvent *e);
    //@}

    //! \name Dialogs
    //@{
    void showFindDialogue();
    //! Show plot style dialog for the active MultiLayer / activeGraph / specified curve or the activeGraph options dialog if no curve is specified (curveKey = -1).
    void showPlotDialog(int curveKey = -1);
    QDialog *showScaleDialog();
    QDialog *showPlot3dDialog();
    AxesDialog *showScalePageFromAxisDialog(int axisPos);
    AxesDialog *showAxisPageFromAxisDialog(int axisPos);
    void showAxisDialog();
    void showGridDialog();
    void showGeneralPlotDialog();
    void showResults(bool ok);
    void showResults(const QString &s, bool ok = true);
    void showTextDialog();
    void showLineDialog();
    void showTitleDialog();
    void showExportASCIIDialog();
    void showCurvesDialog();
    void showCurveRangeDialog();
    CurveRangeDialog *showCurveRangeDialog(Graph *g, int curve);
    void showPlotAssociations(int curve);

    void showXAxisTitleDialog();
    void showYAxisTitleDialog();
    void showRightAxisTitleDialog();
    void showTopAxisTitleDialog();

    void showGraphContextMenu();
    void showLayerButtonContextMenu();
    void showWindowContextMenu();
    void showWindowTitleBarMenu();
    QMenu *showCurveContextMenuImpl(int);
    void showCurveContextMenu(int curveKey)
    {
        auto m = showCurveContextMenuImpl(curveKey);
        if (m)
            m->exec(QCursor::pos());
    }
    void showCurvePlotDialog();
    void showCurveWorksheet();
    void showCurveWorksheet(Graph *g, int curveIndex);
    /// @return the menu item, which is owned by this. May be null
    QMenu *showWindowPopupMenuImpl(QTreeWidgetItem *it);
    void showWindowPopupMenu(const QPoint &p);

    //! Connected to the context menu signal from lv; it's called when there are several items selected in the list
    QMenu *showListViewSelectionMenuImpl();
    void showListViewSelectionMenu(const QPoint &p) { showListViewSelectionMenuImpl()->exec(p); }

    //! Connected to the context menu signal from lv; it's called when there are no items selected in the list
    QMenu *showListViewPopupMenuImpl();
    void showListViewPopupMenu(const QPoint &p) { showListViewPopupMenuImpl()->exec(p); }

    void showMoreWindows();
    QMenu *showMarkerPopupMenuImpl();
    void showMarkerPopupMenu()
    {
        auto m = showMarkerPopupMenuImpl();
        if (m)
            m->exec(QCursor::pos());
    }
    void showPlotWizard();
    void showFitPolynomDialog();
    void showIntegrationDialog();
    void showInterpolationDialog();
    void showExpGrowthDialog();
    void showExpDecayDialog();
    void showExpDecayDialog(int type);
    void showTwoExpDecayDialog();
    void showExpDecay3Dialog();
    void showRowStatistics();
    void showColStatistics();
    void showFitDialog();
    void showImageDialog();
    void showPlotGeometryDialog();
    void showLayerDialog();
    void showPreferencesDialog();
    void showSmoothSavGolDialog();
    void showSmoothFFTDialog();
    void showSmoothAverageDialog();
    void showSmoothDialog(int m);
    void showFilterDialog(int filter);
    void lowPassFilterDialog();
    void highPassFilterDialog();
    void bandPassFilterDialog();
    void bandBlockFilterDialog();
    void showFFTDialog();
    //@}

    void translateCurveHor();
    void translateCurveVert();

    //! Removes the curve identified by a key stored in the data() of actionRemoveCurve.
    void removeCurve();
    void hideCurve();
    void hideOtherCurves();
    void showAllCurves();
    void setCurveFullRange();

    void updateConfirmOptions(bool askTables, bool askMatrixes, bool askPlots2D, bool askPlots3D,
                              bool askNotes);

    //! \name Plot3D Tools
    //@{
    void toggle3DAnimation(bool on = true);
    //! Turns perspective mode on or off
    void togglePerspective(bool on = true);
    //! Resets rotation of 3D plots to default values
    void resetRotation();
    //! Finds best layout for the 3D plot
    void fitFrameToLayer();
    void setFramed3DPlot();
    void setBoxed3DPlot();
    void removeAxes3DPlot();
    void removeGrid3DPlot();
    void setHiddenLineGrid3DPlot();
    void setLineGrid3DPlot();
    void setPoints3DPlot();
    void setCrosses3DPlot();
    void setCones3DPlot();
    void setBars3DPlot();
    void setFilledMesh3DPlot();
    void setEmptyFloor3DPlot();
    void setFloorData3DPlot();
    void setFloorIso3DPlot();
    void setFloorGrid3DPlot(bool on);
    void setCeilGrid3DPlot(bool on);
    void setRightGrid3DPlot(bool on);
    void setLeftGrid3DPlot(bool on);
    void setFrontGrid3DPlot(bool on);
    void setBackGrid3DPlot(bool on);
    void pickPlotStyle(QAction *action);
    void pickCoordSystem(QAction *action);
    void pickFloorStyle(QAction *action);
    void custom3DActions(MyWidget *w);
    void custom3DGrids(int grids);
    //@}

    void updateRecentProjectsList();

#ifdef SEARCH_FOR_UPDATES
    //!  connected to the finished(QNetworkReply*) signal of the http object
    void receivedVersionFile(QNetworkReply *);
    //!  called when the user presses the actionCheckUpdates
    void searchForUpdates();
#endif

    //! Open SciDAVis homepage in external browser
    void showHomePage();
    //! Open forums page at SF.net in external browser
    void showForums();
    //! Open bug tracking system at SF.net in external browser
    void showBugTracker();
#ifdef DOWNLOAD_LINKS
    //! Show download page in external browser
    void downloadManual();
#endif

    void parseCommandLineArguments(const QStringList &args);
    void createLanguagesList();
    void switchToLanguage(int param);
    void switchToLanguage(const QString &locale);
    void switchToJapanese();
    void switchToEnglish();

    bool alreadyUsedName(const QString &label);
    bool projectHas2DPlots();
    bool projectHas3DPlots();
    bool projectHasMatrices();

    //! Returns a pointer to the window named "name"
    MyWidget *window(const QString &name);

    //! Returns a list with the names of all the matrices in the project
    QStringList matrixNames();

    //! \name Notes
    //@{
    //! Creates a new empty note window
    Note *newNote(const QString &caption = QString());
    Note *openNote(ApplicationWindow *app, const QStringList &flist);
    void initNote(Note *m, const QString &caption);
    void saveNoteAs();
    //@}

    //! \name Folders
    //@{
    //! Returns a to the current folder in the project
    Folder *currentFolder() { return current_folder; };
    //! Adds a new folder to the project
    void addFolder();
    //! Deletes the current folder
    void deleteFolder();

    //! Ask confirmation from user, deletes the folder f if user confirms and returns true, otherwise returns false;
    bool deleteFolder(Folder *f);

    //! Deletes the currently selected items from the list view #lv.
    void deleteSelectedItems();

    //! Sets all items in the folders list view to be deactivated
    void deactivateFolders();

    //! Changes the current folder
    bool changeFolder(Folder *newFolder, bool force = false);

    //! Changes the current folder when the user changes the current item in the QListView "folders"
    void folderItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    //! Changes the current folder when the user double-clicks on a folder item in the QListView "lv"
    void folderItemDoubleClicked(QTreeWidgetItem *it, int column);

    //!  creates and opens the context menu of a folder list view item
    /**
     * \param it list view item
     * \param p mouse global position
     * \param fromFolders: true means that the user clicked right mouse buttom on an item from
     *QListView "folders" false means that the user clicked right mouse buttom on an item from
     *QListView "lv"
     */
    QMenu *showFolderPopupMenuImpl(QTreeWidgetItem *, bool fromFolders);
    //!  connected to the SIGNAL contextMenuRequested from the list views
    void showFolderPopupMenu(const QPoint &p, bool fromFolders = true);

    //!  starts renaming the selected folder by creating a built-in text editor
    void startRenameFolder();

    //  //!  starts renaming the selected folder by creating a built-in text editor
    void startRenameFolder(QTreeWidgetItem *item, int);

    //!  checks whether the new folder name is valid and modifies the name
    void renameFolder(QTreeWidgetItem *it, int col, const QString &);

    //!  forces showing all windows in the current folder and subfolders, depending on the user's viewing policy
    void showAllFolderWindows();

    //!  forces hidding all windows in the current folder and subfolders, depending on the user's viewing policy
    void hideAllFolderWindows();

    //!  hides all windows in folder f
    void hideFolderWindows(Folder *f);

    //!  pops up folder information
    void folderProperties();

    //!  pops up information about the selected window item
    void windowProperties();

    //!  pops up information about the current project
    void projectProperties();

    //! Pops up a file dialog and invokes appendProject(const QString&) on the result.
    void appendProject();
    //! Open the specified project file and add it as a subfolder to the current folder.
    void appendProject(const QString &file_name);
    void saveAsProject();
    void saveFolderAsProject(Folder *f);
    //! Write a folder to the uncompressed project file fn; false if nothing was saved
    bool saveFolder(Folder *folder, const QString &fn);
    //! Write a folder to projectPath (compressed if it ends in ".gz"); false on failure
    bool writeProjectFile(Folder *folder, const QString &projectPath);
    //! Turn auto-save recovery copies on (regular interactive sessions only; off by default so
    //! that automation, OLE servers and tests never write or offer recovery copies)
    static void setCrashRecoveryEnabled(bool on);
    //! Folder holding the auto-save recovery copies
    static QString recoveryDirPath();
    //! Write an auto-save recovery copy of a modified project (never touches the project file)
    void writeRecoveryFile();
    //! Remove this window's recovery copy (project saved, loaded or closed regularly)
    void removeRecoveryFile();
    void rawSaveFolder(Folder *folder, QIODevice *device);
    QByteArray saveProjectToByteArray();
    //! Stand-alone project text holding only the given windows (in this order), without the log
    QByteArray saveWindowsToByteArray(const QList<MyWidget *> &windows);
    void setOleEmbeddedMode(bool embedded);
    bool isOleEmbeddedMode() const { return d_ole_embedded; }
    bool isNonInteractive() const { return d_headless_mode || m_batch; }
    static bool isNonInteractiveContext(const QObject *context = nullptr);
    //! False in headless (--headless, the SDK's launch()) and batch sessions: they must not
    //! change the user's stored settings (last graph settings, recent projects, language...).
    bool persistsUserSettings() const;
    void setOleEmbeddedCallbacks(std::function<void()> onUpdate, std::function<void()> onClose)
    {
        d_ole_on_update = std::move(onUpdate);
        d_ole_on_close = std::move(onClose);
    }

    //!  adds a folder list item to the list view "lv"
    void addFolderListViewItem(Folder *f);

    //!  adds a widget list item to the list view "lv"
    void addListViewItem(MyWidget *w);

    //!  hides or shows windows in the current folder and changes the view windows policy
    void setShowWindowsPolicy(bool);

    //!  returns a pointer to the root project folder
    Folder *projectFolder();

    //!  used by the findDialog
    void find(const QString &s, bool windowNames, bool labels, bool folderNames, bool caseSensitive,
              bool partialMatch, bool subfolders);

    //!  initializes the list of items dragged by the user
    void dragFolderItems(QList<QTreeWidgetItem *> items) { draggedItems = items; };

    //!  Drop the objects in the list draggedItems to the folder of the destination item
    void dropFolderItems(QTreeWidgetItem *dest);

    //!  moves a folder item to another
    /**
     * \param src source folder item
     * \param dest destination folder item
     */
    void moveFolder(FolderListItem *src, FolderListItem *dest);
    //@}

    //! \name Scripting
    //@{
    //! notify the user that an error occured in the scripting system
    void scriptError(const QString &message, const QString &scriptName, int lineNumber);
    //! execute all notes marked auto-exec
    void executeNotes();
    //! show scripting language selection dialog
    void showScriptingLangDialog();
    //! create a new environment for the current scripting language
    void restartScriptingEnv();
    //! print to scripting console (if available) or to stdout
    void scriptPrint(const QString &text);
    //! switches to the given scripting language; if this is the same as the current one and force is true, restart it
    bool setScriptingLang(const QString &lang, bool force = false, bool batch = false);
    void setScriptingLangForBatch(const QString &lang) { setScriptingLang(lang, false, true); }
    //@}

signals:
    void modified();

private slots:
    void showHelp();
    void chooseHelpFolder();

    // TODO: a lot of this stuff should be private
public:
    //! Tests only: keep recovery copies in \a path instead of the per-user folder (empty = the
    //! regular folder). Lets several test processes run side by side without sharing copies.
    static void setRecoveryDirPathOverride(const QString &path);
    //! Answer to the "Restore unsaved work?" question
    enum RecoveryAnswer { RecoveryRestore, RecoveryDiscard, RecoveryDecideLater };
    //! Tests only: answer the "Restore unsaved work?" question without showing it (receives the
    //! path of the copy's metadata file). An empty function (the default) asks the user.
    static void setCrashRecoveryPromptOverride(std::function<RecoveryAnswer(const QString &)> answer);
    //! Last selected filter in export image dialog
    QString d_image_export_filter;
    bool d_keep_plot_aspect;
    int d_export_vector_size;
    bool d_export_transparency;
    int d_export_quality;
    int d_export_resolution;
    bool d_export_color;
    //! Default paper orientation for image exports.
    int d_export_orientation;
    //! Locale used to specify the decimal separators in imported ASCII files
    QLocale d_ASCII_import_locale;
    //! Last selected filter in import ASCII dialog
    QString d_ASCII_file_filter;
    bool d_convert_to_numeric;
    //! Specifies if only the Tables/Matrices in the current folder should be displayed in the Add/remove curve dialog.
    bool d_show_current_folder;
    bool d_scale_plots_on_print, d_print_cropmarks;
    bool d_show_table_comments;
    bool d_extended_plot_dialog;
    bool d_extended_import_ASCII_dialog;
    bool d_extended_export_dialog;
    bool d_extended_open_dialog;
    bool generateUniformFitPoints;
    bool generatePeakCurves;
    QColor peakCurvesColor;
    //! User defined size for the Add/Remove curves dialog
    QSize d_add_curves_dialog_size;

    //! Scale the errors output in fit operations with reduced chi^2
    bool fit_scale_errors;

    //! Number of points in a generated fit curve
    int fitPoints;

    //! Calculate only 2 points in a generated linear fit function curve
    bool d_2_linear_fit_points;

    bool pasteFitResultsToPlot;

    //! Write fit output information to Result Log
    bool writeFitResultsToLog;

    //! precision used for the output of the fit operations
    int fit_output_precision;

    //! default precision to be used for all other operations than fitting
    int d_decimal_digits;

    char d_default_numeric_format;

    //! pointer to the current folder in the project
    Folder *current_folder;
    //! Describes which windows are shown when the folder becomes the current folder
    ShowWindowsPolicy show_windows_policy;
    enum { MaxRecentProjects = 10 };
    //! File version code used when opening project files (= maj * 100 + min * 10 + patch)
    int d_file_version;

    QColor workspaceColor, panelsColor, panelsTextColor;
    QString appStyle, workingDir;

    //! Path to the folder where the last template file was opened/saved
    QString templatesDir;
    bool smooth3DMesh, autoScaleFonts, autoResizeLayers;
#ifdef SEARCH_FOR_UPDATES
    bool autoSearchUpdates;
#endif
    bool confirmCloseTable, confirmCloseMatrix, confirmClosePlot2D, confirmClosePlot3D;
    bool confirmCloseFolder, confirmCloseNotes;
    bool canvasFrameOn, titleOn, autoSave, drawBackbones, allAxesOn, autoscale2DPlots,
            antialiasing2DPlots;
    int majTicksStyle, minTicksStyle, legendFrameStyle, autoSaveTime, axesLineWidth,
            canvasFrameWidth;
    QColor legendBackground, legendTextColor, defaultArrowColor;
    int defaultArrowLineWidth, defaultArrowHeadLength, defaultArrowHeadAngle;
    bool defaultArrowHeadFill;
    Qt::PenStyle defaultArrowLineStyle;
    int majTicksLength, minTicksLength, defaultPlotMargin;
    int defaultCurveStyle, defaultCurveLineWidth, defaultSymbolSize;
    int undoLimit = 50;
    QFont appFont, plot3DTitleFont, plot3DNumbersFont, plot3DAxesFont;
    QFont tableTextFont, tableHeaderFont, plotAxesFont, plotLegendFont, plotNumbersFont,
            plotTitleFont;
    QColor tableBkgdColor, tableTextColor, tableHeaderColor;
    QString projectname, columnSeparator, appLanguage;
    QString configFilePath, logInfo, fitPluginsPath, asciiDirPath, imagesDirPath;
    //! Base name of this window's auto-save recovery copy (empty: none written yet)
    QString d_recovery_id;
    int logID, asciiID, closeID, exportID, printAllID, ignoredLines, savingTimerId,
            plot3DResolution;
    bool renameColumns, copiedLayer, strip_spaces, simplify_spaces;
    QStringList recentProjects;
    QStringList tableWindows();
    bool saved, showPlot3DProjection, showPlot3DLegend, orthogonal3DPlots = false, autoscale3DPlots;
    QStringList plot3DColors, locales;
    QStringList functions; // user-defined functions;
    QStringList xFunctions, yFunctions, rFunctions,
            thetaFunctions; // user functions for parametric and polar plots
    QStringList fitFunctions; // user-defined fit functions;
    QStringList surfaceFunc; // user-defined surface functions;

    //! List of tables and matrices renamed in order to avoid conflicts when appending a project to a folder
    QStringList renamedTables;
    Graph::MarkerType copiedMarkerType;

    //! \name variables used when user copy/paste markers
    //@{
    QString auxMrkText;
    QFont auxMrkFont;
    QColor auxMrkColor, auxMrkBkgColor;
    QPoint auxMrkStart, auxMrkEnd;
    Qt::PenStyle auxMrkStyle;
    QString auxMrkFileName;
    int auxMrkBkg, auxMrkWidth;
    //@}

    bool startArrowOn, endArrowOn, fillArrowHead;
    int arrowHeadLength, arrowHeadAngle, specialPlotMenuID, statMenuID, panelMenuID, plot3dID;
    int plotMenuID, newMenuID, recentMenuID, setAsMenuID, fillMenuID;
    int translateMenuID, smoothMenuID, filterMenuID, fitExpMenuID, multiPeakMenuID;

#ifdef SEARCH_FOR_UPDATES
    //! Equals true if an automatical search for updates was performed on start-up otherwise is set to false;
    bool autoSearchUpdatesRequest;
#endif

    //! The scripting language to use for new projects.
    QString defaultScriptingLang;
    /// location of translation resources
    QString qmPath;

    QMenu *gadgetsMenu = nullptr;
    QMenu *dataMaskMenu = nullptr;
    QMenu *materialsAnalysisMenu = nullptr;
    QMenu *curveMathMenu = nullptr;
    QMenu *advFitMenu = nullptr;
    QMenu *peaksBaselineMenu = nullptr;
    QMenu *advSignalMenu = nullptr;
    QMenu *graphStatsMenu = nullptr;
    QMenu *recalcMenu = nullptr;
    QMenu *framePresetsMenu = nullptr;
    QMenu *stylePresetsMenu = nullptr;
    QMenu *pasteFormatSpecialMenu = nullptr;

protected:
    //! Show a context menu for the widget
    QMenu *showWindowMenuImpl(MyWidget *widget);
    void showWindowMenu(MyWidget *widget) { showWindowMenuImpl(widget)->exec(QCursor::pos()); }

private:
    bool m_batch = false;

    //! Create a menu for toggeling the toolbars
    QMenu *createToolbarsMenu();

    //! Check whether a table is valid for a 3D plot and display an appropriate error if not
    bool validFor3DPlot(Table *table);
    //! Check whether a table is valid for a 2D plot and display an appropriate error if not
    bool validFor2DPlot(Table *table, int type);

    //! Workaround for the new colors introduced in rev 447
    unsigned int convertOldToNewColorIndex(unsigned int cindex);

    //! Stores the pointers to the dragged items from the FolderListViews objects
    QList<QTreeWidgetItem *> draggedItems;

    QString helpFilePath;

#ifdef SEARCH_FOR_UPDATES
    //! Used when checking for new versions
    QNetworkAccessManager http;
    //! Used when checking for new versions
    QByteArray version_buffer;
#endif

    Graph *lastCopiedLayer;
    QSplitter *explorerSplitter;

    QMenu *windowsMenu, *view, *graph, *file, *format, *calcul, *edit, *dataMenu, *recent,
            *exportPlot, *toolbarsMenu;
    QMenu *d_quick_fit_menu;
    QMenu *help, *type, *plot2D, *plot3D, *specialPlot, *panels, *stat, *decay, *filter;
    QMenu *matrixMenu, *plot3DMenu, *plotDataMenu, *tableMenu;
    QMenu *smooth, *translateMenu, *multiPeakMenu;
    QMenu *scriptingMenu;
    QAction *actionCopyStatusBarText;
    QAction *actionEditCurveRange, *actionCurveFullRange, *actionShowAllCurves, *actionHideCurve,
            *actionHideOtherCurves;
    QAction *actionEditFunction, *actionRemoveCurve, *actionShowCurveWorksheet,
            *actionShowCurvePlotDialog;
    QAction *actionNewProject, *actionNewNote, *actionNewTable, *actionNewFunctionPlot,
            *actionNewSurfacePlot, *actionNewMatrix, *actionNewGraph;
    QAction *actionOpen, *actionLoadImage, *actionSaveProject, *actionSaveProjectAs,
            *actionImportImage;
    QAction *actionLoad, *actionUndo, *actionRedo;
    QAction *actionCopyWindow;
    QAction *actionCutSelection, *actionCopySelection, *actionPasteSelection, *actionClearSelection;
    QAction *locktoolbar;
    QAction *actionShowExplorer, *actionShowLog, *actionAddLayer, *actionShowLayerDialog,
            *actionAutomaticLayout;
    QAction *actionShowHistory;
#ifdef SCRIPTING_CONSOLE
    QAction *actionShowConsole;
#endif

    QAction *actionExportGraph, *actionExportAllGraphs, *actionPrint, *actionPrintAllPlots,
            *actionShowExportASCIIDialog;
    QAction *actionExportPDF;
    QAction *actionCloseAllWindows, *actionClearLogInfo, *actionShowPlotWizard,
            *actionShowConfigureDialog;
    QAction *actionShowCurvesDialog, *actionAddErrorBars, *actionAddFunctionCurve, *actionUnzoom,
            *actionNewLegend, *actionAddImage, *actionAddText;
    QAction *actionPlotL, *actionPlotP, *actionPlotLP, *actionPlotVerticalDropLines,
            *actionPlotSpline;
    QAction *actionPlotVertSteps, *actionPlotHorSteps, *actionPlotVerticalBars;
    QAction *actionPlotHorizontalBars, *actionPlotArea, *actionPlotPie, *actionPlotVectXYAM,
            *actionPlotVectXYXY;
    QAction *actionPlotHistogram, *actionPlotStackedHistograms, *actionPlot2VerticalLayers,
            *actionPlot2HorizontalLayers, *actionPlot4Layers, *actionPlotStackedLayers;
    QAction *actionPlot3DRibbon, *actionPlot3DBars, *actionPlot3DScatter, *actionPlot3DTrajectory;
    QAction *actionShowColStatistics, *actionShowRowStatistics, *actionShowIntDialog;
    QAction *actionDifferentiate, *actionFitLinear, *actionShowFitPolynomDialog;
    QAction *actionShowExpDecayDialog, *actionShowTwoExpDecayDialog, *actionShowExpDecay3Dialog;
    QAction *actionFitExpGrowth, *actionFitSigmoidal, *actionFitGauss, *actionFitLorentz,
            *actionShowFitDialog;
    QAction *actionShowAxisDialog, *actionShowTitleDialog;
    QAction *actionAbout;
    QAction *actionShowHelp;
#ifdef DYNAMIC_MANUAL_PATH
    QAction *actionChooseHelpFolder;
#endif
    QAction *actionRename, *actionCloseWindow, *actionConvertTable;
    QAction *actionAddColToTable, *actionDeleteLayer, *actionInterpolate;
    QAction *actionResizeActiveWindow, *actionHideActiveWindow;
    QAction *actionShowMoreWindows, *actionPixelLineProfile, *actionIntensityTable;
    QAction *actionShowLineDialog, *actionShowImageDialog, *actionShowTextDialog;
    QAction *actionActivateWindow, *actionMinimizeWindow, *actionMaximizeWindow,
            *actionResizeWindow, *actionPrintWindow;
    QAction *actionShowPlotGeometryDialog, *actionEditSurfacePlot, *actionAdd3DData;
    QAction *actionMatrixDeterminant;
    QAction *actionConvertMatrix, *actionInvertMatrix;
    QAction *actionPlot3DWireFrame, *actionPlot3DHiddenLine, *actionPlot3DPolygons,
            *actionPlot3DWireSurface;
    QAction *actionColorMap, *actionContourMap, *actionGrayMap;
    QAction *actionDeleteFitTables, *actionShowGridDialog, *actionTimeStamp;
    QAction *actionSmoothSavGol, *actionSmoothFFT, *actionSmoothAverage, *actionFFT;
    QAction *actionLowPassFilter, *actionHighPassFilter, *actionBandPassFilter,
            *actionBandBlockFilter;
    QAction *actionConvolute, *actionDeconvolute, *actionCorrelate, *actionAutoCorrelate;
    QAction *actionTranslateHor, *actionTranslateVert;
    QAction *actionBoxPlot, *actionMultiPeakGauss, *actionMultiPeakLorentz;
    QAction *actionMultiPeakGaussAmp = nullptr, *actionMultiPeakPseudoVoigt = nullptr,
            *actionMultiPeakPearsonVII = nullptr;
    QAction *actionPeakFitDialog = nullptr, *actionPeakAnalyzerDialog = nullptr,
            *actionAutoPeakGauss = nullptr, *actionAutoPeakLorentz = nullptr,
            *actionAutoPeakPseudoVoigt = nullptr;
    QAction *actionCopyGraphOle = nullptr;
    QAction *actionOleEmbedWholeProject = nullptr, *actionRepairOleRegistration = nullptr;
    QMenu *languageMenu = nullptr, *colorPaletteMenu = nullptr, *aspectRatioMenu = nullptr;
    QAction *actionLangEnglish = nullptr, *actionLangJapanese = nullptr;
    QAction *actionPlotDoubleY = nullptr, *actionPlotYOffsetStack = nullptr,
            *actionPlotViolin = nullptr, *actionPlotPareto = nullptr,
            *actionPlotQQ = nullptr, *actionPlotTernary = nullptr;
    QAction *actionColorPaletteEditor = nullptr;
    QAction *actionAspect1_1 = nullptr, *actionAspect4_3 = nullptr,
            *actionAspect16_9 = nullptr, *actionAspect3_2 = nullptr,
            *actionAspectCustom = nullptr, *actionKeepAspectRatio = nullptr;
    QAction *actionTabbedWorkspaceView = nullptr;
    QAction *actionGadgetVerticalCursor = nullptr, *actionGadgetRegionStats = nullptr,
            *actionGadgetQuickFit = nullptr, *actionGadgetQuickPeaks = nullptr,
            *actionGadgetQuickSigmoidal = nullptr, *actionGadgetIntegrate = nullptr,
            *actionGadgetInterpolate = nullptr, *actionGadgetFFT = nullptr,
            *actionGadgetRiseTime = nullptr, *actionGadgetCluster = nullptr,
            *actionGadgetClear = nullptr;
    QAction *btnMaskPoints = nullptr, *btnUnmaskPoints = nullptr,
            *actionMaskRegionROI = nullptr, *actionUnmaskRegionROI = nullptr,
            *actionClearAllMasks = nullptr;
    QAction *actionSimpleCurveMath = nullptr, *actionSubtractStraightLine = nullptr,
            *actionAverageMultipleCurves = nullptr, *actionNormalizeCurves = nullptr,
            *actionCurveIntersections = nullptr, *actionPolygonLoopArea = nullptr;
    QAction *actionFitApparentLinear = nullptr, *actionFitYorkLinear = nullptr,
            *actionGlobalCurveFit = nullptr, *actionImplicitODRFit = nullptr,
            *actionModelComparison = nullptr, *actionSurfaceFit3D = nullptr,
            *actionBatchPeakAnalysis = nullptr, *action2DPeakAnalysis = nullptr;
    QAction *actionSmoothLOWESS = nullptr, *actionSmoothWhittaker = nullptr,
            *actionSmoothMedian = nullptr, *actionIirDigitalFilter = nullptr,
            *actionStftSpectrogram = nullptr, *actionHilbertEnvelope = nullptr,
            *actionWaveletTransform = nullptr, *actionWelchPsdCoherence = nullptr,
            *actionMatrixFFT2D = nullptr, *actionMatrixIFFT2D = nullptr;
    QAction *actionConfidenceEllipse = nullptr, *actionGroupComparison = nullptr,
            *actionROCCurve = nullptr, *actionKaplanMeier = nullptr,
            *actionPCABiplot = nullptr;
    QActionGroup *recalcModeGroup = nullptr;
    QAction *actionRecalcAuto = nullptr, *actionRecalcManual = nullptr,
            *actionRecalcDisabled = nullptr, *actionRecalculateNow = nullptr,
            *actionRepeatLinearFitAll = nullptr, *actionRepeatNormalizeAll = nullptr,
            *actionManageDialogThemes = nullptr;
    QAction *actionPageSetup = nullptr, *actionFitPageToLayers = nullptr,
            *actionFitLayersToPage = nullptr, *actionTogglePageView = nullptr,
            *actionAddLinkedLayer = nullptr, *actionAddInsetLayer = nullptr,
            *actionFrameBoxInward = nullptr, *actionFrameBoxPlain = nullptr,
            *actionFrameOpenL = nullptr, *actionFrameZeroCrossed = nullptr,
            *actionToggleMiniToolbar = nullptr;
    QAction *actionGadgetDifferentiate = nullptr, *actionGadgetIntersect = nullptr,
            *actionGadgetIntegrate2D = nullptr;
    QAction *actionPiecewiseLinearFit = nullptr, *actionRobustLinearFit = nullptr,
            *actionCompareDatasetsFit = nullptr, *actionMultipleLinearRegression = nullptr;
    QAction *actionSavGolSmoothDerivative = nullptr, *actionPeakEnvelope = nullptr,
            *actionDecimateResample = nullptr;
    QAction *actionNormalityTest = nullptr, *actionBlandAltman = nullptr,
            *actionTwoWayANOVA = nullptr, *actionGraphDigitizer = nullptr;
    QAction *actionCopyGraphFormat = nullptr, *actionPasteGraphFormat = nullptr,
            *actionPasteFormatFonts = nullptr, *actionPasteFormatColors = nullptr,
            *actionPasteFormatScales = nullptr, *actionPasteFormatFolder = nullptr,
            *actionMergeGraphs = nullptr, *actionBatchPlot = nullptr;
    QAction *actionStyleISIJ = nullptr, *actionStyleSlide = nullptr;
    QAction *actionGadgetDeltaCursor = nullptr, *actionLogisticRegression = nullptr,
            *actionTensileYield = nullptr, *actionArrheniusPlot = nullptr, *actionTaucPlot = nullptr,
            *actionFillBetweenCurves = nullptr, *actionWaterfallStackOffset = nullptr,
            *actionResetStackOffsets = nullptr;
    QAction *actionWeibullPlot = nullptr, *actionOutlierTest = nullptr,
            *actionNonparametricTests = nullptr, *actionPairedComparisonPlot = nullptr,
            *actionCorrelationMatrix = nullptr, *actionClustering = nullptr;
    QAction *actionCopyPageImage = nullptr, *actionGadgetImageProfile = nullptr,
            *actionMatrixFilter2D = nullptr, *actionFourierSelfDeconvolve = nullptr,
            *actionEISAnalyzer = nullptr, *actionPassingBablokDeming = nullptr;
    QAction *actionToggleFocusHudMode = nullptr, *actionCommandPalette = nullptr;
    QActionGroup *workspaceLayoutGroup = nullptr;
    QAction *actionWorkspaceLayoutTabbed = nullptr, *actionWorkspaceLayoutSplit = nullptr,
            *actionWorkspaceLayoutMdi = nullptr;
    QAction *actionOpenPythonConsole = nullptr, *actionRunPythonScript = nullptr;
    QAction *actionSampleGallery = nullptr, *actionKissingerDialog = nullptr,
            *actionWorkHardeningDialog = nullptr, *actionXRDAnalyzerDialog = nullptr,
            *actionVaspPlotterDialog = nullptr, *actionExportPythonScript = nullptr;
    QAction *actionReimportData = nullptr, *actionPlotStackedBars = nullptr,
            *actionPlotPolarData = nullptr, *actionApplyStyleToMultipleGraphs = nullptr,
            *actionApplyTableTemplate = nullptr, *actionQuickLinearFit = nullptr;
    bool d_in_aspect_about_to_be_removed = false;
    QPointer<QDialog> d_active_peak_analyzer_dlg;
    QToolButton *d_btn_style_presets_popup = nullptr;
    QWidget *d_explorer_container = nullptr;
    QGridLayout *d_explorer_bar_grid = nullptr;
    QLineEdit *d_explorer_search = nullptr;
    QComboBox *d_explorer_type_filter = nullptr;
    QCheckBox *d_explorer_subfolder_check = nullptr;
    QToolButton *d_btn_explorer_new_folder = nullptr, *d_btn_explorer_new_table = nullptr,
                *d_btn_explorer_new_graph = nullptr, *d_btn_tabbed_view = nullptr;
    QTranslator *d_builtin_ja_translator = nullptr;
#ifdef SEARCH_FOR_UPDATES
    QAction *actionCheckUpdates;
#endif
    QAction *actionHomePage;
#ifdef DOWNLOAD_LINKS
    QAction *actionDownloadManual;
#endif
    QAction *actionHelpForums;
    QAction *actionHelpBugReports;
    QAction *actionShowPlotDialog, *actionShowScaleDialog, *actionOpenTemplate, *actionSaveTemplate;
    QAction *actionNextWindow;
    QAction *actionPrevWindow;
    QAction *actionScriptingLang, *actionRestartScripting, *actionClearTable, *actionGoToCell;
    QAction *actionNoteExecute, *actionNoteExecuteAll, *actionNoteEvaluate, *actionSaveNote;
    QAction *actionAnimate, *actionPerspective, *actionFitFrame, *actionResetRotation;

    QActionGroup *dataTools;
    QAction *btnCursor, *btnSelect, *btnPicker, *btnRemovePoints, *btnMovePoints;
    QAction *btnZoomIn, *btnZoomOut, *btnPointer, *btnLine, *btnArrow;

    QActionGroup *coord;
    QAction *Box;
    QAction *Frame;
    QAction *None;

    QActionGroup *grids;
    QAction *front;
    QAction *back;
    QAction *right;
    QAction *left;
    QAction *ceil;
    QAction *floor;

    QActionGroup *floorstyle;
    QAction *floordata;
    QAction *flooriso;
    QAction *floornone;

    QActionGroup *plotstyle;
    QAction *wireframe;
    QAction *hiddenline;
    QAction *polygon;
    QAction *filledmesh;
    QAction *pointstyle;
    QAction *barstyle;
    QAction *conestyle, *crossHairStyle;

    //! Manages connection between plot actions and Graph::CurveType values (not used by all plot actions).
    QSignalMapper *d_plot_mapper;

    QLabel *d_status_info;

    Project *d_project;
    QWidget *d_central_container = nullptr;
    QToolBar *d_ribbon_toolbar = nullptr;
    ModernRibbonBar *d_modern_ribbon = nullptr;
    GraphInspectorDock *d_graph_inspector_dock = nullptr;
    QAction *actionToggleGraphInspector = nullptr;
    bool d_inspector_user_closed = false;
    bool d_inspector_user_pinned = false;
    AutoUpdater *d_auto_updater = nullptr;
    PythonAutomationServer *d_python_server = nullptr;
    bool d_headless_mode = false;
    bool d_ole_embedded = false;
    std::function<void()> d_ole_on_update;
    std::function<void()> d_ole_on_close;

    AxesDialog *openAxesDialogConfigured(const std::function<void(AxesDialog *)> &configure);
    Plot3DDialog *openPlot3dDialogConfigured(const std::function<void(Plot3DDialog *)> &configure);

    struct SavedGraphSettings {
        bool valid = false;
        bool pageViewMode = false;
        int pageUnit = 0;
        double pageWidth = 520.0;
        double pageHeight = 520.0;
        int pageDpi = 300;
        double pageZoom = 1.0;
        QColor pageBg = QColor(Qt::white);
        bool keepAspectRatio = true;
        double targetAspectRatio = 1.0;
        bool panelLabelsEnabled = false;
        int panelLabelPlacement = 0;
        QString panelLabelPattern;
        int panelLabelFontPt = 0;
        int unitFormat = 0;
        bool gridMajorX = false;
        bool gridMajorY = false;
        bool gridMinorX = false;
        bool gridMinorY = false;
        bool hasVisualStyle = false;
        QFont axesFont;
        QFont numbersFont;
        QFont legendFont;
        int legendFrameStyle = 1;
        QColor legendTextColor = QColor(Qt::black);
        QColor legendBackground = QColor(Qt::white);
        int axesLineWidth = 2;
        int majTicksStyle = 1;
        int minTicksStyle = 1;
        int majTicksLength = 9;
        int minTicksLength = 5;
    };

    void captureLastGraphGeometry(MultiLayer *ml, bool allowHiddenGeometry = false);
    void captureLastGraphSettingsForExit(MultiLayer *ml);
    void captureLastTableGeometry(Table *t, bool allowHiddenGeometry = false);
    void applyLastGraphWindowGeometry(MultiLayer *g);
    void applyLastGraphSettings(MultiLayer *g);
    void finalizeLastGraphSettings(MultiLayer *g);
    void applyLastTableWindowGeometry(Table *w);

    QString d_saved_workspace_layout_mode;
    double d_saved_split_left_ratio = 0.40;
    bool d_restoring_window_geometry = false;
    bool d_has_last_graph_geometry = false;
    QRect d_last_graph_geometry;
    bool d_last_graph_maximized = false;
    bool d_has_last_table_geometry = false;
    QRect d_last_table_geometry;
    SavedGraphSettings d_restored_graph_settings;
    SavedGraphSettings d_exit_graph_settings;

    // ---- Beginner navigation, menus and UI state (implemented in ApplicationWindowUx.cpp) ----
public:
    //! Location of the bundled F1 guide (manual/index.html), or the Japanese README on GitHub
    QUrl quickGuideUrl() const;
    //! Human readable name of the active graph tool mode; empty when the pointer is active
    QString currentToolModeName() const;
    //! Refresh the "Mode: ... (Esc to cancel)" indicator in the status bar
    void refreshToolModeIndicator();
    //! Leave the current graph tool mode (zoom, data reader, gadgets, ...)
    void cancelCurrentToolMode();
    //! Plot the selected (or all) columns of a table; shows where to fix problems. Returns the new graph.
    MultiLayer *plotColumnsFromTable(Table *table, int curveType);
    //! Why a 2D plot cannot be made from this table (empty when it can)
    QString plotBlockerForTable(Table *table, int curveType) const;
    //! Non-modal hint near the mouse pointer plus a status bar message
    void showUxHint(const QString &text, int timeoutMs = 8000);
    //! Settings: show domain-specific features outside metallurgy (ROC, Kaplan-Meier, EIS, ...)
    bool showsAdvancedDomainFeatures() const { return d_show_advanced_domain_features; }
    void setShowAdvancedDomainFeatures(bool show);
    //! Object names of the actions hidden unless "Show other-field / advanced features" is on
    static QStringList advancedDomainActionNames();
    //! C15: show the legacy UI (classic toolbar menus, Plot Wizard, time stamp, scripting
    //! language). Off by default; the actions themselves stay callable from scripts / OLE.
    bool showsLegacyUi() const { return d_show_legacy_ui; }
    void setShowLegacyUi(bool show);
    //! Object names of the actions hidden (with their shortcuts) unless "Show Legacy UI" is on
    static QStringList legacyUiActionNames();
    //! Resolve export filename suffix without misclassifying dotted alloy names (e.g. Al-5.5Zn-2.5Mg)
    static QString resolveImageExportFileName(const QString &rawFileName,
                                              const QString &selectedFilter,
                                              bool *outAppended = nullptr);
    //! Check destination writability without truncating an existing file
    static bool probeExportWritableNoTruncate(const QString &filePath);
    //! Atomically/safely replace or create finalTargetPath from validated non-empty tmpPath
    //! without ever deleting finalTargetPath before replacement succeeds.
    static bool commitExportTempFile(const QString &tmpPath, const QString &finalTargetPath);
    //! Safe export transaction: writes to short sibling '.tgexp_<8hex>.<ext>', verifies non-empty
    //! output, and commits onto finalTargetPath via commitExportTempFile().
    static bool safeExportWriteTransaction(
            const QString &finalTargetPath, const QString &extLower,
            const std::function<bool(const QString &writePath)> &writerFn);
    //! Fill a graph context menu (shared by the canvas and the layer button menus)
    void buildGraphContextMenu(QMenu &cm, MultiLayer *plot, bool fromLayerButton);

    //! B4: faint "paste from Excel / drop CSV" hint over an empty worksheet
    void attachEmptyTableHint(Table *table);
    //! B4: "Plot this data" bar on a worksheet that just received data
    void offerPlotAfterImport(Table *table);

    //! Batch window creation/loading guard for O(N) multi-sheet / project load operations
    void beginBatchWindowUpdates();
    void endBatchWindowUpdates();
    bool isBatchWindowUpdatesActive() const { return d_batch_window_updates_depth > 0; }
    void syncBatchWindowName(MyWidget *w);
    void removeBatchWindow(MyWidget *w);
    bool isBatchNameUsedByOther(const QString &candName, MyWidget *exclude = nullptr) const;
    QString nextBatchUniqueName(const QString &base, MyWidget *exclude = nullptr);

    struct BatchWindowUpdates
    {
        ApplicationWindow *d_app = nullptr;
        explicit BatchWindowUpdates(ApplicationWindow *app) : d_app(app)
        {
            if (d_app)
                d_app->beginBatchWindowUpdates();
        }
        ~BatchWindowUpdates()
        {
            if (d_app)
                d_app->endBatchWindowUpdates();
        }
        BatchWindowUpdates(const BatchWindowUpdates &) = delete;
        BatchWindowUpdates &operator=(const BatchWindowUpdates &) = delete;
    };

private:
    void installUxNavigation();
    void applyAdvancedFeatureVisibility();
    bool d_show_advanced_domain_features = false;
    void installLegacyUiSwitch();
    void applyLegacyUiVisibility();
    //! Re-translate the View menu switches of G1 / C15 (called by translateActionsStrings())
    void retranslateUxSwitches();
    bool d_show_legacy_ui = false;
    QToolButton *d_tool_mode_indicator = nullptr;
    QTimer *d_tool_mode_poll = nullptr;
    QString d_last_tool_mode_text;
    int d_batch_window_updates_depth = 0;
    bool d_batch_pending_explorer_filter = false;
    bool d_batch_pending_lv_resize = false;
    bool d_batch_pending_auto_axis_refresh = false;
    QSet<MyWidget *> d_batch_hidden_set;
    QHash<MyWidget *, QString> d_batch_widget_name;
    QHash<QString, int> d_batch_name_counts;
    QHash<QString, int> d_batch_unique_seq;
    QLockFile *d_project_lock = nullptr;
    bool d_project_read_only_lock = false;
    QFileSystemWatcher *d_project_watcher = nullptr;
    QString d_watched_project_path;
    QDateTime d_watched_project_mtime;
    qint64 d_watched_project_size = -1;
    bool d_external_change_notified = false;
    bool d_ignore_watcher_changes = false;
    QWidget *d_empty_graph_pane = nullptr;
    QHash<Folder *, QPointer<MyWidget>> d_folder_active_windows;

    bool loadProjectImpl(const QString &fn, ApplicationWindow **outReplacement = nullptr);
    QString projectLockFilePath(const QString &fn) const;
    bool isProjectFileLockedByOther(const QString &fn, qint64 *holderPidOut = nullptr) const;
    bool acquireProjectFileLock(const QString &fn, bool interactive);
    void releaseProjectFileLock();
    void trackOpenedProjectFile(const QString &fn);
    void untrackOpenedProjectFile();
    bool checkExternalModificationBeforeSave(bool *outSavedAsNew = nullptr);
    bool offerBackupIfPresent(const QString &corruptFn, bool *outAccepted = nullptr);
    bool guardProjectActionOutsideOleEmbedded(const QString &actionDesc,
                                              const QString &filePathToOpen = QString());
    void clearLastCopiedLayerIfReferencing(MyWidget *w);
    static bool isInternalHelperWindow(const MyWidget *w);
    void updateExplorerFolderPaneVisibility();
    void retranslateOpenModelessDialogs();
    bool d_suppress_explorer_user_track = false;
    bool d_explorer_user_shown_in_narrow_split = false;
    bool d_explorer_auto_collapsed_for_split = false;
    bool d_suppress_delete_folder_undo_clear = false;
    bool d_in_insert_translated_strings = false;
    QPointer<MultiLayer> d_last_copied_layer_source_ml;
    void updateEmptyGraphPanePlaceholder();
    void rebuildStylePresetsMenu();
    MultiLayer *openMultiLayerFromTemplateFile(const QString &fn, QString *errOut = nullptr);

private slots:
    void removeDependentTableStatistics(const AbstractAspect *aspect);
    //! Set the active window selected from the context menu's dependency list
    /**
     * See also: http://doc.trolltech.com/4.3/mainwindows-recentfiles-mainwindow-cpp.html
     */
    void setActiveWindowFromAction();

    void handleAspectAdded(const AbstractAspect *aspect, int index);
    void handleAspectAboutToBeRemoved(const AbstractAspect *aspect, int index);
    void handleAspectDescriptionChanged(const AbstractAspect *aspect);
protected slots:
    void lockToolbar(const bool status);
};

#endif
