/***************************************************************************
        File                 : ApplicationWindow_Menus.cpp
        Project              : SciDAVis
        Description          : SciDAVis's main window (actions, menus, toolbars & context menus)
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

void ApplicationWindow::initToolBars()
{
    setWindowIcon(QIcon(":/appicon"));
    QPixmap openIcon, saveIcon;

    file_tools = new QToolBar(tr("File"), this);
    file_tools->setObjectName("file_tools"); // this is needed for QMainWindow::restoreState()
    file_tools->setIconSize(QSize(22, 22));
    addToolBar(Qt::TopToolBarArea, file_tools);

    file_tools->addAction(actionNewProject);

    QMenu *menu_new_aspect = new QMenu(this);
    menu_new_aspect->addAction(actionNewTable);
    menu_new_aspect->addAction(WorkbookManager::instance(this)->newWorkbookAction());
    menu_new_aspect->addAction(actionNewMatrix);
    menu_new_aspect->addAction(actionNewNote);
    menu_new_aspect->addAction(actionNewGraph);
    menu_new_aspect->addAction(actionNewFunctionPlot);
    menu_new_aspect->addAction(actionNewSurfacePlot);
    QToolButton *btn_new_aspect = new QToolButton(this);
    btn_new_aspect->setMenu(menu_new_aspect);
    btn_new_aspect->setPopupMode(QToolButton::InstantPopup);
    btn_new_aspect->setIcon(IconLoader::load("file-new"));
    btn_new_aspect->setToolTip(tr("New Aspect"));
    file_tools->addWidget(btn_new_aspect);

    file_tools->addAction(actionOpen);
    file_tools->addAction(actionLoad);
    file_tools->addAction(actionSaveProject);

    file_tools->addSeparator();

    file_tools->addAction(actionPrint);
    file_tools->addAction(actionExportPDF);

    file_tools->addSeparator();

    file_tools->addAction(actionShowExplorer);
    file_tools->addAction(actionShowLog);
    file_tools->addAction(locktoolbar);

    edit_tools = new QToolBar(tr("Edit"), this);
    edit_tools->setObjectName("edit_tools"); // this is needed for QMainWindow::restoreState()
    edit_tools->setIconSize(QSize(22, 22));
    addToolBar(edit_tools);

    edit_tools->addAction(actionUndo);
    edit_tools->addAction(actionRedo);
    edit_tools->addAction(actionCutSelection);
    edit_tools->addAction(actionCopySelection);
    edit_tools->addAction(actionPasteSelection);
    edit_tools->addAction(actionClearSelection);

    graph_tools = new QToolBar(tr("Graph"), this);
    graph_tools->setObjectName("graph_tools"); // this is needed for QMainWindow::restoreState()
    graph_tools->setIconSize(QSize(22, 22));
    addToolBar(graph_tools);

    dataTools = new QActionGroup(this);
    dataTools->setExclusive(true);

    btnPointer = new QAction(tr("Disable &Tools"), this);
    btnPointer->setObjectName(QStringLiteral("btnPointer")); // ribbon / command search binding
    btnPointer->setActionGroup(dataTools);
    btnPointer->setCheckable(true);
    btnPointer->setIcon(IconLoader::load("pointer"));
    btnPointer->setChecked(true);
    graph_tools->addAction(btnPointer);

    graph_tools->addSeparator();

    QMenu *menu_layers = new QMenu(this);
    QToolButton *btn_layers = new QToolButton(this);
    btn_layers->setMenu(menu_layers);
    btn_layers->setPopupMode(QToolButton::InstantPopup);
    btn_layers->setIcon(IconLoader::load("layer-arrange"));
    btn_layers->setToolTip(tr("Manage layers"));
    graph_tools->addWidget(btn_layers);

    menu_layers->addAction(actionAutomaticLayout);
    menu_layers->addAction(actionAddLayer);
    menu_layers->addAction(actionAddLinkedLayer);
    menu_layers->addAction(actionAddInsetLayer);
    menu_layers->addAction(actionDeleteLayer);
    menu_layers->addAction(actionShowLayerDialog);
    menu_layers->addSeparator();
    menu_layers->addAction(actionPageSetup);
    menu_layers->addAction(actionFitPageToLayers);
    menu_layers->addAction(actionFitLayersToPage);
    menu_layers->addAction(actionTogglePageView);

    QMenu *menu_curves = new QMenu(this);
    QToolButton *btn_curves = new QToolButton(this);
    btn_curves->setMenu(menu_curves);
    btn_curves->setPopupMode(QToolButton::InstantPopup);
    btn_curves->setIcon(IconLoader::load("plot-line-symbol"));
    btn_curves->setToolTip(tr("Add curves / error bars"));
    graph_tools->addWidget(btn_curves);

    menu_curves->addAction(actionShowCurvesDialog);
    menu_curves->addAction(actionAddErrorBars);
    menu_curves->addAction(actionAddFunctionCurve);

    QMenu *menu_plot_enrichments = new QMenu(this);
    QToolButton *btn_plot_enrichments = new QToolButton(this);
    btn_plot_enrichments->setMenu(menu_plot_enrichments);
    btn_plot_enrichments->setPopupMode(QToolButton::InstantPopup);
    btn_plot_enrichments->setIcon(IconLoader::load("text-label"));
    btn_plot_enrichments->setToolTip(tr("Enrichments"));
    graph_tools->addWidget(btn_plot_enrichments);

    actionAddText = new QAction(tr("Add &Text"), this);
    actionAddText->setObjectName(QStringLiteral("actionAddText")); // ribbon / command search binding
    actionAddText->setShortcut(tr("Ctrl+Shift+T"));
    actionAddText->setIcon(IconLoader::load("text-label"));
    actionAddText->setCheckable(true);
    connect(actionAddText, SIGNAL(triggered()), this, SLOT(addText()));
    menu_plot_enrichments->addAction(actionAddText);

    btnArrow = new QAction(tr("Draw &Arrow"), this);
    btnArrow->setShortcut(tr("CTRL+ALT+A"));
    btnArrow->setActionGroup(dataTools);
    btnArrow->setCheckable(true);
    btnArrow->setIcon(IconLoader::load("line-arrow"));
    menu_plot_enrichments->addAction(btnArrow);

    btnLine = new QAction(tr("Draw &Line"), this);
    btnLine->setShortcut(tr("CTRL+ALT+L"));
    btnLine->setActionGroup(dataTools);
    btnLine->setCheckable(true);
    btnLine->setIcon(IconLoader::load("plot-line"));
    menu_plot_enrichments->addAction(btnLine);

    menu_plot_enrichments->addAction(actionTimeStamp);
    menu_plot_enrichments->addAction(actionAddImage);
    menu_plot_enrichments->addAction(actionNewLegend);

    graph_tools->addSeparator();

    btnZoomIn = new QAction(tr("&Zoom In"), this);
    btnZoomIn->setObjectName(QStringLiteral("btnZoomIn")); // ribbon / command search binding
    btnZoomIn->setShortcuts({ QKeySequence(tr("Ctrl++")), QKeySequence(QStringLiteral("Ctrl+=")) });
    btnZoomIn->setActionGroup(dataTools);
    btnZoomIn->setCheckable(true);
    btnZoomIn->setIcon(IconLoader::load("zoom-in"));
    graph_tools->addAction(btnZoomIn);

    btnZoomOut = new QAction(tr("&Zoom Out"), this);
    btnZoomOut->setObjectName(QStringLiteral("btnZoomOut")); // ribbon / command search binding
    btnZoomOut->setShortcut(tr("Ctrl+-"));
    btnZoomOut->setActionGroup(dataTools);
    btnZoomOut->setCheckable(true);
    btnZoomOut->setIcon(IconLoader::load("zoom-out"));
    graph_tools->addAction(btnZoomOut);

    graph_tools->addAction(actionUnzoom);

    graph_tools->addSeparator();

    btnPicker = new QAction(tr("S&creen Reader"), this);
    btnPicker->setObjectName(QStringLiteral("btnPicker")); // ribbon / command search binding
    btnPicker->setActionGroup(dataTools);
    btnPicker->setCheckable(true);
    btnPicker->setIcon(IconLoader::load("screen-reader"));
    graph_tools->addAction(btnPicker);

    btnCursor = new QAction(tr("&Data Reader"), this);
    btnCursor->setObjectName(QStringLiteral("btnCursor")); // ribbon / command search binding
    btnCursor->setShortcut(tr("CTRL+D"));
    btnCursor->setActionGroup(dataTools);
    btnCursor->setCheckable(true);
    btnCursor->setIcon(IconLoader::load("data-reader"));
    graph_tools->addAction(btnCursor);

    btnSelect = new QAction(tr("&Select Data Range"), this);
    btnSelect->setShortcut(tr("Ctrl+Alt+S"));
    btnSelect->setActionGroup(dataTools);
    btnSelect->setCheckable(true);
    btnSelect->setIcon(IconLoader::load("gadget-delta"));
    graph_tools->addAction(btnSelect);

    btnMovePoints = new QAction(tr("&Move Data Points..."), this);
    btnMovePoints->setShortcut(tr("Ctrl+ALT+M"));
    btnMovePoints->setActionGroup(dataTools);
    btnMovePoints->setCheckable(true);
    btnMovePoints->setIcon(IconLoader::load("move-points"));

    btnRemovePoints = new QAction(tr("Remove &Bad Data Points..."), this);
    btnRemovePoints->setShortcut(tr("Ctrl+Shift+B"));
    btnRemovePoints->setActionGroup(dataTools);
    btnRemovePoints->setCheckable(true);
    btnRemovePoints->setIcon(IconLoader::load("remove-points"));

    btnMaskPoints = new QAction(tr("&Mask Data Points (Click)"), this);
    btnMaskPoints->setShortcut(tr("Ctrl+Shift+M"));
    btnMaskPoints->setActionGroup(dataTools);
    btnMaskPoints->setCheckable(true);
    btnMaskPoints->setIcon(IconLoader::load("mask-points"));
    btnMaskPoints->setToolTip(tr("Mask data points on graph without deleting table values"));
    graph_tools->addAction(btnMaskPoints);

    btnUnmaskPoints = new QAction(tr("&Unmask Data Points (Click)"), this);
    btnUnmaskPoints->setShortcut(tr("Ctrl+Alt+U"));
    btnUnmaskPoints->setActionGroup(dataTools);
    btnUnmaskPoints->setCheckable(true);
    btnUnmaskPoints->setIcon(IconLoader::load("unmask-points"));
    btnUnmaskPoints->setToolTip(tr("Unmask previously masked data points on graph"));
    graph_tools->addAction(btnUnmaskPoints);

    connect(dataTools, SIGNAL(triggered(QAction *)), this, SLOT(pickDataTool(QAction *)));

    plot_tools = new QToolBar(tr("Plot"), this);
    plot_tools->setObjectName("plot_tools"); // this is needed for QMainWindow::restoreState()
    plot_tools->setIconSize(QSize(22, 22));
    addToolBar(Qt::TopToolBarArea, plot_tools);

    QMenu *menu_plot_linespoints = new QMenu(this);
    QToolButton *btn_plot_linespoints = new QToolButton(this);
    btn_plot_linespoints->setMenu(menu_plot_linespoints);
    btn_plot_linespoints->setPopupMode(QToolButton::InstantPopup);
    btn_plot_linespoints->setIcon(IconLoader::load("plot-line-symbol"));
    btn_plot_linespoints->setToolTip(tr("Lines and/or symbols"));
    plot_tools->addWidget(btn_plot_linespoints);
    menu_plot_linespoints->addAction(actionPlotL);
    menu_plot_linespoints->addAction(actionPlotP);
    menu_plot_linespoints->addAction(actionPlotLP);
    menu_plot_linespoints->addAction(actionPlotSpline);
    menu_plot_linespoints->addAction(actionPlotVerticalDropLines);
    menu_plot_linespoints->addAction(actionPlotHorSteps);
    menu_plot_linespoints->addAction(actionPlotVertSteps);
    menu_plot_linespoints->addSeparator();
    menu_plot_linespoints->addAction(actionPlotDoubleY);
    menu_plot_linespoints->addAction(actionPlotYOffsetStack);

    QMenu *menu_plot_bars = new QMenu(this);
    QToolButton *btn_plot_bars = new QToolButton(this);
    btn_plot_bars->setMenu(menu_plot_bars);
    btn_plot_bars->setPopupMode(QToolButton::InstantPopup);
    btn_plot_bars->setIcon(IconLoader::load("plot-bars"));
    plot_tools->addWidget(btn_plot_bars);
    menu_plot_bars->addAction(actionPlotVerticalBars);
    menu_plot_bars->addAction(actionPlotHorizontalBars);
    menu_plot_bars->addAction(actionPlotPareto);

    plot_tools->addAction(actionPlotArea);
    plot_tools->addAction(actionPlotPie);
    plot_tools->addAction(actionPlotHistogram);
    plot_tools->addAction(actionBoxPlot);
    plot_tools->addAction(actionPlotViolin);
    plot_tools->addAction(actionPlotQQ);
    plot_tools->addAction(actionPlotTernary);

    QMenu *menu_plot_vect = new QMenu(this);
    QToolButton *btn_plot_vect = new QToolButton(this);
    btn_plot_vect->setMenu(menu_plot_vect);
    btn_plot_vect->setPopupMode(QToolButton::InstantPopup);
    btn_plot_vect->setIcon(IconLoader::load("plot-vectors"));
    plot_tools->addWidget(btn_plot_vect);
    menu_plot_vect->addAction(actionPlotVectXYXY);
    menu_plot_vect->addAction(actionPlotVectXYAM);

    plot_tools->addSeparator();

    plot_tools->addAction(actionPlot3DRibbon);
    plot_tools->addAction(actionPlot3DBars);
    plot_tools->addAction(actionPlot3DScatter);
    plot_tools->addAction(actionPlot3DTrajectory);

    table_tools = new QToolBar(tr("Table"), this);
    table_tools->setObjectName("table_tools"); // this is needed for QMainWindow::restoreState()
    table_tools->setIconSize(QSize(22, 22));
    addToolBar(Qt::TopToolBarArea, table_tools);

    graph_tools->setEnabled(false);
    table_tools->setEnabled(false);
    plot_tools->setEnabled(false);

    d_status_info = new QLabel(this);
    d_status_info->setObjectName(QStringLiteral("StatusBarContextLabel"));
    d_status_info->setFrameStyle(QFrame::NoFrame);
    d_status_info->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_status_info->setStyleSheet(QStringLiteral(
            "QLabel#StatusBarContextLabel { color: #334155; padding: 0 8px; "
            "font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif; font-size: 9.5pt; }"));
    d_status_info->setText(tr("Ready — Select columns to plot or press Ctrl+K to search commands"));
    d_status_info->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(d_status_info, SIGNAL(customContextMenuRequested(const QPoint &)), this,
            SLOT(showStatusBarContextMenu(const QPoint &)));

    statusBar()->setSizeGripEnabled(true);
    statusBar()->addWidget(d_status_info, 1);

    QToolButton *statusCmdHintBtn = new QToolButton(statusBar());
    statusCmdHintBtn->setObjectName(QStringLiteral("StatusBarCommandHintBtn"));
    statusCmdHintBtn->setAutoRaise(false);
    statusCmdHintBtn->setCursor(Qt::PointingHandCursor);
    statusCmdHintBtn->setFocusPolicy(Qt::NoFocus);
    statusCmdHintBtn->setText(tr("Ctrl+K Search Commands"));
    statusCmdHintBtn->setToolTip(tr("Open Fuzzy-Search Command Palette (Ctrl+K)"));
    statusCmdHintBtn->setStyleSheet(QStringLiteral(
            "QToolButton#StatusBarCommandHintBtn {"
            "  background-color: #F1F5F9; color: #475569; border: 1px solid #CBD5E1;"
            "  border-radius: 9px; padding: 1px 10px; margin-right: 4px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 9pt; font-weight: 600;"
            "}"
            "QToolButton#StatusBarCommandHintBtn:hover {"
            "  background-color: #EFF6FF; color: #1D4ED8; border: 1px solid #93C5FD;"
            "}"));
    connect(statusCmdHintBtn, &QToolButton::clicked, this, [this]() {
        if (d_modern_ribbon)
            d_modern_ribbon->openCommandPalette();
    });
    statusBar()->addPermanentWidget(statusCmdHintBtn);

    matrix_plot_tools = new QToolBar(tr("Matrix Plot"), this);
    matrix_plot_tools->setObjectName("matrix_plot_tools");
    addToolBar(Qt::BottomToolBarArea, matrix_plot_tools);

    matrix_plot_tools->addAction(actionPlot3DWireFrame);
    matrix_plot_tools->addAction(actionPlot3DHiddenLine);

    matrix_plot_tools->addAction(actionPlot3DPolygons);
    matrix_plot_tools->addAction(actionPlot3DWireSurface);

    matrix_plot_tools->addSeparator();

    matrix_plot_tools->addAction(actionPlot3DBars);
    matrix_plot_tools->addAction(actionPlot3DScatter);

    matrix_plot_tools->addSeparator();
    matrix_plot_tools->addAction(actionColorMap);
    matrix_plot_tools->addAction(actionContourMap);
    matrix_plot_tools->addAction(actionGrayMap);

    matrix_plot_tools->setEnabled(false);
}

void ApplicationWindow::lockToolbar(const bool status)
{
    if (status) {
        file_tools->setMovable(false);
        edit_tools->setMovable(false);
        graph_tools->setMovable(false);
        graph_3D_tools->setMovable(false);
        plot_tools->setMovable(false);
        table_tools->setMovable(false);
        matrix_plot_tools->setMovable(false);
        locktoolbar->setIcon(IconLoader::load("lock"));
    } else {
        file_tools->setMovable(true);
        edit_tools->setMovable(true);
        graph_tools->setMovable(true);
        graph_3D_tools->setMovable(true);
        plot_tools->setMovable(true);
        table_tools->setMovable(true);
        matrix_plot_tools->setMovable(true);
        locktoolbar->setIcon(IconLoader::load("unlock"));
    }
}

void ApplicationWindow::retranslateOpenModelessDialogs()
{
    QEvent langChangeEvent(QEvent::LanguageChange);
    for (QDialog *dlg : findChildren<QDialog *>()) {
        if (!dlg || dlg->isModal())
            continue;
        qApp->sendEvent(dlg, &langChangeEvent);
        for (QWidget *child : dlg->findChildren<QWidget *>()) {
            if (child)
                qApp->sendEvent(child, &langChangeEvent);
        }
    }
}

void ApplicationWindow::insertTranslatedStrings()
{
    struct ScopedInInsertTranslated {
        bool &flag;
        bool prev;
        explicit ScopedInInsertTranslated(bool &f) : flag(f), prev(f) { flag = true; }
        ~ScopedInInsertTranslated() { flag = prev; }
    } insertLangGuard(d_in_insert_translated_strings);

    if (projectname == "untitled")
        setWindowTitle(tr("ToriGraph - untitled"));

    if (Folder *rf = projectFolder()) {
        if (rf->name() == QLatin1String("UNTITLED")
            || rf->name() == QString::fromUtf8("\xE7\x84\xA1\xE9\xA1\x8C")) {
            const QString locRoot = tr("UNTITLED");
            rf->setName(locRoot);
            if (rf->folderListItem()) {
                QSignalBlocker sbFolders(&folders);
                rf->folderListItem()->setText(0, locRoot);
            }
        }
    }

    if (folders.headerItem())
        folders.headerItem()->setText(0, tr("Folder"));

    if (lv.headerItem()) {
        lv.headerItem()->setText(0, tr("Name"));
        lv.headerItem()->setText(1, tr("Type"));
        lv.headerItem()->setText(2, tr("View"));
        lv.headerItem()->setText(3, tr("Created"));
        lv.headerItem()->setText(4, tr("Label"));
    }

    for (int i = 0; i < lv.topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = lv.topLevelItem(i);
        if (!it)
            continue;
        if (it->type() == FolderListItem::FolderType) {
            it->setText(1, tr("Folder"));
        } else if (it->type() == WindowListItem::WindowType) {
            MyWidget *w = static_cast<WindowListItem *>(it)->window();
            if (!w)
                continue;
            if (w->inherits("Matrix"))
                it->setText(1, tr("Matrix"));
            else if (w->inherits("Table"))
                it->setText(1, tr("Table"));
            else if (w->inherits("Note"))
                it->setText(1, tr("Note"));
            else if (w->inherits("MultiLayer"))
                it->setText(1, tr("Graph"));
            else if (w->inherits("Graph3D"))
                it->setText(1, tr("3D Graph"));
            it->setText(2, w->aspect());
            const QString typeStr = it->text(1);
            const QString lblStr = w->windowLabel();
            QString tip = w->name();
            if (!typeStr.isEmpty())
                tip += QStringLiteral("\n") + tr("Type") + QStringLiteral(": ") + typeStr;
            if (!w->aspect().isEmpty())
                tip += QStringLiteral("  (") + w->aspect() + QStringLiteral(")");
            if (!lblStr.isEmpty())
                tip += QStringLiteral("\n") + tr("Label") + QStringLiteral(": ") + lblStr;
            it->setToolTip(0, tip);
            it->setToolTip(1, tip);
        }
    }
    if (lv.columnCount() >= 5 && lv.isColumnHidden(3)) {
        lv.resizeColumnToContents(1);
        lv.resizeColumnToContents(2);
    }

    explorerWindow.setWindowTitle(tr("Project Explorer"));
    if (d_explorer_search) {
        d_explorer_search->setPlaceholderText(tr("Search windows / labels..."));
        d_explorer_search->setToolTip(tr("Live-filter windows by name, type, or label"));
    }
    if (d_explorer_type_filter && d_explorer_type_filter->count() >= 6) {
        d_explorer_type_filter->setItemText(0, tr("All Windows"));
        d_explorer_type_filter->setItemText(1, tr("Tables (Workbooks)"));
        d_explorer_type_filter->setItemText(2, tr("Graphs (2D/3D)"));
        d_explorer_type_filter->setItemText(3, tr("Matrices"));
        d_explorer_type_filter->setItemText(4, tr("Notes / Scripts"));
        d_explorer_type_filter->setItemText(5, tr("Subfolders Only"));
        d_explorer_type_filter->setToolTip(tr("Filter Project Explorer by window type"));
    }
    if (d_explorer_subfolder_check) {
        d_explorer_subfolder_check->setText(tr("Subfolders"));
        d_explorer_subfolder_check->setToolTip(
                tr("Include windows from subfolders when searching/filtering"));
    }
    if (d_btn_explorer_new_folder)
        d_btn_explorer_new_folder->setToolTip(tr("New Folder"));
    if (d_btn_explorer_new_table)
        d_btn_explorer_new_table->setToolTip(tr("New Table (Workbook)"));
    if (d_btn_explorer_new_graph)
        d_btn_explorer_new_graph->setToolTip(tr("New 2D Graph"));
    if (d_btn_tabbed_view) {
        d_btn_tabbed_view->setText(tr("Tabbed View"));
        d_btn_tabbed_view->setToolTip(tr("Toggle Origin-style Tabbed Workspace vs SubWindows"));
    }

    logWindow.setWindowTitle(tr("Results Log"));
#ifdef SCRIPTING_CONSOLE
    consoleWindow.setWindowTitle(tr("Scripting Console"));
#endif
    for (QUndoView *uv : findChildren<QUndoView *>()) {
        if (uv->parentWidget() && uv->parentWidget()->inherits("QDialog"))
            uv->parentWidget()->setWindowTitle(tr("Undo/Redo History"));
        else
            uv->setWindowTitle(tr("Undo/Redo History"));
    }

    table_tools->setWindowTitle(tr("Table"));
    plot_tools->setWindowTitle(tr("Plot"));
    graph_tools->setWindowTitle(tr("Graph"));
    file_tools->setWindowTitle(tr("File"));
    edit_tools->setWindowTitle(tr("Edit"));
    matrix_plot_tools->setWindowTitle(tr("Matrix Plot"));
    graph_3D_tools->setWindowTitle(tr("3D Surface"));
    for (QToolBar *tb : findChildren<QToolBar *>()) {
        if (!tb || tb->parentWidget() != this || !tb->toggleViewAction())
            continue;
        if (tb == d_ribbon_toolbar
            || tb->objectName() == QLatin1String("ModernRibbonToolBar")) {
            tb->toggleViewAction()->setVisible(false);
            tb->toggleViewAction()->setEnabled(false);
            continue;
        }
        tb->toggleViewAction()->setText(tb->windowTitle());
    }

    file->setTitle(tr("&File"));
    edit->setTitle(tr("&Edit"));
    view->setTitle(tr("&View"));
    scriptingMenu->setTitle(tr("&Scripting"));
    graph->setTitle(tr("&Graph"));
    plot3DMenu->setTitle(tr("3D &Plot"));
    matrixMenu->setTitle(tr("&Matrix"));
    tableMenu->setTitle(tr("&Table"));
    plot2D->setTitle(tr("&Plot"));
    dataMenu->setTitle(tr("Wor&ksheet Analysis"));
    plotDataMenu->setTitle(tr("Too&ls"));
    calcul->setTitle(tr("&Analysis"));
    d_quick_fit_menu->setTitle(tr("&Quick Fit"));
    format->setTitle(tr("F&ormat"));
    windowsMenu->setTitle(tr("&Windows"));
    help->setTitle(tr("&Help"));
    if (colorPaletteMenu) {
        colorPaletteMenu->setTitle(tr("Apply Color &Palette"));
        for (QAction *palAct : colorPaletteMenu->actions()) {
            if (!palAct || palAct->isSeparator())
                continue;
            const QString key = palAct->data().toString();
            if (!key.isEmpty())
                palAct->setText(tr(key.toUtf8().constData()));
        }
    }
    if (aspectRatioMenu)
        aspectRatioMenu->setTitle(tr("Graph &Aspect Ratio"));
    if (framePresetsMenu)
        framePresetsMenu->setTitle(tr("&Frame Presets"));
    if (stylePresetsMenu)
        stylePresetsMenu->setTitle(tr("&Style Presets"));
    if (pasteFormatSpecialMenu)
        pasteFormatSpecialMenu->setTitle(tr("Paste Format &Special"));
    if (d_btn_style_presets_popup)
        d_btn_style_presets_popup->setToolTip(tr("1-Click Publication & Slide Style Presets"));
    if (languageMenu)
        languageMenu->setTitle(tr("&Language (言語)"));

    type->setTitle(tr("&New"));
    recent->setTitle(tr("&Recent Projects"));
    exportPlot->setTitle(tr("&Export Graph"));

    specialPlot->setTitle(tr("Special Line/Symb&ol"));
    stat->setTitle(tr("Statistical &Graphs"));
    panels->setTitle(tr("Pa&nel"));
    plot3D->setTitle(tr("3&D Plot"));

    translateMenu->setTitle(tr("&Translate"));
    smooth->setTitle(tr("&Smooth"));
    filter->setTitle(tr("&FFT Filter"));
    decay->setTitle(tr("Fit E&xponential Decay"));
    multiPeakMenu->setTitle(tr("Fit &Multi-Peak"));
    if (QMenu *menuMoreGadgets = findChild<QMenu *>(QStringLiteral("menuMoreGadgets")))
        menuMoreGadgets->setTitle(tr("&More Gadgets"));
    if (QMenu *menuFileTemplates = findChild<QMenu *>(QStringLiteral("menuFileTemplates")))
        menuFileTemplates->setTitle(tr("&Templates (.qpt)"));
    if (stylePresetsMenu)
        rebuildStylePresetsMenu();
    if (gadgetsMenu)
        gadgetsMenu->setTitle(tr("Ga&dgets"));
    if (dataMaskMenu)
        dataMaskMenu->setTitle(tr("Data &Masking"));
    if (materialsAnalysisMenu)
        materialsAnalysisMenu->setTitle(tr("Mat&erials Analysis"));
    if (curveMathMenu)
        curveMathMenu->setTitle(tr("&Mathematics"));
    if (advFitMenu)
        advFitMenu->setTitle(tr("&Fitting"));
    if (peaksBaselineMenu)
        peaksBaselineMenu->setTitle(tr("&Peaks and Baseline"));
    if (advSignalMenu)
        advSignalMenu->setTitle(tr("Signal &Processing"));
    if (graphStatsMenu)
        graphStatsMenu->setTitle(tr("S&tatistics"));
    if (recalcMenu)
        recalcMenu->setTitle(tr("&Recalculation && Themes"));

    translateActionsStrings();
    if (d_ribbon_toolbar)
        d_ribbon_toolbar->setWindowTitle(tr("Modern Ribbon"));

    QEvent langChangeEvent(QEvent::LanguageChange);
    for (MyWidget *w : windowsList()) {
        if (!w)
            continue;
        if (QMenu *sysMenu = w->systemMenu()) {
            for (QAction *act : sysMenu->actions()) {
                if (!act || act->isSeparator())
                    continue;
                QString srcEn = act->property("_toriSysMenuSourceEn").toString();
                if (srcEn.isEmpty()) {
                    const QString cur = act->text();
                    if (cur == QString::fromUtf8("\xE5\x85\x83\xE3\x81\xAE\xE3\x82\xB5\xE3\x82\xA4\xE3\x82\xBA\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99(&R)")
                        || cur.endsWith(QLatin1String("(&R)")))
                        srcEn = QStringLiteral("&Restore");
                    else if (cur == QString::fromUtf8("\xE7\xA7\xBB\xE5\x8B\x95(&M)"))
                        srcEn = QStringLiteral("&Move");
                    else if (cur == QString::fromUtf8("\xE3\x82\xB5\xE3\x82\xA4\xE3\x82\xBA\xE5\xA4\x89\xE6\x9B\xB4(&S)")
                             || cur == QString::fromUtf8("\xE3\x82\xB5\xE3\x82\xA4\xE3\x82\xBA(&S)"))
                        srcEn = QStringLiteral("&Size");
                    else if (cur == QString::fromUtf8("\xE6\x9C\x80\xE5\xB0\x8F\xE5\x8C\x96(&N)"))
                        srcEn = QStringLiteral("Mi&nimize");
                    else if (cur == QString::fromUtf8("\xE6\x9C\x80\xE5\xA4\xA7\xE5\x8C\x96(&X)"))
                        srcEn = QStringLiteral("Ma&ximize");
                    else if (cur == QString::fromUtf8("\xE5\xB8\xB8\xE3\x81\xAB\xE6\x89\x8B\xE5\x89\x8D\xE3\x81\xAB\xE8\xA1\xA8\xE7\xA4\xBA(&T)"))
                        srcEn = QStringLiteral("Stay on &Top");
                    else if (cur == QString::fromUtf8("\xE9\x96\x89\xE3\x81\x98\xE3\x82\x8B(&C)"))
                        srcEn = QStringLiteral("&Close");
                    else
                        srcEn = cur;
                    act->setProperty("_toriSysMenuSourceEn", srcEn);
                }
                act->setText(
                        QCoreApplication::translate("QMdiSubWindow", srcEn.toUtf8().constData()));
            }
        }
        qApp->sendEvent(w, &langChangeEvent);
        if (w->inherits("Table")) {
            Table *tbl = static_cast<Table *>(w);
            if (tbl->d_future_table)
                qApp->sendEvent(tbl->d_future_table, &langChangeEvent);
        } else if (w->inherits("Matrix")) {
            Matrix *mat = static_cast<Matrix *>(w);
            if (mat->d_future_matrix)
                qApp->sendEvent(mat->d_future_matrix, &langChangeEvent);
        }
        for (QWidget *child : w->findChildren<QWidget *>()) {
            if (child)
                qApp->sendEvent(child, &langChangeEvent);
        }
    }

    MyWidget *activeWin = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    customMenu(activeWin);
    if (activeWin)
        customToolBars(activeWin);

    if (d_modern_ribbon)
        d_modern_ribbon->retranslateUi();
    if (d_graph_inspector_dock)
        d_graph_inspector_dock->retranslateUi();
    if (GraphMiniToolbar *mtb = GraphMiniToolbar::instance())
        mtb->retranslateUi();
    updateStatusBarForWindow(d_status_info, d_workspace.activeSubWindow());
    d_last_tool_mode_text.clear();
    refreshToolModeIndicator();
    retranslateOpenModelessDialogs();
    updateExplorerAdaptiveLayout(false);
    ModernUiTheme::upgradeAllWindowIcons(this);
}

void ApplicationWindow::initMainMenu()
{
    file = new QMenu(this);
    file->setTitle(tr("&File"));
    file->setFont(appFont);

    type = file->addMenu(tr("&New"));
    type->setFont(appFont);
    type->addAction(actionNewProject);
    type->addAction(actionNewTable);
    type->addAction(WorkbookManager::instance(this)->newWorkbookAction());
    type->addAction(actionNewMatrix);
    type->addAction(actionNewNote);
    type->addAction(actionNewGraph);
    type->addAction(actionNewFunctionPlot);
    type->addAction(actionNewSurfacePlot);

    file->addAction(actionOpen);

    recent = file->addMenu(tr("&Recent Projects"));
    recent->setFont(appFont);

    file->addSeparator();

    file->addAction(actionLoadImage);
    file->addAction(actionImportImage);
    file->addAction(actionGraphDigitizer);

    file->addSeparator();

    file->addAction(actionSaveProject);
    file->addAction(actionSaveProjectAs);

    file->addSeparator();
    {
        QMenu *templatesSub = file->addMenu(tr("&Templates (.qpt)"));
        templatesSub->setObjectName(QStringLiteral("menuFileTemplates"));
        templatesSub->setFont(appFont);
        templatesSub->addAction(actionApplyTableTemplate);
        templatesSub->addSeparator();
        templatesSub->addAction(actionOpenTemplate);
        templatesSub->addAction(actionSaveTemplate);
    }
    file->addSeparator();

    exportPlot = file->addMenu(tr("&Export Graph"));
    exportPlot->addAction(actionExportGraph);
    exportPlot->addAction(actionExportPDF);
    exportPlot->addAction(actionExportAllGraphs);
    if (actionExportPythonScript) {
        exportPlot->addSeparator();
        exportPlot->addAction(actionExportPythonScript);
    }
    {
        // E8: write graphs into a .pptx as editable objects (no member action: the text is
        // refreshed in translateActionsStrings)
        ToriGraphPptx::registerTranslations();
        auto *actionSendToPowerPoint = new QAction(IconLoader::load("copy-ole"),
                                                   ToriGraphPptx::sendToPowerPointMenuText(), this);
        actionSendToPowerPoint->setObjectName(QStringLiteral("actionSendToPowerPoint"));
        actionSendToPowerPoint->setToolTip(ToriGraphPptx::sendToPowerPointToolTip());
        actionSendToPowerPoint->setStatusTip(ToriGraphPptx::sendToPowerPointToolTip());
        connect(actionSendToPowerPoint, &QAction::triggered, this,
                [this]() { ToriGraphPptx::sendGraphsToPowerPoint(this); });
        file->addAction(actionSendToPowerPoint);
    }

    file->addAction(actionPrint);
    file->addAction(actionPrintAllPlots);

    file->addSeparator();

    file->addAction(actionShowExportASCIIDialog);
    file->addAction(actionLoad);
    {
        // D1: Excel .xlsx import (no member action: text is refreshed in translateActionsStrings)
        XlsxImport::registerTranslations();
        auto *actionImportExcel =
                new QAction(IconLoader::load("import-csv"), tr("Import &Excel Workbook (.xlsx)..."),
                            this);
        actionImportExcel->setObjectName(QStringLiteral("actionImportExcel"));
        actionImportExcel->setToolTip(
                tr("Import every sheet of an Excel workbook (.xlsx) as worksheets"));
        connect(actionImportExcel, &QAction::triggered, this,
                [this]() { XlsxImport::importWithDialog(this); });
        file->addAction(actionImportExcel);
    }

    file->addSeparator();

    file->addAction(actionCloseAllWindows);

    edit = new QMenu(this);
    edit->setFont(appFont);
    edit->setTitle(tr("&Edit"));
    edit->addAction(actionUndo);
    edit->addAction(actionRedo);

    edit->addSeparator();

    edit->addAction(actionCutSelection);
    edit->addAction(actionCopySelection);
    edit->addAction(actionCopyPageImage);
    edit->addAction(actionPasteSelection);
    edit->addAction(actionClearSelection);
    edit->addSeparator();
    pasteFormatSpecialMenu = new QMenu(this);
    pasteFormatSpecialMenu->setFont(appFont);
    pasteFormatSpecialMenu->setTitle(tr("Paste Format &Special"));
    if (actionPasteFormatFonts)
        pasteFormatSpecialMenu->addAction(actionPasteFormatFonts);
    if (actionPasteFormatColors)
        pasteFormatSpecialMenu->addAction(actionPasteFormatColors);
    if (actionPasteFormatScales)
        pasteFormatSpecialMenu->addAction(actionPasteFormatScales);
    pasteFormatSpecialMenu->addSeparator();
    if (actionPasteFormatFolder)
        pasteFormatSpecialMenu->addAction(actionPasteFormatFolder);

    edit->addAction(actionCopyGraphFormat);
    edit->addAction(actionPasteGraphFormat);
    edit->addMenu(pasteFormatSpecialMenu);
    edit->addAction(actionCopyGraphOle);
    if (actionOleEmbedWholeProject)
        edit->addAction(actionOleEmbedWholeProject);
    edit->addAction(actionCopyWindow);
    edit->addAction(actionBatchPlot);

    edit->addSeparator();

    edit->addAction(actionDeleteFitTables);
    edit->addAction(actionClearLogInfo);

    edit->addSeparator();

    edit->addAction(actionShowConfigureDialog);
    for (int i = 0; i < SettingsTransfer::ActCount; ++i) // I10: export / import / reset settings
        edit->addAction(SettingsTransfer::action(this, SettingsTransfer::ActionId(i)));

    view = new QMenu(this);
    view->setFont(appFont);
    view->setTitle(tr("&View"));
    toolbarsMenu = createToolbarsMenu();
    if (!toolbarsMenu)
        toolbarsMenu = new QMenu(this);
    toolbarsMenu->setTitle(tr("Toolbars"));

    view->addMenu(toolbarsMenu);
    view->addAction(locktoolbar);
    if (actionCommandPalette)
        view->addAction(actionCommandPalette);
    if (actionToggleFocusHudMode)
        view->addAction(actionToggleFocusHudMode);
    view->addSeparator();
    if (actionWorkspaceLayoutTabbed)
        view->addAction(actionWorkspaceLayoutTabbed);
    if (actionWorkspaceLayoutSplit)
        view->addAction(actionWorkspaceLayoutSplit);
    if (actionWorkspaceLayoutMdi)
        view->addAction(actionWorkspaceLayoutMdi);
    view->addSeparator();
    view->addAction(actionTabbedWorkspaceView);
    if (actionToggleGraphInspector)
        view->addAction(actionToggleGraphInspector);
    view->addAction(actionToggleMiniToolbar);
    connect(view, SIGNAL(aboutToShow()), this, SLOT(syncGraphPageAndToolbarMenuState()));
    view->addSeparator();
    view->addAction(actionShowPlotWizard);
    view->addAction(actionShowExplorer);
    view->addAction(actionShowLog);
    view->addAction(actionShowHistory);
#ifdef SCRIPTING_CONSOLE
    view->addAction(actionShowConsole);
#endif

    languageMenu = new QMenu(this);
    languageMenu->setFont(appFont);
    languageMenu->setTitle(tr("&Language (言語)"));
    languageMenu->addAction(actionLangJapanese);
    languageMenu->addAction(actionLangEnglish);
    view->addSeparator();
    view->addMenu(languageMenu);

    colorPaletteMenu = new QMenu(this);
    colorPaletteMenu->setFont(appFont);
    colorPaletteMenu->setTitle(tr("Apply Color &Palette"));
    Graph::registerPaletteTranslations();
    static const char *const presetPalettes[] = {
        QT_TR_NOOP("ToriGraph Standard"), // default for new curves
        QT_TR_NOOP("Matplotlib (tab10)"), // was "ISIJ / Origin Default" (still resolves)
        QT_TR_NOOP("Origin Classic"),
        QT_TR_NOOP("Viridis"),
        QT_TR_NOOP("Plasma"),
        QT_TR_NOOP("Inferno"),
        QT_TR_NOOP("CoolWarm"),
        QT_TR_NOOP("Nature (NPG)"),
        QT_TR_NOOP("Science (AAAS)"),
        QT_TR_NOOP("Okabe-Ito (Colorblind Safe)"),
        QT_TR_NOOP("Monochrome / Grayscale")
    };
    for (const char *palName : presetPalettes) {
        QAction *palAct = colorPaletteMenu->addAction(
                tr(palName), this, SLOT(applyColorPaletteByAction()));
        palAct->setData(QString::fromUtf8(palName));
    }
    colorPaletteMenu->addSeparator();
    colorPaletteMenu->addAction(actionColorPaletteEditor);

    aspectRatioMenu = new QMenu(this);
    aspectRatioMenu->setFont(appFont);
    aspectRatioMenu->setTitle(tr("Graph &Aspect Ratio"));
    aspectRatioMenu->addAction(actionAspect1_1);
    aspectRatioMenu->addAction(actionAspect4_3);
    aspectRatioMenu->addAction(actionAspect16_9);
    aspectRatioMenu->addAction(actionAspect3_2);
    aspectRatioMenu->addAction(actionAspectCustom);
    aspectRatioMenu->addSeparator();
    aspectRatioMenu->addAction(actionKeepAspectRatio);

    framePresetsMenu = new QMenu(this);
    framePresetsMenu->setFont(appFont);
    framePresetsMenu->setTitle(tr("&Frame Presets"));
    framePresetsMenu->addAction(actionFrameBoxInward);
    framePresetsMenu->addAction(actionFrameBoxPlain);
    framePresetsMenu->addAction(actionFrameOpenL);
    framePresetsMenu->addAction(actionFrameZeroCrossed);

    stylePresetsMenu = new QMenu(this);
    stylePresetsMenu->setFont(appFont);
    stylePresetsMenu->setTitle(tr("&Style Presets"));
    connect(stylePresetsMenu, &QMenu::aboutToShow, this,
            &ApplicationWindow::rebuildStylePresetsMenu);
    rebuildStylePresetsMenu();

    graph = new QMenu(this);
    graph->setFont(appFont);
    graph->setTitle(tr("&Graph"));
    connect(graph, SIGNAL(aboutToShow()), this, SLOT(syncGraphPageAndToolbarMenuState()));
    graph->addAction(actionShowCurvesDialog);
    graph->addAction(actionAddErrorBars);
    graph->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActErrorBand)); // E11
    graph->addAction(actionAddFunctionCurve);

    graph->addSeparator();

    graph->addAction(actionAddText);
    graph->addAction(btnArrow);
    graph->addAction(btnLine);
    graph->addAction(actionTimeStamp);
    graph->addAction(actionAddImage);
    graph->addAction(actionNewLegend);

    graph->addSeparator(); // layers section
    graph->addAction(actionAutomaticLayout);
    graph->addAction(actionAddLayer);
    graph->addAction(actionAddLinkedLayer);
    graph->addAction(actionAddInsetLayer);
    if (actionMergeGraphs)
        graph->addAction(actionMergeGraphs);
    graph->addAction(actionDeleteLayer);
    graph->addAction(actionShowLayerDialog);
    graph->addSeparator();
    graph->addAction(actionWaterfallStackOffset);
    graph->addAction(actionResetStackOffsets);
    graph->addAction(actionBatchPlot);
    graph->addSeparator();
    graph->addAction(actionGraphDigitizer);

    plot3DMenu = new QMenu(this);
    plot3DMenu->setFont(appFont);
    plot3DMenu->setTitle(tr("3D &Plot"));

    plot3DMenu->addAction(actionPlot3DWireFrame);
    plot3DMenu->addAction(actionPlot3DHiddenLine);

    plot3DMenu->addAction(actionPlot3DPolygons);
    plot3DMenu->addAction(actionPlot3DWireSurface);

    plot3DMenu->addSeparator();

    plot3DMenu->addAction(actionPlot3DBars);
    plot3DMenu->addAction(actionPlot3DScatter);

    plot3DMenu->addSeparator();
    plot3DMenu->addAction(actionColorMap);
    plot3DMenu->addAction(actionContourMap);
    plot3DMenu->addAction(actionGrayMap);

    matrixMenu = new QMenu(this);
    matrixMenu->setFont(appFont);
    matrixMenu->setTitle(tr("&Matrix"));

    tableMenu = new QMenu(this);
    tableMenu->setFont(appFont);
    tableMenu->setTitle(tr("&Table"));

    recalcMenu = new QMenu(this);
    recalcMenu->setFont(appFont);
    recalcMenu->setTitle(tr("&Recalculation && Themes"));
    recalcMenu->addAction(actionRecalcAuto);
    recalcMenu->addAction(actionRecalcManual);
    recalcMenu->addAction(actionRecalcDisabled);
    recalcMenu->addSeparator();
    recalcMenu->addAction(actionRecalculateNow);
    recalcMenu->addSeparator();
    recalcMenu->addAction(actionRepeatLinearFitAll);
    recalcMenu->addAction(actionRepeatNormalizeAll);
    recalcMenu->addSeparator();
    recalcMenu->addAction(actionManageDialogThemes);
    connect(recalcMenu, SIGNAL(aboutToShow()), this, SLOT(syncRecalcMenuState()));

    initPlotMenu();
    initTableAnalysisMenu();
    initPlotDataMenu();

    gadgetsMenu = new QMenu(this);
    gadgetsMenu->setFont(appFont);
    gadgetsMenu->setTitle(tr("Ga&dgets"));
    gadgetsMenu->addAction(actionGadgetVerticalCursor);
    gadgetsMenu->addAction(actionGadgetDeltaCursor);
    gadgetsMenu->addSeparator();
    gadgetsMenu->addAction(actionGadgetRegionStats);
    gadgetsMenu->addAction(actionGadgetQuickFit);
    gadgetsMenu->addAction(actionGadgetQuickPeaks);
    gadgetsMenu->addAction(actionGadgetIntegrate);
    gadgetsMenu->addAction(actionGadgetDifferentiate);
    gadgetsMenu->addSeparator();
    {
        QMenu *moreGadgets = gadgetsMenu->addMenu(tr("&More Gadgets"));
        moreGadgets->setObjectName(QStringLiteral("menuMoreGadgets"));
        moreGadgets->setFont(appFont);
        moreGadgets->addAction(actionGadgetQuickSigmoidal);
        moreGadgets->addAction(actionGadgetInterpolate);
        moreGadgets->addAction(actionGadgetIntersect);
        moreGadgets->addAction(actionGadgetFFT);
        moreGadgets->addSeparator();
        moreGadgets->addAction(actionGadgetIntegrate2D);
        moreGadgets->addAction(actionGadgetImageProfile);
        moreGadgets->addAction(actionGadgetRiseTime);
        moreGadgets->addAction(actionGadgetCluster);
    }
    gadgetsMenu->addSeparator();
    gadgetsMenu->addAction(actionGadgetClear);

    if (graph_tools) {
        graph_tools->addSeparator();
        QToolButton *btnGadgetsPopup = new QToolButton(this);
        btnGadgetsPopup->setMenu(gadgetsMenu);
        btnGadgetsPopup->setPopupMode(QToolButton::InstantPopup);
        btnGadgetsPopup->setIcon(IconLoader::load("gadget-delta"));
        btnGadgetsPopup->setToolTip(tr("Interactive ROI Gadgets"));
        graph_tools->addWidget(btnGadgetsPopup);

        d_btn_style_presets_popup = new QToolButton(this);
        d_btn_style_presets_popup->setMenu(stylePresetsMenu);
        d_btn_style_presets_popup->setPopupMode(QToolButton::InstantPopup);
        d_btn_style_presets_popup->setIcon(IconLoader::load("layer-arrange"));
        d_btn_style_presets_popup->setToolTip(tr("1-Click Publication & Slide Style Presets"));
        graph_tools->addWidget(d_btn_style_presets_popup);

        graph_tools->addAction(actionCopyGraphFormat);
        graph_tools->addAction(actionPasteGraphFormat);
        graph_tools->addAction(actionBatchPlot);
        graph_tools->addAction(actionRecalculateNow);
    }

    calcul = new QMenu(this);
    calcul->setFont(appFont);
    calcul->setTitle(tr("&Analysis"));

    // Structured OriginPro Analysis Domain Submenus
    materialsAnalysisMenu = calcul->addMenu(tr("Mat&erials Analysis"));
    materialsAnalysisMenu->setFont(appFont);
    if (actionTensileYield)
        materialsAnalysisMenu->addAction(actionTensileYield);
    if (actionWorkHardeningDialog)
        materialsAnalysisMenu->addAction(actionWorkHardeningDialog);
    if (actionKissingerDialog)
        materialsAnalysisMenu->addAction(actionKissingerDialog);
    if (actionArrheniusPlot)
        materialsAnalysisMenu->addAction(actionArrheniusPlot);
    if (actionXRDAnalyzerDialog)
        materialsAnalysisMenu->addAction(actionXRDAnalyzerDialog);
    if (actionVaspPlotterDialog)
        materialsAnalysisMenu->addAction(actionVaspPlotterDialog);
    if (actionWeibullPlot)
        materialsAnalysisMenu->addAction(actionWeibullPlot);
    if (actionTaucPlot || actionEISAnalyzer) {
        materialsAnalysisMenu->addSeparator();
        if (actionTaucPlot)
            materialsAnalysisMenu->addAction(actionTaucPlot);
        if (actionEISAnalyzer)
            materialsAnalysisMenu->addAction(actionEISAnalyzer);
    }

    curveMathMenu = calcul->addMenu(tr("&Mathematics"));
    curveMathMenu->setFont(appFont);
    curveMathMenu->addAction(actionSimpleCurveMath);
    curveMathMenu->addAction(actionSubtractStraightLine);
    curveMathMenu->addAction(actionAverageMultipleCurves);
    curveMathMenu->addAction(actionNormalizeCurves);
    curveMathMenu->addAction(actionCurveIntersections);
    curveMathMenu->addAction(actionPolygonLoopArea);
    curveMathMenu->addAction(actionFillBetweenCurves);
    curveMathMenu->addAction(actionWaterfallStackOffset);
    curveMathMenu->addAction(actionResetStackOffsets);
    curveMathMenu->addSeparator();
    curveMathMenu->addAction(actionDifferentiate);
    curveMathMenu->addAction(actionSavGolSmoothDerivative);
    curveMathMenu->addAction(actionShowIntDialog);
    curveMathMenu->addAction(actionInterpolate);

    advFitMenu = calcul->addMenu(tr("&Fitting"));
    advFitMenu->setFont(appFont);

    peaksBaselineMenu = calcul->addMenu(tr("&Peaks and Baseline"));
    peaksBaselineMenu->setFont(appFont);

    advSignalMenu = calcul->addMenu(tr("Signal &Processing"));
    advSignalMenu->setFont(appFont);

    graphStatsMenu = calcul->addMenu(tr("S&tatistics"));
    graphStatsMenu->setFont(appFont);
    graphStatsMenu->addAction(actionNormalityTest);
    graphStatsMenu->addAction(actionOutlierTest);
    graphStatsMenu->addAction(actionConfidenceEllipse);
    graphStatsMenu->addAction(actionGroupComparison);
    graphStatsMenu->addAction(actionNonparametricTests);
    graphStatsMenu->addAction(actionPairedComparisonPlot);
    graphStatsMenu->addAction(actionTwoWayANOVA);
    graphStatsMenu->addAction(actionBlandAltman);
    graphStatsMenu->addAction(actionPassingBablokDeming);
    graphStatsMenu->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActLognormalProbability)); // E11
    graphStatsMenu->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActGumbelProbability)); // E11
    graphStatsMenu->addAction(actionROCCurve);
    graphStatsMenu->addAction(actionKaplanMeier);
    graphStatsMenu->addAction(actionCorrelationMatrix);
    graphStatsMenu->addAction(actionClustering);
    graphStatsMenu->addAction(actionPCABiplot);

    calcul->addMenu(recalcMenu);
    calcul->addSeparator();

    translateMenu = calcul->addMenu(tr("&Translate"));
    translateMenu->setFont(appFont);
    translateMenu->addAction(actionTranslateVert);
    translateMenu->addAction(actionTranslateHor);

    smooth = advSignalMenu->addMenu(tr("&Smooth"));
    smooth->setFont(appFont);
    smooth->addAction(actionSmoothSavGol);
    smooth->addAction(actionSmoothAverage);
    smooth->addAction(actionSmoothFFT);
    smooth->addAction(actionSmoothLOWESS);
    smooth->addAction(actionSmoothWhittaker);
    smooth->addAction(actionSmoothMedian);

    advSignalMenu->addAction(actionSavGolSmoothDerivative);

    filter = advSignalMenu->addMenu(tr("&FFT Filter"));
    filter->setFont(appFont);
    filter->addAction(actionLowPassFilter);
    filter->addAction(actionHighPassFilter);
    filter->addAction(actionBandPassFilter);
    filter->addAction(actionBandBlockFilter);

    advSignalMenu->addAction(actionIirDigitalFilter);
    advSignalMenu->addAction(actionDecimateResample);
    advSignalMenu->addSeparator();
    advSignalMenu->addAction(actionFFT);
    advSignalMenu->addAction(actionStftSpectrogram);
    advSignalMenu->addAction(actionHilbertEnvelope);
    advSignalMenu->addAction(actionPeakEnvelope);
    advSignalMenu->addAction(actionWaveletTransform);
    advSignalMenu->addAction(actionWelchPsdCoherence);
    advSignalMenu->addAction(actionFourierSelfDeconvolve);
    advSignalMenu->addSeparator();
    advSignalMenu->addAction(actionMatrixFFT2D);
    advSignalMenu->addAction(actionMatrixIFFT2D);
    advSignalMenu->addAction(actionMatrixFilter2D);

    d_quick_fit_menu = new QMenu(this);
    d_quick_fit_menu->setTitle(tr("&Quick Fit"));

    if (actionQuickLinearFit)
        d_quick_fit_menu->addAction(actionQuickLinearFit);
    d_quick_fit_menu->addAction(actionFitLinear);
    d_quick_fit_menu->addAction(actionFitApparentLinear);
    d_quick_fit_menu->addAction(actionFitYorkLinear);
    d_quick_fit_menu->addAction(actionPiecewiseLinearFit);
    d_quick_fit_menu->addAction(actionRobustLinearFit);
    d_quick_fit_menu->addAction(actionShowFitPolynomDialog);

    d_quick_fit_menu->addSeparator();

    decay = d_quick_fit_menu->addMenu(tr("Fit E&xponential Decay"));
    decay->setFont(appFont);
    decay->addAction(actionShowExpDecayDialog);
    decay->addAction(actionShowTwoExpDecayDialog);
    decay->addAction(actionShowExpDecay3Dialog);

    d_quick_fit_menu->addAction(actionFitExpGrowth);
    d_quick_fit_menu->addAction(actionFitSigmoidal);
    d_quick_fit_menu->addAction(actionFitGauss);
    d_quick_fit_menu->addAction(actionFitLorentz);

    multiPeakMenu = new QMenu(tr("Fit &Multi-Peak"), this);
    multiPeakMenu->setFont(appFont);
    multiPeakMenu->addAction(actionPeakFitDialog);
    multiPeakMenu->addSeparator();
    multiPeakMenu->addAction(actionAutoPeakGauss);
    multiPeakMenu->addAction(actionAutoPeakLorentz);
    multiPeakMenu->addAction(actionAutoPeakPseudoVoigt);
    multiPeakMenu->addSeparator();
    multiPeakMenu->addAction(actionMultiPeakGauss);
    multiPeakMenu->addAction(actionMultiPeakGaussAmp);
    multiPeakMenu->addAction(actionMultiPeakLorentz);
    multiPeakMenu->addAction(actionMultiPeakPseudoVoigt);
    multiPeakMenu->addAction(actionMultiPeakPearsonVII);

    d_quick_fit_menu->addSeparator();
    d_quick_fit_menu->addAction(actionGlobalCurveFit);
    d_quick_fit_menu->addAction(actionImplicitODRFit);
    d_quick_fit_menu->addAction(actionModelComparison);
    d_quick_fit_menu->addAction(actionCompareDatasetsFit);
    d_quick_fit_menu->addAction(actionMultipleLinearRegression);
    d_quick_fit_menu->addAction(actionLogisticRegression);
    d_quick_fit_menu->addAction(actionSurfaceFit3D);

    // Populate structured Fitting & Peaks submenus
    if (actionQuickLinearFit)
        advFitMenu->addAction(actionQuickLinearFit);
    advFitMenu->addAction(actionFitLinear);
    advFitMenu->addAction(actionFitApparentLinear);
    advFitMenu->addAction(actionFitYorkLinear);
    advFitMenu->addAction(actionPassingBablokDeming);
    advFitMenu->addAction(actionPiecewiseLinearFit);
    advFitMenu->addAction(actionRobustLinearFit);
    advFitMenu->addAction(actionShowFitPolynomDialog);
    advFitMenu->addSeparator();
    advFitMenu->addMenu(decay);
    advFitMenu->addAction(actionFitExpGrowth);
    advFitMenu->addAction(actionFitSigmoidal);
    advFitMenu->addAction(actionFitGauss);
    advFitMenu->addAction(actionFitLorentz);
    advFitMenu->addSeparator();
    advFitMenu->addAction(actionGlobalCurveFit);
    advFitMenu->addAction(actionImplicitODRFit);
    advFitMenu->addAction(actionModelComparison);
    advFitMenu->addAction(actionCompareDatasetsFit);
    advFitMenu->addAction(actionMultipleLinearRegression);
    advFitMenu->addAction(actionLogisticRegression);
    advFitMenu->addAction(actionSurfaceFit3D);
    advFitMenu->addSeparator();
    advFitMenu->addAction(actionShowFitDialog);

    peaksBaselineMenu->addAction(actionPeakAnalyzerDialog);
    peaksBaselineMenu->addAction(actionBatchPeakAnalysis);
    peaksBaselineMenu->addAction(action2DPeakAnalysis);
    peaksBaselineMenu->addAction(actionFourierSelfDeconvolve);
    peaksBaselineMenu->addSeparator();
    peaksBaselineMenu->addMenu(multiPeakMenu);

    // C6: "Quick Fit" repeated almost every item of "Fitting", and Peak Analyzer was listed
    // multiple times. Fitting and Peaks and Baseline are the single homes now.
    calcul->addAction(actionShowFitDialog);

    format = new QMenu(this);
    format->setFont(appFont);
    format->setTitle(tr("F&ormat"));
    connect(format, SIGNAL(aboutToShow()), this, SLOT(syncGraphPageAndToolbarMenuState()));

    scriptingMenu = new QMenu(this);
    scriptingMenu->setFont(appFont);
    scriptingMenu->setTitle(tr("&Scripting"));

    windowsMenu = new QMenu(this);
    windowsMenu->setFont(appFont);
    windowsMenu->setTitle(tr("&Windows"));
    connect(windowsMenu, SIGNAL(aboutToShow()), this, SLOT(windowsMenuAboutToShow()));

    help = new QMenu(this);
    help->setFont(appFont);
    help->setTitle(tr("&Help"));

    help->addAction(actionShowHelp);
    if (actionSampleGallery)
        help->addAction(actionSampleGallery);
#ifdef DYNAMIC_MANUAL_PATH
    help->addAction(actionChooseHelpFolder);
#endif
    help->addSeparator();
    help->addAction(actionHomePage);
#ifdef SEARCH_FOR_UPDATES
    help->addAction(actionCheckUpdates);
#endif
#ifdef DOWNLOAD_LINKS
    help->addAction(actionDownloadManual);
#endif
    help->addSeparator();
    help->addAction(actionHelpForums);
    help->addAction(actionHelpBugReports);
    if (actionRepairOleRegistration)
        help->addAction(actionRepairOleRegistration);
    help->addSeparator();
    help->addAction(actionAbout);

    disableActions();
}

void ApplicationWindow::initPlotDataMenu()
{
    plotDataMenu = new QMenu(this);
    plotDataMenu->setFont(appFont);
    plotDataMenu->setTitle(tr("Too&ls"));

    plotDataMenu->addAction(btnPointer);
    plotDataMenu->addAction(btnZoomIn);
    plotDataMenu->addAction(btnZoomOut);
    plotDataMenu->addAction(actionUnzoom);
    plotDataMenu->addSeparator();

    plotDataMenu->addAction(btnPicker);
    plotDataMenu->addAction(btnCursor);
    plotDataMenu->addAction(btnSelect);

    plotDataMenu->addSeparator();

    plotDataMenu->addAction(btnMovePoints);
    plotDataMenu->addAction(btnRemovePoints);
    plotDataMenu->addSeparator();

    dataMaskMenu = plotDataMenu->addMenu(tr("Data &Masking"));
    dataMaskMenu->setFont(appFont);
    dataMaskMenu->addAction(btnMaskPoints);
    dataMaskMenu->addAction(btnUnmaskPoints);
    dataMaskMenu->addSeparator();
    dataMaskMenu->addAction(actionMaskRegionROI);
    dataMaskMenu->addAction(actionUnmaskRegionROI);
    dataMaskMenu->addSeparator();
    dataMaskMenu->addAction(actionClearAllMasks);

    plotDataMenu->addSeparator();
    plotDataMenu->addAction(actionGraphDigitizer);
    plotDataMenu->addAction(actionToggleMiniToolbar);
    connect(plotDataMenu, SIGNAL(aboutToShow()), this, SLOT(syncGraphPageAndToolbarMenuState()));
}

void ApplicationWindow::initPlotMenu()
{
    plot2D = new QMenu(this);
    plot2D->setFont(appFont);
    plot2D->setTitle(tr("&Plot"));

    plot2D->addAction(actionPlotL);
    plot2D->addAction(actionPlotP);
    plot2D->addAction(actionPlotLP);

    specialPlot = plot2D->addMenu(tr("Special Line/Symb&ol"));
    specialPlot->setFont(appFont);
    specialPlot->addAction(actionPlotVerticalDropLines);
    specialPlot->addAction(actionPlotSpline);
    specialPlot->addAction(actionPlotVertSteps);
    specialPlot->addAction(actionPlotHorSteps);
    specialPlot->addSeparator();
    specialPlot->addAction(actionPlotDoubleY);
    specialPlot->addAction(actionPlotYOffsetStack);

    plot2D->addSeparator();
    plot2D->addAction(actionPlotDoubleY);
    plot2D->addAction(actionPlotYOffsetStack);
    plot2D->addAction(actionPlotTernary);
    plot2D->addSeparator();

    plot2D->addAction(actionPlotVerticalBars);
    if (actionPlotStackedBars)
        plot2D->addAction(actionPlotStackedBars);
    plot2D->addAction(actionPlotHorizontalBars);
    plot2D->addAction(actionPlotPareto);
    plot2D->addAction(actionPlotArea);
    if (actionPlotPolarData)
        plot2D->addAction(actionPlotPolarData);
    plot2D->addAction(actionPlotPie);
    plot2D->addAction(actionPlotVectXYXY);
    plot2D->addAction(actionPlotVectXYAM);
    plot2D->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActColorScatter)); // E11
    plot2D->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActDensity2D)); // E11b
    plot2D->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActPoleFigure)); // E11b

    plot2D->addSeparator();

    stat = plot2D->addMenu(tr("Statistical &Graphs"));
    stat->setFont(appFont);
    stat->addAction(actionBoxPlot);
    stat->addAction(actionPlotViolin);
    stat->addAction(actionPlotHistogram);
    stat->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActHistogramCumulative)); // E11
    stat->addAction(actionPlotStackedHistograms);
    stat->addAction(actionPlotPareto);
    stat->addAction(actionPlotQQ);

    panels = plot2D->addMenu(tr("Pa&nel"));
    panels->setFont(appFont);
    panels->addAction(actionPlot2VerticalLayers);
    panels->addAction(actionPlot2HorizontalLayers);
    panels->addAction(actionPlot4Layers);
    panels->addAction(actionPlotStackedLayers);
    panels->addSeparator();
    panels->addAction(actionPlotDoubleY);
    panels->addAction(actionPlotYOffsetStack);

    plot2D->addSeparator();

    plot3D = plot2D->addMenu(tr("3&D Plot"));
    plot3D->setFont(appFont);
    plot3D->addAction(actionPlot3DRibbon);
    plot3D->addAction(actionPlot3DBars);
    plot3D->addAction(actionPlot3DScatter);
    plot3D->addAction(actionPlot3DTrajectory);
}

void ApplicationWindow::initTableAnalysisMenu()
{
    dataMenu = new QMenu(this);
    dataMenu->setFont(appFont);
    dataMenu->setTitle(tr("Wor&ksheet Analysis"));

    dataMenu->addAction(actionShowColStatistics);
    dataMenu->addAction(actionShowRowStatistics);

    dataMenu->addSeparator();
    dataMenu->addAction(actionFFT);
    dataMenu->addSeparator();
    dataMenu->addAction(actionCorrelate);
    dataMenu->addAction(actionAutoCorrelate);
    dataMenu->addSeparator();
    dataMenu->addAction(actionConvolute);
    dataMenu->addAction(actionDeconvolute);

    dataMenu->addSeparator();
    if (materialsAnalysisMenu) {
        dataMenu->addMenu(materialsAnalysisMenu);
        dataMenu->addSeparator();
    }
    dataMenu->addAction(actionPeakAnalyzerDialog);
    dataMenu->addAction(actionShowFitDialog);
    dataMenu->addAction(actionMultipleLinearRegression);
    dataMenu->addAction(actionLogisticRegression);
    if (actionKissingerDialog)
        dataMenu->addAction(actionKissingerDialog);
    if (actionXRDAnalyzerDialog)
        dataMenu->addAction(actionXRDAnalyzerDialog);
    if (actionVaspPlotterDialog)
        dataMenu->addAction(actionVaspPlotterDialog);
    dataMenu->addAction(actionSurfaceFit3D);
    dataMenu->addAction(action2DPeakAnalysis);
    dataMenu->addSeparator();
    dataMenu->addAction(actionNormalityTest);
    dataMenu->addAction(actionOutlierTest);
    dataMenu->addAction(actionGroupComparison);
    dataMenu->addAction(actionNonparametricTests);
    dataMenu->addAction(actionPairedComparisonPlot);
    dataMenu->addAction(actionTwoWayANOVA);
    dataMenu->addAction(actionBlandAltman);
    dataMenu->addAction(actionPassingBablokDeming);
    dataMenu->addAction(actionWeibullPlot);
    dataMenu->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActLognormalProbability)); // E11
    dataMenu->addAction(ExtraPlotTypes::action(this, ExtraPlotTypes::ActGumbelProbability)); // E11
    dataMenu->addAction(actionROCCurve);
    dataMenu->addAction(actionKaplanMeier);
    dataMenu->addAction(actionCorrelationMatrix);
    dataMenu->addAction(actionClustering);
    dataMenu->addAction(actionPCABiplot);
    dataMenu->addSeparator();
    dataMenu->addAction(actionGraphDigitizer);
    if (recalcMenu) {
        dataMenu->addSeparator();
        dataMenu->addMenu(recalcMenu);
    }
}

void ApplicationWindow::customMenu(MyWidget *w)
{
    QList<QMenu *> desiredMenus;
    desiredMenus << file << edit << view << scriptingMenu;
    if (scriptingMenu && scriptingMenu->menuAction())
        scriptingMenu->menuAction()->setVisible(true);

    scriptingMenu->clear();
    if (actionOpenPythonConsole)
        scriptingMenu->addAction(actionOpenPythonConsole);
    if (actionRunPythonScript)
        scriptingMenu->addAction(actionRunPythonScript);
    if (actionOpenPythonConsole || actionRunPythonScript)
        scriptingMenu->addSeparator();
#ifdef SCRIPTING_DIALOG
    scriptingMenu->addAction(actionScriptingLang);
#endif
    scriptingMenu->addAction(actionRestartScripting);
    if (actionManageDialogThemes)
        actionManageDialogThemes->setVisible(showsLegacyUi() || showsAdvancedDomainFeatures());
#ifdef DYNAMIC_MANUAL_PATH
    if (actionChooseHelpFolder)
        actionChooseHelpFolder->setVisible(showsLegacyUi());
#endif

    // these use the same keyboard shortcut (Ctrl+Return) and should not be enabled outside Note
    actionNoteExecute->setEnabled(false);
    actionNoteExecuteAll->setEnabled(false);
    actionNoteEvaluate->setEnabled(false);

    if (w) {
        actionPrintAllPlots->setEnabled(projectHas2DPlots());
        actionPrint->setEnabled(true);
        actionCutSelection->setEnabled(true);
        actionCopySelection->setEnabled(true);
        if (actionCopyPageImage)
            actionCopyPageImage->setEnabled(w->inherits("MultiLayer"));
        actionPasteSelection->setEnabled(true);
        actionClearSelection->setEnabled(true);
        actionSaveTemplate->setEnabled(true);

        if (w->inherits("MultiLayer")) {
            desiredMenus << graph << plotDataMenu << calcul;
            if (gadgetsMenu)
                desiredMenus << gadgetsMenu;
            desiredMenus << format;

            exportPlot->setEnabled(true);
            actionShowExportASCIIDialog->setEnabled(false);
            // file->setItemEnabled (closeID,true);

            format->clear();
            format->addAction(actionShowPlotDialog);
            format->addSeparator();
            format->addAction(actionShowScaleDialog);
            format->addAction(actionShowAxisDialog);
            actionShowAxisDialog->setEnabled(true);
            format->addSeparator();
            format->addAction(actionShowGridDialog);
            format->addAction(actionShowTitleDialog);
            format->addSeparator();
            format->addAction(actionColorPaletteEditor);
            if (colorPaletteMenu)
                format->addMenu(colorPaletteMenu);
            if (aspectRatioMenu) {
                if (actionKeepAspectRatio)
                    actionKeepAspectRatio->setChecked(static_cast<MultiLayer *>(w)->keepAspectRatio());
                format->addMenu(aspectRatioMenu);
            }
            if (framePresetsMenu)
                format->addMenu(framePresetsMenu);
            if (stylePresetsMenu)
                format->addMenu(stylePresetsMenu);
            if (actionApplyStyleToMultipleGraphs)
                format->addAction(actionApplyStyleToMultipleGraphs);
            format->addSeparator();
            format->addAction(actionCopyGraphFormat);
            format->addAction(actionPasteGraphFormat);
            if (pasteFormatSpecialMenu)
                format->addMenu(pasteFormatSpecialMenu);
            format->addAction(actionBatchPlot);
            format->addAction(actionWaterfallStackOffset);
            format->addAction(actionResetStackOffsets);
            format->addSeparator();
            format->addAction(actionPageSetup);
            format->addAction(actionFitPageToLayers);
            format->addAction(actionFitLayersToPage);
            format->addAction(actionTogglePageView);
            if (actionToggleGraphInspector)
                format->addAction(actionToggleGraphInspector);
            format->addAction(actionToggleMiniToolbar);
            syncGraphPageAndToolbarMenuState();
        } else if (w->inherits("Graph3D")) {
            disableActions();

            desiredMenus << format;

            actionPrint->setEnabled(true);
            actionSaveTemplate->setEnabled(true);
            exportPlot->setEnabled(true);
            // file->setItemEnabled (closeID,true);

            format->clear();
            format->addAction(actionShowPlotDialog);
            format->addAction(actionShowScaleDialog);
            format->addAction(actionShowAxisDialog);
            format->addAction(actionShowTitleDialog);
            format->addSeparator();
            format->addAction(actionSurfaceFit3D);
            if (((Graph3D *)w)->coordStyle() == Qwt3D::NOCOORD)
                actionShowAxisDialog->setEnabled(false);
        } else if (w->inherits("Table")) {
            desiredMenus << plot2D << dataMenu;

            actionShowExportASCIIDialog->setEnabled(true);
            exportPlot->setEnabled(false);
            // file->setItemEnabled (closeID,true);

            qDeleteAll(tableMenu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
            tableMenu->clear();
            static_cast<Table *>(w)->d_future_table->fillProjectMenu(tableMenu);
            tableMenu->addSeparator();
            if (actionReimportData) {
                const bool hasSource = static_cast<Table *>(w)->hasImportSource();
                actionReimportData->setEnabled(hasSource);
                tableMenu->addAction(actionReimportData);
            }
            if (actionApplyTableTemplate)
                tableMenu->addAction(actionApplyTableTemplate);
            tableMenu->addAction(actionShowExportASCIIDialog);
            tableMenu->addSeparator();
            tableMenu->addAction(actionConvertTable);
            ModernUiTheme::upgradeMenuIcons(tableMenu);
            desiredMenus << tableMenu;
        } else if (w->inherits("Matrix")) {
            desiredMenus << plot3DMenu;

            qDeleteAll(matrixMenu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
            matrixMenu->clear();
            static_cast<Matrix *>(w)->d_future_matrix->fillProjectMenu(matrixMenu);
            matrixMenu->addSeparator();
            matrixMenu->addAction(actionInvertMatrix);
            matrixMenu->addAction(actionMatrixDeterminant);
            matrixMenu->addSeparator();
            matrixMenu->addAction(actionMatrixFFT2D);
            matrixMenu->addAction(actionMatrixIFFT2D);
            matrixMenu->addAction(actionMatrixFilter2D);
            matrixMenu->addAction(actionSurfaceFit3D);
            matrixMenu->addAction(action2DPeakAnalysis);
            matrixMenu->addSeparator();
            matrixMenu->addAction(actionConvertMatrix);
            ModernUiTheme::upgradeMenuIcons(matrixMenu);
            desiredMenus << matrixMenu;
        } else if (w->inherits("Note")) {
            actionSaveTemplate->setEnabled(false);
            actionNoteExecute->setEnabled(true);
            actionNoteExecuteAll->setEnabled(true);
            actionNoteEvaluate->setEnabled(true);
            scriptingMenu->addSeparator();
            scriptingMenu->addAction(actionNoteExecute);
            scriptingMenu->addAction(actionNoteExecuteAll);
            scriptingMenu->addAction(actionNoteEvaluate);

            actionNoteExecute->disconnect(SIGNAL(triggered()));
            actionNoteExecuteAll->disconnect(SIGNAL(triggered()));
            actionNoteEvaluate->disconnect(SIGNAL(triggered()));
            connect(actionNoteExecute, SIGNAL(triggered()), w, SLOT(execute()));
            connect(actionNoteExecuteAll, SIGNAL(triggered()), w, SLOT(executeAll()));
            connect(actionNoteEvaluate, SIGNAL(triggered()), w, SLOT(evaluate()));
        } else
            disableActions();
    } else
        disableActions();

    if (windowsMenu)
        windowsMenu->setEnabled(w != nullptr);
    desiredMenus << windowsMenu << help;

    QList<QAction *> desiredActs;
    desiredActs.reserve(desiredMenus.size());
    for (QMenu *m : desiredMenus) {
        if (m && m->menuAction())
            desiredActs << m->menuAction();
    }
    if (menuBar()->actions() != desiredActs) {
        menuBar()->clear();
        for (QMenu *m : desiredMenus) {
            if (m)
                menuBar()->addMenu(m);
        }
    }
}

void ApplicationWindow::disableActions()
{
    actionSaveTemplate->setEnabled(false);
    actionPrintAllPlots->setEnabled(false);
    actionPrint->setEnabled(false);
    actionShowExportASCIIDialog->setEnabled(false);
    exportPlot->setEnabled(false);
    // file->setItemEnabled (closeID,false);

    actionUndo->setEnabled(false);
    actionRedo->setEnabled(false);

    actionCutSelection->setEnabled(false);
    actionCopySelection->setEnabled(false);
    if (actionCopyPageImage)
        actionCopyPageImage->setEnabled(false);
    actionPasteSelection->setEnabled(d_workspace.subWindowList().isEmpty());
    actionClearSelection->setEnabled(false);
}

void ApplicationWindow::customToolBars(MyWidget *w)
{
    if (w) {
        if (!projectHas3DPlots())
            graph_3D_tools->setEnabled(false);
        if (!projectHas2DPlots())
            graph_tools->setEnabled(false);
        if (!projectHasMatrices())
            matrix_plot_tools->setEnabled(false);
        if (tableWindows().count() <= 0) {
            table_tools->setEnabled(false);
            plot_tools->setEnabled(false);
        }

        if (w->inherits("MultiLayer")) {
            graph_tools->setEnabled(true);
            graph_3D_tools->setEnabled(false);
            table_tools->setEnabled(false);
            matrix_plot_tools->setEnabled(false);

            Graph *g = static_cast<MultiLayer *>(w)->activeGraph();
            if (g) {
                dataTools->blockSignals(true);
                if (g->rangeSelectorsEnabled())
                    btnSelect->setChecked(true);
                else if (g->zoomOn())
                    btnZoomIn->setChecked(true);
                else if (g->drawArrow())
                    btnArrow->setChecked(true);
                else if (g->drawLineActive())
                    btnLine->setChecked(true);
                else if (g->activeTool() == 0)
                    btnPointer->setChecked(true);
                else
                    switch (g->activeTool()->rtti()) {
                    case PlotToolInterface::DataPicker:
                        switch (static_cast<DataPickerTool *>(g->activeTool())->mode()) {
                        case DataPickerTool::Display:
                            btnCursor->setChecked(true);
                            break;
                        case DataPickerTool::Move:
                            btnMovePoints->setChecked(true);
                            break;
                        case DataPickerTool::Remove:
                            btnRemovePoints->setChecked(true);
                            break;
                        case DataPickerTool::Mask:
                            if (btnMaskPoints)
                                btnMaskPoints->setChecked(true);
                            break;
                        case DataPickerTool::Unmask:
                            if (btnUnmaskPoints)
                                btnUnmaskPoints->setChecked(true);
                            break;
                        }
                        break;
                    case PlotToolInterface::ScreenPicker:
                        btnPicker->setChecked(true);
                        break;
                    case PlotToolInterface::ImageProfileGadget:
                        btnPointer->setChecked(true);
                        break;
                    default:
                        btnPointer->setChecked(true);
                        break;
                    }
                dataTools->blockSignals(false);
            }
            if (g && g->curves() > 0) {
                plot_tools->setEnabled(true);
                QwtPlotCurve *c = g->curve(g->curves() - 1);
                // plot tools managed by d_plot_mapper
                for (int i = 0; i <= (int)Graph::VerticalSteps; i++) {
                    QAction *a = static_cast<QAction *>(d_plot_mapper->mapping(i));
                    if (a)
                        a->setEnabled(Graph::canConvertTo(c, (Graph::CurveType)i));
                }
                // others
                actionPlotPie->setEnabled(Graph::canConvertTo(c, Graph::Pie));
                actionPlotVectXYAM->setEnabled(Graph::canConvertTo(c, Graph::VectXYAM));
                actionPlotVectXYXY->setEnabled(Graph::canConvertTo(c, Graph::VectXYXY));
                actionBoxPlot->setEnabled(Graph::canConvertTo(c, Graph::Box));
                actionPlotDoubleY->setEnabled(g->curves() >= 2);
                actionPlotYOffsetStack->setEnabled(g->curves() >= 2);
                actionPlotViolin->setEnabled(Graph::canConvertTo(c, Graph::Box));
                actionPlotPareto->setEnabled(false);
                actionPlotQQ->setEnabled(false);
                actionPlotTernary->setEnabled(false);
                // 3D plots
                actionPlot3DRibbon->setEnabled(false);
                actionPlot3DScatter->setEnabled(false);
                actionPlot3DTrajectory->setEnabled(false);
                actionPlot3DBars->setEnabled(false);
            } else
                plot_tools->setEnabled(false);
        } else if (w->inherits("Table")) {
            table_tools->clear();
            static_cast<Table *>(w)->d_future_table->fillProjectToolBar(table_tools);
            ModernUiTheme::upgradeAllWindowIcons(table_tools);
            table_tools->setEnabled(true);

            graph_tools->setEnabled(false);
            graph_3D_tools->setEnabled(false);
            matrix_plot_tools->setEnabled(false);

            plot_tools->setEnabled(true);
            // plot tools managed by d_plot_mapper
            for (int i = 0; i <= (int)Graph::VerticalSteps; i++) {
                QAction *a = static_cast<QAction *>(d_plot_mapper->mapping(i));
                if (a)
                    a->setEnabled(true);
            }
            // others
            actionPlotPie->setEnabled(true);
            actionPlotVectXYAM->setEnabled(true);
            actionPlotVectXYXY->setEnabled(true);
            actionBoxPlot->setEnabled(true);
            actionPlotDoubleY->setEnabled(true);
            actionPlotYOffsetStack->setEnabled(true);
            actionPlotViolin->setEnabled(true);
            actionPlotPareto->setEnabled(true);
            actionPlotQQ->setEnabled(true);
            actionPlotTernary->setEnabled(true);
            // 3D plots
            actionPlot3DRibbon->setEnabled(true);
            actionPlot3DScatter->setEnabled(true);
            actionPlot3DTrajectory->setEnabled(true);
            actionPlot3DBars->setEnabled(true);
        } else if (w->inherits("Matrix")) {
            graph_tools->setEnabled(false);
            graph_3D_tools->setEnabled(false);
            table_tools->setEnabled(false);
            plot_tools->setEnabled(false);
            matrix_plot_tools->setEnabled(true);
        } else if (w->inherits("Graph3D")) {
            graph_tools->setEnabled(false);
            table_tools->setEnabled(false);
            plot_tools->setEnabled(false);
            matrix_plot_tools->setEnabled(false);

            Graph3D *plot = (Graph3D *)w;
            if (plot->plotStyle() == Qwt3D::NOPLOT)
                graph_3D_tools->setEnabled(false);
            else
                graph_3D_tools->setEnabled(true);

            custom3DActions(w);
        } else if (w->inherits("Note")) {
            graph_tools->setEnabled(false);
            graph_3D_tools->setEnabled(false);
            table_tools->setEnabled(false);
            plot_tools->setEnabled(false);
            matrix_plot_tools->setEnabled(false);
        }

    } else {
        graph_tools->setEnabled(false);
        table_tools->setEnabled(false);
        plot_tools->setEnabled(false);
        graph_3D_tools->setEnabled(false);
        matrix_plot_tools->setEnabled(false);
    }
}

QMenu *ApplicationWindow::showCurveContextMenuImpl(int curveKey)
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return nullptr;

    Graph *g = static_cast<MultiLayer *>(d_workspace.activeSubWindow())->activeGraph();
    if (!g)
        return nullptr;

    const int curveIdx = g->curveIndex(curveKey);
    if (curveIdx < 0 || curveIdx >= g->curves())
        return nullptr;

    QwtPlotItem *plotIt = g->plotItem(curveIdx);
    if (!plotIt || !plotIt->isVisible())
        return nullptr;

    if (Spectrogram *sp = dynamic_cast<Spectrogram *>(plotIt)) {
        auto curveMenu = new QMenu(this);
        actionShowCurvePlotDialog->setData(curveKey);
        curveMenu->addAction(sp->title().text(), this, SLOT(showCurvePlotDialog()));
        curveMenu->addSeparator();
        curveMenu->addAction(actionShowCurveWorksheet);
        actionShowCurveWorksheet->setData(curveKey);
        curveMenu->addAction(actionShowCurvePlotDialog);
        curveMenu->addSeparator();
        curveMenu->addAction(actionRemoveCurve);
        actionRemoveCurve->setData(curveKey);
        return curveMenu;
    }

    QwtPlotCurve *qwtCurve = g->curve(curveIdx);
    PlotCurve *pc = dynamic_cast<PlotCurve *>(qwtCurve);
    DataCurve *dc = dynamic_cast<DataCurve *>(qwtCurve);
    if (!pc)
        return nullptr;

    auto curveMenu = new QMenu(this);
    actionShowCurvePlotDialog->setData(curveKey);
    curveMenu->addAction(pc->title().text(), this, SLOT(showCurvePlotDialog()));
    curveMenu->addSeparator();

    if (pc->type() != Graph::ErrorBars && pc->type() != Graph::Pie && pc->type() != Graph::Box) {
        QMenu *plotTypeMenu = curveMenu->addMenu(tr("Change &Plot Type"));
        QAction *actLine = plotTypeMenu->addAction(tr("&Line"));
        QAction *actScatter = plotTypeMenu->addAction(tr("&Scatter"));
        QAction *actLineSym = plotTypeMenu->addAction(tr("Line + &Symbol"));
        QPointer<Graph> gGuard(g);
        connect(actLine, &QAction::triggered, this, [this, gGuard, curveIdx]() {
            if (gGuard && curveIdx >= 0 && curveIdx < gGuard->curves()) {
                MultiLayer *ml = gGuard->multiLayerParent();
                const QString before = ml ? ml->captureUndoSnapshot() : QString();
                if (ml)
                    invalidateModelessGraphDialogs(ml);
                gGuard->setCurveType(curveIdx, Graph::Line);
                gGuard->updatePlot();
                if (ml)
                    ml->pushUndoSnapshot(tr("Change Plot Type"), before);
                emit modified();
            }
        });
        connect(actScatter, &QAction::triggered, this, [this, gGuard, curveIdx]() {
            if (gGuard && curveIdx >= 0 && curveIdx < gGuard->curves()) {
                MultiLayer *ml = gGuard->multiLayerParent();
                const QString before = ml ? ml->captureUndoSnapshot() : QString();
                if (ml)
                    invalidateModelessGraphDialogs(ml);
                gGuard->setCurveType(curveIdx, Graph::Scatter);
                gGuard->updatePlot();
                if (ml)
                    ml->pushUndoSnapshot(tr("Change Plot Type"), before);
                emit modified();
            }
        });
        connect(actLineSym, &QAction::triggered, this, [this, gGuard, curveIdx]() {
            if (gGuard && curveIdx >= 0 && curveIdx < gGuard->curves()) {
                MultiLayer *ml = gGuard->multiLayerParent();
                const QString before = ml ? ml->captureUndoSnapshot() : QString();
                if (ml)
                    invalidateModelessGraphDialogs(ml);
                gGuard->setCurveType(curveIdx, Graph::LineSymbols);
                gGuard->updatePlot();
                if (ml)
                    ml->pushUndoSnapshot(tr("Change Plot Type"), before);
                emit modified();
            }
        });
        curveMenu->addSeparator();
    }

    curveMenu->addAction(actionHideCurve);
    actionHideCurve->setData(curveKey);

    if (g->visibleCurves() > 1 && pc->type() == Graph::Function) {
        curveMenu->addAction(actionHideOtherCurves);
        actionHideOtherCurves->setData(curveKey);
    } else if (dc && pc->type() != Graph::Function) {
        if ((g->visibleCurves() - dc->errorBarsList().count()) > 1) {
            curveMenu->addAction(actionHideOtherCurves);
            actionHideOtherCurves->setData(curveKey);
        }
    }

    if (g->visibleCurves() != g->curves())
        curveMenu->addAction(actionShowAllCurves);
    curveMenu->addSeparator();

    if (pc->type() == Graph::Function) {
        curveMenu->addAction(actionEditFunction);
        actionEditFunction->setData(curveKey);
    } else if (dc && pc->type() != Graph::ErrorBars) {
        curveMenu->addAction(actionEditCurveRange);
        actionEditCurveRange->setData(curveKey);

        curveMenu->addAction(actionCurveFullRange);
        actionCurveFullRange->setEnabled(!dc->isFullRange());
        actionCurveFullRange->setData(curveKey);

        curveMenu->addSeparator();
    }

    int clickedTableRow = -1;
    if (dc && g->plotWidget() && g->plotWidget()->canvas() && dc->dataSize() > 0) {
        const QPoint canvasPos = g->plotWidget()->canvas()->mapFromGlobal(QCursor::pos());
        const QwtScaleMap xMap = g->plotWidget()->canvasMap(dc->xAxis());
        const QwtScaleMap yMap = g->plotWidget()->canvasMap(dc->yAxis());
        double bestDistSq = 1e30;
        int bestPt = -1;
        for (int k = 0; k < int(dc->dataSize()); ++k) {
            if (dc->isPointMasked(k))
                continue;
            const double xv = dc->x(k);
            const double yv = dc->y(k);
            if (!std::isfinite(xv) || !std::isfinite(yv))
                continue;
            const double dx = xMap.xTransform(xv) - double(canvasPos.x());
            const double dy = yMap.xTransform(yv) - double(canvasPos.y());
            const double d2 = dx * dx + dy * dy;
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                bestPt = k;
            }
        }
        if (bestPt >= 0)
            clickedTableRow = dc->tableRow(bestPt);
    }

    actionShowCurveWorksheet->setText(tr("Go to &Worksheet"));
    curveMenu->addAction(actionShowCurveWorksheet);
    actionShowCurveWorksheet->setData(curveKey);
    actionShowCurveWorksheet->setProperty("_targetRow", clickedTableRow);

    curveMenu->addAction(actionShowCurvePlotDialog);
    actionShowCurvePlotDialog->setData(curveKey);

    if (pc->type() != Graph::Function && pc->type() != Graph::ErrorBars) {
        curveMenu->addSeparator();
        curveMenu->addAction(actionFitLinear);
        curveMenu->addAction(actionShowFitDialog);
        curveMenu->addAction(actionPeakAnalyzerDialog);
        curveMenu->addAction(actionPeakFitDialog);
        if (smooth)
            curveMenu->addMenu(smooth);
        else
            curveMenu->addAction(actionSmoothSavGol);
        curveMenu->addAction(actionSavGolSmoothDerivative);
        curveMenu->addAction(actionNormalizeCurves);
        curveMenu->addAction(actionWaterfallStackOffset);
        if (dataMaskMenu) {
            curveMenu->addSeparator();
            curveMenu->addMenu(dataMaskMenu);
        }
    }

    curveMenu->addSeparator();

    curveMenu->addAction(actionRemoveCurve);
    actionRemoveCurve->setData(curveKey);
    return curveMenu;
}

void ApplicationWindow::windowsMenuAboutToShow()
{
    QList<QMdiSubWindow *> windows = d_workspace.subWindowList();
    int n = int(windows.count());
    if (!n)
        return;

    windowsMenu->clear();
    if (actionTabbedWorkspaceView) {
        actionTabbedWorkspaceView->setChecked(isTabbedWorkspaceView());
        windowsMenu->addAction(actionTabbedWorkspaceView);
        windowsMenu->addSeparator();
    }
    windowsMenu->addAction(tr("&Cascade"), this, SLOT(cascade()));
    windowsMenu->addAction(tr("&Tile"), &d_workspace, SLOT(tileSubWindows()));
    windowsMenu->addSeparator();
    windowsMenu->addAction(actionNextWindow);
    windowsMenu->addAction(actionPrevWindow);
    windowsMenu->addSeparator();
    windowsMenu->addAction(actionRename);
    windowsMenu->addAction(actionCopyWindow);
    windowsMenu->addSeparator();
    windowsMenu->addAction(actionResizeActiveWindow);
    windowsMenu->addAction(tr("&Hide Window"), this, SLOT(hideActiveWindow()));
    windowsMenu->addAction(IconLoader::load("close"), tr("Close &Window"), this,
                           SLOT(closeActiveWindow()), Qt::CTRL + Qt::Key_W);

    if (n > 0 && n < 10) {
        windowsMenu->addSeparator();
        for (int i = 0; i < n; ++i) {
            MyWidget *widget = qobject_cast<MyWidget *>(windows.at(i));
            if (!widget)
                continue;
            QAction *actId =
                    windowsMenu->addAction(widget->name(), this, SLOT(windowsMenuActivated(bool)));
            actId->setData(i);
            actId->setCheckable(true);
            actId->setChecked(d_workspace.activeSubWindow() == windows.at(i));
        }
    } else if (n >= 10) {
        windowsMenu->addSeparator();
        for (int i = 0; i < 9; ++i) {
            MyWidget *widget = qobject_cast<MyWidget *>(windows.at(i));
            if (!widget)
                continue;
            QAction *actId =
                    windowsMenu->addAction(widget->name(), this, SLOT(windowsMenuActivated(bool)));
            actId->setData(i);
            actId->setCheckable(true);
            actId->setChecked(d_workspace.activeSubWindow() == windows.at(i));
        }
        windowsMenu->addSeparator();
        windowsMenu->addAction(tr("More windows..."), this, SLOT(showMoreWindows()));
    }
}

QMenu *ApplicationWindow::showMarkerPopupMenuImpl()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return nullptr;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return nullptr;

    // P2-14: If the user right-clicked in empty outer margins of the graph (outside the plot
    // canvas and not on an outer legend box) while a marker remained selected, clear the stale
    // marker selection and open the graph context menu instead of a stale marker menu.
    if (!isNonInteractiveContext(this) && g->plotWidget() && g->plotWidget()->canvas()
        && g->underMouse()) {
        const QPoint canvasPos = g->plotWidget()->canvas()->mapFromGlobal(QCursor::pos());
        const bool inCanvas = g->plotWidget()->canvas()->rect().contains(canvasPos);
        Legend *selLeg = dynamic_cast<Legend *>(g->selectedMarkerPtr());
        const bool onOuterLegend =
                selLeg && selLeg->isVisible() && selLeg->rect().contains(canvasPos);
        if (!inCanvas && !onOuterLegend) {
            g->setSelectedMarker(-1);
            g->setCurrentSelection(Graph::GraphSelection(), true);
            auto *cm = new QMenu(this);
            buildGraphContextMenu(*cm, ml, false);
            ModernUiTheme::upgradeMenuIcons(cm);
            return cm;
        }
    }

    auto markerMenu = new QMenu(this);

    const bool isPrimaryLegend =
            (g->legend() != nullptr && g->selectedMarkerPtr() == g->legend());

    if (g->imageMarkerSelected()) {
        markerMenu->addAction(IconLoader::load("gadget-profile-2d"),
                              tr("&View Pixel Line profile"), this, SLOT(pixelLineProfile()));
        markerMenu->addAction(tr("&Intensity Matrix"), this, SLOT(intensityTable()));
        markerMenu->addSeparator();
    }

    if (g->arrowMarkerSelected()) {
        markerMenu->addAction(tr("&Properties..."), this, SLOT(showLineDialog()));
    } else if (g->imageMarkerSelected()) {
        markerMenu->addAction(tr("&Properties..."), this, SLOT(showImageDialog()));
    } else if (isPrimaryLegend) {
        markerMenu->addAction(tr("Legend &Properties..."), this, SLOT(showTextDialog()));
        QPointer<MultiLayer> mlGuard(ml);
        QPointer<Graph> gGuard(g);
        markerMenu->addAction(tr("&Reconstruct Legend (Ctrl+L)"), this,
                              [this, mlGuard, gGuard]() {
                                  if (gGuard) {
                                      const QString before = mlGuard
                                              ? mlGuard->captureUndoSnapshot()
                                              : QString();
                                      gGuard->reconstructLegend();
                                      if (mlGuard)
                                          mlGuard->pushUndoSnapshot(tr("Reconstruct Legend"),
                                                                    before);
                                      emit modified();
                                  }
                              });
        if (Legend *leg = dynamic_cast<Legend *>(g->selectedMarkerPtr())) {
            QMenu *colMenu = markerMenu->addMenu(tr("Legend &Columns"));
            for (int ncols = 1; ncols <= 4; ++ncols) {
                QAction *actCol = colMenu->addAction(
                        (ncols == 1) ? tr("&1 Column (Vertical)")
                                     : tr("&%1 Columns (Horizontal)").arg(ncols));
                actCol->setCheckable(true);
                actCol->setChecked(leg->numColumns() == ncols);
                connect(actCol, &QAction::triggered, this, [this, mlGuard, gGuard, ncols]() {
                    if (gGuard && gGuard->legend()) {
                        const QString before =
                                mlGuard ? mlGuard->captureUndoSnapshot() : QString();
                        gGuard->legend()->setNumColumns(ncols);
                        gGuard->replot();
                        if (mlGuard)
                            mlGuard->pushUndoSnapshot(tr("Change Legend Columns"), before);
                        emit modified();
                    }
                });
            }
        }
    } else {
        markerMenu->addAction(tr("Edit &Text / Properties..."), this, SLOT(showTextDialog()));
    }

    markerMenu->addSeparator();
    markerMenu->addAction(IconLoader::load("cut"), tr("&Cut"), this, SLOT(cutSelection()));
    markerMenu->addAction(IconLoader::load("copy"), tr("&Copy"), this, SLOT(copySelection()));
    markerMenu->addAction(IconLoader::load("clear"), tr("&Delete"), this,
                          SLOT(clearSelection()));
    markerMenu->addSeparator();
    markerMenu->addAction(actionUnzoom);
    markerMenu->addAction(actionCopyGraphOle);
    ModernUiTheme::upgradeMenuIcons(markerMenu);

    return markerMenu;
}

void ApplicationWindow::showMoreWindows()
{
    if (explorerWindow.isVisible()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::information(this, "ToriGraph",
                                     tr("Please use the project explorer to select a window!"));
        }
    } else
        explorerWindow.show();
}

void ApplicationWindow::windowsMenuActivated(bool checked)
{
    Q_UNUSED(checked)

    QAction *act = qobject_cast<QAction *>(sender());
    if (!act)
        return;
    int id = act->data().toInt();

    QList<QMdiSubWindow *> windows = d_workspace.subWindowList();
    MyWidget *w = qobject_cast<MyWidget *>(windows.at(id));
    if (w) {
        w->showNormal();
        w->setFocus();
        if (hidden(w)) {
            hiddenWindows.takeAt(hiddenWindows.indexOf(w));
            d_batch_hidden_set.remove(w);
            setListView(w->name(), tr("Normal"));
        }
    }
}

QMenu *ApplicationWindow::showListViewSelectionMenuImpl()
{
    QMenu *cm = new QMenu(this);
    cm->addAction(IconLoader::duotoneIcon("delete"), tr("&Delete Selection"), this,
                  SLOT(deleteSelectedItems()), Qt::Key_F8);
    ModernUiTheme::upgradeMenuIcons(cm);
    return cm;
}

QMenu *ApplicationWindow::showListViewPopupMenuImpl()
{
    QMenu *cm = new QMenu(this);
    QMenu *window = cm->addMenu(tr("New &Window"));

    window->addAction(actionNewTable);
    window->addAction(WorkbookManager::instance(this)->newWorkbookAction());
    window->addAction(actionNewMatrix);
    window->addAction(actionNewNote);
    window->addAction(actionNewGraph);
    window->addAction(actionNewFunctionPlot);
    window->addAction(actionNewSurfacePlot);
    cm->addAction(IconLoader::duotoneIcon("folder-closed"), tr("New F&older"), this,
                  SLOT(addFolder()), Qt::Key_F7);
    cm->addSeparator();
    cm->addAction(tr("Auto &Column Width"), &lv, SLOT(adjustColumns()));
    ModernUiTheme::upgradeMenuIcons(cm);
    return cm;
}

void ApplicationWindow::showWindowPopupMenu(const QPoint &p)
{
    if (isNonInteractiveContext(this))
        return;
    if (auto m = showWindowPopupMenuImpl(lv.itemAt(p)))
        m->exec(lv.mapToGlobal(p) + QPoint(0, lv.header()->height()));
}

QMenu *ApplicationWindow::showWindowPopupMenuImpl(QTreeWidgetItem *it)
{
    if (folders.isRenaming())
        return nullptr;

    if (!it)
        return showListViewPopupMenuImpl();

    int selected = 0;
    QTreeWidgetItemIterator itv(&lv);
    while (*itv) {
        if ((*itv)->isSelected())
            selected++;

        if (selected > 1)
            return showListViewSelectionMenuImpl();
        itv++;
    }

    if (auto fl = dynamic_cast<FolderListItem *>(it)) {
        current_folder = fl->folder();
        return showFolderPopupMenuImpl(fl, false);
    }

    if (auto wli = dynamic_cast<WindowListItem *>(it))
        if (auto w = wli->window())
            return showWindowMenuImpl(w);
    return nullptr;
}

void ApplicationWindow::showGraphContextMenu()
{
    if (isNonInteractiveContext(this))
        return;
    MultiLayer *plot = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!plot)
        return;
    QMenu cm(this);
    buildGraphContextMenu(cm, plot, false);
    ModernUiTheme::upgradeMenuIcons(&cm);
    cm.exec(QCursor::pos());
}

void ApplicationWindow::showLayerButtonContextMenu()
{
    if (isNonInteractiveContext(this))
        return;
    MultiLayer *plot = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!plot || !plot->activeGraph())
        return;
    QMenu cm(this);
    buildGraphContextMenu(cm, plot, true);
    ModernUiTheme::upgradeMenuIcons(&cm);
    cm.exec(QCursor::pos());
}

void ApplicationWindow::showWindowContextMenu()
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = (QWidget *)d_workspace.activeSubWindow();
    if (!w)
        return;

    QMenu cm(this);
    if (w->inherits("MultiLayer")) {
        MultiLayer *g = (MultiLayer *)w;
        if (copiedLayer) {
            cm.addAction(IconLoader::load("paste"), tr("&Paste Layer"), this,
                         SLOT(pasteSelection()));
            cm.addSeparator();
        }

        cm.addAction(actionAddLayer);
        if (g->layers() != 0) {
            cm.addAction(actionDeleteLayer);
            cm.addSeparator();
            cm.addAction(actionShowPlotGeometryDialog);
            cm.addAction(actionShowLayerDialog);
            cm.addSeparator();
        } else
            cm.addSeparator();
        cm.addAction(actionRename);
        cm.addAction(actionCopyWindow);
        cm.addAction(actionBatchPlot);
        cm.addAction(actionCopyGraphFormat);
        cm.addAction(actionPasteGraphFormat);
        if (stylePresetsMenu)
            cm.addMenu(stylePresetsMenu);
        cm.addSeparator();
        cm.addAction(actionColorPaletteEditor);
        if (colorPaletteMenu)
            cm.addMenu(colorPaletteMenu);
        if (aspectRatioMenu) {
            if (actionKeepAspectRatio)
                actionKeepAspectRatio->setChecked(g->keepAspectRatio());
            cm.addMenu(aspectRatioMenu);
        }
        cm.addSeparator();
        cm.addAction(actionUnzoom);
        cm.addAction(actionCopyGraphOle);
        if (QAction *aPptx = findChild<QAction *>(QStringLiteral("actionSendToPowerPoint")))
            cm.addAction(aPptx); // E8
        cm.addAction(IconLoader::load("copy"), tr("&Copy Page"), g, SLOT(copyAllLayers()));
        cm.addAction(tr("E&xport Page"), this, SLOT(exportGraph()));
        cm.addAction(actionPrint);
        cm.addSeparator();
        cm.addAction(actionCloseWindow);
    } else if (w->inherits("Graph3D")) {
        Graph3D *g = (Graph3D *)w;
        if (!g->hasData()) {
            QMenu *plot3D = cm.addMenu(tr("3D &Plot"));
            plot3D->addAction(actionAdd3DData);
            plot3D->addAction(tr("&Matrix..."), this, SLOT(add3DMatrixPlot()));
            plot3D->addAction(actionEditSurfacePlot);
        } else {
            if (g->getTable())
                cm.addAction(tr("Choose &Data Set..."), this, SLOT(change3DData()));
            else if (g->matrix())
                cm.addAction(tr("Choose &Matrix..."), this, SLOT(change3DMatrix()));
            else if (g->userFunction())
                cm.addAction(actionEditSurfacePlot);
            cm.addAction(IconLoader::load("clear"), tr("C&lear"), g, SLOT(clearData()));
        }

        cm.addSeparator();
        cm.addAction(actionRename);
        cm.addAction(actionCopyWindow);
        cm.addSeparator();
        cm.addAction(tr("&Copy Graph"), g, SLOT(copyImage()));
        if (QAction *aPptx = findChild<QAction *>(QStringLiteral("actionSendToPowerPoint")))
            cm.addAction(aPptx); // E8
        cm.addAction(tr("&Export"), this, SLOT(exportGraph()));
        cm.addAction(actionPrint);
        cm.addSeparator();
        cm.addAction(actionCloseWindow);
    } else if (w->inherits("Matrix")) {
        Matrix *t = (Matrix *)w;
        cm.addAction(IconLoader::load("cut"), tr("Cu&t"), t, SLOT(cutSelection()));
        cm.addAction(IconLoader::load("copy"), tr("&Copy"), t, SLOT(copySelection()));
        cm.addAction(IconLoader::load("paste"), tr("&Paste"), t, SLOT(pasteSelection()));
        cm.addSeparator();
        cm.addAction(tr("&Insert Row"), t, SLOT(insertRow()));
        cm.addAction(tr("&Insert Column"), t, SLOT(insertColumn()));
        if (t->rowsSelected()) {
            cm.addAction(IconLoader::load("close"), tr("&Delete Rows"), t,
                         SLOT(deleteSelectedRows()));
        } else if (t->columnsSelected()) {
            cm.addAction(IconLoader::load("close"), tr("&Delete Columns"), t,
                         SLOT(deleteSelectedColumns()));
        }
        cm.addAction(IconLoader::load("clear"), tr("Clea&r"), t, SLOT(clearSelection()));
    }
    cm.exec(QCursor::pos());
}

void ApplicationWindow::showWindowTitleBarMenu()
{
    if (!qobject_cast<MyWidget *>(d_workspace.activeSubWindow()))
        return;

    showWindowMenu(qobject_cast<MyWidget *>(d_workspace.activeSubWindow()));
}

void ApplicationWindow::custom3DActions(MyWidget *w)
{
    if (w && w->inherits("Graph3D")) {
        Graph3D *plot = (Graph3D *)w;
        actionAnimate->setChecked(plot->isAnimated());
        actionPerspective->setChecked(!plot->isOrthogonal());
        switch (plot->plotStyle()) {
        case FILLEDMESH:
            wireframe->setChecked(false);
            hiddenline->setChecked(false);
            polygon->setChecked(false);
            filledmesh->setChecked(true);
            pointstyle->setChecked(false);
            barstyle->setChecked(false);
            conestyle->setChecked(false);
            crossHairStyle->setChecked(false);
            break;

        case FILLED:
            wireframe->setChecked(false);
            hiddenline->setChecked(false);
            polygon->setChecked(true);
            filledmesh->setChecked(false);
            pointstyle->setChecked(false);
            barstyle->setChecked(false);
            conestyle->setChecked(false);
            crossHairStyle->setChecked(false);
            break;

        case Qwt3D::USER:
            wireframe->setChecked(false);
            hiddenline->setChecked(false);
            polygon->setChecked(false);
            filledmesh->setChecked(false);

            if (plot->pointType() == Graph3D::VerticalBars) {
                pointstyle->setChecked(false);
                conestyle->setChecked(false);
                crossHairStyle->setChecked(false);
                barstyle->setChecked(true);
            } else if (plot->pointType() == Graph3D::Dots) {
                pointstyle->setChecked(true);
                barstyle->setChecked(false);
                conestyle->setChecked(false);
                crossHairStyle->setChecked(false);
            } else if (plot->pointType() == Graph3D::HairCross) {
                pointstyle->setChecked(false);
                barstyle->setChecked(false);
                conestyle->setChecked(false);
                crossHairStyle->setChecked(true);
            } else if (plot->pointType() == Graph3D::Cones) {
                pointstyle->setChecked(false);
                barstyle->setChecked(false);
                conestyle->setChecked(true);
                crossHairStyle->setChecked(false);
            }
            break;

        case WIREFRAME:
            wireframe->setChecked(true);
            hiddenline->setChecked(false);
            polygon->setChecked(false);
            filledmesh->setChecked(false);
            pointstyle->setChecked(false);
            barstyle->setChecked(false);
            conestyle->setChecked(false);
            crossHairStyle->setChecked(false);
            break;

        case HIDDENLINE:
            wireframe->setChecked(false);
            hiddenline->setChecked(true);
            polygon->setChecked(false);
            filledmesh->setChecked(false);
            pointstyle->setChecked(false);
            barstyle->setChecked(false);
            conestyle->setChecked(false);
            crossHairStyle->setChecked(false);
            break;

        default:
            break;
        }

        switch (plot->coordStyle()) {
        case Qwt3D::NOCOORD:
            None->setChecked(true);
            Box->setChecked(false);
            Frame->setChecked(false);
            break;

        case Qwt3D::BOX:
            None->setChecked(false);
            Box->setChecked(true);
            Frame->setChecked(false);
            break;

        case Qwt3D::FRAME:
            None->setChecked(false);
            Box->setChecked(false);
            Frame->setChecked(true);
            break;
        }

        switch (plot->floorStyle()) {
        case NOFLOOR:
            floornone->setChecked(true);
            flooriso->setChecked(false);
            floordata->setChecked(false);
            break;

        case FLOORISO:
            floornone->setChecked(false);
            flooriso->setChecked(true);
            floordata->setChecked(false);
            break;

        case FLOORDATA:
            floornone->setChecked(false);
            flooriso->setChecked(false);
            floordata->setChecked(true);
            break;
        }
        custom3DGrids(plot->grids());
    }
}

void ApplicationWindow::custom3DGrids(int grids)
{
    if (Qwt3D::BACK & grids)
        back->setChecked(true);
    else
        back->setChecked(false);

    if (Qwt3D::FRONT & grids)
        front->setChecked(true);
    else
        front->setChecked(false);

    if (Qwt3D::CEIL & grids)
        ceil->setChecked(true);
    else
        ceil->setChecked(false);

    if (Qwt3D::FLOOR & grids)
        floor->setChecked(true);
    else
        floor->setChecked(false);

    if (Qwt3D::RIGHT & grids)
        right->setChecked(true);
    else
        right->setChecked(false);

    if (Qwt3D::LEFT & grids)
        left->setChecked(true);
    else
        left->setChecked(false);
}

void ApplicationWindow::initPlot3DToolBar()
{
    graph_3D_tools = new QToolBar(tr("3D Surface"), this);
    graph_3D_tools->setObjectName(
            "graph_3D_tools"); // this is needed for QMainWindow::restoreState()
    graph_3D_tools->setIconSize(QSize(20, 20));
    addToolBarBreak(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, graph_3D_tools);

    coord = new QActionGroup(this);
    Box = new QAction(coord);
    Box->setIcon(IconLoader::load("frame-box-plain"));
    Box->setCheckable(true);

    Frame = new QAction(coord);
    Frame->setIcon(IconLoader::load("frame-open-l"));
    Frame->setCheckable(true);

    None = new QAction(coord);
    None->setIcon(IconLoader::load("clear"));
    None->setCheckable(true);

    graph_3D_tools->addAction(Frame);
    graph_3D_tools->addAction(Box);
    graph_3D_tools->addAction(None);
    Box->setChecked(true);

    graph_3D_tools->addSeparator();

    // grid actions
    grids = new QActionGroup(this);
    grids->setEnabled(true);
    grids->setExclusive(false);
    front = new QAction(grids);
    front->setCheckable(true);
    front->setIcon(IconLoader::load("axis-grid"));
    back = new QAction(grids);
    back->setCheckable(true);
    back->setIcon(IconLoader::load("axis-grid"));
    right = new QAction(grids);
    right->setCheckable(true);
    right->setIcon(IconLoader::load("axis-grid"));
    left = new QAction(grids);
    left->setCheckable(true);
    left->setIcon(IconLoader::load("axis-grid"));
    ceil = new QAction(grids);
    ceil->setCheckable(true);
    ceil->setIcon(IconLoader::load("axis-grid"));
    floor = new QAction(grids);
    floor->setCheckable(true);
    floor->setIcon(IconLoader::load("axis-grid"));

    graph_3D_tools->addAction(front);
    graph_3D_tools->addAction(back);
    graph_3D_tools->addAction(right);
    graph_3D_tools->addAction(left);
    graph_3D_tools->addAction(ceil);
    graph_3D_tools->addAction(floor);

    graph_3D_tools->addSeparator();

    actionPerspective = new QAction(this);
    actionPerspective->setCheckable(true);
    actionPerspective->setIcon(IconLoader::load("plot-3d-wireframe"));
    graph_3D_tools->addAction(actionPerspective);
    actionPerspective->setChecked(!orthogonal3DPlots);
    connect(actionPerspective, SIGNAL(toggled(bool)), this, SLOT(togglePerspective(bool)));

    actionResetRotation = new QAction(this);
    actionResetRotation->setCheckable(false);
    actionResetRotation->setIcon(IconLoader::load("recalc-now"));
    graph_3D_tools->addAction(actionResetRotation);
    connect(actionResetRotation, SIGNAL(triggered()), this, SLOT(resetRotation()));

    actionFitFrame = new QAction(this);
    actionFitFrame->setCheckable(false);
    actionFitFrame->setIcon(IconLoader::load("fit-page"));
    graph_3D_tools->addAction(actionFitFrame);
    connect(actionFitFrame, SIGNAL(triggered()), this, SLOT(fitFrameToLayer()));

    graph_3D_tools->addSeparator();

    // plot style actions
    plotstyle = new QActionGroup(this);
    wireframe = new QAction(plotstyle);
    wireframe->setCheckable(true);
    wireframe->setEnabled(true);
    wireframe->setIcon(IconLoader::load("plot-3d-wireframe"));
    hiddenline = new QAction(plotstyle);
    hiddenline->setCheckable(true);
    hiddenline->setEnabled(true);
    hiddenline->setIcon(IconLoader::load("plot-3d-wireframe"));
    polygon = new QAction(plotstyle);
    polygon->setCheckable(true);
    polygon->setEnabled(true);
    polygon->setIcon(IconLoader::load("plot-3d-surface"));
    filledmesh = new QAction(plotstyle);
    filledmesh->setCheckable(true);
    filledmesh->setIcon(IconLoader::load("plot-3d-surface"));
    pointstyle = new QAction(plotstyle);
    pointstyle->setCheckable(true);
    pointstyle->setIcon(IconLoader::load("plot-3d-scatter"));

    conestyle = new QAction(plotstyle);
    conestyle->setCheckable(true);
    conestyle->setIcon(IconLoader::load("plot-3d-bars"));

    crossHairStyle = new QAction(plotstyle);
    crossHairStyle->setCheckable(true);
    crossHairStyle->setIcon(IconLoader::load("plot-3d-scatter"));

    barstyle = new QAction(plotstyle);
    barstyle->setCheckable(true);
    barstyle->setIcon(IconLoader::load("plot-bars"));

    graph_3D_tools->addAction(barstyle);
    graph_3D_tools->addAction(pointstyle);

    graph_3D_tools->addAction(conestyle);
    graph_3D_tools->addAction(crossHairStyle);
    graph_3D_tools->addSeparator();

    graph_3D_tools->addAction(wireframe);
    graph_3D_tools->addAction(hiddenline);
    graph_3D_tools->addAction(polygon);
    graph_3D_tools->addAction(filledmesh);
    filledmesh->setChecked(true);

    graph_3D_tools->addSeparator();

    // floor actions
    floorstyle = new QActionGroup(this);
    floordata = new QAction(floorstyle);
    floordata->setCheckable(true);
    floordata->setIcon(IconLoader::load("plot-heatmap"));
    flooriso = new QAction(floorstyle);
    flooriso->setCheckable(true);
    flooriso->setIcon(IconLoader::load("plot-contour"));
    floornone = new QAction(floorstyle);
    floornone->setCheckable(true);
    floornone->setIcon(IconLoader::load("frame-box-plain"));

    graph_3D_tools->addAction(floordata);
    graph_3D_tools->addAction(flooriso);
    graph_3D_tools->addAction(floornone);
    floornone->setChecked(true);

    graph_3D_tools->addSeparator();

    actionAnimate = new QAction(this);
    actionAnimate->setCheckable(true);
    actionAnimate->setIcon(IconLoader::load("python-run"));
    graph_3D_tools->addAction(actionAnimate);

    connect(actionAnimate, SIGNAL(toggled(bool)), this, SLOT(toggle3DAnimation(bool)));
    connect(coord, SIGNAL(triggered(QAction *)), this, SLOT(pickCoordSystem(QAction *)));
    connect(floorstyle, SIGNAL(triggered(QAction *)), this, SLOT(pickFloorStyle(QAction *)));
    connect(plotstyle, SIGNAL(triggered(QAction *)), this, SLOT(pickPlotStyle(QAction *)));

    connect(left, SIGNAL(triggered(bool)), this, SLOT(setLeftGrid3DPlot(bool)));
    connect(right, SIGNAL(triggered(bool)), this, SLOT(setRightGrid3DPlot(bool)));
    connect(ceil, SIGNAL(triggered(bool)), this, SLOT(setCeilGrid3DPlot(bool)));
    connect(floor, SIGNAL(triggered(bool)), this, SLOT(setFloorGrid3DPlot(bool)));
    connect(back, SIGNAL(triggered(bool)), this, SLOT(setBackGrid3DPlot(bool)));
    connect(front, SIGNAL(triggered(bool)), this, SLOT(setFrontGrid3DPlot(bool)));
}

void ApplicationWindow::createActions()
{
    actionNewProject = new QAction(IconLoader::load("file-new"), tr("New &Project"), this);
    actionNewProject->setShortcut(tr("Ctrl+N"));
    connect(actionNewProject, SIGNAL(triggered()), this, SLOT(newProject()));

    actionNewGraph = new QAction(IconLoader::load("new-graph"), tr("New &Graph"), this);
    actionNewGraph->setShortcut(tr("Ctrl+G"));
    connect(actionNewGraph, SIGNAL(triggered()), this, SLOT(newGraph()));

    actionNewNote = new QAction(IconLoader::load("new-note"), tr("New &Note / Script"), this);
    actionNewNote->setShortcut(tr("Ctrl+ALT+N"));
    connect(actionNewNote, SIGNAL(triggered()), this, SLOT(newNote()));

    actionNewTable = new QAction(IconLoader::load("new-table"), tr("New &Table"), this);
    actionNewTable->setShortcut(tr("Ctrl+T"));
    connect(actionNewTable, SIGNAL(triggered()), this, SLOT(newTable()));

    actionNewMatrix = new QAction(IconLoader::load("new-matrix"), tr("New &Matrix"), this);
    actionNewMatrix->setShortcut(tr("Ctrl+M"));
    connect(actionNewMatrix, SIGNAL(triggered()), this, SLOT(newMatrix()));

    actionNewFunctionPlot =
            new QAction(IconLoader::load("function-plot"), tr("New &Function Plot"), this);
    actionNewFunctionPlot->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(actionNewFunctionPlot, SIGNAL(triggered()), this, SLOT(functionDialog()));

    actionNewSurfacePlot =
            new QAction(IconLoader::load("plot-3d-surface"), tr("New 3D &Surface Plot"), this);
    actionNewSurfacePlot->setShortcut(tr("Ctrl+ALT+Z"));
    connect(actionNewSurfacePlot, SIGNAL(triggered()), this, SLOT(newSurfacePlot()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionOpen = new QAction(IconLoader::load("file-open"), tr("&Open") + "...", this);
    actionOpen->setShortcut(tr("Ctrl+O"));
    connect(actionOpen, SIGNAL(triggered()), this, SLOT(open()));

    actionLoadImage = new QAction(tr("Open Image as &Graph..."), this);
    actionLoadImage->setShortcut(tr("Ctrl+I"));
    connect(actionLoadImage, SIGNAL(triggered()), this, SLOT(loadImage()));

    actionImportImage = new QAction(tr("Import Image to &Matrix..."), this);
    connect(actionImportImage, SIGNAL(triggered()), this, SLOT(importImage()));

    actionSaveProject = new QAction(IconLoader::load("file-save"), tr("&Save Project"), this);
    actionSaveProject->setShortcut(tr("Ctrl+S"));
    connect(actionSaveProject, SIGNAL(triggered()), this, SLOT(saveProject()));
    savedProject();

    actionSaveProjectAs = new QAction(tr("Save Project &As..."), this);
    actionSaveProjectAs->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    connect(actionSaveProjectAs, SIGNAL(triggered()), this, SLOT(saveProjectAs()));

    actionOpenTemplate =
            new QAction(IconLoader::load("file-open"), tr("Open Temp&late..."), this);
    connect(actionOpenTemplate, SIGNAL(triggered()), this, SLOT(openTemplate()));

    actionSaveTemplate =
            new QAction(IconLoader::load("file-save-as"), tr("Save As &Template..."), this);
    connect(actionSaveTemplate, SIGNAL(triggered()), this, SLOT(saveAsTemplate()));

    actionSaveNote = new QAction(tr("Save Note As..."), this);
    connect(actionSaveNote, SIGNAL(triggered()), this, SLOT(saveNoteAs()));

    actionLoad = new QAction(IconLoader::load("import-csv"), tr("&Import ASCII..."), this);
    connect(actionLoad, SIGNAL(triggered()), this, SLOT(importASCII()));

    actionUndo = new QAction(IconLoader::load("undo"), tr("&Undo"), this);
    actionUndo->setShortcut(tr("Ctrl+Z"));
    connect(actionUndo, SIGNAL(triggered()), this, SLOT(undo()));
    actionUndo->setEnabled(false);

    actionRedo = new QAction(IconLoader::load("redo"), tr("&Redo"), this);
    actionRedo->setShortcuts(QList<QKeySequence>()
                             << QKeySequence(tr("Ctrl+Y"))
                             << QKeySequence(tr("Ctrl+Shift+Z")));
    connect(actionRedo, SIGNAL(triggered()), this, SLOT(redo()));
    actionRedo->setEnabled(false);

    actionCopyWindow = new QAction(IconLoader::load("duplicate"), tr("Duplicat&e"), this);
    connect(actionCopyWindow, SIGNAL(triggered()), this, SLOT(clone()));

    actionCutSelection = new QAction(IconLoader::load("cut"), tr("Cu&t Selection"), this);
    actionCutSelection->setShortcut(tr("Ctrl+X"));
    connect(actionCutSelection, SIGNAL(triggered()), this, SLOT(cutSelection()));

    actionCopySelection = new QAction(IconLoader::load("copy"), tr("&Copy Selection"), this);
    actionCopySelection->setShortcut(tr("Ctrl+C"));
    connect(actionCopySelection, SIGNAL(triggered()), this, SLOT(copySelection()));

    actionPasteSelection =
            new QAction(IconLoader::load("paste"), tr("P&aste Selection"), this);
    actionPasteSelection->setShortcut(tr("Ctrl+V"));
    connect(actionPasteSelection, SIGNAL(triggered()), this, SLOT(pasteSelection()));

    actionClearSelection =
            new QAction(IconLoader::load("clear"), tr("&Delete Selection"), this);
    actionClearSelection->setShortcut(tr("Del", "delete key"));
    connect(actionClearSelection, SIGNAL(triggered()), this, SLOT(clearSelection()));

    locktoolbar = new QAction(IconLoader::load("unlock"), tr("Loc&k Toolbars"), this);
    locktoolbar->setCheckable(true);
    connect(locktoolbar, SIGNAL(toggled(bool)), this, SLOT(lockToolbar(bool)));

    actionShowExplorer = explorerWindow.toggleViewAction();
    actionShowExplorer->setIcon(IconLoader::load("folder-closed"));
    actionShowExplorer->setShortcut(tr("Ctrl+E"));

    actionShowLog = logWindow.toggleViewAction();
    actionShowLog->setIcon(IconLoader::load("log-window"));

    actionShowHistory = new QAction(tr("Undo/Redo &History"), this);
    connect(actionShowHistory, SIGNAL(triggered(bool)), this, SLOT(showHistory()));

#ifdef SCRIPTING_CONSOLE
    actionShowConsole = consoleWindow.toggleViewAction();
#endif

    actionAddLayer = new QAction(IconLoader::load("layer-add"), tr("Add La&yer"), this);
    actionAddLayer->setShortcut(tr("Ctrl+Shift+L"));
    connect(actionAddLayer, SIGNAL(triggered()), this, SLOT(addLayer()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionShowLayerDialog =
            new QAction(IconLoader::load("layer-arrange"), tr("Arran&ge Layers") + "...", this);
    actionShowLayerDialog->setShortcut(tr("Ctrl+Shift+A"));
    connect(actionShowLayerDialog, SIGNAL(triggered()), this, SLOT(showLayerDialog()));

    actionAutomaticLayout =
            new QAction(IconLoader::load("layer-arrange"), tr("Automatic Layout"), this);
    connect(actionAutomaticLayout, SIGNAL(triggered()), this, SLOT(autoArrangeLayers()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionExportGraph = new QAction(tr("&Current") + "...", this);
    actionExportGraph->setShortcut(tr("Ctrl+Shift+E"));
    connect(actionExportGraph, SIGNAL(triggered()), this, SLOT(exportGraph()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionExportAllGraphs = new QAction(tr("&All") + "...", this);
    actionExportAllGraphs->setShortcut(tr("Ctrl+Shift+X"));
    connect(actionExportAllGraphs, SIGNAL(triggered()), this, SLOT(exportAllGraphs()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionExportPDF = new QAction(IconLoader::load("export-pdf"), tr("&Export PDF") + "...", this);
    actionExportPDF->setShortcut(tr("Ctrl+Alt+P"));
    connect(actionExportPDF, SIGNAL(triggered()), this, SLOT(exportPDF()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionPrint = new QAction(IconLoader::load("print"), tr("&Print") + "...", this);
    actionPrint->setShortcut(tr("Ctrl+P"));
    connect(actionPrint, SIGNAL(triggered()), this, SLOT(print()));

    actionPrintAllPlots = new QAction(tr("Print All Plo&ts"), this);
    connect(actionPrintAllPlots, SIGNAL(triggered()), this, SLOT(printAllPlots()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionShowExportASCIIDialog = new QAction(tr("E&xport ASCII") + "...", this);
    connect(actionShowExportASCIIDialog, SIGNAL(triggered()), this, SLOT(showExportASCIIDialog()));

    actionCloseAllWindows = new QAction(IconLoader::load("close"), tr("&Quit"), this);
    actionCloseAllWindows->setShortcut(tr("Ctrl+Q"));
    connect(actionCloseAllWindows, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));

    actionClearLogInfo = new QAction(tr("Clear &Log Information"), this);
    connect(actionClearLogInfo, SIGNAL(triggered()), this, SLOT(clearLogInfo()));

    actionDeleteFitTables =
            new QAction(IconLoader::load("close"), tr("Delete &Fit Tables"), this);
    connect(actionDeleteFitTables, SIGNAL(triggered()), this, SLOT(deleteFitTables()));

    // FIXME: "..." should be added before translating, but this would break translations
    actionShowPlotWizard =
            new QAction(IconLoader::load("peak-analyzer"), tr("Plot &Wizard") + "...", this);
    actionShowPlotWizard->setShortcut(tr("Ctrl+Alt+W"));
    connect(actionShowPlotWizard, SIGNAL(triggered()), this, SLOT(showPlotWizard()));

    actionShowConfigureDialog = new QAction(tr("Prefere&nces..."), this);
    connect(actionShowConfigureDialog, SIGNAL(triggered()), this, SLOT(showPreferencesDialog()));

    actionShowCurvesDialog =
            new QAction(IconLoader::load("plot-line-symbol"), tr("Add/Remove &Curve..."), this);
    actionShowCurvesDialog->setShortcut(tr("Ctrl+Shift+D"));
    connect(actionShowCurvesDialog, SIGNAL(triggered()), this, SLOT(showCurvesDialog()));

    actionAddErrorBars =
            new QAction(IconLoader::load("error-bars"), tr("Add &Error Bars..."), this);
    actionAddErrorBars->setShortcut(tr("Ctrl+B"));
    connect(actionAddErrorBars, SIGNAL(triggered()), this, SLOT(addErrorBars()));

    actionAddFunctionCurve = new QAction(IconLoader::load("formula"), tr("Add &Function..."), this);
    actionAddFunctionCurve->setShortcut(tr("Ctrl+Alt+F"));
    connect(actionAddFunctionCurve, SIGNAL(triggered()), this, SLOT(addFunctionCurve()));

    actionUnzoom = new QAction(IconLoader::load("zoom-auto"), tr("&Rescale to Show All"), this);
    actionUnzoom->setShortcut(tr("Ctrl+R"));
    connect(actionUnzoom, SIGNAL(triggered()), this, SLOT(setAutoScale()));

    actionNewLegend = new QAction(IconLoader::load("legend"), tr("&New / Reconstruct Legend"), this);
    actionNewLegend->setShortcut(tr("Ctrl+L"));
    connect(actionNewLegend, SIGNAL(triggered()), this, SLOT(newLegend()));

    actionTimeStamp = new QAction(IconLoader::load("log-window"), tr("Add Time Stamp"), this);
    actionTimeStamp->setShortcut(tr("Ctrl+ALT+T"));
    connect(actionTimeStamp, SIGNAL(triggered()), this, SLOT(addTimeStamp()));

    actionAddImage = new QAction(IconLoader::load("export-image"), tr("Add &Image"), this);
    actionAddImage->setShortcut(tr("Ctrl+Shift+G"));
    connect(actionAddImage, SIGNAL(triggered()), this, SLOT(addImage()));

    d_plot_mapper = new QSignalMapper;
    connect(d_plot_mapper, SIGNAL(mapped(int)), this, SLOT(selectPlotType(int)));

    actionPlotL = new QAction(IconLoader::load("plot-line"), tr("&Line"), this);
    actionPlotL->setObjectName(QStringLiteral("actionPlotL"));
    connect(actionPlotL, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotL, Graph::Line);

    actionPlotP = new QAction(IconLoader::load("plot-scatter"), tr("&Scatter"), this);
    actionPlotP->setObjectName(QStringLiteral("actionPlotP"));
    connect(actionPlotP, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotP, Graph::Scatter);

    actionPlotLP = new QAction(IconLoader::load("plot-line-symbol"), tr("Line + S&ymbol"), this);
    actionPlotLP->setObjectName(QStringLiteral("actionPlotLP"));
    connect(actionPlotLP, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotLP, Graph::LineSymbols);

    actionPlotVerticalDropLines =
            new QAction(IconLoader::load("plot-droplines"), tr("Vertical &Drop Lines"), this);
    actionPlotVerticalDropLines->setObjectName(QStringLiteral("actionPlotVerticalDropLines"));
    connect(actionPlotVerticalDropLines, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotVerticalDropLines, Graph::VerticalDropLines);

    actionPlotSpline = new QAction(IconLoader::load("plot-spline"), tr("&Spline"), this);
    actionPlotSpline->setObjectName(QStringLiteral("actionPlotSpline"));
    connect(actionPlotSpline, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotSpline, Graph::Spline);

    actionPlotHorSteps = new QAction(IconLoader::load("plot-steps"), tr("&Horizontal Steps"), this);
    actionPlotHorSteps->setObjectName(QStringLiteral("actionPlotHorSteps"));
    connect(actionPlotHorSteps, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotHorSteps, Graph::HorizontalSteps);

    actionPlotVertSteps =
            new QAction(IconLoader::load("plot-steps"), tr("&Vertical Steps"), this);
    actionPlotVertSteps->setObjectName(QStringLiteral("actionPlotVertSteps"));
    connect(actionPlotVertSteps, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotVertSteps, Graph::VerticalSteps);

    actionPlotVerticalBars =
            new QAction(IconLoader::load("plot-bars"), tr("&Vertical Bars"), this);
    actionPlotVerticalBars->setObjectName(QStringLiteral("actionPlotVerticalBars"));
    connect(actionPlotVerticalBars, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotVerticalBars, Graph::VerticalBars);

    actionPlotStackedBars =
            new QAction(IconLoader::load("plot-bars"), tr("Stacked &Bars"), this);
    actionPlotStackedBars->setToolTip(tr("Plot vertical stacked bar chart"));
    connect(actionPlotStackedBars, SIGNAL(triggered()), this, SLOT(plotStackedBars()));

    actionPlotPolarData =
            new QAction(IconLoader::load("plot-radar"), tr("&Polar Plot..."), this);
    actionPlotPolarData->setToolTip(tr("Plot polar (theta, radius) data from active worksheet"));
    connect(actionPlotPolarData, SIGNAL(triggered()), this, SLOT(plotPolarData()));

    actionPlotHorizontalBars =
            new QAction(IconLoader::load("plot-hbar"), tr("&Horizontal Bars"), this);
    actionPlotHorizontalBars->setObjectName(QStringLiteral("actionPlotHorizontalBars"));
    connect(actionPlotHorizontalBars, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotHorizontalBars, Graph::HorizontalBars);

    actionPlotArea = new QAction(IconLoader::load("plot-area"), tr("&Area"), this);
    actionPlotArea->setObjectName(QStringLiteral("actionPlotArea"));
    connect(actionPlotArea, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotArea, Graph::Area);

    actionPlotPie = new QAction(IconLoader::load("plot-pie"), tr("&Pie"), this);
    actionPlotPie->setObjectName(QStringLiteral("actionPlotPie"));
    connect(actionPlotPie, SIGNAL(triggered()), this, SLOT(plotPie()));

    actionPlotVectXYAM = new QAction(IconLoader::load("plot-vectors"), tr("Vectors XY&AM"), this);
    actionPlotVectXYAM->setObjectName(QStringLiteral("actionPlotVectXYAM"));
    connect(actionPlotVectXYAM, SIGNAL(triggered()), this, SLOT(plotVectXYAM()));

    actionPlotVectXYXY = new QAction(IconLoader::load("plot-vectors"), tr("Vectors &XYXY"), this);
    actionPlotVectXYXY->setObjectName(QStringLiteral("actionPlotVectXYXY"));
    connect(actionPlotVectXYXY, SIGNAL(triggered()), this, SLOT(plotVectXYXY()));

    actionPlotHistogram = new QAction(IconLoader::load("plot-histogram"), tr("&Histogram"), this);
    actionPlotHistogram->setObjectName(QStringLiteral("actionPlotHistogram"));
    connect(actionPlotHistogram, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionPlotHistogram, Graph::Histogram);

    actionPlotStackedHistograms =
            new QAction(IconLoader::load("plot-histogram"), tr("&Stacked Histogram"), this);
    connect(actionPlotStackedHistograms, SIGNAL(triggered()), this, SLOT(plotStackedHistograms()));

    actionPlot2VerticalLayers =
            new QAction(IconLoader::load("layer-arrange"), tr("&Vertical 2 Layers"), this);
    connect(actionPlot2VerticalLayers, SIGNAL(triggered()), this, SLOT(plot2VerticalLayers()));

    actionPlot2HorizontalLayers =
            new QAction(IconLoader::load("layer-arrange"), tr("&Horizontal 2 Layers"), this);
    connect(actionPlot2HorizontalLayers, SIGNAL(triggered()), this, SLOT(plot2HorizontalLayers()));

    actionPlot4Layers = new QAction(IconLoader::load("layer-arrange"), tr("&4 Layers"), this);
    connect(actionPlot4Layers, SIGNAL(triggered()), this, SLOT(plot4Layers()));

    actionPlotStackedLayers =
            new QAction(IconLoader::load("plot-waterfall"), tr("&Stacked Layers"), this);
    connect(actionPlotStackedLayers, SIGNAL(triggered()), this, SLOT(plotStackedLayers()));

    actionPlot3DRibbon = new QAction(IconLoader::load("plot-3d-ribbon"), tr("&Ribbon"), this);
    connect(actionPlot3DRibbon, SIGNAL(triggered()), this, SLOT(plot3DRibbon()));

    actionPlot3DBars = new QAction(IconLoader::load("plot-3d-bars"), tr("&Bars"), this);
    connect(actionPlot3DBars, SIGNAL(triggered()), this, SLOT(plot3DBars()));

    actionPlot3DScatter = new QAction(IconLoader::load("plot-3d-scatter"), tr("&Scatter"), this);
    connect(actionPlot3DScatter, SIGNAL(triggered()), this, SLOT(plot3DScatter()));

    actionPlot3DTrajectory =
            new QAction(IconLoader::load("plot-3d-ribbon"), tr("&Trajectory"), this);
    connect(actionPlot3DTrajectory, SIGNAL(triggered()), this, SLOT(plot3DTrajectory()));

    actionShowColStatistics =
            new QAction(IconLoader::load("col-stats"), tr("Statistics on &Columns"), this);
    connect(actionShowColStatistics, SIGNAL(triggered()), this, SLOT(showColStatistics()));

    actionShowRowStatistics =
            new QAction(IconLoader::load("row-stats"), tr("Statistics on &Rows"), this);
    connect(actionShowRowStatistics, SIGNAL(triggered()), this, SLOT(showRowStatistics()));

    actionShowIntDialog = new QAction(tr("&Integrate ..."), this);
    connect(actionShowIntDialog, SIGNAL(triggered()), this, SLOT(showIntegrationDialog()));

    actionInterpolate = new QAction(tr("Inte&rpolate ..."), this);
    connect(actionInterpolate, SIGNAL(triggered()), this, SLOT(showInterpolationDialog()));

    actionLowPassFilter = new QAction(tr("&Low Pass..."), this);
    connect(actionLowPassFilter, SIGNAL(triggered()), this, SLOT(lowPassFilterDialog()));

    actionHighPassFilter = new QAction(tr("&High Pass..."), this);
    connect(actionHighPassFilter, SIGNAL(triggered()), this, SLOT(highPassFilterDialog()));

    actionBandPassFilter = new QAction(tr("&Band Pass..."), this);
    connect(actionBandPassFilter, SIGNAL(triggered()), this, SLOT(bandPassFilterDialog()));

    actionBandBlockFilter = new QAction(tr("&Band Block..."), this);
    connect(actionBandBlockFilter, SIGNAL(triggered()), this, SLOT(bandBlockFilterDialog()));

    actionFFT = new QAction(tr("&FFT..."), this);
    connect(actionFFT, SIGNAL(triggered()), this, SLOT(showFFTDialog()));

    actionSmoothSavGol = new QAction(tr("&Savitzky-Golay..."), this);
    connect(actionSmoothSavGol, SIGNAL(triggered()), this, SLOT(showSmoothSavGolDialog()));

    actionSmoothFFT = new QAction(tr("&FFT Filter..."), this);
    connect(actionSmoothFFT, SIGNAL(triggered()), this, SLOT(showSmoothFFTDialog()));

    actionSmoothAverage = new QAction(tr("Moving Window &Average..."), this);
    connect(actionSmoothAverage, SIGNAL(triggered()), this, SLOT(showSmoothAverageDialog()));

    actionDifferentiate = new QAction(tr("&Differentiate"), this);
    connect(actionDifferentiate, SIGNAL(triggered()), this, SLOT(differentiate()));

    actionFitLinear = new QAction(tr("Fit &Linear"), this);
    connect(actionFitLinear, SIGNAL(triggered()), this, SLOT(fitLinear()));

    actionShowFitPolynomDialog = new QAction(tr("Fit &Polynomial ..."), this);
    connect(actionShowFitPolynomDialog, SIGNAL(triggered()), this, SLOT(showFitPolynomDialog()));

    actionShowExpDecayDialog = new QAction(tr("&First Order ..."), this);
    connect(actionShowExpDecayDialog, SIGNAL(triggered()), this, SLOT(showExpDecayDialog()));

    actionShowTwoExpDecayDialog = new QAction(tr("&Second Order ..."), this);
    connect(actionShowTwoExpDecayDialog, SIGNAL(triggered()), this, SLOT(showTwoExpDecayDialog()));

    actionShowExpDecay3Dialog = new QAction(tr("&Third Order ..."), this);
    connect(actionShowExpDecay3Dialog, SIGNAL(triggered()), this, SLOT(showExpDecay3Dialog()));

    actionFitExpGrowth = new QAction(tr("Fit Exponential Gro&wth ..."), this);
    connect(actionFitExpGrowth, SIGNAL(triggered()), this, SLOT(showExpGrowthDialog()));

    actionFitSigmoidal = new QAction(tr("Fit &Boltzmann (Sigmoidal)"), this);
    connect(actionFitSigmoidal, SIGNAL(triggered()), this, SLOT(fitSigmoidal()));

    actionFitGauss = new QAction(tr("Fit &Gaussian"), this);
    connect(actionFitGauss, SIGNAL(triggered()), this, SLOT(fitGauss()));

    actionFitLorentz = new QAction(tr("Fit Lorent&zian"), this);
    connect(actionFitLorentz, SIGNAL(triggered()), this, SLOT(fitLorentz()));

    actionShowFitDialog = new QAction(tr("Fit &Wizard..."), this);
    actionShowFitDialog->setShortcut(tr("Ctrl+Shift+Y"));
    connect(actionShowFitDialog, SIGNAL(triggered()), this, SLOT(showFitDialog()));

    actionShowPlotDialog = new QAction(tr("&Plot ..."), this);
    connect(actionShowPlotDialog, SIGNAL(triggered()), this, SLOT(showGeneralPlotDialog()));

    actionShowScaleDialog = new QAction(tr("&Scales..."), this);
    connect(actionShowScaleDialog, SIGNAL(triggered()), this, SLOT(showScaleDialog()));

    actionShowAxisDialog = new QAction(tr("&Axes..."), this);
    connect(actionShowAxisDialog, SIGNAL(triggered()), this, SLOT(showAxisDialog()));

    actionShowGridDialog = new QAction(tr("&Grid ..."), this);
    connect(actionShowGridDialog, SIGNAL(triggered()), this, SLOT(showGridDialog()));

    actionShowTitleDialog = new QAction(tr("&Title ..."), this);
    connect(actionShowTitleDialog, SIGNAL(triggered()), this, SLOT(showTitleDialog()));

    actionAbout = new QAction(tr("&About ToriGraph"), this);
    connect(actionAbout, SIGNAL(triggered()), this, SLOT(about()));

    actionShowHelp = new QAction(tr("&Help"), this);
    actionShowHelp->setShortcuts({ QKeySequence(Qt::Key_F1) });
    connect(actionShowHelp, SIGNAL(triggered()), this, SLOT(showHelp()));

#ifdef DYNAMIC_MANUAL_PATH
    actionChooseHelpFolder = new QAction(tr("&Choose Help Folder..."), this);
    connect(actionChooseHelpFolder, SIGNAL(triggered()), this, SLOT(chooseHelpFolder()));
#endif

    actionRename = new QAction(tr("&Rename Window"), this);
    connect(actionRename, SIGNAL(triggered()), this, SLOT(renameActiveWindow()));

    actionCloseWindow = new QAction(IconLoader::load("close"), tr("Close &Window"), this);
    actionCloseWindow->setShortcut(tr("Ctrl+W"));
    connect(actionCloseWindow, SIGNAL(triggered()), this, SLOT(closeActiveWindow()));

    actionDeleteLayer = new QAction(IconLoader::load("clear"), tr("&Remove Layer"), this);
    actionDeleteLayer->setShortcut(tr("Ctrl+Alt+R"));
    connect(actionDeleteLayer, SIGNAL(triggered()), this, SLOT(deleteLayer()));

    actionResizeActiveWindow =
            new QAction(IconLoader::load("fit-page"), tr("Window &Geometry..."), this);
    connect(actionResizeActiveWindow, SIGNAL(triggered()), this, SLOT(resizeActiveWindow()));

    actionHideActiveWindow = new QAction(tr("&Hide Window"), this);
    connect(actionHideActiveWindow, SIGNAL(triggered()), this, SLOT(hideActiveWindow()));

    actionShowMoreWindows = new QAction(tr("More windows..."), this);
    connect(actionShowMoreWindows, SIGNAL(triggered()), this, SLOT(showMoreWindows()));

    actionPixelLineProfile =
            new QAction(IconLoader::load("gadget-profile-2d"), tr("&View Pixel Line Profile"), this);
    connect(actionPixelLineProfile, SIGNAL(triggered()), this, SLOT(pixelLineProfile()));

    actionIntensityTable = new QAction(tr("&Intensity Table"), this);
    connect(actionIntensityTable, SIGNAL(triggered()), this, SLOT(intensityTable()));

    actionShowLineDialog = new QAction(tr("&Properties"), this);
    connect(actionShowLineDialog, SIGNAL(triggered()), this, SLOT(showLineDialog()));

    actionShowImageDialog = new QAction(tr("&Properties"), this);
    connect(actionShowImageDialog, SIGNAL(triggered()), this, SLOT(showImageDialog()));

    actionShowTextDialog = new QAction(tr("&Properties"), this);
    connect(actionShowTextDialog, SIGNAL(triggered()), this, SLOT(showTextDialog()));

    actionActivateWindow = new QAction(tr("&Activate Window"), this);
    connect(actionActivateWindow, SIGNAL(triggered()), this, SLOT(activateSubWindow()));

    actionMinimizeWindow = new QAction(tr("Mi&nimize Window"), this);
    connect(actionMinimizeWindow, SIGNAL(triggered()), this, SLOT(minimizeWindow()));

    actionMaximizeWindow = new QAction(tr("Ma&ximize Window"), this);
    connect(actionMaximizeWindow, SIGNAL(triggered()), this, SLOT(maximizeWindow()));

    actionResizeWindow = new QAction(IconLoader::load("fit-page"), tr("Re&size Window..."), this);
    connect(actionResizeWindow, SIGNAL(triggered()), this, SLOT(resizeWindow()));

    actionPrintWindow = new QAction(IconLoader::load("print"), tr("&Print Window"), this);
    connect(actionPrintWindow, SIGNAL(triggered()), this, SLOT(printWindow()));

    actionShowPlotGeometryDialog =
            new QAction(IconLoader::load("fit-page"), tr("&Layer Geometry"), this);
    connect(actionShowPlotGeometryDialog, SIGNAL(triggered()), this,
            SLOT(showPlotGeometryDialog()));

    actionEditSurfacePlot = new QAction(tr("&Surface..."), this);
    connect(actionEditSurfacePlot, SIGNAL(triggered()), this, SLOT(editSurfacePlot()));

    actionAdd3DData = new QAction(tr("&Data Set..."), this);
    connect(actionAdd3DData, SIGNAL(triggered()), this, SLOT(add3DData()));

    actionInvertMatrix = new QAction(tr("&Invert"), this);
    connect(actionInvertMatrix, SIGNAL(triggered()), this, SLOT(invertMatrix()));

    actionMatrixDeterminant = new QAction(tr("&Determinant"), this);
    connect(actionMatrixDeterminant, SIGNAL(triggered()), this, SLOT(matrixDeterminant()));

    actionConvertMatrix = new QAction(tr("&Convert to Table"), this);
    connect(actionConvertMatrix, SIGNAL(triggered()), this, SLOT(convertMatrixToTable()));

    actionConvertTable = new QAction(tr("Convert to &Matrix"), this);
    connect(actionConvertTable, SIGNAL(triggered()), this, SLOT(convertTableToMatrix()));

    actionPlot3DWireFrame =
            new QAction(IconLoader::load("plot-3d-wireframe"), tr("3D &Wire Frame"), this);
    connect(actionPlot3DWireFrame, SIGNAL(triggered()), this, SLOT(plot3DWireframe()));

    actionPlot3DHiddenLine =
            new QAction(IconLoader::load("plot-3d-wireframe"), tr("3D &Hidden Line"), this);
    connect(actionPlot3DHiddenLine, SIGNAL(triggered()), this, SLOT(plot3DHiddenLine()));

    actionPlot3DPolygons = new QAction(IconLoader::load("plot-3d-surface"), tr("3D &Polygons"), this);
    connect(actionPlot3DPolygons, SIGNAL(triggered()), this, SLOT(plot3DPolygons()));

    actionPlot3DWireSurface =
            new QAction(IconLoader::load("plot-3d-surface"), tr("3D Wire &Surface"), this);
    connect(actionPlot3DWireSurface, SIGNAL(triggered()), this, SLOT(plot3DWireSurface()));

    actionColorMap =
            new QAction(IconLoader::load("plot-heatmap"), tr("Contour - &Color Fill"), this);
    connect(actionColorMap, SIGNAL(triggered()), this, SLOT(plotColorMap()));

    actionContourMap = new QAction(IconLoader::load("plot-contour"), tr("Contour &Lines"), this);
    connect(actionContourMap, SIGNAL(triggered()), this, SLOT(plotContour()));

    actionGrayMap = new QAction(IconLoader::load("plot-heatmap"), tr("&Gray Scale Map"), this);
    connect(actionGrayMap, SIGNAL(triggered()), this, SLOT(plotGrayScale()));

    actionCorrelate = new QAction(tr("Co&rrelate"), this);
    connect(actionCorrelate, SIGNAL(triggered()), this, SLOT(correlate()));

    actionAutoCorrelate = new QAction(tr("&Autocorrelate"), this);
    connect(actionAutoCorrelate, SIGNAL(triggered()), this, SLOT(autoCorrelate()));

    actionConvolute = new QAction(tr("&Convolute"), this);
    connect(actionConvolute, SIGNAL(triggered()), this, SLOT(convolute()));

    actionDeconvolute = new QAction(tr("&Deconvolute"), this);
    connect(actionDeconvolute, SIGNAL(triggered()), this, SLOT(deconvolute()));

    actionTranslateHor = new QAction(tr("&Horizontal"), this);
    connect(actionTranslateHor, SIGNAL(triggered()), this, SLOT(translateCurveHor()));

    actionTranslateVert = new QAction(tr("&Vertical"), this);
    connect(actionTranslateVert, SIGNAL(triggered()), this, SLOT(translateCurveVert()));

    actionBoxPlot = new QAction(IconLoader::load("plot-box"), tr("&Box Plot"), this);
    connect(actionBoxPlot, SIGNAL(triggered()), d_plot_mapper, SLOT(map()));
    d_plot_mapper->setMapping(actionBoxPlot, Graph::Box);

    actionMultiPeakGauss = new QAction(tr("&Gaussian..."), this);
    connect(actionMultiPeakGauss, SIGNAL(triggered()), this, SLOT(fitMultiPeakGauss()));

    actionMultiPeakLorentz = new QAction(tr("&Lorentzian..."), this);
    connect(actionMultiPeakLorentz, SIGNAL(triggered()), this, SLOT(fitMultiPeakLorentz()));

    actionMultiPeakGaussAmp = new QAction(tr("Gaussian (&Amplitude)..."), this);
    connect(actionMultiPeakGaussAmp, SIGNAL(triggered()), this, SLOT(fitMultiPeakGaussAmp()));

    actionMultiPeakPseudoVoigt = new QAction(tr("&Pseudo-Voigt..."), this);
    connect(actionMultiPeakPseudoVoigt, SIGNAL(triggered()), this, SLOT(fitMultiPeakPseudoVoigt()));

    actionMultiPeakPearsonVII = new QAction(tr("Pearson &VII..."), this);
    connect(actionMultiPeakPearsonVII, SIGNAL(triggered()), this, SLOT(fitMultiPeakPearsonVII()));

    actionPeakAnalyzerDialog =
            new QAction(IconLoader::load("peak-analyzer"), tr("Peak &Analyzer..."), this);
    actionPeakAnalyzerDialog->setShortcut(tr("Ctrl+Shift+P"));
    connect(actionPeakAnalyzerDialog, SIGNAL(triggered()), this, SLOT(showPeakAnalyzerDialog()));

    actionPeakFitDialog = new QAction(tr("&Peak / Multi-Peak Fit..."), this);
    connect(actionPeakFitDialog, SIGNAL(triggered()), this, SLOT(showPeakFitDialog()));

    actionAutoPeakGauss = new QAction(tr("Auto Peak Fit (&Gaussian)"), this);
    connect(actionAutoPeakGauss, SIGNAL(triggered()), this, SLOT(fitAutoPeakGauss()));

    actionAutoPeakLorentz = new QAction(tr("Auto Peak Fit (&Lorentzian)"), this);
    connect(actionAutoPeakLorentz, SIGNAL(triggered()), this, SLOT(fitAutoPeakLorentz()));

    actionAutoPeakPseudoVoigt = new QAction(tr("Auto Peak Fit (Pseudo-&Voigt)"), this);
    connect(actionAutoPeakPseudoVoigt, SIGNAL(triggered()), this, SLOT(fitAutoPeakPseudoVoigt()));

    actionPlotDoubleY = new QAction(IconLoader::load("plot-line-symbol"), tr("&Double-Y Plot"), this);
    connect(actionPlotDoubleY, SIGNAL(triggered()), this, SLOT(plotDoubleY()));

    actionPlotYOffsetStack =
            new QAction(IconLoader::load("plot-waterfall"), tr("&Waterfall / Y-Offset Stack"), this);
    connect(actionPlotYOffsetStack, SIGNAL(triggered()), this, SLOT(plotYOffsetStack()));

    actionPlotViolin = new QAction(IconLoader::load("plot-box"), tr("&Violin Plot"), this);
    connect(actionPlotViolin, SIGNAL(triggered()), this, SLOT(plotViolin()));

    actionPlotPareto = new QAction(IconLoader::load("plot-bars"), tr("Pa&reto Chart"), this);
    connect(actionPlotPareto, SIGNAL(triggered()), this, SLOT(plotPareto()));

    actionPlotQQ = new QAction(IconLoader::load("plot-scatter"), tr("Normal &Q-Q Plot"), this);
    connect(actionPlotQQ, SIGNAL(triggered()), this, SLOT(plotQQ()));

    actionPlotTernary =
            new QAction(IconLoader::load("plot-scatter"), tr("&Ternary Plot (A-B-C)"), this);
    connect(actionPlotTernary, SIGNAL(triggered()), this, SLOT(plotTernary()));

    actionColorPaletteEditor = new QAction(tr("Color &Palette Editor..."), this);
    connect(actionColorPaletteEditor, SIGNAL(triggered()), this,
            SLOT(showColorPaletteEditorDialog()));

    actionAspect1_1 = new QAction(tr("&1:1 (Square - ISIJ)"), this);
    actionAspect1_1->setData(1.0);
    connect(actionAspect1_1, SIGNAL(triggered()), this, SLOT(applyAspectRatioByAction()));

    actionAspect4_3 = new QAction(tr("&4:3 (Standard)"), this);
    actionAspect4_3->setData(4.0 / 3.0);
    connect(actionAspect4_3, SIGNAL(triggered()), this, SLOT(applyAspectRatioByAction()));

    actionAspect16_9 = new QAction(tr("1&6:9 (Widescreen)"), this);
    actionAspect16_9->setData(16.0 / 9.0);
    connect(actionAspect16_9, SIGNAL(triggered()), this, SLOT(applyAspectRatioByAction()));

    actionAspect3_2 = new QAction(tr("&3:2 (Classic)"), this);
    actionAspect3_2->setData(1.5);
    connect(actionAspect3_2, SIGNAL(triggered()), this, SLOT(applyAspectRatioByAction()));

    actionAspectCustom = new QAction(tr("&Custom Aspect Ratio..."), this);
    actionAspectCustom->setData(0.0);
    connect(actionAspectCustom, SIGNAL(triggered()), this, SLOT(applyAspectRatioByAction()));

    actionKeepAspectRatio = new QAction(tr("&Keep Aspect Ratio on Resize"), this);
    actionKeepAspectRatio->setCheckable(true);
    actionKeepAspectRatio->setChecked(true);
    connect(actionKeepAspectRatio, SIGNAL(toggled(bool)), this, SLOT(setGraphKeepAspectRatio(bool)));

    actionTabbedWorkspaceView = new QAction(tr("Tabbed Workspace &View"), this);
    actionTabbedWorkspaceView->setCheckable(true);
    actionTabbedWorkspaceView->setChecked(false);
    connect(actionTabbedWorkspaceView, SIGNAL(toggled(bool)), this,
            SLOT(setTabbedWorkspaceView(bool)));

    actionLangJapanese = new QAction(tr("日本語 (&Japanese)"), this);
    actionLangJapanese->setCheckable(true);
    actionLangJapanese->setChecked(appLanguage.startsWith("ja"));
    connect(actionLangJapanese, SIGNAL(triggered()), this, SLOT(switchToJapanese()));

    actionLangEnglish = new QAction(tr("&English"), this);
    actionLangEnglish->setCheckable(true);
    actionLangEnglish->setChecked(!appLanguage.startsWith("ja"));
    connect(actionLangEnglish, SIGNAL(triggered()), this, SLOT(switchToEnglish()));

    actionCopyGraphOle = new QAction(IconLoader::load("copy-ole"),
                                     ToriGraphOleUi::copyForPowerPointMenuText(), this);
    actionCopyGraphOle->setToolTip(ToriGraphOleUi::copyForPowerPointToolTip());
    actionCopyGraphOle->setStatusTip(ToriGraphOleUi::copyForPowerPointToolTip());
    connect(actionCopyGraphOle, SIGNAL(triggered()), this, SLOT(copyGraphAsOleObject()));
    actionOleEmbedWholeProject = new QAction(ToriGraphOleUi::embedWholeProjectMenuText(), this);
    actionOleEmbedWholeProject->setObjectName(QStringLiteral("actionOleEmbedWholeProject"));
    actionOleEmbedWholeProject->setCheckable(true);
#ifdef Q_OS_WIN
    actionOleEmbedWholeProject->setChecked(SciDAVisOle::embedScopeSetting()
                                           == SciDAVisOle::EmbedScope::WholeProject);
#endif
    connect(actionOleEmbedWholeProject, &QAction::toggled, this,
            &ApplicationWindow::setOleEmbedWholeProject);
    actionRepairOleRegistration = new QAction(ToriGraphOleUi::repairRegistrationMenuText(), this);
    actionRepairOleRegistration->setObjectName(QStringLiteral("actionRepairOleRegistration"));
    connect(actionRepairOleRegistration, &QAction::triggered, this,
            &ApplicationWindow::repairOleRegistration);
#ifndef Q_OS_WIN
    actionOleEmbedWholeProject->setVisible(false);
    actionRepairOleRegistration->setVisible(false);
#endif

#ifdef SEARCH_FOR_UPDATES
    actionCheckUpdates =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("update-check")),
                        tr("Check for &Updates..."), this);
    actionCheckUpdates->setObjectName(QStringLiteral("actionCheckUpdates"));
    connect(actionCheckUpdates, SIGNAL(triggered()), this, SLOT(checkForUpdatesManual()));
#endif // defined SEARCH_FOR_UPDATES

    actionHomePage = new QAction(tr("&ToriGraph Homepage"), this);
    connect(actionHomePage, SIGNAL(triggered()), this, SLOT(showHomePage()));

    actionHelpForums = new QAction(tr("ToriGraph &Forums"), this);
    connect(actionHelpForums, SIGNAL(triggered()), this, SLOT(showForums()));

    actionHelpBugReports = new QAction(tr("Report a &Bug"), this);
    connect(actionHelpBugReports, SIGNAL(triggered()), this, SLOT(showBugTracker()));

#ifdef DOWNLOAD_LINKS
    actionDownloadManual = new QAction(tr("Download &Manual"), this);
    connect(actionDownloadManual, SIGNAL(triggered()), this, SLOT(downloadManual()));
#endif

#ifdef SCRIPTING_DIALOG
    actionScriptingLang = new QAction(tr("Scripting &Language"), this);
    connect(actionScriptingLang, SIGNAL(triggered()), this, SLOT(showScriptingLangDialog()));
#endif

    actionRestartScripting = new QAction(tr("&Restart Scripting"), this);
    connect(actionRestartScripting, SIGNAL(triggered()), this, SLOT(restartScriptingEnv()));

    actionNoteExecute = new QAction(tr("E&xecute"), this);
    actionNoteExecute->setShortcut(tr("Ctrl+J"));

    actionNoteExecuteAll = new QAction(tr("Execute &All"), this);
    actionNoteExecuteAll->setShortcut(tr("Ctrl+Shift+J"));

    actionNoteEvaluate = new QAction(tr("&Evaluate Expression"), this);
    actionNoteEvaluate->setShortcut(tr("Ctrl+Return"));

    actionShowCurvePlotDialog = new QAction(tr("&Plot details..."), this);
    connect(actionShowCurvePlotDialog, SIGNAL(triggered()), this, SLOT(showCurvePlotDialog()));

    actionShowCurveWorksheet = new QAction(tr("&Worksheet"), this);
    connect(actionShowCurveWorksheet, SIGNAL(triggered()), this, SLOT(showCurveWorksheet()));

    actionCurveFullRange = new QAction(tr("&Reset to Full Range"), this);
    connect(actionCurveFullRange, SIGNAL(triggered()), this, SLOT(setCurveFullRange()));

    actionEditCurveRange = new QAction(tr("Edit &Range..."), this);
    connect(actionEditCurveRange, SIGNAL(triggered()), this, SLOT(showCurveRangeDialog()));

    actionRemoveCurve = new QAction(IconLoader::load("close"), tr("&Delete"), this);
    connect(actionRemoveCurve, SIGNAL(triggered()), this, SLOT(removeCurve()));

    actionHideCurve = new QAction(tr("&Hide"), this);
    connect(actionHideCurve, SIGNAL(triggered()), this, SLOT(hideCurve()));

    actionHideOtherCurves = new QAction(tr("Hide &Other Curves"), this);
    connect(actionHideOtherCurves, SIGNAL(triggered()), this, SLOT(hideOtherCurves()));

    actionShowAllCurves = new QAction(tr("&Show All Curves"), this);
    connect(actionShowAllCurves, SIGNAL(triggered()), this, SLOT(showAllCurves()));

    actionEditFunction = new QAction(tr("&Edit Function..."), this);
    connect(actionEditFunction, SIGNAL(triggered()), this, SLOT(showFunctionDialog()));

    // Phase 3: OriginPro Interactive Gadgets
    actionGadgetVerticalCursor = new QAction(tr("&Vertical Cursor (All Layers)"), this);
    connect(actionGadgetVerticalCursor, SIGNAL(triggered()), this,
            SLOT(attachVerticalCursorGadget()));

    actionGadgetRegionStats = new QAction(tr("&Region Statistics (ROI)..."), this);
    connect(actionGadgetRegionStats, SIGNAL(triggered()), this, SLOT(attachRegionStatsGadget()));

    actionGadgetQuickFit = new QAction(tr("Quick &Fit (ROI)..."), this);
    connect(actionGadgetQuickFit, SIGNAL(triggered()), this, SLOT(attachQuickFitGadget()));

    actionGadgetQuickPeaks = new QAction(tr("Quick &Peaks (ROI)..."), this);
    connect(actionGadgetQuickPeaks, SIGNAL(triggered()), this, SLOT(attachQuickPeaksGadget()));

    actionGadgetQuickSigmoidal = new QAction(tr("Quick &Sigmoidal Fit (ROI)..."), this);
    connect(actionGadgetQuickSigmoidal, SIGNAL(triggered()), this,
            SLOT(attachQuickSigmoidalGadget()));

    actionGadgetIntegrate = new QAction(tr("&Integrate (ROI)..."), this);
    connect(actionGadgetIntegrate, SIGNAL(triggered()), this, SLOT(attachIntegrateGadget()));

    actionGadgetInterpolate = new QAction(tr("In&terpolate (ROI)..."), this);
    connect(actionGadgetInterpolate, SIGNAL(triggered()), this, SLOT(attachInterpolateGadget()));

    actionGadgetFFT = new QAction(tr("FF&T Preview (ROI)..."), this);
    connect(actionGadgetFFT, SIGNAL(triggered()), this, SLOT(attachFFTGadget()));

    actionGadgetRiseTime = new QAction(tr("Rise &Time (Pro)..."), this);
    connect(actionGadgetRiseTime, SIGNAL(triggered()), this, SLOT(attachRiseTimeGadget()));

    actionGadgetCluster = new QAction(tr("&Cluster Selector (Pro)..."), this);
    connect(actionGadgetCluster, SIGNAL(triggered()), this, SLOT(attachClusterGadget()));

    actionGadgetClear = new QAction(tr("Clear Active &Gadget / ROI"), this);
    connect(actionGadgetClear, SIGNAL(triggered()), this, SLOT(clearActiveGadget()));

    // Phase 3: Non-Destructive Data Masking
    actionMaskRegionROI = new QAction(tr("Mask &Region (ROI)"), this);
    connect(actionMaskRegionROI, SIGNAL(triggered()), this, SLOT(maskRegionGadget()));

    actionUnmaskRegionROI = new QAction(tr("Unmask Re&gion (ROI)"), this);
    connect(actionUnmaskRegionROI, SIGNAL(triggered()), this, SLOT(unmaskRegionGadget()));

    actionClearAllMasks = new QAction(tr("&Clear All Masks on Graph"), this);
    connect(actionClearAllMasks, SIGNAL(triggered()), this, SLOT(clearAllMasksOnGraph()));

    // Phase 3: Curve Mathematics
    actionSimpleCurveMath = new QAction(tr("&Simple Curve Math (A +-*/ B)..."), this);
    connect(actionSimpleCurveMath, SIGNAL(triggered()), this, SLOT(showSimpleCurveMathDialog()));

    actionSubtractStraightLine = new QAction(tr("Su&btract Straight Line..."), this);
    connect(actionSubtractStraightLine, SIGNAL(triggered()), this,
            SLOT(showSubtractStraightLineDialog()));

    actionAverageMultipleCurves =
            new QAction(tr("&Average Multiple Curves (Mean \xC2\xB1 SD)..."), this);
    connect(actionAverageMultipleCurves, SIGNAL(triggered()), this,
            SLOT(showAverageMultipleCurvesDialog()));

    actionNormalizeCurves = new QAction(tr("&Normalize Curves..."), this);
    connect(actionNormalizeCurves, SIGNAL(triggered()), this, SLOT(showNormalizeCurvesDialog()));

    actionCurveIntersections = new QAction(tr("Curve &Intersections && X/Y Lookup..."), this);
    connect(actionCurveIntersections, SIGNAL(triggered()), this,
            SLOT(showCurveIntersectionsDialog()));

    actionPolygonLoopArea = new QAction(tr("Calculate &Polygon / Loop Area"), this);
    connect(actionPolygonLoopArea, SIGNAL(triggered()), this, SLOT(calculatePolygonLoopArea()));

    // Phase 3: Advanced Fitting, Surface Fit & Batch / 2D Peaks
    actionFitApparentLinear = new QAction(tr("&Apparent Linear Fit (Log Axes)"), this);
    connect(actionFitApparentLinear, SIGNAL(triggered()), this, SLOT(fitApparentLinear()));

    actionFitYorkLinear = new QAction(tr("Linear Fit with X && Y Errors (&York)..."), this);
    connect(actionFitYorkLinear, SIGNAL(triggered()), this, SLOT(showYorkLinearFitDialog()));

    actionGlobalCurveFit = new QAction(tr("&Global Curve Fit (Shared Parameters)..."), this);
    connect(actionGlobalCurveFit, SIGNAL(triggered()), this, SLOT(showGlobalCurveFitDialog()));

    actionImplicitODRFit = new QAction(tr("&Implicit / ODR Curve Fit..."), this);
    connect(actionImplicitODRFit, SIGNAL(triggered()), this, SLOT(showImplicitODRFitDialog()));

    actionModelComparison = new QAction(tr("&Compare && Rank Fit Models (AIC/BIC)..."), this);
    connect(actionModelComparison, SIGNAL(triggered()), this, SLOT(showModelComparisonDialog()));

    actionSurfaceFit3D = new QAction(tr("3D &Surface Fit (XYZ / Matrix)..."), this);
    connect(actionSurfaceFit3D, SIGNAL(triggered()), this, SLOT(showSurfaceFit3DDialog()));

    actionBatchPeakAnalysis = new QAction(tr("&Batch Peak Analysis (All Curves)..."), this);
    connect(actionBatchPeakAnalysis, SIGNAL(triggered()), this, SLOT(showBatchPeakAnalysisDialog()));

    action2DPeakAnalysis = new QAction(tr("&2D Peak Analysis (Matrix / Image)..."), this);
    connect(action2DPeakAnalysis, SIGNAL(triggered()), this, SLOT(show2DPeakAnalysisDialog()));

    // Phase 3: Advanced Signal Processing & Smoothing
    actionSmoothLOWESS = new QAction(tr("&LOWESS Smooth..."), this);
    connect(actionSmoothLOWESS, SIGNAL(triggered()), this, SLOT(showSmoothLOWESSDialog()));

    actionSmoothWhittaker = new QAction(tr("&Whittaker Smooth..."), this);
    connect(actionSmoothWhittaker, SIGNAL(triggered()), this, SLOT(showSmoothWhittakerDialog()));

    actionSmoothMedian = new QAction(tr("M&edian Filter Smooth..."), this);
    connect(actionSmoothMedian, SIGNAL(triggered()), this, SLOT(showSmoothMedianDialog()));

    actionIirDigitalFilter =
            new QAction(tr("&IIR Digital Filter (Butterworth/Chebyshev/Bessel)..."), this);
    connect(actionIirDigitalFilter, SIGNAL(triggered()), this, SLOT(showIirDigitalFilterDialog()));

    actionStftSpectrogram = new QAction(tr("&Short-Time Fourier Transform (STFT)..."), this);
    connect(actionStftSpectrogram, SIGNAL(triggered()), this, SLOT(showStftSpectrogramDialog()));

    actionHilbertEnvelope = new QAction(tr("&Hilbert Transform (Envelope)..."), this);
    connect(actionHilbertEnvelope, SIGNAL(triggered()), this, SLOT(showHilbertEnvelopeDialog()));

    actionWaveletTransform = new QAction(tr("&Wavelet Transform (CWT / DWT Denoise)..."), this);
    connect(actionWaveletTransform, SIGNAL(triggered()), this, SLOT(showWaveletTransformDialog()));

    actionWelchPsdCoherence = new QAction(tr("Welch &PSD && Cross-Coherence..."), this);
    connect(actionWelchPsdCoherence, SIGNAL(triggered()), this, SLOT(showWelchPsdCoherenceDialog()));

    actionMatrixFFT2D = new QAction(tr("2D &FFT (Matrix)"), this);
    connect(actionMatrixFFT2D, SIGNAL(triggered()), this, SLOT(runMatrixFFT2D()));

    actionMatrixIFFT2D = new QAction(tr("2D &IFFT (Inverse Matrix FFT)"), this);
    connect(actionMatrixIFFT2D, SIGNAL(triggered()), this, SLOT(runMatrixIFFT2D()));

    // Phase 3: Graph Statistics
    actionConfidenceEllipse = new QAction(tr("2D &Confidence / Prediction Ellipse..."), this);
    connect(actionConfidenceEllipse, SIGNAL(triggered()), this,
            SLOT(showConfidenceEllipseDialog()));

    actionGroupComparison =
            new QAction(tr("&t-Test / One-Way ANOVA + Significance Brackets..."), this);
    connect(actionGroupComparison, SIGNAL(triggered()), this, SLOT(showGroupComparisonDialog()));

    actionROCCurve = new QAction(tr("&ROC Curve (AUC)..."), this);
    connect(actionROCCurve, SIGNAL(triggered()), this, SLOT(showROCCurveDialog()));

    actionKaplanMeier = new QAction(tr("&Kaplan-Meier Survival Analysis..."), this);
    connect(actionKaplanMeier, SIGNAL(triggered()), this, SLOT(showKaplanMeierDialog()));

    actionPCABiplot = new QAction(tr("&Principal Component Analysis (PCA Biplot)..."), this);
    connect(actionPCABiplot, SIGNAL(triggered()), this, SLOT(showPCABiplotDialog()));

    // Phase 3: Dynamic Recalculation ("Green Lock") & Dialog Themes
    recalcModeGroup = new QActionGroup(this);
    recalcModeGroup->setExclusive(true);

    actionRecalcAuto = new QAction(tr("&Auto Recalculate (Green Lock)"), this);
    actionRecalcAuto->setCheckable(true);
    actionRecalcAuto->setChecked(true);
    actionRecalcAuto->setActionGroup(recalcModeGroup);
    connect(actionRecalcAuto, SIGNAL(triggered()), this, SLOT(setRecalcModeAuto()));

    actionRecalcManual = new QAction(tr("&Manual Recalculate"), this);
    actionRecalcManual->setCheckable(true);
    actionRecalcManual->setActionGroup(recalcModeGroup);
    connect(actionRecalcManual, SIGNAL(triggered()), this, SLOT(setRecalcModeManual()));

    actionRecalcDisabled = new QAction(tr("&Disable Recalculation"), this);
    actionRecalcDisabled->setCheckable(true);
    actionRecalcDisabled->setActionGroup(recalcModeGroup);
    connect(actionRecalcDisabled, SIGNAL(triggered()), this, SLOT(setRecalcModeDisabled()));

    actionRecalculateNow =
            new QAction(IconLoader::load("lock"), tr("&Recalculate Pending Links Now"), this);
    actionRecalculateNow->setShortcut(tr("Ctrl+Shift+U"));
    connect(actionRecalculateNow, SIGNAL(triggered()), this, SLOT(recalculatePendingLinksNow()));

    actionRepeatLinearFitAll = new QAction(tr("Repeat &Linear Fit for All Curves"), this);
    connect(actionRepeatLinearFitAll, SIGNAL(triggered()), this,
            SLOT(repeatLinearFitForAllCurves()));

    actionRepeatNormalizeAll = new QAction(tr("Repeat &Normalization for All Curves"), this);
    connect(actionRepeatNormalizeAll, SIGNAL(triggered()), this,
            SLOT(repeatNormalizationForAllCurves()));

    actionManageDialogThemes = new QAction(tr("Manage Dialog &Themes..."), this);
    connect(actionManageDialogThemes, SIGNAL(triggered()), this, SLOT(showManageDialogThemes()));

    // Phase 4: OriginPro Page / Layer / Frame / Mini-Toolbar UX
    actionPageSetup = new QAction(tr("Page &Setup..."), this);
    connect(actionPageSetup, SIGNAL(triggered()), this, SLOT(showPageSetupDialog()));

    actionFitPageToLayers = new QAction(tr("Fit &Page to Layers"), this);
    connect(actionFitPageToLayers, SIGNAL(triggered()), this, SLOT(fitPageToLayers()));

    actionFitLayersToPage = new QAction(tr("Fit &Layers to Page"), this);
    connect(actionFitLayersToPage, SIGNAL(triggered()), this, SLOT(fitLayersToPage()));

    actionTogglePageView = new QAction(tr("Toggle Page &View"), this);
    actionTogglePageView->setCheckable(true);
    actionTogglePageView->setChecked(false);
    connect(actionTogglePageView, SIGNAL(triggered()), this, SLOT(togglePageViewMode()));

    actionAddLinkedLayer = new QAction(tr("Add Linked Top-&X / Right-Y Layer"), this);
    connect(actionAddLinkedLayer, SIGNAL(triggered()), this, SLOT(addLinkedTopRightLayer()));

    actionAddInsetLayer = new QAction(tr("Add &Zoom Inset Layer"), this);
    connect(actionAddInsetLayer, SIGNAL(triggered()), this, SLOT(addZoomInsetLayer()));

    actionFrameBoxInward = new QAction(tr("&Box Frame (Inward Ticks)"), this);
    connect(actionFrameBoxInward, SIGNAL(triggered()), this, SLOT(applyFrameBoxInward()));

    actionFrameBoxPlain = new QAction(tr("Box Frame (&Plain Top/Right)"), this);
    connect(actionFrameBoxPlain, SIGNAL(triggered()), this, SLOT(applyFrameBoxPlain()));

    actionFrameOpenL = new QAction(tr("&Open L-Frame"), this);
    connect(actionFrameOpenL, SIGNAL(triggered()), this, SLOT(applyFrameOpenL()));

    actionFrameZeroCrossed = new QAction(tr("&Zero-Crossed Axes"), this);
    connect(actionFrameZeroCrossed, SIGNAL(triggered()), this, SLOT(applyFrameZeroCrossed()));

    actionToggleMiniToolbar = new QAction(tr("Toggle Graph Mini-Tool&bar"), this);
    actionToggleMiniToolbar->setCheckable(true);
    actionToggleMiniToolbar->setChecked(GraphMiniToolbar::isMiniToolbarEnabled());
    connect(actionToggleMiniToolbar, SIGNAL(toggled(bool)), this,
            SLOT(toggleGraphMiniToolbar(bool)));

    actionToggleGraphInspector =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("inspector")),
                        tr("&Graph Inspector Dock"), this);
    actionToggleGraphInspector->setObjectName(QStringLiteral("actionToggleGraphInspector"));
    actionToggleGraphInspector->setCheckable(true);
    actionToggleGraphInspector->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_I));
    actionToggleGraphInspector->setShortcutContext(Qt::ApplicationShortcut);
    actionToggleGraphInspector->setToolTip(
            tr("Toggle Graph Inspector Dock (Ctrl+Shift+I)"));
    connect(actionToggleGraphInspector, &QAction::triggered, this,
            [this](bool) { toggleGraphInspectorDock(); });
    addAction(actionToggleGraphInspector);

    // Phase 4: Additional Gadgets
    actionGadgetDifferentiate = new QAction(tr("&Differentiate Gadget..."), this);
    connect(actionGadgetDifferentiate, SIGNAL(triggered()), this,
            SLOT(attachDifferentiateGadget()));

    actionGadgetIntersect = new QAction(tr("&Intersect Gadget..."), this);
    connect(actionGadgetIntersect, SIGNAL(triggered()), this, SLOT(attachIntersectGadget()));

    actionGadgetIntegrate2D = new QAction(tr("&2D Integration Gadget (Matrix/Contour)..."), this);
    connect(actionGadgetIntegrate2D, SIGNAL(triggered()), this, SLOT(attachIntegrate2DGadget()));

    // Phase 4: Piecewise, Robust & Quantile, Compare Datasets, MLR
    actionPiecewiseLinearFit = new QAction(tr("&Piecewise Linear Fit (Breakpoint)..."), this);
    connect(actionPiecewiseLinearFit, SIGNAL(triggered()), this,
            SLOT(showPiecewiseLinearFitDialog()));

    actionRobustLinearFit = new QAction(tr("&Robust && Quantile Linear Fit..."), this);
    connect(actionRobustLinearFit, SIGNAL(triggered()), this, SLOT(showRobustLinearFitDialog()));

    actionCompareDatasetsFit = new QAction(tr("Compare &Datasets (F-Test / AIC)..."), this);
    connect(actionCompareDatasetsFit, SIGNAL(triggered()), this,
            SLOT(showCompareDatasetsFitDialog()));

    actionMultipleLinearRegression = new QAction(tr("&Multiple Linear Regression (MLR)..."), this);
    connect(actionMultipleLinearRegression, SIGNAL(triggered()), this,
            SLOT(showMultipleLinearRegressionDialog()));

    // Phase 4: Signal Processing Additions
    actionSavGolSmoothDerivative = new QAction(tr("Savitzky-Golay Smooth &Derivative..."), this);
    connect(actionSavGolSmoothDerivative, SIGNAL(triggered()), this,
            SLOT(showSavGolSmoothDerivativeDialog()));

    actionPeakEnvelope = new QAction(tr("Peak &Envelope (Upper / Lower)..."), this);
    connect(actionPeakEnvelope, SIGNAL(triggered()), this, SLOT(showPeakEnvelopeAnalysisDialog()));

    actionDecimateResample = new QAction(tr("&Decimate / Anti-Alias Resample..."), this);
    connect(actionDecimateResample, SIGNAL(triggered()), this, SLOT(showDecimateResampleDialog()));

    // Phase 4: Normality Test, Bland-Altman, Two-Way ANOVA & Graph Digitizer
    actionNormalityTest = new QAction(tr("&Normality Test (Shapiro-Wilk / KS / AD)..."), this);
    connect(actionNormalityTest, SIGNAL(triggered()), this, SLOT(showNormalityTestDialog()));

    actionBlandAltman = new QAction(tr("&Bland-Altman Method Comparison..."), this);
    connect(actionBlandAltman, SIGNAL(triggered()), this, SLOT(showBlandAltmanPlotDialog()));

    actionTwoWayANOVA = new QAction(tr("T&wo-Way ANOVA && Interaction Plot..."), this);
    connect(actionTwoWayANOVA, SIGNAL(triggered()), this, SLOT(showTwoWayANOVAPlotDialog()));

    actionGraphDigitizer = new QAction(tr("&Digitizer (Extract Data from Image)..."), this);
    connect(actionGraphDigitizer, SIGNAL(triggered()), this, SLOT(showGraphDigitizerDialog()));

    // Phase 5: OriginPro Format Copy/Paste, Batch Plotting, Style Presets, Delta Cursor, Waterfall & Analysis
    actionCopyGraphFormat = new QAction(IconLoader::load("format-copy"), tr("Copy For&mat"), this);
    actionCopyGraphFormat->setShortcut(tr("Ctrl+Shift+C"));
    actionCopyGraphFormat->setToolTip(tr("Copy graph format (fonts, axes, ticks, page size, curve widths, reference lines)"));
    connect(actionCopyGraphFormat, SIGNAL(triggered()), this, SLOT(copyGraphFormat()));

    actionPasteGraphFormat = new QAction(IconLoader::load("format-paste"), tr("Paste F&ormat"), this);
    actionPasteGraphFormat->setShortcut(tr("Ctrl+Shift+V"));
    actionPasteGraphFormat->setToolTip(tr("Paste copied graph style (fonts, colors, symbols, frame & ticks) onto active graph"));
    connect(actionPasteGraphFormat, SIGNAL(triggered()), this, SLOT(pasteGraphFormat()));

    actionPasteFormatFonts = new QAction(tr("Paste &Fonts Only"), this);
    actionPasteFormatFonts->setToolTip(tr("Paste only font family, sizes, weights, and alignments onto active graph"));
    connect(actionPasteFormatFonts, SIGNAL(triggered()), this, SLOT(pasteGraphFormatFontsOnly()));

    actionPasteFormatColors = new QAction(tr("Paste &Colors && Symbols Only"), this);
    actionPasteFormatColors->setToolTip(tr("Paste only curve pens, symbol styles/sizes, and colors onto active graph"));
    connect(actionPasteFormatColors, SIGNAL(triggered()), this, SLOT(pasteGraphFormatColorsOnly()));

    actionPasteFormatScales = new QAction(tr("Paste &Axis Scales Only"), this);
    actionPasteFormatScales->setToolTip(tr("Paste axis min/max ranges, tick steps, log10 scale, and numeric formats"));
    connect(actionPasteFormatScales, SIGNAL(triggered()), this, SLOT(pasteGraphFormatAxisScalesOnly()));

    actionPasteFormatFolder = new QAction(tr("Paste Format to &All Graphs in Folder"), this);
    actionPasteFormatFolder->setToolTip(tr("Apply copied graph format to every 2D graph window in the current folder"));
    connect(actionPasteFormatFolder, SIGNAL(triggered()), this, SLOT(pasteGraphFormatToFolder()));

    actionMergeGraphs = new QAction(IconLoader::duotoneIcon(QStringLiteral("split-view"), 16, false),
                                    tr("&Merge Graph Windows..."), this);
    actionMergeGraphs->setObjectName(QStringLiteral("actionMergeGraphs"));
    actionMergeGraphs->setToolTip(tr("Merge 2 or more open graph windows into a publication multi-panel figure (a)(b)(c)(d)"));
    connect(actionMergeGraphs, SIGNAL(triggered()), this, SLOT(mergeOpenGraphsDialog()));

    actionBatchPlot = new QAction(IconLoader::load("duplicate"), tr("Duplicate (Batc&h Plotting)..."), this);
    actionBatchPlot->setToolTip(tr("Batch-clone graph across multiple worksheets or Y columns"));
    connect(actionBatchPlot, SIGNAL(triggered()), this, SLOT(showBatchPlotDialog()));

    actionStyleISIJ = new QAction(tr("&ISIJ Journal (85 mm, 600 DPI)"), this);
    actionStyleISIJ->setToolTip(tr("Apply 1-column ISIJ / Metallurgy journal style (85 mm, 600 DPI, inward ticks, Arial)"));
    connect(actionStyleISIJ, SIGNAL(triggered()), this, SLOT(applyStyleISIJJournal()));

    actionStyleSlide = new QAction(tr("&Lab Slide (18 pt @ 140%, 83.3 mm)"), this);
    actionStyleSlide->setToolTip(
            tr("Apply lab presentation slide style (83.3 mm, 12.9 pt Arial -> 18 pt at 140% in PowerPoint)"));
    connect(actionStyleSlide, SIGNAL(triggered()), this, SLOT(applyStylePresentationSlide()));

    actionApplyStyleToMultipleGraphs =
            new QAction(tr("Apply Style Preset to &Multiple Graphs..."), this);
    connect(actionApplyStyleToMultipleGraphs, SIGNAL(triggered()), this,
            SLOT(showApplyStylePresetToMultipleGraphsDialog()));

    actionApplyTableTemplate =
            new QAction(IconLoader::load("file-open"), tr("Plot Table Using &Template..."), this);
    connect(actionApplyTableTemplate, SIGNAL(triggered()), this,
            SLOT(applyTemplateToActiveTable()));

    actionReimportData =
            new QAction(IconLoader::load("import-csv"), tr("Re-import &Source Data"), this);
    connect(actionReimportData, SIGNAL(triggered()), this, SLOT(reimportActiveTable()));

    actionQuickLinearFit =
            new QAction(tr("Quick &Linear Fit (No Dialog)"), this);
    connect(actionQuickLinearFit, SIGNAL(triggered()), this, SLOT(quickLinearFitSelection()));

    actionGadgetDeltaCursor = new QAction(IconLoader::load("gadget-delta"), tr("&Delta Cursor (C1-C2)..."), this);
    actionGadgetDeltaCursor->setToolTip(tr("Dual-crosshair Delta Cursor (dx, dy, slope, ratio, area, RMS)"));
    connect(actionGadgetDeltaCursor, SIGNAL(triggered()), this, SLOT(attachDeltaCursorGadget()));

    actionLogisticRegression = new QAction(tr("Binary &Logistic Regression..."), this);
    connect(actionLogisticRegression, SIGNAL(triggered()), this, SLOT(showLogisticRegressionDialog()));

    actionTensileYield = new QAction(tr("&Tensile / 0.2% Yield Analysis..."), this);
    actionTensileYield->setObjectName(QStringLiteral("actionTensileYieldDialog"));
    actionTensileYield->setToolTip(
            tr("Tensile stress-strain analysis: Young's modulus E, 0.2% proof stress, UTS & True Stress-Strain"));
    connect(actionTensileYield, SIGNAL(triggered()), this, SLOT(showTensileYieldDialog()));

    actionArrheniusPlot = new QAction(tr("&Arrhenius Plot (Activation Energy)..."), this);
    connect(actionArrheniusPlot, SIGNAL(triggered()), this, SLOT(showArrheniusPlotDialog()));

    actionTaucPlot = new QAction(tr("&Tauc Plot (Optical Bandgap)..."), this);
    connect(actionTaucPlot, SIGNAL(triggered()), this, SLOT(showTaucPlotDialog()));

    actionFillBetweenCurves = new QAction(tr("&Fill Area Between Curves..."), this);
    connect(actionFillBetweenCurves, SIGNAL(triggered()), this, SLOT(showFillBetweenCurvesDialog()));

    actionWaterfallStackOffset = new QAction(tr("&Waterfall / Stack Offset..."), this);
    connect(actionWaterfallStackOffset, SIGNAL(triggered()), this, SLOT(showStackOffsetsDialog()));

    actionResetStackOffsets = new QAction(tr("Reset Stac&k Offsets"), this);
    connect(actionResetStackOffsets, SIGNAL(triggered()), this, SLOT(resetActiveGraphStackOffsets()));

    actionWeibullPlot = new QAction(tr("&Weibull Probability Plot..."), this);
    connect(actionWeibullPlot, SIGNAL(triggered()), this, SLOT(showWeibullPlotDialog()));

    actionOutlierTest = new QAction(tr("&Outlier Test (Grubbs / ESD / Dixon)..."), this);
    connect(actionOutlierTest, SIGNAL(triggered()), this, SLOT(showOutlierTestDialog()));

    actionNonparametricTests = new QAction(tr("&Nonparametric Tests (Mann-Whitney / Kruskal-Wallis / Auto)..."), this);
    connect(actionNonparametricTests, SIGNAL(triggered()), this, SLOT(showNonparametricTestsDialog()));

    actionPairedComparisonPlot = new QAction(tr("&Paired Comparison Plot (Before-After)..."), this);
    connect(actionPairedComparisonPlot, SIGNAL(triggered()), this, SLOT(showPairedComparisonPlotDialog()));

    actionCorrelationMatrix = new QAction(tr("&Correlation Matrix Heatmap..."), this);
    connect(actionCorrelationMatrix, SIGNAL(triggered()), this, SLOT(showCorrelationMatrixDialog()));

    actionClustering = new QAction(tr("&Clustering (K-Means / Dendrogram)..."), this);
    connect(actionClustering, SIGNAL(triggered()), this, SLOT(showClusteringDialog()));

    // Round 2: High-DPI Page Copy, 2D Cross-Section Profile Gadget & Domain Analysis
    actionCopyPageImage =
            new QAction(IconLoader::load("copy-page-image"), tr("Copy Pa&ge as Image (High-DPI PNG)"), this);
    actionCopyPageImage->setShortcut(tr("Ctrl+Alt+C"));
    actionCopyPageImage->setToolTip(
            tr("Copy active graph page to clipboard as a 300 DPI PNG image"));
    connect(actionCopyPageImage, SIGNAL(triggered()), this, SLOT(copyPageAsHighDpiImage()));

    actionGadgetImageProfile = new QAction(
            IconLoader::load("gadget-delta"),
            tr("2D &Cross-Section Profile Gadget (Image / Heatmap)"), this);
    actionGadgetImageProfile->setToolTip(
            tr("Interactive horizontal/vertical 2D cross-section profile gadget for heatmaps and images"));
    connect(actionGadgetImageProfile, SIGNAL(triggered()), this, SLOT(attachImageProfileGadget()));

    actionMatrixFilter2D = new QAction(tr("2D &Spatial Filter && Plane Leveling..."), this);
    actionMatrixFilter2D->setToolTip(
            tr("Apply 2D Gaussian, median, Sobel/Laplacian filter or polynomial plane leveling to a matrix"));
    connect(actionMatrixFilter2D, SIGNAL(triggered()), this, SLOT(showMatrixFilter2DDialog()));

    actionFourierSelfDeconvolve =
            new QAction(tr("&Fourier Self-Deconvolution (Peak Sharpening)..."), this);
    actionFourierSelfDeconvolve->setToolTip(
            tr("Sharpen overlapping spectral peaks via Kauppinen Fourier self-deconvolution"));
    connect(actionFourierSelfDeconvolve, SIGNAL(triggered()), this,
            SLOT(showFourierSelfDeconvolveDialog()));

    actionEISAnalyzer = new QAction(tr("&EIS Nyquist && Bode Analyzer..."), this);
    actionEISAnalyzer->setToolTip(
            tr("Electrochemical Impedance Spectroscopy (Nyquist, Bode & Randles circuit fit)"));
    connect(actionEISAnalyzer, SIGNAL(triggered()), this, SLOT(showEISAnalyzerDialog()));

    actionSampleGallery = new QAction(IconLoader::duotoneIcon(QStringLiteral("sample-gallery")),
                                      tr("Sample Project &Gallery..."), this);
    actionSampleGallery->setToolTip(
            tr("Open Built-In Scientific Sample Project Gallery"));
    connect(actionSampleGallery, SIGNAL(triggered()), this, SLOT(showSampleGalleryDialog()));

    actionWorkHardeningDialog =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("work-hardening")),
                        tr("&Work Hardening && Considere Analysis..."), this);
    actionWorkHardeningDialog->setToolTip(
            tr("True Stress-Strain, Hollomon n, Crussard-Jaoul & Considere Necking"));
    connect(actionWorkHardeningDialog, SIGNAL(triggered()), this, SLOT(showWorkHardeningDialog()));

    actionKissingerDialog =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("kissinger-tds")),
                        tr("&Kissinger / Thermal Desorption (TDS)..."), this);
    actionKissingerDialog->setToolTip(
            tr("Kissinger Thermal Desorption (TDS / DSC) Activation Energy Analysis"));
    connect(actionKissingerDialog, SIGNAL(triggered()), this, SLOT(showKissingerDialog()));

    actionXRDAnalyzerDialog =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("xrd-peaks")),
                        tr("&XRD Peak && Williamson-Hall Analyzer..."), this);
    actionXRDAnalyzerDialog->setToolTip(
            tr("Powder XRD K-alpha2 Stripping, Scherrer & Williamson-Hall Analysis"));
    connect(actionXRDAnalyzerDialog, SIGNAL(triggered()), this, SLOT(showXRDAnalyzerDialog()));

    actionVaspPlotterDialog =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("vasp-bands")),
                        tr("&VASP Electronic Band && DOS Plotter..."), this);
    actionVaspPlotterDialog->setToolTip(
            tr("VASP EIGENVAL / DOSCAR Electronic Band Structure & DOS Plotter"));
    connect(actionVaspPlotterDialog, SIGNAL(triggered()), this, SLOT(showVaspPlotterDialog()));

    actionExportPythonScript =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("code-export")),
                        tr("Export Reproducible &Python Script..."), this);
    actionExportPythonScript->setToolTip(
            tr("Generate Reproducible Python Script from Active Graph"));
    connect(actionExportPythonScript, SIGNAL(triggered()), this, SLOT(exportActiveGraphToPython()));

    actionPassingBablokDeming =
            new QAction(tr("Passing-&Bablok && Deming Regression..."), this);
    actionPassingBablokDeming->setToolTip(
            tr("Method-comparison regression with errors in both X and Y (Passing-Bablok & Deming)"));
    connect(actionPassingBablokDeming, SIGNAL(triggered()), this,
            SLOT(showPassingBablokDemingDialog()));

    actionCopyStatusBarText = new QAction(tr("&Copy status bar text"), this);
    connect(actionCopyStatusBarText, SIGNAL(triggered()), this, SLOT(copyStatusBarText()));

    actionCommandPalette =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("command-palette")),
                        tr("Command &Palette..."), this);
    actionCommandPalette->setObjectName(QStringLiteral("actionCommandPalette"));
    actionCommandPalette->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_K));
    actionCommandPalette->setShortcutContext(Qt::ApplicationShortcut);
    actionCommandPalette->setToolTip(tr("Open Command Palette (Ctrl+K)"));
    connect(actionCommandPalette, SIGNAL(triggered()), this, SLOT(openCommandPalette()));
    addAction(actionCommandPalette);

    actionToggleFocusHudMode =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("focus-mode")),
                        tr("&Focus HUD Mode"), this);
    actionToggleFocusHudMode->setObjectName(QStringLiteral("actionToggleFocusHudMode"));
    actionToggleFocusHudMode->setCheckable(true);
    actionToggleFocusHudMode->setShortcut(QKeySequence(Qt::Key_F11));
    actionToggleFocusHudMode->setShortcutContext(Qt::ApplicationShortcut);
    actionToggleFocusHudMode->setToolTip(tr("Toggle Focus HUD Mode (F11)"));
    connect(actionToggleFocusHudMode, SIGNAL(triggered()), this, SLOT(toggleFocusHudMode()));
    addAction(actionToggleFocusHudMode);

    workspaceLayoutGroup = new QActionGroup(this);
    workspaceLayoutGroup->setExclusive(true);

    actionWorkspaceLayoutTabbed =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("layout-tabs")),
                        tr("&Tabbed Workspace"), this);
    actionWorkspaceLayoutTabbed->setObjectName(QStringLiteral("actionWorkspaceLayoutTabbed"));
    actionWorkspaceLayoutTabbed->setCheckable(true);
    actionWorkspaceLayoutTabbed->setChecked(true);
    workspaceLayoutGroup->addAction(actionWorkspaceLayoutTabbed);
    connect(actionWorkspaceLayoutTabbed, SIGNAL(triggered()), this,
            SLOT(setWorkspaceLayoutTabbed()));

    actionWorkspaceLayoutSplit =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("layout-split")),
                        tr("&Split View (Table + Graph)"), this);
    actionWorkspaceLayoutSplit->setObjectName(QStringLiteral("actionWorkspaceLayoutSplit"));
    actionWorkspaceLayoutSplit->setCheckable(true);
    workspaceLayoutGroup->addAction(actionWorkspaceLayoutSplit);
    connect(actionWorkspaceLayoutSplit, SIGNAL(triggered()), this,
            SLOT(setWorkspaceLayoutSplit()));

    actionWorkspaceLayoutMdi =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("layout-mdi")),
                        tr("Free &MDI Windows"), this);
    actionWorkspaceLayoutMdi->setObjectName(QStringLiteral("actionWorkspaceLayoutMdi"));
    actionWorkspaceLayoutMdi->setCheckable(true);
    workspaceLayoutGroup->addAction(actionWorkspaceLayoutMdi);
    connect(actionWorkspaceLayoutMdi, SIGNAL(triggered()), this, SLOT(setWorkspaceLayoutMdi()));

    actionOpenPythonConsole =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("python-console")),
                        tr("Open &Python Console..."), this);
    actionOpenPythonConsole->setObjectName(QStringLiteral("actionOpenPythonConsole"));
    actionOpenPythonConsole->setShortcut(QKeySequence(Qt::Key_F9));
    actionOpenPythonConsole->setShortcutContext(Qt::ApplicationShortcut);
    actionOpenPythonConsole->setToolTip(tr("Open Interactive In-Process Python Console"));
    connect(actionOpenPythonConsole, SIGNAL(triggered()), this, SLOT(openPythonConsole()));
    addAction(actionOpenPythonConsole);

    actionRunPythonScript =
            new QAction(IconLoader::duotoneIcon(QStringLiteral("python-run")),
                        tr("Run Python &Script..."), this);
    actionRunPythonScript->setObjectName(QStringLiteral("actionRunPythonScript"));
    actionRunPythonScript->setToolTip(tr("Select and Execute a .py Automation Script"));
    connect(actionRunPythonScript, SIGNAL(triggered()), this, SLOT(runPythonScriptDialog()));

    // Ensure every QAction member has a deterministic objectName for RPC safety & inspection
#define ASSIGN_ACT_OBJNAME(a)                                                                      \
    do {                                                                                           \
        if ((a) && (a)->objectName().isEmpty())                                                    \
            (a)->setObjectName(QStringLiteral(#a));                                                \
    } while (0)
    ASSIGN_ACT_OBJNAME(actionCopyStatusBarText);
    ASSIGN_ACT_OBJNAME(actionEditCurveRange);
    ASSIGN_ACT_OBJNAME(actionCurveFullRange);
    ASSIGN_ACT_OBJNAME(actionShowAllCurves);
    ASSIGN_ACT_OBJNAME(actionHideCurve);
    ASSIGN_ACT_OBJNAME(actionHideOtherCurves);
    ASSIGN_ACT_OBJNAME(actionEditFunction);
    ASSIGN_ACT_OBJNAME(actionRemoveCurve);
    ASSIGN_ACT_OBJNAME(actionShowCurveWorksheet);
    ASSIGN_ACT_OBJNAME(actionShowCurvePlotDialog);
    ASSIGN_ACT_OBJNAME(actionNewProject);
    ASSIGN_ACT_OBJNAME(actionNewNote);
    ASSIGN_ACT_OBJNAME(actionNewTable);
    ASSIGN_ACT_OBJNAME(actionNewFunctionPlot);
    ASSIGN_ACT_OBJNAME(actionNewSurfacePlot);
    ASSIGN_ACT_OBJNAME(actionNewMatrix);
    ASSIGN_ACT_OBJNAME(actionNewGraph);
    ASSIGN_ACT_OBJNAME(actionOpen);
    ASSIGN_ACT_OBJNAME(actionLoadImage);
    ASSIGN_ACT_OBJNAME(actionSaveProject);
    ASSIGN_ACT_OBJNAME(actionSaveProjectAs);
    ASSIGN_ACT_OBJNAME(actionImportImage);
    ASSIGN_ACT_OBJNAME(actionLoad);
    ASSIGN_ACT_OBJNAME(actionUndo);
    ASSIGN_ACT_OBJNAME(actionRedo);
    ASSIGN_ACT_OBJNAME(actionCopyWindow);
    ASSIGN_ACT_OBJNAME(actionCutSelection);
    ASSIGN_ACT_OBJNAME(actionCopySelection);
    ASSIGN_ACT_OBJNAME(actionPasteSelection);
    ASSIGN_ACT_OBJNAME(actionClearSelection);
    ASSIGN_ACT_OBJNAME(locktoolbar);
    ASSIGN_ACT_OBJNAME(actionShowExplorer);
    ASSIGN_ACT_OBJNAME(actionShowLog);
    ASSIGN_ACT_OBJNAME(actionAddLayer);
    ASSIGN_ACT_OBJNAME(actionShowLayerDialog);
    ASSIGN_ACT_OBJNAME(actionAutomaticLayout);
    ASSIGN_ACT_OBJNAME(actionShowHistory);
    ASSIGN_ACT_OBJNAME(actionExportGraph);
    ASSIGN_ACT_OBJNAME(actionExportAllGraphs);
    ASSIGN_ACT_OBJNAME(actionPrint);
    ASSIGN_ACT_OBJNAME(actionPrintAllPlots);
    ASSIGN_ACT_OBJNAME(actionShowExportASCIIDialog);
    ASSIGN_ACT_OBJNAME(actionExportPDF);
    ASSIGN_ACT_OBJNAME(actionCloseAllWindows);
    ASSIGN_ACT_OBJNAME(actionClearLogInfo);
    ASSIGN_ACT_OBJNAME(actionShowPlotWizard);
    ASSIGN_ACT_OBJNAME(actionShowConfigureDialog);
    ASSIGN_ACT_OBJNAME(actionShowCurvesDialog);
    ASSIGN_ACT_OBJNAME(actionAddErrorBars);
    ASSIGN_ACT_OBJNAME(actionAddFunctionCurve);
    ASSIGN_ACT_OBJNAME(actionUnzoom);
    ASSIGN_ACT_OBJNAME(actionNewLegend);
    ASSIGN_ACT_OBJNAME(actionAddImage);
    ASSIGN_ACT_OBJNAME(actionPlotStackedHistograms);
    ASSIGN_ACT_OBJNAME(actionPlot2VerticalLayers);
    ASSIGN_ACT_OBJNAME(actionPlot2HorizontalLayers);
    ASSIGN_ACT_OBJNAME(actionPlot4Layers);
    ASSIGN_ACT_OBJNAME(actionPlotStackedLayers);
    ASSIGN_ACT_OBJNAME(actionPlot3DRibbon);
    ASSIGN_ACT_OBJNAME(actionPlot3DBars);
    ASSIGN_ACT_OBJNAME(actionPlot3DScatter);
    ASSIGN_ACT_OBJNAME(actionPlot3DTrajectory);
    ASSIGN_ACT_OBJNAME(actionShowColStatistics);
    ASSIGN_ACT_OBJNAME(actionShowRowStatistics);
    ASSIGN_ACT_OBJNAME(actionShowIntDialog);
    ASSIGN_ACT_OBJNAME(actionDifferentiate);
    ASSIGN_ACT_OBJNAME(actionFitLinear);
    ASSIGN_ACT_OBJNAME(actionShowFitPolynomDialog);
    ASSIGN_ACT_OBJNAME(actionShowExpDecayDialog);
    ASSIGN_ACT_OBJNAME(actionShowTwoExpDecayDialog);
    ASSIGN_ACT_OBJNAME(actionShowExpDecay3Dialog);
    ASSIGN_ACT_OBJNAME(actionFitExpGrowth);
    ASSIGN_ACT_OBJNAME(actionFitSigmoidal);
    ASSIGN_ACT_OBJNAME(actionFitGauss);
    ASSIGN_ACT_OBJNAME(actionFitLorentz);
    ASSIGN_ACT_OBJNAME(actionShowFitDialog);
    ASSIGN_ACT_OBJNAME(actionShowAxisDialog);
    ASSIGN_ACT_OBJNAME(actionShowTitleDialog);
    ASSIGN_ACT_OBJNAME(actionAbout);
    ASSIGN_ACT_OBJNAME(actionShowHelp);
#ifdef DYNAMIC_MANUAL_PATH
    ASSIGN_ACT_OBJNAME(actionChooseHelpFolder);
#endif
    ASSIGN_ACT_OBJNAME(actionRename);
    ASSIGN_ACT_OBJNAME(actionCloseWindow);
    ASSIGN_ACT_OBJNAME(actionConvertTable);
    ASSIGN_ACT_OBJNAME(actionDeleteLayer);
    ASSIGN_ACT_OBJNAME(actionInterpolate);
    ASSIGN_ACT_OBJNAME(actionResizeActiveWindow);
    ASSIGN_ACT_OBJNAME(actionHideActiveWindow);
    ASSIGN_ACT_OBJNAME(actionShowMoreWindows);
    ASSIGN_ACT_OBJNAME(actionPixelLineProfile);
    ASSIGN_ACT_OBJNAME(actionIntensityTable);
    ASSIGN_ACT_OBJNAME(actionShowLineDialog);
    ASSIGN_ACT_OBJNAME(actionShowImageDialog);
    ASSIGN_ACT_OBJNAME(actionShowTextDialog);
    ASSIGN_ACT_OBJNAME(actionActivateWindow);
    ASSIGN_ACT_OBJNAME(actionMinimizeWindow);
    ASSIGN_ACT_OBJNAME(actionMaximizeWindow);
    ASSIGN_ACT_OBJNAME(actionResizeWindow);
    ASSIGN_ACT_OBJNAME(actionPrintWindow);
    ASSIGN_ACT_OBJNAME(actionShowPlotGeometryDialog);
    ASSIGN_ACT_OBJNAME(actionEditSurfacePlot);
    ASSIGN_ACT_OBJNAME(actionAdd3DData);
    ASSIGN_ACT_OBJNAME(actionMatrixDeterminant);
    ASSIGN_ACT_OBJNAME(actionConvertMatrix);
    ASSIGN_ACT_OBJNAME(actionInvertMatrix);
    ASSIGN_ACT_OBJNAME(actionPlot3DWireFrame);
    ASSIGN_ACT_OBJNAME(actionPlot3DHiddenLine);
    ASSIGN_ACT_OBJNAME(actionPlot3DPolygons);
    ASSIGN_ACT_OBJNAME(actionPlot3DWireSurface);
    ASSIGN_ACT_OBJNAME(actionColorMap);
    ASSIGN_ACT_OBJNAME(actionContourMap);
    ASSIGN_ACT_OBJNAME(actionGrayMap);
    ASSIGN_ACT_OBJNAME(actionDeleteFitTables);
    ASSIGN_ACT_OBJNAME(actionShowGridDialog);
    ASSIGN_ACT_OBJNAME(actionTimeStamp);
    ASSIGN_ACT_OBJNAME(actionSmoothSavGol);
    ASSIGN_ACT_OBJNAME(actionSmoothFFT);
    ASSIGN_ACT_OBJNAME(actionSmoothAverage);
    ASSIGN_ACT_OBJNAME(actionFFT);
    ASSIGN_ACT_OBJNAME(actionLowPassFilter);
    ASSIGN_ACT_OBJNAME(actionHighPassFilter);
    ASSIGN_ACT_OBJNAME(actionBandPassFilter);
    ASSIGN_ACT_OBJNAME(actionBandBlockFilter);
    ASSIGN_ACT_OBJNAME(actionConvolute);
    ASSIGN_ACT_OBJNAME(actionDeconvolute);
    ASSIGN_ACT_OBJNAME(actionCorrelate);
    ASSIGN_ACT_OBJNAME(actionAutoCorrelate);
    ASSIGN_ACT_OBJNAME(actionTranslateHor);
    ASSIGN_ACT_OBJNAME(actionTranslateVert);
    ASSIGN_ACT_OBJNAME(actionBoxPlot);
    ASSIGN_ACT_OBJNAME(actionMultiPeakGauss);
    ASSIGN_ACT_OBJNAME(actionMultiPeakLorentz);
    ASSIGN_ACT_OBJNAME(actionMultiPeakGaussAmp);
    ASSIGN_ACT_OBJNAME(actionMultiPeakPseudoVoigt);
    ASSIGN_ACT_OBJNAME(actionMultiPeakPearsonVII);
    ASSIGN_ACT_OBJNAME(actionPeakFitDialog);
    ASSIGN_ACT_OBJNAME(actionPeakAnalyzerDialog);
    ASSIGN_ACT_OBJNAME(actionAutoPeakGauss);
    ASSIGN_ACT_OBJNAME(actionAutoPeakLorentz);
    ASSIGN_ACT_OBJNAME(actionAutoPeakPseudoVoigt);
    ASSIGN_ACT_OBJNAME(actionCopyGraphOle);
    ASSIGN_ACT_OBJNAME(actionLangEnglish);
    ASSIGN_ACT_OBJNAME(actionLangJapanese);
    ASSIGN_ACT_OBJNAME(actionPlotDoubleY);
    ASSIGN_ACT_OBJNAME(actionPlotYOffsetStack);
    ASSIGN_ACT_OBJNAME(actionPlotViolin);
    ASSIGN_ACT_OBJNAME(actionPlotPareto);
    ASSIGN_ACT_OBJNAME(actionPlotQQ);
    ASSIGN_ACT_OBJNAME(actionPlotTernary);
    ASSIGN_ACT_OBJNAME(actionColorPaletteEditor);
    ASSIGN_ACT_OBJNAME(actionAspect1_1);
    ASSIGN_ACT_OBJNAME(actionAspect4_3);
    ASSIGN_ACT_OBJNAME(actionAspect16_9);
    ASSIGN_ACT_OBJNAME(actionAspect3_2);
    ASSIGN_ACT_OBJNAME(actionAspectCustom);
    ASSIGN_ACT_OBJNAME(actionKeepAspectRatio);
    ASSIGN_ACT_OBJNAME(actionTabbedWorkspaceView);
    ASSIGN_ACT_OBJNAME(actionGadgetVerticalCursor);
    ASSIGN_ACT_OBJNAME(actionGadgetRegionStats);
    ASSIGN_ACT_OBJNAME(actionGadgetQuickFit);
    ASSIGN_ACT_OBJNAME(actionGadgetQuickPeaks);
    ASSIGN_ACT_OBJNAME(actionGadgetQuickSigmoidal);
    ASSIGN_ACT_OBJNAME(actionGadgetIntegrate);
    ASSIGN_ACT_OBJNAME(actionGadgetInterpolate);
    ASSIGN_ACT_OBJNAME(actionGadgetFFT);
    ASSIGN_ACT_OBJNAME(actionGadgetRiseTime);
    ASSIGN_ACT_OBJNAME(actionGadgetCluster);
    ASSIGN_ACT_OBJNAME(actionGadgetClear);
    ASSIGN_ACT_OBJNAME(actionMaskRegionROI);
    ASSIGN_ACT_OBJNAME(actionUnmaskRegionROI);
    ASSIGN_ACT_OBJNAME(actionClearAllMasks);
    ASSIGN_ACT_OBJNAME(actionSimpleCurveMath);
    ASSIGN_ACT_OBJNAME(actionSubtractStraightLine);
    ASSIGN_ACT_OBJNAME(actionAverageMultipleCurves);
    ASSIGN_ACT_OBJNAME(actionNormalizeCurves);
    ASSIGN_ACT_OBJNAME(actionCurveIntersections);
    ASSIGN_ACT_OBJNAME(actionPolygonLoopArea);
    ASSIGN_ACT_OBJNAME(actionFitApparentLinear);
    ASSIGN_ACT_OBJNAME(actionFitYorkLinear);
    ASSIGN_ACT_OBJNAME(actionGlobalCurveFit);
    ASSIGN_ACT_OBJNAME(actionImplicitODRFit);
    ASSIGN_ACT_OBJNAME(actionModelComparison);
    ASSIGN_ACT_OBJNAME(actionSurfaceFit3D);
    ASSIGN_ACT_OBJNAME(actionBatchPeakAnalysis);
    ASSIGN_ACT_OBJNAME(action2DPeakAnalysis);
    ASSIGN_ACT_OBJNAME(actionSmoothLOWESS);
    ASSIGN_ACT_OBJNAME(actionSmoothWhittaker);
    ASSIGN_ACT_OBJNAME(actionSmoothMedian);
    ASSIGN_ACT_OBJNAME(actionIirDigitalFilter);
    ASSIGN_ACT_OBJNAME(actionStftSpectrogram);
    ASSIGN_ACT_OBJNAME(actionHilbertEnvelope);
    ASSIGN_ACT_OBJNAME(actionWaveletTransform);
    ASSIGN_ACT_OBJNAME(actionWelchPsdCoherence);
    ASSIGN_ACT_OBJNAME(actionMatrixFFT2D);
    ASSIGN_ACT_OBJNAME(actionMatrixIFFT2D);
    ASSIGN_ACT_OBJNAME(actionConfidenceEllipse);
    ASSIGN_ACT_OBJNAME(actionGroupComparison);
    ASSIGN_ACT_OBJNAME(actionROCCurve);
    ASSIGN_ACT_OBJNAME(actionKaplanMeier);
    ASSIGN_ACT_OBJNAME(actionPCABiplot);
    ASSIGN_ACT_OBJNAME(actionRecalcAuto);
    ASSIGN_ACT_OBJNAME(actionRecalcManual);
    ASSIGN_ACT_OBJNAME(actionRecalcDisabled);
    ASSIGN_ACT_OBJNAME(actionRecalculateNow);
    ASSIGN_ACT_OBJNAME(actionRepeatLinearFitAll);
    ASSIGN_ACT_OBJNAME(actionRepeatNormalizeAll);
    ASSIGN_ACT_OBJNAME(actionManageDialogThemes);
    ASSIGN_ACT_OBJNAME(actionPageSetup);
    ASSIGN_ACT_OBJNAME(actionFitPageToLayers);
    ASSIGN_ACT_OBJNAME(actionFitLayersToPage);
    ASSIGN_ACT_OBJNAME(actionTogglePageView);
    ASSIGN_ACT_OBJNAME(actionAddLinkedLayer);
    ASSIGN_ACT_OBJNAME(actionAddInsetLayer);
    ASSIGN_ACT_OBJNAME(actionFrameBoxInward);
    ASSIGN_ACT_OBJNAME(actionFrameBoxPlain);
    ASSIGN_ACT_OBJNAME(actionFrameOpenL);
    ASSIGN_ACT_OBJNAME(actionFrameZeroCrossed);
    ASSIGN_ACT_OBJNAME(actionToggleMiniToolbar);
    ASSIGN_ACT_OBJNAME(actionGadgetDifferentiate);
    ASSIGN_ACT_OBJNAME(actionGadgetIntersect);
    ASSIGN_ACT_OBJNAME(actionGadgetIntegrate2D);
    ASSIGN_ACT_OBJNAME(actionPiecewiseLinearFit);
    ASSIGN_ACT_OBJNAME(actionRobustLinearFit);
    ASSIGN_ACT_OBJNAME(actionCompareDatasetsFit);
    ASSIGN_ACT_OBJNAME(actionMultipleLinearRegression);
    ASSIGN_ACT_OBJNAME(actionSavGolSmoothDerivative);
    ASSIGN_ACT_OBJNAME(actionPeakEnvelope);
    ASSIGN_ACT_OBJNAME(actionDecimateResample);
    ASSIGN_ACT_OBJNAME(actionNormalityTest);
    ASSIGN_ACT_OBJNAME(actionBlandAltman);
    ASSIGN_ACT_OBJNAME(actionTwoWayANOVA);
    ASSIGN_ACT_OBJNAME(actionGraphDigitizer);
    ASSIGN_ACT_OBJNAME(actionCopyGraphFormat);
    ASSIGN_ACT_OBJNAME(actionPasteGraphFormat);
    ASSIGN_ACT_OBJNAME(actionPasteFormatFonts);
    ASSIGN_ACT_OBJNAME(actionPasteFormatColors);
    ASSIGN_ACT_OBJNAME(actionPasteFormatScales);
    ASSIGN_ACT_OBJNAME(actionPasteFormatFolder);
    ASSIGN_ACT_OBJNAME(actionBatchPlot);
    ASSIGN_ACT_OBJNAME(actionStyleISIJ);
    ASSIGN_ACT_OBJNAME(actionStyleSlide);
    ASSIGN_ACT_OBJNAME(actionApplyStyleToMultipleGraphs);
    ASSIGN_ACT_OBJNAME(actionApplyTableTemplate);
    ASSIGN_ACT_OBJNAME(actionReimportData);
    ASSIGN_ACT_OBJNAME(actionQuickLinearFit);
    ASSIGN_ACT_OBJNAME(actionPlotStackedBars);
    ASSIGN_ACT_OBJNAME(actionPlotPolarData);
    ASSIGN_ACT_OBJNAME(actionGadgetDeltaCursor);
    ASSIGN_ACT_OBJNAME(actionLogisticRegression);
    ASSIGN_ACT_OBJNAME(actionArrheniusPlot);
    ASSIGN_ACT_OBJNAME(actionTaucPlot);
    ASSIGN_ACT_OBJNAME(actionFillBetweenCurves);
    ASSIGN_ACT_OBJNAME(actionWaterfallStackOffset);
    ASSIGN_ACT_OBJNAME(actionResetStackOffsets);
    ASSIGN_ACT_OBJNAME(actionWeibullPlot);
    ASSIGN_ACT_OBJNAME(actionOutlierTest);
    ASSIGN_ACT_OBJNAME(actionNonparametricTests);
    ASSIGN_ACT_OBJNAME(actionPairedComparisonPlot);
    ASSIGN_ACT_OBJNAME(actionCorrelationMatrix);
    ASSIGN_ACT_OBJNAME(actionClustering);
    ASSIGN_ACT_OBJNAME(actionCopyPageImage);
    ASSIGN_ACT_OBJNAME(actionGadgetImageProfile);
    ASSIGN_ACT_OBJNAME(actionMatrixFilter2D);
    ASSIGN_ACT_OBJNAME(actionFourierSelfDeconvolve);
    ASSIGN_ACT_OBJNAME(actionEISAnalyzer);
    ASSIGN_ACT_OBJNAME(actionSampleGallery);
    ASSIGN_ACT_OBJNAME(actionWorkHardeningDialog);
    ASSIGN_ACT_OBJNAME(actionKissingerDialog);
    ASSIGN_ACT_OBJNAME(actionXRDAnalyzerDialog);
    ASSIGN_ACT_OBJNAME(actionVaspPlotterDialog);
    ASSIGN_ACT_OBJNAME(actionExportPythonScript);
    ASSIGN_ACT_OBJNAME(actionPassingBablokDeming);
    ASSIGN_ACT_OBJNAME(actionHomePage);
#ifdef DOWNLOAD_LINKS
    ASSIGN_ACT_OBJNAME(actionDownloadManual);
#endif
    ASSIGN_ACT_OBJNAME(actionHelpForums);
    ASSIGN_ACT_OBJNAME(actionHelpBugReports);
    ASSIGN_ACT_OBJNAME(actionShowPlotDialog);
    ASSIGN_ACT_OBJNAME(actionShowScaleDialog);
    ASSIGN_ACT_OBJNAME(actionOpenTemplate);
    ASSIGN_ACT_OBJNAME(actionSaveTemplate);
    ASSIGN_ACT_OBJNAME(actionNextWindow);
    ASSIGN_ACT_OBJNAME(actionPrevWindow);
#ifdef SCRIPTING_DIALOG
    ASSIGN_ACT_OBJNAME(actionScriptingLang);
#endif
    ASSIGN_ACT_OBJNAME(actionRestartScripting);
    ASSIGN_ACT_OBJNAME(actionNoteExecute);
    ASSIGN_ACT_OBJNAME(actionNoteExecuteAll);
    ASSIGN_ACT_OBJNAME(actionNoteEvaluate);
    ASSIGN_ACT_OBJNAME(actionSaveNote);
#undef ASSIGN_ACT_OBJNAME
}

void ApplicationWindow::translateActionsStrings()
{
    actionShowCurvePlotDialog->setText(tr("&Plot details..."));
    actionShowCurveWorksheet->setText(tr("&Worksheet"));
    actionRemoveCurve->setText(tr("&Delete"));
    actionEditFunction->setText(tr("&Edit Function..."));
    actionCopyStatusBarText->setText(tr("&Copy status bar text"));

    actionCurveFullRange->setText(tr("&Reset to Full Range"));
    actionEditCurveRange->setText(tr("Edit &Range..."));
    actionHideCurve->setText(tr("&Hide"));
    actionHideOtherCurves->setText(tr("Hide &Other Curves"));
    actionShowAllCurves->setText(tr("&Show All Curves"));

    actionNewProject->setText(tr("New &Project"));
    actionNewProject->setToolTip(tr("Open a new project"));
    actionNewProject->setShortcut(tr("Ctrl+N"));

    actionNewGraph->setText(tr("New &Graph"));
    actionNewGraph->setToolTip(tr("Create an empty 2D plot"));
    actionNewGraph->setShortcut(tr("Ctrl+G"));

    actionNewNote->setText(tr("New &Note / Script"));
    actionNewNote->setShortcut(tr("Ctrl+ALT+N"));
    actionNewNote->setToolTip(tr("Create an empty note / script window"));

    actionNewTable->setText(tr("New &Table"));
    actionNewTable->setShortcut(tr("Ctrl+T"));
    actionNewTable->setToolTip(tr("New table"));

    actionNewMatrix->setText(tr("New &Matrix"));
    actionNewMatrix->setShortcut(tr("Ctrl+M"));
    actionNewMatrix->setToolTip(tr("New matrix"));

    actionNewFunctionPlot->setText(tr("New &Function Plot"));
    actionNewFunctionPlot->setToolTip(tr("Create a new 2D function plot"));
    actionNewFunctionPlot->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));

    actionNewSurfacePlot->setText(tr("New 3D &Surface Plot"));
    actionNewSurfacePlot->setToolTip(tr("Create a new 3D surface plot"));
    actionNewSurfacePlot->setShortcut(tr("Ctrl+ALT+Z"));

    // FIXME: "..." should be added before translating, but this would break translations
    actionOpen->setText(tr("&Open") + "...");
    actionOpen->setShortcut(tr("Ctrl+O"));
    actionOpen->setToolTip(tr("Open project"));

    actionLoadImage->setText(tr("Open Image as &Graph..."));
    actionLoadImage->setShortcut(tr("Ctrl+I"));

    actionImportImage->setText(tr("Import Image to &Matrix..."));

    actionSaveProject->setText(tr("&Save Project"));
    actionSaveProject->setToolTip(tr("Save project"));
    actionSaveProject->setShortcut(tr("Ctrl+S"));

    actionSaveProjectAs->setText(tr("Save Project &As..."));
    actionSaveProjectAs->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));

    actionOpenTemplate->setText(tr("Open Te&mplate..."));
    actionOpenTemplate->setToolTip(tr("Open template"));

    actionSaveTemplate->setText(tr("Save As &Template..."));
    actionSaveTemplate->setToolTip(tr("Save window as template"));

    actionLoad->setText(tr("&Import ASCII..."));
    actionLoad->setToolTip(tr("Import data file(s)"));
    actionLoad->setShortcut(tr("Ctrl+Alt+I"));
    if (auto *aPptx = findChild<QAction *>(QStringLiteral("actionSendToPowerPoint"))) {
        aPptx->setText(ToriGraphPptx::sendToPowerPointMenuText()); // E8
        aPptx->setToolTip(ToriGraphPptx::sendToPowerPointToolTip());
        aPptx->setStatusTip(ToriGraphPptx::sendToPowerPointToolTip());
    }
    if (auto *actionImportExcel = findChild<QAction *>(QStringLiteral("actionImportExcel"))) {
        XlsxImport::registerTranslations();
        actionImportExcel->setText(tr("Import &Excel Workbook (.xlsx)..."));
        actionImportExcel->setToolTip(
                tr("Import every sheet of an Excel workbook (.xlsx) as worksheets"));
    }
    if (WorkbookManager *wb = WorkbookManager::existing(this))
        wb->retranslateUi(); // D8: New Workbook

    actionUndo->setText(tr("&Undo"));
    actionUndo->setToolTip(tr("Undo changes"));
    actionUndo->setShortcut(tr("Ctrl+Z"));

    actionRedo->setText(tr("&Redo"));
    actionRedo->setToolTip(tr("Redo changes"));
    actionRedo->setShortcuts(QList<QKeySequence>()
                             << QKeySequence(tr("Ctrl+Y"))
                             << QKeySequence(tr("Ctrl+Shift+Z")));
    updateUndoRedoActionsForActiveWindow();

    actionCopyWindow->setText(tr("Duplicat&e"));
    actionCopyWindow->setToolTip(tr("Duplicate window"));

    actionCutSelection->setText(tr("Cu&t Selection"));
    actionCutSelection->setToolTip(tr("Cut selection"));
    actionCutSelection->setShortcut(tr("Ctrl+X"));

    actionCopySelection->setText(tr("&Copy Selection"));
    actionCopySelection->setToolTip(tr("Copy selection"));
    actionCopySelection->setShortcut(tr("Ctrl+C"));

    actionPasteSelection->setText(tr("P&aste Selection"));
    actionPasteSelection->setToolTip(tr("Paste selection"));
    actionPasteSelection->setShortcut(tr("Ctrl+V"));

    actionClearSelection->setText(tr("&Delete Selection"));
    actionClearSelection->setToolTip(tr("Delete selection"));
    actionClearSelection->setShortcut(tr("Del", "delete key"));

    actionShowExplorer->setText(tr("Project &Explorer"));
    actionShowExplorer->setShortcut(tr("Ctrl+E"));
    actionShowExplorer->setToolTip(tr("Show project explorer"));

    actionShowLog->setText(tr("&Results Log"));
    actionShowLog->setToolTip(tr("Show analysis results"));

#ifdef SCRIPTING_CONSOLE
    actionShowConsole->setText(tr("&Console"));
    actionShowConsole->setToolTip(tr("Show Scripting console"));
#endif

    actionAddLayer->setText(tr("Add La&yer"));
    actionAddLayer->setToolTip(tr("Add Layer"));
    actionAddLayer->setShortcut(tr("Ctrl+Shift+L"));

    // FIXME: "..." should be added before translating, but this would break translations
    actionShowLayerDialog->setText(tr("Arran&ge Layers") + "...");
    actionShowLayerDialog->setToolTip(tr("Arrange Layers"));
    actionShowLayerDialog->setShortcut(tr("Ctrl+Shift+A"));

    actionAutomaticLayout->setText(tr("Automatic Layout"));
    actionAutomaticLayout->setToolTip(tr("Automatic Layout"));

    // FIXME: "..." should be added before translating, but this would break translations
    actionExportGraph->setText(tr("&Current") + "...");
    actionExportGraph->setShortcut(tr("Ctrl+Shift+E"));
    actionExportGraph->setToolTip(tr("Export current graph"));

    // FIXME: "..." should be added before translating, but this would break translations
    actionExportAllGraphs->setText(tr("&All") + "...");
    actionExportAllGraphs->setShortcut(tr("Ctrl+Shift+X"));
    actionExportAllGraphs->setToolTip(tr("Export all graphs"));

    // FIXME: "..." should be added before translating, but this would break translations
    actionExportPDF->setText(tr("&Export PDF") + "...");
    actionExportPDF->setShortcut(tr("Ctrl+Alt+P"));
    actionExportPDF->setToolTip(tr("Export to PDF"));

    // FIXME: "..." should be added before translating, but this would break translations
    actionPrint->setText(tr("&Print") + "...");
    actionPrint->setShortcut(tr("Ctrl+P"));
    actionPrint->setToolTip(tr("Print window"));

    // FIXME: "..." should be added before translating, but this would break translations
    actionPrintAllPlots->setText(tr("Print All Plo&ts") + "...");
    // FIXME: "..." should be added before translating, but this would break translations
    actionShowExportASCIIDialog->setText(tr("E&xport ASCII") + "...");

    actionCloseAllWindows->setText(tr("&Quit"));
    actionCloseAllWindows->setShortcut(tr("Ctrl+Q"));

    actionClearLogInfo->setText(tr("Clear &Log Information"));
    actionDeleteFitTables->setText(tr("Delete &Fit Tables"));

    actionShowPlotWizard->setText(tr("Plot &Wizard"));
    actionShowPlotWizard->setShortcut(tr("Ctrl+Alt+W"));
    toolbarsMenu->setTitle(tr("Toolbars"));

    actionShowConfigureDialog->setText(tr("Prefere&nces..."));

    actionShowCurvesDialog->setText(tr("Add/Remove &Curve..."));
    actionShowCurvesDialog->setShortcut(tr("Ctrl+Shift+D"));
    actionShowCurvesDialog->setToolTip(tr("Add curve to graph"));

    actionAddErrorBars->setText(tr("Add &Error Bars..."));
    actionAddErrorBars->setToolTip(tr("Add Error Bars..."));
    actionAddErrorBars->setShortcut(tr("Ctrl+B"));

    actionAddFunctionCurve->setText(tr("Add &Function..."));
    actionAddFunctionCurve->setToolTip(tr("Add Function..."));
    actionAddFunctionCurve->setShortcut(tr("Ctrl+Alt+F"));

    actionUnzoom->setText(tr("&Rescale to Show All"));
    actionUnzoom->setShortcut(tr("Ctrl+R"));
    actionUnzoom->setToolTip(tr("Best fit"));

    actionNewLegend->setText(tr("&New / Reconstruct Legend"));
    actionNewLegend->setShortcut(tr("Ctrl+L"));
    actionNewLegend->setToolTip(tr("Add or reconstruct legend"));

    actionTimeStamp->setText(tr("Add Time Stamp"));
    actionTimeStamp->setShortcut(tr("Ctrl+ALT+T"));
    actionTimeStamp->setToolTip(tr("Date & time "));

    actionAddImage->setText(tr("Add &Image"));
    actionAddImage->setToolTip(tr("Add Image"));
    actionAddImage->setShortcut(tr("Ctrl+Shift+G"));

    actionPlotL->setText(tr("&Line"));
    actionPlotL->setToolTip(tr("Plot as line"));

    actionPlotP->setText(tr("&Scatter"));
    actionPlotP->setToolTip(tr("Plot as symbols"));

    actionPlotLP->setText(tr("Line + S&ymbol"));
    actionPlotLP->setToolTip(tr("Plot as line + symbols"));

    actionPlotVerticalDropLines->setText(tr("Vertical &Drop Lines"));

    actionPlotSpline->setText(tr("&Spline"));
    actionPlotVertSteps->setText(tr("&Vertical Steps"));
    actionPlotHorSteps->setText(tr("&Horizontal Steps"));

    actionPlotVerticalBars->setText(tr("&Vertical Bars"));
    actionPlotVerticalBars->setToolTip(tr("Plot with vertical bars"));

    if (actionPlotStackedBars) {
        actionPlotStackedBars->setText(tr("Stacked &Bars"));
        actionPlotStackedBars->setToolTip(tr("Plot vertical stacked bar chart"));
    }
    if (actionPlotPolarData) {
        actionPlotPolarData->setText(tr("&Polar Plot..."));
        actionPlotPolarData->setToolTip(
                tr("Plot polar (theta, radius) data from active worksheet"));
    }

    actionPlotHorizontalBars->setText(tr("&Horizontal Bars"));
    actionPlotHorizontalBars->setToolTip(tr("Plot with horizontal bars"));

    actionPlotArea->setText(tr("&Area"));
    actionPlotArea->setToolTip(tr("Plot area"));

    actionPlotPie->setText(tr("&Pie"));
    actionPlotPie->setToolTip(tr("Plot pie"));

    actionPlotVectXYXY->setText(tr("Vectors &XYXY"));
    actionPlotVectXYXY->setToolTip(tr("Vectors XYXY"));

    actionPlotVectXYAM->setText(tr("Vectors XY&AM"));
    actionPlotVectXYAM->setToolTip(tr("Vectors XYAM"));

    actionPlotHistogram->setText(tr("&Histogram"));
    actionPlotStackedHistograms->setText(tr("&Stacked Histogram"));
    actionPlot2VerticalLayers->setText(tr("&Vertical 2 Layers"));
    actionPlot2HorizontalLayers->setText(tr("&Horizontal 2 Layers"));
    actionPlot4Layers->setText(tr("&4 Layers"));
    actionPlotStackedLayers->setText(tr("&Stacked Layers"));

    actionPlot3DRibbon->setText(tr("&Ribbon"));
    actionPlot3DRibbon->setToolTip(tr("Plot 3D ribbon"));

    actionPlot3DBars->setText(tr("&Bars"));
    actionPlot3DBars->setToolTip(tr("Plot 3D bars"));

    actionPlot3DScatter->setText(tr("&Scatter"));
    actionPlot3DScatter->setToolTip(tr("Plot 3D scatter"));

    actionPlot3DTrajectory->setText(tr("&Trajectory"));
    actionPlot3DTrajectory->setToolTip(tr("Plot 3D trajectory"));

    actionColorMap->setText(tr("Contour + &Color Fill"));
    actionColorMap->setToolTip(tr("Contour Lines + Color Fill"));

    actionContourMap->setText(tr("Contour &Lines"));
    actionContourMap->setToolTip(tr("Contour Lines"));

    actionGrayMap->setText(tr("&Gray Scale Map"));
    actionGrayMap->setToolTip(tr("Gray Scale Map"));

    actionShowColStatistics->setText(tr("Statistics on &Columns"));
    actionShowColStatistics->setToolTip(tr("Selected columns statistics"));

    actionShowRowStatistics->setText(tr("Statistics on &Rows"));
    actionShowRowStatistics->setToolTip(tr("Selected rows statistics"));
    actionShowIntDialog->setText(tr("&Integrate ..."));
    actionInterpolate->setText(tr("Inte&rpolate ..."));
    actionLowPassFilter->setText(tr("&Low Pass..."));
    actionHighPassFilter->setText(tr("&High Pass..."));
    actionBandPassFilter->setText(tr("&Band Pass..."));
    actionBandBlockFilter->setText(tr("&Band Block..."));
    actionFFT->setText(tr("&FFT..."));
    actionSmoothSavGol->setText(tr("&Savitzky-Golay..."));
    actionSmoothFFT->setText(tr("&FFT Filter..."));
    actionSmoothAverage->setText(tr("Moving Window &Average..."));
    actionDifferentiate->setText(tr("&Differentiate"));
    actionFitLinear->setText(tr("Fit &Linear"));
    actionShowFitPolynomDialog->setText(tr("Fit &Polynomial ..."));
    actionShowExpDecayDialog->setText(tr("&First Order ..."));
    actionShowTwoExpDecayDialog->setText(tr("&Second Order ..."));
    actionShowExpDecay3Dialog->setText(tr("&Third Order ..."));
    actionFitExpGrowth->setText(tr("Fit Exponential Gro&wth ..."));
    actionFitSigmoidal->setText(tr("Fit &Boltzmann (Sigmoidal)"));
    actionFitGauss->setText(tr("Fit &Gaussian"));
    actionFitLorentz->setText(tr("Fit Lorent&zian"));

    actionShowFitDialog->setText(tr("Fit &Wizard..."));
    actionShowFitDialog->setShortcut(tr("Ctrl+Shift+Y"));

    actionShowPlotDialog->setText(tr("&Plot ..."));
    actionShowScaleDialog->setText(tr("&Scales..."));
    actionShowAxisDialog->setText(tr("&Axes..."));
    actionShowGridDialog->setText(tr("&Grid ..."));
    actionShowTitleDialog->setText(tr("&Title ..."));

    actionAbout->setText(tr("&About ToriGraph"));

    actionShowHelp->setText(tr("&Help"));
    actionShowHelp->setShortcuts({ QKeySequence(Qt::Key_F1) });

#ifdef DYNAMIC_MANUAL_PATH
    actionChooseHelpFolder->setText(tr("&Choose Help Folder..."));
#endif

    actionRename->setText(tr("&Rename Window"));

    actionCloseWindow->setText(tr("Close &Window"));
    actionCloseWindow->setShortcut(tr("Ctrl+W"));

    actionDeleteLayer->setText(tr("&Remove Layer"));
    actionDeleteLayer->setShortcut(tr("Ctrl+Alt+R"));

    actionResizeActiveWindow->setText(tr("Window &Geometry..."));
    actionHideActiveWindow->setText(tr("&Hide Window"));
    actionShowMoreWindows->setText(tr("More Windows..."));
    actionPixelLineProfile->setText(tr("&View Pixel Line Profile"));
    actionIntensityTable->setText(tr("&Intensity Table"));
    actionShowLineDialog->setText(tr("&Properties"));
    actionShowImageDialog->setText(tr("&Properties"));
    actionShowTextDialog->setText(tr("&Properties"));
    actionActivateWindow->setText(tr("&Activate Window"));
    actionMinimizeWindow->setText(tr("Mi&nimize Window"));
    actionMaximizeWindow->setText(tr("Ma&ximize Window"));
    actionResizeWindow->setText(tr("Re&size Window..."));
    actionPrintWindow->setText(tr("&Print Window"));
    actionShowPlotGeometryDialog->setText(tr("&Layer Geometry"));
    actionEditSurfacePlot->setText(tr("&Surface..."));
    actionAdd3DData->setText(tr("&Data Set..."));
    actionInvertMatrix->setText(tr("&Invert"));
    actionMatrixDeterminant->setText(tr("&Determinant"));
    actionConvertMatrix->setText(tr("&Convert to Table"));
    actionConvertTable->setText(tr("Convert to &Matrix"));
    actionPlot3DWireFrame->setText(tr("3D &Wire Frame"));
    actionPlot3DHiddenLine->setText(tr("3D &Hidden Line"));
    actionPlot3DPolygons->setText(tr("3D &Polygons"));
    actionPlot3DWireSurface->setText(tr("3D Wire &Surface"));
    actionCorrelate->setText(tr("Co&rrelate"));
    actionAutoCorrelate->setText(tr("&Autocorrelate"));
    actionConvolute->setText(tr("&Convolute"));
    actionDeconvolute->setText(tr("&Deconvolute"));
    actionTranslateHor->setText(tr("&Horizontal"));
    actionTranslateVert->setText(tr("&Vertical"));

    actionBoxPlot->setText(tr("&Box Plot"));
    actionBoxPlot->setToolTip(tr("Box and whiskers plot"));

    actionMultiPeakGauss->setText(tr("&Gaussian..."));
    actionMultiPeakLorentz->setText(tr("&Lorentzian..."));
    if (actionMultiPeakGaussAmp)
        actionMultiPeakGaussAmp->setText(tr("Gaussian (&Amplitude)..."));
    if (actionMultiPeakPseudoVoigt)
        actionMultiPeakPseudoVoigt->setText(tr("&Pseudo-Voigt..."));
    if (actionMultiPeakPearsonVII)
        actionMultiPeakPearsonVII->setText(tr("Pearson &VII..."));
    if (actionPeakAnalyzerDialog) {
        actionPeakAnalyzerDialog->setText(tr("Peak &Analyzer..."));
        actionPeakAnalyzerDialog->setShortcut(tr("Ctrl+Shift+P"));
        actionPeakAnalyzerDialog->setToolTip(
                tr("Peak Analyzer (Baseline, Peak Finding, Deconvolution & Integration)"));
    }
    if (actionPeakFitDialog) {
        actionPeakFitDialog->setText(tr("&Peak / Multi-Peak Fit..."));
    }
    if (actionAutoPeakGauss)
        actionAutoPeakGauss->setText(tr("Auto Peak Fit (&Gaussian)"));
    if (actionAutoPeakLorentz)
        actionAutoPeakLorentz->setText(tr("Auto Peak Fit (&Lorentzian)"));
    if (actionAutoPeakPseudoVoigt)
        actionAutoPeakPseudoVoigt->setText(tr("Auto Peak Fit (Pseudo-&Voigt)"));
    if (actionPlotDoubleY) {
        actionPlotDoubleY->setText(tr("&Double-Y Plot"));
        actionPlotDoubleY->setToolTip(tr("Plot two Y columns on Left Y and Right Y axes"));
    }
    if (actionPlotYOffsetStack) {
        actionPlotYOffsetStack->setText(tr("&Waterfall / Y-Offset Stack"));
        actionPlotYOffsetStack->setToolTip(
                tr("Stack multiple curves vertically with automatic Y offset (XRD / Spectroscopy)"));
    }
    if (actionPlotViolin) {
        actionPlotViolin->setText(tr("&Violin Plot"));
        actionPlotViolin->setToolTip(tr("Plot Gaussian KDE violin + box & jittered data points"));
    }
    if (actionPlotPareto) {
        actionPlotPareto->setText(tr("Pa&reto Chart"));
        actionPlotPareto->setToolTip(
                tr("Descending bar chart with cumulative percentage curve on Right Y"));
    }
    if (actionPlotQQ) {
        actionPlotQQ->setText(tr("Normal &Q-Q Plot"));
        actionPlotQQ->setToolTip(
                tr("Normal Quantile-Quantile probability plot with reference line"));
    }
    if (actionPlotTernary) {
        actionPlotTernary->setText(tr("&Ternary Plot (A-B-C)"));
        actionPlotTernary->setToolTip(
                tr("Equilateral triangle ternary plot from 3 columns (A+B+C=100%)"));
    }
    if (actionColorPaletteEditor) {
        actionColorPaletteEditor->setText(tr("Color &Palette Editor..."));
        actionColorPaletteEditor->setToolTip(
                tr("Edit and apply publication color palettes and multi-curve gradients"));
    }
    if (actionAspect1_1)
        actionAspect1_1->setText(tr("&1:1 (Square - ISIJ)"));
    if (actionAspect4_3)
        actionAspect4_3->setText(tr("&4:3 (Standard)"));
    if (actionAspect16_9)
        actionAspect16_9->setText(tr("1&6:9 (Widescreen)"));
    if (actionAspect3_2)
        actionAspect3_2->setText(tr("&3:2 (Classic)"));
    if (actionAspectCustom)
        actionAspectCustom->setText(tr("&Custom Aspect Ratio..."));
    if (actionKeepAspectRatio)
        actionKeepAspectRatio->setText(tr("&Keep Aspect Ratio on Resize"));
    if (actionTabbedWorkspaceView) {
        actionTabbedWorkspaceView->setText(tr("Tabbed Workspace &View"));
        actionTabbedWorkspaceView->setToolTip(
                tr("Switch workspace between Origin-style Tabbed View and SubWindows"));
    }
    if (actionLangJapanese) {
        actionLangJapanese->setText(tr("日本語 (&Japanese)"));
        actionLangJapanese->setChecked(appLanguage.startsWith("ja"));
    }
    if (actionLangEnglish) {
        actionLangEnglish->setText(tr("&English"));
        actionLangEnglish->setChecked(!appLanguage.startsWith("ja"));
    }
    if (actionCopyGraphOle) {
        actionCopyGraphOle->setText(ToriGraphOleUi::copyForPowerPointMenuText());
        actionCopyGraphOle->setToolTip(ToriGraphOleUi::copyForPowerPointToolTip());
        actionCopyGraphOle->setStatusTip(ToriGraphOleUi::copyForPowerPointToolTip());
    }
    if (actionOleEmbedWholeProject)
        actionOleEmbedWholeProject->setText(ToriGraphOleUi::embedWholeProjectMenuText());
    if (actionRepairOleRegistration)
        actionRepairOleRegistration->setText(ToriGraphOleUi::repairRegistrationMenuText());
    actionHomePage->setText(tr("&ToriGraph Homepage"));
#ifdef SEARCH_FOR_UPDATES
    actionCheckUpdates->setText(tr("Check for &Updates..."));
#endif
    actionHelpForums->setText(tr("Visit ToriGraph &Forums"));
    actionHelpBugReports->setText(tr("Report a &Bug"));
#ifdef DOWNLOAD_LINKS
    actionDownloadManual->setText(tr("Download &Manual"));
#endif

#ifdef SCRIPTING_DIALOG
    actionScriptingLang->setText(tr("Scripting &Language"));
#endif
    actionRestartScripting->setText(tr("&Restart Scripting"));

    actionNoteExecute->setText(tr("E&xecute"));
    actionNoteExecute->setShortcut(tr("Ctrl+J"));

    actionNoteExecuteAll->setText(tr("Execute &All"));
    actionNoteExecuteAll->setShortcut(tr("Ctrl+Shift+J"));

    actionNoteEvaluate->setText(tr("&Evaluate Expression"));
    actionNoteEvaluate->setShortcut(tr("Ctrl+Return"));

    btnPointer->setText(tr("Disable &tools"));
    btnPointer->setToolTip(tr("Pointer"));

    btnZoomIn->setText(tr("&Zoom In"));
    btnZoomIn->setShortcuts({ QKeySequence(tr("Ctrl++")), QKeySequence(QStringLiteral("Ctrl+=")) });
    btnZoomIn->setToolTip(tr("Zoom In"));

    btnZoomOut->setText(tr("Zoom &Out"));
    btnZoomOut->setShortcut(tr("Ctrl+-"));
    btnZoomOut->setToolTip(tr("Zoom Out"));

    btnCursor->setText(tr("&Data Reader"));
    btnCursor->setShortcut(tr("CTRL+D"));
    btnCursor->setToolTip(tr("Data reader"));

    btnSelect->setText(tr("&Select Data Range"));
    btnSelect->setShortcut(tr("Ctrl+Alt+S"));
    btnSelect->setToolTip(tr("Select data range"));

    btnPicker->setText(tr("S&creen Reader"));
    btnPicker->setToolTip(tr("Screen reader"));

    btnMovePoints->setText(tr("&Move Data Points..."));
    btnMovePoints->setShortcut(tr("Ctrl+ALT+M"));
    btnMovePoints->setToolTip(tr("Move data points"));

    btnRemovePoints->setText(tr("Remove &Bad Data Points..."));
    btnRemovePoints->setShortcut(tr("Ctrl+Shift+B"));
    btnRemovePoints->setToolTip(tr("Remove data points"));

    if (btnMaskPoints) {
        btnMaskPoints->setText(tr("&Mask Data Points (Click)"));
        btnMaskPoints->setShortcut(tr("Ctrl+Shift+M"));
        btnMaskPoints->setToolTip(
                tr("Mask data points on graph without deleting table values"));
    }
    if (btnUnmaskPoints) {
        btnUnmaskPoints->setText(tr("&Unmask Data Points (Click)"));
        btnUnmaskPoints->setShortcut(tr("Ctrl+Alt+U"));
        btnUnmaskPoints->setToolTip(tr("Unmask previously masked data points on graph"));
    }
    if (actionMaskRegionROI)
        actionMaskRegionROI->setText(tr("Mask &Region (ROI)"));
    if (actionUnmaskRegionROI)
        actionUnmaskRegionROI->setText(tr("Unmask Re&gion (ROI)"));
    if (actionClearAllMasks)
        actionClearAllMasks->setText(tr("&Clear All Masks on Graph"));

    if (actionGadgetVerticalCursor)
        actionGadgetVerticalCursor->setText(tr("&Vertical Cursor (All Layers)"));
    if (actionGadgetRegionStats)
        actionGadgetRegionStats->setText(tr("&Region Statistics (ROI)..."));
    if (actionGadgetQuickFit)
        actionGadgetQuickFit->setText(tr("Quick &Fit (ROI)..."));
    if (actionGadgetQuickPeaks)
        actionGadgetQuickPeaks->setText(tr("Quick &Peaks (ROI)..."));
    if (actionGadgetQuickSigmoidal)
        actionGadgetQuickSigmoidal->setText(tr("Quick &Sigmoidal Fit (ROI)..."));
    if (actionGadgetIntegrate)
        actionGadgetIntegrate->setText(tr("&Integrate (ROI)..."));
    if (actionGadgetInterpolate)
        actionGadgetInterpolate->setText(tr("In&terpolate (ROI)..."));
    if (actionGadgetFFT)
        actionGadgetFFT->setText(tr("FF&T Preview (ROI)..."));
    if (actionGadgetRiseTime)
        actionGadgetRiseTime->setText(tr("Rise &Time (Pro)..."));
    if (actionGadgetCluster)
        actionGadgetCluster->setText(tr("&Cluster Selector (Pro)..."));
    if (actionGadgetClear)
        actionGadgetClear->setText(tr("Clear Active &Gadget / ROI"));

    if (actionSimpleCurveMath)
        actionSimpleCurveMath->setText(tr("&Simple Curve Math (A +-*/ B)..."));
    if (actionSubtractStraightLine)
        actionSubtractStraightLine->setText(tr("Su&btract Straight Line..."));
    if (actionAverageMultipleCurves)
        actionAverageMultipleCurves->setText(tr("&Average Multiple Curves (Mean \xC2\xB1 SD)..."));
    if (actionNormalizeCurves)
        actionNormalizeCurves->setText(tr("&Normalize Curves..."));
    if (actionCurveIntersections)
        actionCurveIntersections->setText(tr("Curve &Intersections && X/Y Lookup..."));
    if (actionPolygonLoopArea)
        actionPolygonLoopArea->setText(tr("Calculate &Polygon / Loop Area"));

    if (actionFitApparentLinear)
        actionFitApparentLinear->setText(tr("&Apparent Linear Fit (Log Axes)"));
    if (actionFitYorkLinear)
        actionFitYorkLinear->setText(tr("Linear Fit with X && Y Errors (&York)..."));
    if (actionGlobalCurveFit)
        actionGlobalCurveFit->setText(tr("&Global Curve Fit (Shared Parameters)..."));
    if (actionImplicitODRFit)
        actionImplicitODRFit->setText(tr("&Implicit / ODR Curve Fit..."));
    if (actionModelComparison)
        actionModelComparison->setText(tr("&Compare && Rank Fit Models (AIC/BIC)..."));
    if (actionSurfaceFit3D)
        actionSurfaceFit3D->setText(tr("3D &Surface Fit (XYZ / Matrix)..."));
    if (actionBatchPeakAnalysis)
        actionBatchPeakAnalysis->setText(tr("&Batch Peak Analysis (All Curves)..."));
    if (action2DPeakAnalysis)
        action2DPeakAnalysis->setText(tr("&2D Peak Analysis (Matrix / Image)..."));

    if (actionSmoothLOWESS)
        actionSmoothLOWESS->setText(tr("&LOWESS Smooth..."));
    if (actionSmoothWhittaker)
        actionSmoothWhittaker->setText(tr("&Whittaker Smooth..."));
    if (actionSmoothMedian)
        actionSmoothMedian->setText(tr("M&edian Filter Smooth..."));
    if (actionIirDigitalFilter)
        actionIirDigitalFilter->setText(
                tr("&IIR Digital Filter (Butterworth/Chebyshev/Bessel)..."));
    if (actionStftSpectrogram)
        actionStftSpectrogram->setText(tr("&Short-Time Fourier Transform (STFT)..."));
    if (actionHilbertEnvelope)
        actionHilbertEnvelope->setText(tr("&Hilbert Transform (Envelope)..."));
    if (actionWaveletTransform)
        actionWaveletTransform->setText(tr("&Wavelet Transform (CWT / DWT Denoise)..."));
    if (actionWelchPsdCoherence)
        actionWelchPsdCoherence->setText(tr("Welch &PSD && Cross-Coherence..."));
    if (actionMatrixFFT2D)
        actionMatrixFFT2D->setText(tr("2D &FFT (Matrix)"));
    if (actionMatrixIFFT2D)
        actionMatrixIFFT2D->setText(tr("2D &IFFT (Inverse Matrix FFT)"));

    if (actionConfidenceEllipse)
        actionConfidenceEllipse->setText(tr("2D &Confidence / Prediction Ellipse..."));
    if (actionGroupComparison)
        actionGroupComparison->setText(tr("&t-Test / One-Way ANOVA + Significance Brackets..."));
    if (actionROCCurve)
        actionROCCurve->setText(tr("&ROC Curve (AUC)..."));
    if (actionKaplanMeier)
        actionKaplanMeier->setText(tr("&Kaplan-Meier Survival Analysis..."));
    if (actionPCABiplot)
        actionPCABiplot->setText(tr("&Principal Component Analysis (PCA Biplot)..."));

    if (actionRecalcAuto)
        actionRecalcAuto->setText(tr("&Auto Recalculate (Green Lock)"));
    if (actionRecalcManual)
        actionRecalcManual->setText(tr("&Manual Recalculate"));
    if (actionRecalcDisabled)
        actionRecalcDisabled->setText(tr("&Disable Recalculation"));
    if (actionRecalculateNow) {
        actionRecalculateNow->setText(tr("&Recalculate Pending Links Now"));
        actionRecalculateNow->setShortcut(tr("Ctrl+Shift+U"));
    }
    if (actionRepeatLinearFitAll)
        actionRepeatLinearFitAll->setText(tr("Repeat &Linear Fit for All Curves"));
    if (actionRepeatNormalizeAll)
        actionRepeatNormalizeAll->setText(tr("Repeat &Normalization for All Curves"));
    if (actionManageDialogThemes)
        actionManageDialogThemes->setText(tr("Manage Dialog &Themes..."));
    if (locktoolbar)
        locktoolbar->setText(tr("Loc&k Toolbars"));
    if (actionShowHistory)
        actionShowHistory->setText(tr("Undo/Redo &History"));
    if (actionSaveNote)
        actionSaveNote->setText(tr("&Save Note As..."));

    if (actionPageSetup)
        actionPageSetup->setText(tr("Page &Setup..."));
    if (actionFitPageToLayers)
        actionFitPageToLayers->setText(tr("Fit &Page to Layers"));
    if (actionFitLayersToPage)
        actionFitLayersToPage->setText(tr("Fit &Layers to Page"));
    if (actionTogglePageView)
        actionTogglePageView->setText(tr("Toggle Page &View"));
    if (actionAddLinkedLayer)
        actionAddLinkedLayer->setText(tr("Add Linked Top-&X / Right-Y Layer"));
    if (actionAddInsetLayer)
        actionAddInsetLayer->setText(tr("Add &Zoom Inset Layer"));
    if (actionFrameBoxInward)
        actionFrameBoxInward->setText(tr("&Box Frame (Inward Ticks)"));
    if (actionFrameBoxPlain)
        actionFrameBoxPlain->setText(tr("Box Frame (&Plain Top/Right)"));
    if (actionFrameOpenL)
        actionFrameOpenL->setText(tr("&Open L-Frame"));
    if (actionFrameZeroCrossed)
        actionFrameZeroCrossed->setText(tr("&Zero-Crossed Axes"));
    if (actionToggleMiniToolbar)
        actionToggleMiniToolbar->setText(tr("Toggle Graph Mini-Tool&bar"));

    if (actionGadgetDifferentiate)
        actionGadgetDifferentiate->setText(tr("&Differentiate Gadget..."));
    if (actionGadgetIntersect)
        actionGadgetIntersect->setText(tr("&Intersect Gadget..."));
    if (actionGadgetIntegrate2D)
        actionGadgetIntegrate2D->setText(tr("&2D Integration Gadget (Matrix/Contour)..."));

    if (actionPiecewiseLinearFit)
        actionPiecewiseLinearFit->setText(tr("&Piecewise Linear Fit (Breakpoint)..."));
    if (actionRobustLinearFit)
        actionRobustLinearFit->setText(tr("&Robust && Quantile Linear Fit..."));
    if (actionCompareDatasetsFit)
        actionCompareDatasetsFit->setText(tr("Compare &Datasets (F-Test / AIC)..."));
    if (actionMultipleLinearRegression)
        actionMultipleLinearRegression->setText(tr("&Multiple Linear Regression (MLR)..."));

    if (actionSavGolSmoothDerivative)
        actionSavGolSmoothDerivative->setText(tr("Savitzky-Golay Smooth &Derivative..."));
    if (actionPeakEnvelope)
        actionPeakEnvelope->setText(tr("Peak &Envelope (Upper / Lower)..."));
    if (actionDecimateResample)
        actionDecimateResample->setText(tr("&Decimate / Anti-Alias Resample..."));

    if (actionNormalityTest)
        actionNormalityTest->setText(tr("&Normality Test (Shapiro-Wilk / KS / AD)..."));
    if (actionBlandAltman)
        actionBlandAltman->setText(tr("&Bland-Altman Method Comparison..."));
    if (actionTwoWayANOVA)
        actionTwoWayANOVA->setText(tr("T&wo-Way ANOVA && Interaction Plot..."));
    if (actionGraphDigitizer)
        actionGraphDigitizer->setText(tr("&Digitizer (Extract Data from Image)..."));

    if (actionCopyGraphFormat) {
        actionCopyGraphFormat->setText(tr("Copy For&mat"));
        actionCopyGraphFormat->setShortcut(tr("Ctrl+Shift+C"));
        actionCopyGraphFormat->setToolTip(
                tr("Copy graph format (fonts, axes, ticks, page size, curve widths, reference lines)"));
    }
    if (actionPasteGraphFormat) {
        actionPasteGraphFormat->setText(tr("Paste F&ormat"));
        actionPasteGraphFormat->setShortcut(tr("Ctrl+Shift+V"));
        actionPasteGraphFormat->setToolTip(tr("Paste copied graph format onto active graph"));
    }
    if (actionBatchPlot) {
        actionBatchPlot->setText(tr("Duplicate (Batc&h Plotting)..."));
        actionBatchPlot->setToolTip(
                tr("Batch-clone graph across multiple worksheets or Y columns"));
    }
    if (actionPasteFormatFonts) {
        actionPasteFormatFonts->setText(tr("Paste &Fonts Only"));
        actionPasteFormatFonts->setToolTip(
                tr("Paste only font family, sizes, weights, and alignments onto active graph"));
    }
    if (actionPasteFormatColors) {
        actionPasteFormatColors->setText(tr("Paste &Colors && Symbols Only"));
        actionPasteFormatColors->setToolTip(
                tr("Paste only curve pens, symbol styles/sizes, and colors onto active graph"));
    }
    if (actionPasteFormatScales) {
        actionPasteFormatScales->setText(tr("Paste &Axis Scales Only"));
        actionPasteFormatScales->setToolTip(
                tr("Paste axis min/max ranges, tick steps, log10 scale, and numeric formats"));
    }
    if (actionPasteFormatFolder) {
        actionPasteFormatFolder->setText(tr("Paste Format to &All Graphs in Folder"));
        actionPasteFormatFolder->setToolTip(
                tr("Apply copied graph format to every 2D graph window in the current folder"));
    }
    if (actionMergeGraphs) {
        actionMergeGraphs->setText(tr("&Merge Graph Windows..."));
        actionMergeGraphs->setToolTip(
                tr("Merge 2 or more open graph windows into a publication multi-panel figure (a)(b)(c)(d)"));
    }
    if (actionStyleISIJ) {
        actionStyleISIJ->setText(tr("&ISIJ Journal (85 mm, 600 DPI)"));
        actionStyleISIJ->setToolTip(
                tr("Apply 1-column ISIJ / Metallurgy journal style (85 mm, 600 DPI, inward ticks, Arial)"));
    }
    if (actionStyleSlide) {
        actionStyleSlide->setText(tr("&Lab Slide (18 pt @ 140%, 83.3 mm)"));
        actionStyleSlide->setToolTip(
                tr("Apply lab presentation slide style (83.3 mm, 12.9 pt Arial -> 18 pt at 140% in PowerPoint)"));
    }
    if (actionApplyStyleToMultipleGraphs) {
        actionApplyStyleToMultipleGraphs->setText(
                tr("Apply Style Preset to &Multiple Graphs..."));
    }
    if (actionApplyTableTemplate)
        actionApplyTableTemplate->setText(tr("Plot Table Using &Template..."));
    if (actionReimportData)
        actionReimportData->setText(tr("Re-import &Source Data"));
    if (actionQuickLinearFit)
        actionQuickLinearFit->setText(tr("Quick &Linear Fit (No Dialog)"));
    if (actionGadgetDeltaCursor) {
        actionGadgetDeltaCursor->setText(tr("&Delta Cursor (C1-C2)..."));
        actionGadgetDeltaCursor->setToolTip(
                tr("Dual-crosshair Delta Cursor (dx, dy, slope, ratio, area, RMS)"));
    }
    if (actionLogisticRegression)
        actionLogisticRegression->setText(tr("Binary &Logistic Regression..."));
    if (actionTensileYield) {
        actionTensileYield->setText(tr("&Tensile / 0.2% Yield Analysis..."));
        actionTensileYield->setToolTip(
                tr("Tensile stress-strain analysis: Young's modulus E, 0.2% proof stress, UTS & True Stress-Strain"));
    }
    if (actionArrheniusPlot)
        actionArrheniusPlot->setText(tr("&Arrhenius Plot (Activation Energy)..."));
    if (actionTaucPlot)
        actionTaucPlot->setText(tr("&Tauc Plot (Optical Bandgap)..."));
    if (actionFillBetweenCurves)
        actionFillBetweenCurves->setText(tr("&Fill Area Between Curves..."));
    if (actionWaterfallStackOffset)
        actionWaterfallStackOffset->setText(tr("&Waterfall / Stack Offset..."));
    if (actionResetStackOffsets)
        actionResetStackOffsets->setText(tr("Reset Stac&k Offsets"));
    if (actionWeibullPlot)
        actionWeibullPlot->setText(tr("&Weibull Probability Plot..."));
    if (actionOutlierTest)
        actionOutlierTest->setText(tr("&Outlier Test (Grubbs / ESD / Dixon)..."));
    if (actionNonparametricTests)
        actionNonparametricTests->setText(
                tr("&Nonparametric Tests (Mann-Whitney / Kruskal-Wallis / Auto)..."));
    if (actionPairedComparisonPlot)
        actionPairedComparisonPlot->setText(tr("&Paired Comparison Plot (Before-After)..."));
    if (actionCorrelationMatrix)
        actionCorrelationMatrix->setText(tr("&Correlation Matrix Heatmap..."));
    if (actionClustering)
        actionClustering->setText(tr("&Clustering (K-Means / Dendrogram)..."));
    if (actionCopyPageImage) {
        actionCopyPageImage->setText(tr("Copy Pa&ge as Image (High-DPI PNG)"));
        actionCopyPageImage->setShortcut(tr("Ctrl+Alt+C"));
        actionCopyPageImage->setToolTip(
                tr("Copy active graph page to clipboard as a 300 DPI PNG image"));
    }
    if (actionGadgetImageProfile) {
        actionGadgetImageProfile->setText(
                tr("2D &Cross-Section Profile Gadget (Image / Heatmap)"));
        actionGadgetImageProfile->setToolTip(
                tr("Interactive horizontal/vertical 2D cross-section profile gadget for heatmaps and images"));
    }
    if (actionMatrixFilter2D) {
        actionMatrixFilter2D->setText(tr("2D &Spatial Filter && Plane Leveling..."));
        actionMatrixFilter2D->setToolTip(
                tr("Apply 2D Gaussian, median, Sobel/Laplacian filter or polynomial plane leveling to a matrix"));
    }
    if (actionFourierSelfDeconvolve) {
        actionFourierSelfDeconvolve->setText(
                tr("&Fourier Self-Deconvolution (Peak Sharpening)..."));
        actionFourierSelfDeconvolve->setToolTip(
                tr("Sharpen overlapping spectral peaks via Kauppinen Fourier self-deconvolution"));
    }
    if (actionEISAnalyzer) {
        actionEISAnalyzer->setText(tr("&EIS Nyquist && Bode Analyzer..."));
        actionEISAnalyzer->setToolTip(
                tr("Electrochemical Impedance Spectroscopy (Nyquist, Bode & Randles circuit fit)"));
    }
    if (actionSampleGallery) {
        actionSampleGallery->setText(tr("Sample Project &Gallery..."));
        actionSampleGallery->setToolTip(tr("Open Built-In Scientific Sample Project Gallery"));
    }
    if (actionWorkHardeningDialog) {
        actionWorkHardeningDialog->setText(tr("&Work Hardening && Considere Analysis..."));
        actionWorkHardeningDialog->setToolTip(
                tr("True Stress-Strain, Hollomon n, Crussard-Jaoul & Considere Necking"));
    }
    if (actionKissingerDialog) {
        actionKissingerDialog->setText(tr("&Kissinger / Thermal Desorption (TDS)..."));
        actionKissingerDialog->setToolTip(
                tr("Kissinger Thermal Desorption (TDS / DSC) Activation Energy Analysis"));
    }
    if (actionXRDAnalyzerDialog) {
        actionXRDAnalyzerDialog->setText(tr("&XRD Peak && Williamson-Hall Analyzer..."));
        actionXRDAnalyzerDialog->setToolTip(
                tr("Powder XRD K-alpha2 Stripping, Scherrer & Williamson-Hall Analysis"));
    }
    if (actionVaspPlotterDialog) {
        actionVaspPlotterDialog->setText(tr("&VASP Electronic Band && DOS Plotter..."));
        actionVaspPlotterDialog->setToolTip(
                tr("VASP EIGENVAL / DOSCAR Electronic Band Structure & DOS Plotter"));
    }
    if (actionExportPythonScript) {
        actionExportPythonScript->setText(tr("Export Reproducible &Python Script..."));
        actionExportPythonScript->setToolTip(
                tr("Generate Reproducible Python Script from Active Graph"));
    }
    if (actionPassingBablokDeming) {
        actionPassingBablokDeming->setText(tr("Passing-&Bablok && Deming Regression..."));
        actionPassingBablokDeming->setToolTip(
                tr("Method-comparison regression with errors in both X and Y (Passing-Bablok & Deming)"));
    }

    actionAddText->setText(tr("Add &Text"));
    actionAddText->setToolTip(tr("Add Text"));
    actionAddText->setShortcut(tr("Ctrl+Shift+T"));

    actionNextWindow->setText(tr("&Next", "next window"));
    actionPrevWindow->setText(tr("&Previous", "previous window"));

    btnArrow->setText(tr("Draw &Arrow"));
    btnArrow->setShortcut(tr("CTRL+ALT+A"));
    btnArrow->setToolTip(tr("Draw arrow"));

    btnLine->setText(tr("Draw &Line"));
    btnLine->setShortcut(tr("CTRL+ALT+L"));
    btnLine->setToolTip(tr("Draw line"));

    // FIXME: is setText necessary for action groups?
    //	coord->setText( tr( "Coordinates" ) );
    //	coord->setIconText( tr( "&Coord" ) );
    //  coord->setStatusTip( tr( "Coordinates" ) );
    Box->setText(tr("Box"));
    Box->setIconText(tr("Box"));
    Box->setToolTip(tr("Box"));
    Box->setStatusTip(tr("Box"));
    Frame->setText(tr("Frame"));
    Frame->setIconText(tr("&Frame"));
    Frame->setToolTip(tr("Frame"));
    Frame->setStatusTip(tr("Frame"));
    None->setText(tr("No Axes"));
    None->setIconText(tr("No Axes"));
    None->setToolTip(tr("No axes"));
    None->setStatusTip(tr("No axes"));

    front->setToolTip(tr("Front grid"));
    back->setToolTip(tr("Back grid"));
    right->setToolTip(tr("Right grid"));
    left->setToolTip(tr("Left grid"));
    ceil->setToolTip(tr("Ceiling grid"));
    floor->setToolTip(tr("Floor grid"));

    wireframe->setText(tr("Wireframe"));
    wireframe->setIconText(tr("Wireframe"));
    wireframe->setToolTip(tr("Wireframe"));
    wireframe->setStatusTip(tr("Wireframe"));
    hiddenline->setText(tr("Hidden Line"));
    hiddenline->setIconText(tr("Hidden Line"));
    hiddenline->setToolTip(tr("Hidden line"));
    hiddenline->setStatusTip(tr("Hidden line"));
    polygon->setText(tr("Polygon Only"));
    polygon->setIconText(tr("Polygon Only"));
    polygon->setToolTip(tr("Polygon only"));
    polygon->setStatusTip(tr("Polygon only"));
    filledmesh->setText(tr("Mesh && Filled Polygons"));
    filledmesh->setIconText(tr("Mesh && Filled Polygons"));
    filledmesh->setToolTip(tr("Mesh & filled Polygons"));
    filledmesh->setStatusTip(tr("Mesh & filled Polygons"));
    pointstyle->setText(tr("Dots"));
    pointstyle->setIconText(tr("Dots"));
    pointstyle->setToolTip(tr("Dots"));
    pointstyle->setStatusTip(tr("Dots"));
    barstyle->setText(tr("Bars"));
    barstyle->setIconText(tr("Bars"));
    barstyle->setToolTip(tr("Bars"));
    barstyle->setStatusTip(tr("Bars"));
    conestyle->setText(tr("Cones"));
    conestyle->setIconText(tr("Cones"));
    conestyle->setToolTip(tr("Cones"));
    conestyle->setStatusTip(tr("Cones"));
    crossHairStyle->setText(tr("Crosshairs"));
    crossHairStyle->setIconText(tr("Crosshairs"));
    crossHairStyle->setToolTip(tr("Crosshairs"));
    crossHairStyle->setStatusTip(tr("Crosshairs"));

    // floorstyle->setText( tr( "Floor Style" ) );
    // floorstyle->setIconText( tr( "Floor Style" ) );
    // floorstyle->setStatusTip( tr( "Floor Style" ) );
    floordata->setText(tr("Floor Data Projection"));
    floordata->setIconText(tr("Floor Data Projection"));
    floordata->setToolTip(tr("Floor data projection"));
    floordata->setStatusTip(tr("Floor data projection"));
    flooriso->setText(tr("Floor Isolines"));
    flooriso->setIconText(tr("Floor Isolines"));
    flooriso->setToolTip(tr("Floor isolines"));
    flooriso->setStatusTip(tr("Floor isolines"));
    floornone->setText(tr("Empty Floor"));
    floornone->setIconText(tr("Empty Floor"));
    floornone->setToolTip(tr("Empty floor"));
    floornone->setStatusTip(tr("Empty floor"));

    actionAnimate->setText(tr("Animation"));
    actionAnimate->setIconText(tr("Animation"));
    actionAnimate->setToolTip(tr("Animation"));
    actionAnimate->setStatusTip(tr("Animation"));

    actionPerspective->setText(tr("Enable perspective"));
    actionPerspective->setIconText(tr("Enable perspective"));
    actionPerspective->setToolTip(tr("Enable perspective"));
    actionPerspective->setStatusTip(tr("Enable perspective"));

    actionResetRotation->setText(tr("Reset rotation"));
    actionResetRotation->setIconText(tr("Reset rotation"));
    actionResetRotation->setToolTip(tr("Reset rotation"));
    actionResetRotation->setStatusTip(tr("Reset rotation"));

    actionFitFrame->setText(tr("Fit frame to window"));
    actionFitFrame->setIconText(tr("Fit frame to window"));
    actionFitFrame->setToolTip(tr("Fit frame to window"));
    actionFitFrame->setStatusTip(tr("Fit frame to window"));

    if (actionCommandPalette) {
        actionCommandPalette->setText(tr("Command &Palette..."));
        actionCommandPalette->setToolTip(tr("Open Command Palette (Ctrl+K)"));
    }
    if (actionToggleFocusHudMode) {
        actionToggleFocusHudMode->setText(tr("&Focus HUD Mode"));
        actionToggleFocusHudMode->setToolTip(tr("Toggle Focus HUD Mode (F11)"));
    }
    if (actionToggleGraphInspector) {
        actionToggleGraphInspector->setText(tr("&Graph Inspector Dock"));
        actionToggleGraphInspector->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_I));
        actionToggleGraphInspector->setToolTip(
                tr("Toggle Graph Inspector Dock (Ctrl+Shift+I)"));
    }
    if (QToolButton *statusCmdHintBtn =
                statusBar()->findChild<QToolButton *>(QStringLiteral("StatusBarCommandHintBtn"))) {
        statusCmdHintBtn->setText(tr("Ctrl+K Search Commands"));
        statusCmdHintBtn->setToolTip(tr("Open Fuzzy-Search Command Palette (Ctrl+K)"));
    }
    if (actionWorkspaceLayoutTabbed)
        actionWorkspaceLayoutTabbed->setText(tr("&Tabbed Workspace"));
    if (actionWorkspaceLayoutSplit)
        actionWorkspaceLayoutSplit->setText(tr("&Split View (Table + Graph)"));
    if (actionWorkspaceLayoutMdi)
        actionWorkspaceLayoutMdi->setText(tr("Free &MDI Windows"));
    if (actionOpenPythonConsole) {
        actionOpenPythonConsole->setText(tr("Open &Python Console..."));
        actionOpenPythonConsole->setToolTip(tr("Open Interactive In-Process Python Console"));
    }
    if (actionRunPythonScript) {
        actionRunPythonScript->setText(tr("Run Python &Script..."));
        actionRunPythonScript->setToolTip(tr("Select and Execute a .py Automation Script"));
    }
    retranslateUxSwitches(); // G1 / C15 switches (UxNavigation.cpp)

    if (!d_in_insert_translated_strings)
        ModernUiTheme::upgradeAllWindowIcons(this);
}

void ApplicationWindow::showFolderPopupMenu(const QPoint &p, bool fromFolders)
{
    if (isNonInteractiveContext(this))
        return;

    QMenu *cm = nullptr;
    if (fromFolders) {
        cm = showFolderPopupMenuImpl(folders.itemAt(p), fromFolders);
        if (cm)
            cm->exec(folders.mapToGlobal(p));
    } else {
        cm = showFolderPopupMenuImpl(lv.itemAt(p), fromFolders);
        if (cm)
            cm->exec(lv.mapToGlobal(p));
    }
}

QMenu *ApplicationWindow::showFolderPopupMenuImpl(QTreeWidgetItem *it, bool fromFolders)
{
    if (!it || folders.isRenaming())
        return nullptr;

    QMenu *cm = new QMenu(this);
    cm->addAction(tr("&Find..."), this, SLOT(showFindDialogue()));
    cm->addSeparator();
    QAction *appendProjAct = cm->addAction(tr("App&end Project..."), this, SLOT(appendProject()));
    if (appendProjAct && d_ole_embedded)
        appendProjAct->setEnabled(false);
    if (((FolderListItem *)it)->folder()->parent())
        cm->addAction(tr("Save &As Project..."), this, SLOT(saveAsProject()));
    else
        cm->addAction(tr("Save Project &As..."), this, SLOT(saveProjectAs()));
    cm->addSeparator();

    if (fromFolders && show_windows_policy != HideAll) {
        cm->addAction(tr("&Show All Windows"), this, SLOT(showAllFolderWindows()));
        cm->addAction(tr("&Hide All Windows"), this, SLOT(hideAllFolderWindows()));
        cm->addSeparator();
    }

    if (((FolderListItem *)it)->folder()->parent()) {
        cm->addAction(IconLoader::duotoneIcon("delete"), tr("&Delete Folder"), this,
                      SLOT(deleteFolder()), Qt::Key_F8);
        cm->addAction(tr("&Rename"), this, SLOT(startRenameFolder()), Qt::Key_F2);
        cm->addSeparator();
    }

    if (fromFolders) {
        QMenu *window = cm->addMenu(tr("New &Window"));
        window->addAction(actionNewTable);
        window->addAction(WorkbookManager::instance(this)->newWorkbookAction());
        window->addAction(actionNewMatrix);
        window->addAction(actionNewNote);
        window->addAction(actionNewGraph);
        window->addAction(actionNewFunctionPlot);
        window->addAction(actionNewSurfacePlot);
    }

    cm->addAction(IconLoader::duotoneIcon("folder-closed"), tr("New F&older"), this,
                  SLOT(addFolder()), Qt::Key_F7);
    cm->addSeparator();

    QMenu *viewWindowsMenu = cm->addMenu(tr("&View Windows"));
    QStringList lst;
    lst << tr("&None") << tr("&Windows in Active Folder")
        << tr("Windows in &Active Folder && Subfolders");
    for (int i = 0; i < 3; ++i) {
        QAction *actId = viewWindowsMenu->addAction(lst[i], this, SLOT(setShowWindowsPolicy(bool)));
        actId->setData(i);
        actId->setCheckable(true);
        actId->setChecked(show_windows_policy == i);
    }
    cm->addSeparator();
    cm->addAction(tr("&Properties..."), this, SLOT(folderProperties()));
    ModernUiTheme::upgradeMenuIcons(cm);
    return cm;
}

QMenu *ApplicationWindow::createToolbarsMenu()
{
    QMenu *menu = 0;
    QList<QToolBar *> toolbars = this->findChildren<QToolBar *>();
    if (toolbars.size()) {
        menu = new QMenu(this);
        foreach (QToolBar *toolbar, toolbars) {
            if (!toolbar || toolbar == d_ribbon_toolbar
                || toolbar->objectName() == QLatin1String("ModernRibbonToolBar"))
                continue;
            if (toolbar->parentWidget() == this)
                menu->addAction(toolbar->toggleViewAction());
        }
    }
    return menu;
}

QMenu *ApplicationWindow::createPopupMenu()
{
    QMenu *menu = QMainWindow::createPopupMenu();
    if (menu && d_ribbon_toolbar && d_ribbon_toolbar->toggleViewAction())
        menu->removeAction(d_ribbon_toolbar->toggleViewAction());
    return menu;
}

QMenu *ApplicationWindow::showWindowMenuImpl(MyWidget *widget)
{
    d_workspace.setActiveSubWindow(widget); // FIXME not user-friendly, but can't be simply removed

    QMenu *cm = new QMenu(this);
    QMenu *depend_menu = new QMenu(this);

    if (widget->inherits("Table"))
        cm->addAction(actionShowExportASCIIDialog);
    else if (widget->inherits("Note"))
        cm->addAction(actionSaveNote);
    else
        cm->addAction(actionSaveTemplate);
    cm->addAction(actionPrint);
    cm->addAction(actionCopyWindow);
    cm->addSeparator();
    cm->addAction(actionRename);
    cm->addAction(actionCloseWindow);
    if (!hidden(widget))
        cm->addAction(actionHideActiveWindow);
    cm->addAction(actionActivateWindow);
    cm->addAction(actionMinimizeWindow);
    cm->addAction(actionMaximizeWindow);
    cm->addAction(actionResizeWindow);
    cm->addSeparator();
    cm->addAction(tr("&Properties..."), this, SLOT(windowProperties()));
    if (widget->inherits("Table"))
        WorkbookManager::instance(this)->populateWindowMenu(cm, widget); // D8

    int n;
    if (widget->inherits("Table")) {
        QStringList graphs = dependingPlots(widget->name());
        n = graphs.count();
        if (n > 0) {
            cm->addSeparator();
            for (int i = 0; i < n; i++)
                depend_menu->addAction(graphs[i], this, SLOT(setActiveWindowFromAction()));

            depend_menu->setTitle(tr("D&epending Graphs"));
            cm->addMenu(depend_menu);
        }
    } else if (widget->inherits("Matrix")) {
        QStringList graphs = depending3DPlots((Matrix *)widget);
        n = graphs.count();
        if (n > 0) {
            cm->addSeparator();
            for (int i = 0; i < n; i++)
                depend_menu->addAction(graphs[i], this, SLOT(setActiveWindowFromAction()));

            depend_menu->setTitle(tr("D&epending 3D Graphs"));
            cm->addMenu(depend_menu);
        }
    } else if (widget->inherits("MultiLayer")) {
        cm->addSeparator();
        cm->addAction(actionPeakFitDialog);
        cm->addAction(actionAutoPeakGauss);
        cm->addAction(actionAutoPeakLorentz);
        cm->addAction(actionAutoPeakPseudoVoigt);
        cm->addSeparator();
        cm->addAction(actionUnzoom);
        cm->addAction(actionCopyPageImage);
        cm->addAction(actionCopyGraphOle);
        QStringList tbls = multilayerDependencies(widget);
        n = tbls.count();
        if (n > 0) {
            cm->addSeparator();
            for (int i = 0; i < n; i++)
                depend_menu->addAction(tbls[i], this, SLOT(setActiveWindowFromAction()));

            depend_menu->setTitle(tr("D&epends on"));
            cm->addMenu(depend_menu);
        }
    } else if (widget->inherits("Graph3D")) {
        Graph3D *sp = (Graph3D *)widget;
        Matrix *m = sp->matrix();
        QString formula = sp->formula();
        if (!formula.isEmpty()) {
            cm->addSeparator();
            if (formula.contains("_")) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                QStringList tl = formula.split("_", Qt::SkipEmptyParts);
#else
                QStringList tl = formula.split("_", QString::SkipEmptyParts);
#endif

                depend_menu->addAction(tl[0], this, SLOT(setActiveWindowFromAction()));

                depend_menu->setTitle(tr("D&epends on"));
                cm->addMenu(depend_menu);
            } else if (m) {
                depend_menu->addAction(m->name(), this, SLOT(setActiveWindowFromAction()));
                depend_menu->setTitle(tr("D&epends on"));
                cm->addMenu(depend_menu);
            }
        }
    }

    ModernUiTheme::upgradeMenuIcons(cm);
    return cm;
}

void ApplicationWindow::rebuildStylePresetsMenu()
{
    if (!stylePresetsMenu)
        return;
    qDeleteAll(stylePresetsMenu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
    stylePresetsMenu->clear();
    if (actionStyleISIJ)
        stylePresetsMenu->addAction(actionStyleISIJ);
    if (actionStyleSlide)
        stylePresetsMenu->addAction(actionStyleSlide);

    QString legacyId;
    QString legacyLabel;
    const QVector<MultiLayer::GraphStylePresetSpec> presets = MultiLayer::listStylePresets();
    for (const MultiLayer::GraphStylePresetSpec &sp : presets) {
        if (sp.id == QLatin1String("isij_85mm") || sp.id == QLatin1String("lab_slide_83mm"))
            continue;
        if (sp.id == QLatin1String("slide_28pt")) {
            legacyId = sp.id;
            legacyLabel = tr("Large Slide (28 pt, 185 mm)");
            continue;
        }
        const QString presetId = sp.id;
        const QString disp = (sp.id == QLatin1String("journal_2col_175mm"))
                ? tr("Journal 2-Column (175 mm)")
                : sp.name;
        stylePresetsMenu->addAction(disp, this,
                                    [this, presetId]() { applyStylePresetById(presetId); });
    }
    if (!legacyId.isEmpty()) {
        QMenu *moreMenu = stylePresetsMenu->addMenu(tr("More / &Legacy Presets"));
        moreMenu->setFont(appFont);
        moreMenu->addAction(legacyLabel, this,
                            [this, legacyId]() { applyStylePresetById(legacyId); });
    }
    stylePresetsMenu->addSeparator();
    if (actionApplyStyleToMultipleGraphs)
        stylePresetsMenu->addAction(actionApplyStyleToMultipleGraphs);
    stylePresetsMenu->addAction(tr("Manage Style Presets..."), this,
                                SLOT(showStylePresetManagerDialog()));
}
