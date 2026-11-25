/***************************************************************************
        File                 : ApplicationWindow_Plots.cpp
        Project              : SciDAVis
        Description          : SciDAVis's main window (2D/3D plots, curves, layers, graph dialogs, interactions & column MIME drop)
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

namespace {

// Reads the raw QObject* stored in a dialog's "targetMultiLayerPtr" property WITHOUT touching the
// object. QVariant::value<QObject *>() runs qobject_cast on the stored pointer, which dereferences
// it - a use-after-free when the target window has already been deleted (the dialog outlives it).
const QObject *storedTargetPointer(const QVariant &v)
{
    if (!v.isValid() || v.userType() != QMetaType::QObjectStar)
        return nullptr;
    return *static_cast<QObject *const *>(v.constData());
}

// Acklam's rational approximation for the standard normal inverse CDF \Phi^{-1}(p)
double inverseStandardNormalCDF(double p)
{
    if (p <= 0.0)
        return -8.0;
    if (p >= 1.0)
        return 8.0;

    static const double a[] = { -3.969683028665376e+01, 2.209460984245205e+02,
                                -2.759285104469687e+02, 1.383577518672690e+02,
                                -3.066479806614716e+01, 2.506628277459239e+00 };
    static const double b[] = { -5.447609879822406e+01, 1.615858368580409e+02,
                                -1.556989798598866e+02, 6.680131188771972e+01,
                                -1.328068155288572e+01 };
    static const double c[] = { -7.784894002430293e-03, -3.223964580411365e-01,
                                -2.400758277161838e+00, -2.549732539343734e+00,
                                 4.374664141464968e+00,  2.938163982698783e+00 };
    static const double d[] = { 7.784695709041462e-03, 3.224671290700398e-01,
                                2.445134137142996e+00, 3.754408661907416e+00 };

    const double pLow = 0.02425;
    const double pHigh = 1.0 - pLow;

    if (p < pLow) {
        double q = std::sqrt(-2.0 * std::log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5])
                / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else if (p <= pHigh) {
        double q = p - 0.5;
        double r = q * q;
        return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q
                / (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    } else {
        double q = std::sqrt(-2.0 * std::log(1.0 - p));
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5])
                / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
}

} // anonymous namespace

void ApplicationWindow::plot3DRibbon()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *table = static_cast<Table *>(d_workspace.activeSubWindow());
    if (table->selectedColumns().count() == 1) {
        if (!validFor3DPlot(table))
            return;
        dataPlot3D(table, table->colName(table->firstSelectedColumn()));
    } else if (!isNonInteractiveContext(this))
        QMessageBox::warning(this, tr("Plot error"),
                             tr("You must select exactly one column for plotting!"));
}

void ApplicationWindow::plot3DWireframe()
{
    plot3DMatrix(Qwt3D::WIREFRAME);
}

void ApplicationWindow::plot3DHiddenLine()
{
    plot3DMatrix(Qwt3D::HIDDENLINE);
}

void ApplicationWindow::plot3DPolygons()
{
    plot3DMatrix(Qwt3D::FILLED);
}

void ApplicationWindow::plot3DWireSurface()
{
    plot3DMatrix(Qwt3D::FILLEDMESH);
}

void ApplicationWindow::plot3DBars()
{
    MyWidget *w = (MyWidget *)d_workspace.activeSubWindow();
    if (!w)
        return;

    if (w->inherits("Table")) {
        Table *table = static_cast<Table *>(w);

        if (table->selectedColumns().count() == 1) {
            if (!validFor3DPlot(table))
                return;
            dataPlotXYZ(table, table->colName(table->firstSelectedColumn()), Graph3D::Bars);
        } else if (!isNonInteractiveContext(this))
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("You must select exactly one column for plotting!"));
    } else if (w->inherits("Matrix"))
        plot3DMatrix(Qwt3D::USER);
}

void ApplicationWindow::plot3DScatter()
{
    MyWidget *w = (MyWidget *)d_workspace.activeSubWindow();
    if (!w)
        return;

    if (w->inherits("Table")) {
        Table *table = static_cast<Table *>(w);

        if (table->selectedColumns().count() == 1) {
            if (!validFor3DPlot(table))
                return;
            dataPlotXYZ(table, table->colName(table->firstSelectedColumn()), Graph3D::Scatter);
        } else if (!isNonInteractiveContext(this))
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("You must select exactly one column for plotting!"));
    } else if (w->inherits("Matrix"))
        plot3DMatrix(Qwt3D::POINTS);
}

void ApplicationWindow::plot3DTrajectory()
{
    MyWidget *w = (MyWidget *)d_workspace.activeSubWindow();
    if (!w)
        return;

    if (w->inherits("Table")) {
        Table *table = static_cast<Table *>(w);

        if (table->selectedColumns().count() == 1) {
            if (!validFor3DPlot(table))
                return;
            dataPlotXYZ(table, table->colName(table->firstSelectedColumn()), Graph3D::Trajectory);
        } else if (!isNonInteractiveContext(this))
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("You must select exactly one column for plotting!"));
    }
}

void ApplicationWindow::plotPie()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *table = static_cast<Table *>(d_workspace.activeSubWindow());

    QStringList s = table->selectedColumns();
    if (s.isEmpty() && !table->YColumns().isEmpty())
        s << table->YColumns().first();

    if (s.count() != 1) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("You must select exactly one column for plotting!"));
        }
        return;
    }
    if (table->noXColumn()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(0, tr("Error"),
                                  tr("Please set a default X column for this table, first!"));
        }
        return;
    }

    const int startRow = (table->selectedRowCount(false) >= 2) ? table->firstSelectedRow(false) : 0;
    const int endRow = (table->selectedRowCount(false) >= 2) ? table->lastSelectedRow(false) : -1;
    multilayerPlot(table, s, Graph::Pie, startRow, endRow);
}

void ApplicationWindow::plotVectXYXY()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *table = static_cast<Table *>(d_workspace.activeSubWindow());

    if (!validFor2DPlot(table, Graph::VectXYXY))
        return;

    QStringList s = table->selectedColumns();
    if (s.count() == 4) {
        multilayerPlot(table, s, Graph::VectXYXY, table->firstSelectedRow(),
                       table->lastSelectedRow());
    } else if (!isNonInteractiveContext(this))
        QMessageBox::warning(this, tr("Error"),
                             tr("Please select four columns for this operation!"));
}

void ApplicationWindow::plotVectXYAM()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *table = static_cast<Table *>(d_workspace.activeSubWindow());

    if (!validFor2DPlot(table, Graph::VectXYAM))
        return;

    QStringList s = table->selectedColumns();
    if (s.count() == 4) {
        multilayerPlot(table, s, Graph::VectXYAM, table->firstSelectedRow(),
                       table->lastSelectedRow());
    } else if (!isNonInteractiveContext(this))
        QMessageBox::warning(this, tr("Error"),
                             tr("Please select four columns for this operation!"));
}

void ApplicationWindow::plotDoubleY()
{
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w) {
        showUxHint(tr("Double-Y plot requires at least two Y columns or a graph with two or more curves."));
        return;
    }

    MultiLayer *ml = nullptr;
    bool createdNewMl = false;
    QString rightTitle;
    if (w->inherits("Table")) {
        Table *t = static_cast<Table *>(w);
        if (t->noXColumn()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("You need at least one X column for this operation!"));
            }
            return;
        }
        QStringList cols = t->selectedYColumns();
        if (cols.count() < 2) {
            // Try all Y columns if fewer than 2 selected
            cols.clear();
            for (int i = 0; i < t->numCols(); ++i) {
                if (t->colPlotDesignation(i) == SciDAVis::Y)
                    cols << t->colName(i);
            }
        }
        if (cols.count() < 2) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::warning(this, tr("Plot error"),
                                     tr("Please select at least two Y columns for a Double-Y plot!"));
            } else {
                showUxHint(tr("Please select at least two Y columns for a Double-Y plot!"));
            }
            return;
        }
        int startRow = (t->selectedRowCount(false) >= 2) ? t->firstSelectedRow(false) : 0;
        int endRow = (t->selectedRowCount(false) >= 2) ? t->lastSelectedRow(false)
                                                       : (t->numRows() - 1);
        if (startRow < 0 || endRow < startRow) {
            startRow = 0;
            endRow = t->numRows() - 1;
        }
        const int rightColIdx = t->colIndex(cols[1]);
        if (Column *rightCol = t->column(rightColIdx)) {
            rightTitle = rightCol->formattedAxisTitle().trimmed();
        }
        if (rightTitle.isEmpty())
            rightTitle = t->colLabel(rightColIdx);
        ml = multilayerPlot(t, cols, Graph::LineSymbols, startRow, endRow);
        createdNewMl = (ml != nullptr);
    } else if (w->inherits("MultiLayer")) {
        ml = static_cast<MultiLayer *>(w);
    }

    if (!ml)
        return;
    Graph *g = ml->activeGraph();
    if (!g || g->curves() < 2) {
        showUxHint(tr("Double-Y plot needs at least two curves on the graph. Add another Y "
                      "column to this graph first, or select two Y columns in a worksheet."));
        return;
    }

    for (Graph *child : ml->layersList()) {
        if (child && child->linkedParentLayer() == g) {
            showUxHint(tr("This graph layer already uses a linked top/right overlay layer."));
            return;
        }
    }

    // Bind selected curve (if valid and > 0) or curve 1 to Right Y axis
    int rightCurveIdx = 1;
    const QString selCurveTitle = g->selectedCurveTitle();
    if (!selCurveTitle.isNull()) {
        const int selIdx = g->curveIndex(selCurveTitle);
        if (selIdx > 0 && g->curveType(selIdx) != Graph::ErrorBars)
            rightCurveIdx = selIdx;
    }
    QwtPlotCurve *rightCurve = g->curve(rightCurveIdx);
    if (!rightCurve)
        return;

    const QString beforeSnap = createdNewMl ? QString() : ml->captureUndoSnapshot();
    // 凡例はデータ座標で位置を持つので、左軸の尺度が変わる前の画面上の位置を覚えておく。
    const QPoint legendPxBefore = g->legend() ? g->legend()->rect().topLeft() : QPoint();

    rightCurve->setYAxis(QwtPlot::yRight);
    g->enableAxis(QwtPlot::yRight, true);
    g->setAxisBackboneVisible(QwtPlot::yRight, true);
    {
        QStringList tickLbls = g->enabledTickLabels();
        if (tickLbls.size() > QwtPlot::yRight) {
            tickLbls[QwtPlot::yRight] = QStringLiteral("1");
            g->setEnabledTickLabels(tickLbls);
        } else if (QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(QwtPlot::yRight)) {
            sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        }
        int rMajType = ScaleDraw::In;
        int rMinType = ScaleDraw::In;
        if (Plot *pw = g->plotWidget()) {
            const QList<int> majTypes = pw->getMajorTicksType();
            const QList<int> minTypes = pw->getMinorTicksType();
            if (majTypes.size() > QwtPlot::yLeft)
                rMajType = majTypes[QwtPlot::yLeft];
            if (minTypes.size() > QwtPlot::yLeft)
                rMinType = minTypes[QwtPlot::yLeft];
        }
        g->setAxisTicksLength(QwtPlot::yRight, rMajType, rMinType, g->minorTickLength(),
                              g->majorTickLength());
    }
    if (rightTitle.isEmpty()) {
        if (DataCurve *rdc = dynamic_cast<DataCurve *>(rightCurve)) {
            if (Table *rt = rdc->table()) {
                if (Column *rc = rt->column(rt->colIndex(rdc->yColumnName())))
                    rightTitle = rc->formattedAxisTitle().trimmed();
            }
        }
    }
    if (rightTitle.isEmpty())
        rightTitle = rightCurve->title().text();
    {
        QFont rf = g->axisTitleFont(QwtPlot::yRight);
        rf.setFamily("Arial");
        g->setRightAxisTitle(rightTitle);
        g->setAxisTitleFont(QwtPlot::yRight, rf);
    }
    // Also refresh Left Y axis title now that curve 1 moved to Right Y axis
    {
        QList<Column *> leftCols;
        for (int ci = 0; ci < g->curves(); ++ci) {
            QwtPlotCurve *pc = g->curve(ci);
            DataCurve *dc = dynamic_cast<DataCurve *>(pc);
            if (dc && dc->type() != Graph::ErrorBars && pc->yAxis() == QwtPlot::yLeft && dc->table()) {
                if (Column *yc = dc->table()->column(dc->table()->colIndex(dc->yColumnName())))
                    leftCols << yc;
            }
        }
        if (leftCols.size() == 1) {
            QString lt = leftCols.first()->formattedAxisTitle().trimmed();
            if (lt.isEmpty())
                lt = leftCols.first()->name();
            if (!lt.isEmpty()) {
                QFont lf = g->axisTitleFont(QwtPlot::yLeft);
                lf.setFamily("Arial");
                g->setYAxisTitle(lt);
                g->setAxisTitleFont(QwtPlot::yLeft, lf);
            }
        }
    }

    g->setAutoScale();

    // Compute padded scale for Right Y axis from curves attached to yRight
    double yMin = DBL_MAX, yMax = -DBL_MAX;
    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (c && c->yAxis() == QwtPlot::yRight && c->dataSize() > 0) {
            for (int j = 0; j < (int)c->dataSize(); ++j) {
                const double yv = c->y(j);
                if (std::isfinite(yv)) {
                    if (yv < yMin)
                        yMin = yv;
                    if (yv > yMax)
                        yMax = yv;
                }
            }
        }
    }
    if (yMin <= yMax) {
        double span = yMax - yMin;
        if (span <= 0.0)
            span = (std::abs(yMax) > 0.0) ? std::abs(yMax) * 0.1 : 1.0;
        g->setScale(QwtPlot::yRight, yMin - 0.05 * span, yMax + 0.05 * span);
    }
    g->syncAxisColorToCurve(QwtPlot::yRight, rightCurveIdx);
    g->replot();
    // 凡例は 2 本とも左軸に載った広い範囲のデータ座標で置かれている。右の曲線を移して
    // 左軸の範囲が縮むと、同じ座標がページの外へ押し出される。新しく作ったグラフでは
    // 尺度が決まった後に自動配置で置き直し、既存のグラフでは利用者が置いた画面上の
    // 位置を保つ。
    if (Legend *leg = g->legend()) {
        if (createdNewMl)
            g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
        else
            leg->setOrigin(legendPxBefore);
        g->replot();
    }
    if (!createdNewMl && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Convert to Double-Y Plot"), beforeSnap);
    else if (createdNewMl && ml->undoStack())
        ml->undoStack()->clear();
    emit ml->modifiedPlot();
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
        d_graph_inspector_dock->refreshAll();
    updateUndoRedoActionsForActiveWindow();
    emit modified();
}

void ApplicationWindow::plotYOffsetStack()
{
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w)
        return;

    if (w->inherits("Table")) {
        Table *t = static_cast<Table *>(w);
        if (!validFor2DPlot(t, Graph::Line))
            return;
        QStringList cols = (t->selectedColumnCount(SciDAVis::Y) >= 1)
                ? t->drawableColumnSelection()
                : QStringList();
        if (cols.isEmpty()) {
            for (int ci = 0; ci < t->numCols(); ++ci) {
                const SciDAVis::PlotDesignation pd = t->colPlotDesignation(ci);
                if (pd == SciDAVis::Y || pd == SciDAVis::yErr || pd == SciDAVis::xErr)
                    cols << t->colName(ci);
            }
        }
        if (cols.isEmpty()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::warning(this, tr("Plot error"),
                                     tr("Please select one or more Y columns to stack!"));
            }
            return;
        }
        const int sr = (t->selectedRowCount(false) >= 2) ? t->firstSelectedRow(false) : 0;
        const int er = (t->selectedRowCount(false) >= 2) ? t->lastSelectedRow(false) : -1;
        MultiLayer *ml = multilayerPlot(t, cols, Graph::Line, sr, er);
        if (!ml)
            return;
        Graph *g = ml->activeGraph();
        if (!g || g->curves() < 1)
            return;
        // Apply non-destructive display-time stack offset (never mutate source Table cells!)
        g->setStackOffsets(1, 15.0, 0.0, false);
        g->setProperty("stackOffsetMode", 1);
        g->setProperty("yStackOffsetPercent", 15.0);
        g->setProperty("xStackOffsetPercent", 0.0);
        g->setProperty("normalizeStackZeroOne", false);
        g->setProperty("stackAutoscale", true);
        g->setProperty("stackReverseOrder", false);
        emit modified();
        if (!isNonInteractiveContext(this))
            showStackOffsetsDialog();
        return;
    }

    if (w->inherits("MultiLayer")) {
        showStackOffsetsDialog();
    }
}

void ApplicationWindow::plotViolin()
{
    MyWidget *w = qobject_cast<MyWidget *>(d_workspace.activeSubWindow());
    if (!w)
        return;

    MultiLayer *ml = nullptr;
    if (w->inherits("Table")) {
        Table *t = static_cast<Table *>(w);
        QStringList cols = t->selectedYColumns();
        if (cols.isEmpty())
            cols = t->selectedColumns();
        if (cols.isEmpty())
            cols = t->YColumns();
        if (cols.isEmpty() && t->numCols() > 0)
            cols << t->colName(t->numCols() - 1);
        if (cols.isEmpty()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::warning(this, tr("Plot error"),
                                     tr("Please select at least one column for a Violin plot!"));
            }
            return;
        }
        int startRow = (t->selectedRowCount(false) >= 2) ? t->firstSelectedRow(false) : 0;
        int endRow = (t->selectedRowCount(false) >= 2) ? t->lastSelectedRow(false)
                                                       : (t->numRows() - 1);
        if (startRow < 0 || endRow < startRow) {
            startRow = 0;
            endRow = t->numRows() - 1;
        }
        ml = multilayerPlot(t, cols, Graph::Box, startRow, endRow);
    } else if (w->inherits("MultiLayer")) {
        ml = static_cast<MultiLayer *>(w);
    }

    if (!ml)
        return;
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    for (int i = 0; i < g->curves(); ++i) {
        BoxCurve *bc = dynamic_cast<BoxCurve *>(g->curve(i));
        if (bc) {
            bc->setViolinMode(true);
            bc->setShowDataPoints(true);
        }
    }
    g->replot();
    emit modified();
}

void ApplicationWindow::plotPareto()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table")) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("Please activate a Table and select a column for a Pareto chart!"));
        }
        return;
    }

    Table *srcTable = static_cast<Table *>(d_workspace.activeSubWindow());
    QStringList selCols = srcTable->selectedYColumns();
    if (selCols.isEmpty())
        selCols = srcTable->selectedColumns();
    if (selCols.isEmpty())
        selCols = srcTable->YColumns();
    if (selCols.isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("Please select a column containing values for the Pareto chart!"));
        }
        return;
    }

    int yCol = srcTable->colIndex(selCols.first());
    if (yCol < 0)
        return;

    int startRow = (srcTable->selectedRowCount(false) >= 2) ? srcTable->firstSelectedRow(false) : 0;
    int endRow = (srcTable->selectedRowCount(false) >= 2) ? srcTable->lastSelectedRow(false)
                                                          : (srcTable->numRows() - 1);
    if (startRow < 0 || endRow < startRow) {
        startRow = 0;
        endRow = srcTable->numRows() - 1;
    }

    struct ParetoItem {
        double value;
        int origRow;
    };
    QVector<ParetoItem> items;
    double totalSum = 0.0;
    for (int r = startRow; r <= endRow; ++r) {
        QString txt = srcTable->text(r, yCol).trimmed();
        if (txt.isEmpty())
            continue;
        bool ok = false;
        double v = QLocale().toDouble(txt, &ok);
        if (!ok)
            v = txt.toDouble(&ok);
        if (ok && std::isfinite(v) && v >= 0.0) {
            items.append({ v, r + 1 });
            totalSum += v;
        }
    }

    if (items.size() < 2 || totalSum <= 0.0) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(
                    this, tr("Plot error"),
                    tr("At least two non-negative numeric values are required for a Pareto chart!"));
        }
        return;
    }

    std::sort(items.begin(), items.end(),
              [](const ParetoItem &a, const ParetoItem &b) { return a.value > b.value; });

    QString paretoTableName = generateUniqueName(srcTable->name() + "Pareto");
    Table *paretoTable = newTable(paretoTableName, items.size(), 3);
    paretoTable->setColName(0, "Rank");
    paretoTable->setColPlotDesignation(0, SciDAVis::X);
    paretoTable->setColName(1, srcTable->colLabel(yCol) + "_Sorted");
    paretoTable->setColPlotDesignation(1, SciDAVis::Y);
    paretoTable->setColName(2, "Cumulative_Pct");
    paretoTable->setColPlotDesignation(2, SciDAVis::Y);

    double runningSum = 0.0;
    for (int i = 0; i < items.size(); ++i) {
        runningSum += items[i].value;
        double cumPct = (runningSum / totalSum) * 100.0;
        paretoTable->setCell(i, 0, i + 1);
        paretoTable->setCell(i, 1, items[i].value);
        paretoTable->setCell(i, 2, cumPct);
    }

    QStringList barCols;
    barCols << paretoTable->colName(1);
    MultiLayer *ml = multilayerPlot(paretoTable, barCols, Graph::VerticalBars, 0, items.size() - 1);
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (g) {
        g->insertCurve(paretoTable, paretoTable->colName(0), paretoTable->colName(2),
                       Graph::LineSymbols, 0, items.size() - 1);
        if (g->curves() >= 2) {
            QwtPlotCurve *cumCurve = g->curve(g->curves() - 1);
            if (cumCurve) {
                cumCurve->setYAxis(QwtPlot::yRight);
                QPen pen(QColor(213, 94, 0), 2.0);
                cumCurve->setPen(pen);
            }
        }
        g->enableAxis(QwtPlot::yRight, true);
        if (QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(QwtPlot::yRight)) {
            sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
        }
        g->setXAxisTitle(tr("Category Rank"));
        g->setYAxisTitle(srcTable->colLabel(yCol));
        g->setRightAxisTitle(tr("Cumulative percentage / %"));
        g->setScale(QwtPlot::yLeft, 0.0, totalSum * 1.05);
        g->setScale(QwtPlot::yRight, 0.0, 105.0, 20.0);
        g->newLegend();
        g->replot();
    }
    emit modified();
}

void ApplicationWindow::plotQQ()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table")) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(
                    this, tr("Plot error"),
                    tr("Please activate a Table and select a column for a Normal Q-Q plot!"));
        }
        return;
    }

    Table *srcTable = static_cast<Table *>(d_workspace.activeSubWindow());
    QStringList selCols = srcTable->selectedYColumns();
    if (selCols.isEmpty())
        selCols = srcTable->selectedColumns();
    if (selCols.isEmpty())
        selCols = srcTable->YColumns();
    if (selCols.isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("Please select a numeric column for the Normal Q-Q plot!"));
        }
        return;
    }

    int yCol = srcTable->colIndex(selCols.first());
    if (yCol < 0)
        return;

    int startRow = (srcTable->selectedRowCount(false) >= 2) ? srcTable->firstSelectedRow(false) : 0;
    int endRow = (srcTable->selectedRowCount(false) >= 2) ? srcTable->lastSelectedRow(false)
                                                          : (srcTable->numRows() - 1);
    if (startRow < 0 || endRow < startRow) {
        startRow = 0;
        endRow = srcTable->numRows() - 1;
    }

    QVector<double> vals;
    for (int r = startRow; r <= endRow; ++r) {
        QString txt = srcTable->text(r, yCol).trimmed();
        if (txt.isEmpty())
            continue;
        bool ok = false;
        double v = QLocale().toDouble(txt, &ok);
        if (!ok)
            v = txt.toDouble(&ok);
        if (ok && std::isfinite(v))
            vals.append(v);
    }

    const int n = vals.size();
    if (n < 3) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(
                    this, tr("Plot error"),
                    tr("At least 3 numeric data points are required for a Normal Q-Q plot!"));
        }
        return;
    }

    std::sort(vals.begin(), vals.end());

    double sum = 0.0;
    for (double v : vals)
        sum += v;
    const double mean = sum / n;
    double varSum = 0.0;
    for (double v : vals)
        varSum += (v - mean) * (v - mean);
    const double sd = (n > 1) ? std::sqrt(varSum / (n - 1)) : 1.0;

    QString qqTableName = generateUniqueName(srcTable->name() + "QQ");
    Table *qqTable = newTable(qqTableName, n, 3);
    qqTable->setColName(0, "Theoretical_Quantile");
    qqTable->setColPlotDesignation(0, SciDAVis::X);
    qqTable->setColName(1, "Sample_Quantile");
    qqTable->setColPlotDesignation(1, SciDAVis::Y);
    qqTable->setColName(2, "Normal_Ref_Line");
    qqTable->setColPlotDesignation(2, SciDAVis::Y);

    for (int i = 0; i < n; ++i) {
        const double p = (i + 1.0 - 0.375) / (n + 0.25);
        const double z = inverseStandardNormalCDF(p);
        const double refY = mean + sd * z;
        qqTable->setCell(i, 0, z);
        qqTable->setCell(i, 1, vals[i]);
        qqTable->setCell(i, 2, refY);
    }

    QStringList plotCols;
    plotCols << qqTable->colName(1);
    MultiLayer *ml = multilayerPlot(qqTable, plotCols, Graph::Scatter, 0, n - 1);
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (g) {
        g->insertCurve(qqTable, qqTable->colName(0), qqTable->colName(2), Graph::Line, 0, n - 1);
        if (g->curves() >= 2) {
            QwtPlotCurve *refCurve = g->curve(g->curves() - 1);
            if (refCurve)
                refCurve->setPen(QPen(QColor(213, 94, 0), 2.0, Qt::DashLine));
        }
        g->setTitle(tr("Normal Q-Q Plot (%1)").arg(srcTable->colLabel(yCol)));
        g->setXAxisTitle(tr("Theoretical normal quantiles, z"));
        g->setYAxisTitle(tr("Sample quantiles, %1").arg(srcTable->colLabel(yCol)));
        g->setAutoScale();
        g->newLegend();
        g->replot();
    }
    emit modified();
}

void ApplicationWindow::plotTernary()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table")) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(
                    this, tr("Plot error"),
                    tr("Please activate a Table and select 3 columns (A, B, C) for a Ternary plot!"));
        }
        return;
    }

    Table *srcTable = static_cast<Table *>(d_workspace.activeSubWindow());
    QStringList selCols = srcTable->selectedColumns();
    if (selCols.count() < 3 && srcTable->numCols() >= 3) {
        selCols.clear();
        selCols << srcTable->colName(0) << srcTable->colName(1) << srcTable->colName(2);
    }
    if (selCols.count() < 3) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("Please select three columns (A, B, C) for a Ternary plot!"));
        }
        return;
    }

    const int colA = srcTable->colIndex(selCols[0]);
    const int colB = srcTable->colIndex(selCols[1]);
    const int colC = srcTable->colIndex(selCols[2]);
    if (colA < 0 || colB < 0 || colC < 0)
        return;

    int startRow = (srcTable->selectedRowCount(false) >= 2) ? srcTable->firstSelectedRow(false) : 0;
    int endRow = (srcTable->selectedRowCount(false) >= 2) ? srcTable->lastSelectedRow(false)
                                                          : (srcTable->numRows() - 1);
    if (startRow < 0 || endRow < startRow) {
        startRow = 0;
        endRow = srcTable->numRows() - 1;
    }

    QVector<QPointF> ternaryPts;
    for (int r = startRow; r <= endRow; ++r) {
        bool okA = false, okB = false, okC = false;
        double a = QLocale().toDouble(srcTable->text(r, colA), &okA);
        if (!okA)
            a = srcTable->text(r, colA).toDouble(&okA);
        double b = QLocale().toDouble(srcTable->text(r, colB), &okB);
        if (!okB)
            b = srcTable->text(r, colB).toDouble(&okB);
        double c = QLocale().toDouble(srcTable->text(r, colC), &okC);
        if (!okC)
            c = srcTable->text(r, colC).toDouble(&okC);
        if (okA && okB && okC && std::isfinite(a) && std::isfinite(b) && std::isfinite(c)
            && (a + b + c) != 0.0) {
            ternaryPts.append(Graph::ternaryToCartesian(a, b, c));
        }
    }

    if (ternaryPts.isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Plot error"),
                                 tr("No valid ternary data rows (A + B + C != 0) found!"));
        }
        return;
    }

    QString ternTableName = generateUniqueName(srcTable->name() + "Ternary");
    Table *ternTable = newTable(ternTableName, ternaryPts.size(), 2);
    ternTable->setColName(0, "Ternary_X");
    ternTable->setColPlotDesignation(0, SciDAVis::X);
    ternTable->setColName(1, "Ternary_Y");
    ternTable->setColPlotDesignation(1, SciDAVis::Y);

    for (int i = 0; i < ternaryPts.size(); ++i) {
        ternTable->setCell(i, 0, ternaryPts[i].x());
        ternTable->setCell(i, 1, ternaryPts[i].y());
    }

    QStringList plotCols;
    plotCols << ternTable->colName(1);
    MultiLayer *ml = multilayerPlot(ternTable, plotCols, Graph::Scatter, 0, ternaryPts.size() - 1);
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (g) {
        g->setupTernaryPlot(srcTable->colLabel(colA), srcTable->colLabel(colB),
                            srcTable->colLabel(colC));
    }
    emit modified();
}

void ApplicationWindow::remove3DMatrixPlots(Matrix *m)
{
    if (!m)
        return;

    const bool nonInteractive = isNonInteractiveContext(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QList<MyWidget *> windows = windowsList();
    for (MyWidget *hw : hiddenWindows) {
        if (hw && !windows.contains(hw))
            windows.append(hw);
    }
    for (MyWidget *ow : outWindows) {
        if (ow && !windows.contains(ow))
            windows.append(ow);
    }
    for (QMdiSubWindow *sw : d_workspace.subWindowList()) {
        if (MyWidget *mw = qobject_cast<MyWidget *>(sw)) {
            if (!windows.contains(mw))
                windows.append(mw);
        }
    }

    for (MyWidget *w : windows) {
        if (!w)
            continue;
        if (w->inherits("Graph3D") && static_cast<Graph3D *>(w)->matrix() == m) {
            static_cast<Graph3D *>(w)->clearData();
        } else if (w->inherits("MultiLayer")) {
            QWidgetList graphsList = static_cast<MultiLayer *>(w)->graphPtrs();
            for (int j = 0; j < graphsList.count(); j++) {
                Graph *g = static_cast<Graph *>(graphsList.at(j));
                if (!g)
                    continue;
                bool removedAny = false;
                for (int i = g->curves() - 1; i >= 0; --i) {
                    QwtPlotItem *pi = g->plotItem(i);
                    Spectrogram *sp = dynamic_cast<Spectrogram *>(pi);
                    if (sp && sp->matrix() == m) {
                        g->removeCurve(i);
                        removedAny = true;
                    }
                }
                if (Plot *pw = g->plotWidget()) {
                    const QwtPlotItemList directItems = pw->itemList();
                    for (QwtPlotItem *it : directItems) {
                        if (Spectrogram *sp = dynamic_cast<Spectrogram *>(it)) {
                            if (sp->matrix() == m) {
                                sp->detach();
                                delete sp;
                                removedAny = true;
                            }
                        }
                    }
                }
                if (removedAny)
                    g->replot();
            }
        }
    }
    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
}

void ApplicationWindow::updateMatrixPlots(MyWidget *window)
{
    Matrix *m = (Matrix *)window;
    if (!m)
        return;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (w->inherits("Graph3D") && ((Graph3D *)w)->matrix() == m)
            ((Graph3D *)w)->updateMatrixData(m);
        else if (w->inherits("MultiLayer")) {
            QWidgetList graphsList = ((MultiLayer *)w)->graphPtrs();
            for (int j = 0; j < (int)graphsList.count(); j++) {
                Graph *g = (Graph *)graphsList.at(j);
                for (int i = 0; i < g->curves(); i++) {
                    Spectrogram *sp = (Spectrogram *)g->plotItem(i);
                    if (sp && sp->rtti() == QwtPlotItem::Rtti_PlotSpectrogram && sp->matrix() == m)
                        sp->updateData(m);
                }
            }
        }
    }

    QApplication::restoreOverrideCursor();
}

void ApplicationWindow::add3DData()
{
    if (isNonInteractiveContext(this))
        return;
    if (tableWindows().count() <= 0) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no tables available in this project.</h4>"
                                "<p><h4>Please create a table and try again!</h4>"));
        return;
    }

    // TODO: string list -> Column * list
    QStringList zColumns = columnsList(SciDAVis::Z);
    if ((int)zColumns.count() <= 0) {
        QMessageBox::critical(this, tr("Warning"),
                              tr("There are no available columns with plot designation set to Z!"));
        return;
    }

    DataSetDialog *ad = new DataSetDialog(tr("Column") + " : ", this);
    ad->setAttribute(Qt::WA_DeleteOnClose);
    connect(ad, SIGNAL(options(const QString &)), this, SLOT(insertNew3DData(const QString &)));
    ad->setWindowTitle(tr("Choose data set"));
    ad->setCurveNames(zColumns);
    ad->exec();
}

void ApplicationWindow::change3DData()
{
    if (isNonInteractiveContext(this))
        return;
    DataSetDialog *ad = new DataSetDialog(tr("Column") + " : ", this);
    ad->setAttribute(Qt::WA_DeleteOnClose);
    connect(ad, SIGNAL(options(const QString &)), this, SLOT(change3DData(const QString &)));

    ad->setWindowTitle(tr("Choose data set"));
    // TODO: string list -> Column * list
    ad->setCurveNames(columnsList(SciDAVis::Z));
    ad->exec();
}

void ApplicationWindow::change3DMatrix()
{
    if (isNonInteractiveContext(this))
        return;
    DataSetDialog *ad = new DataSetDialog(tr("Matrix") + " : ", this);
    ad->setAttribute(Qt::WA_DeleteOnClose);
    connect(ad, SIGNAL(options(const QString &)), this, SLOT(change3DMatrix(const QString &)));

    ad->setWindowTitle(tr("Choose matrix to plot"));
    ad->setCurveNames(matrixNames());

    Graph3D *g = (Graph3D *)d_workspace.activeSubWindow();
    if (g && g->matrix())
        ad->setCurentDataSet(g->matrix()->name());
    ad->exec();
}

void ApplicationWindow::change3DMatrix(const QString &matrix_name)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        Graph3D *g = (Graph3D *)d_workspace.activeSubWindow();
        Matrix *m = matrix(matrix_name);
        if (m && g)
            g->changeMatrix(m);

        emit modified();
    }
}

void ApplicationWindow::add3DMatrixPlot()
{
    if (isNonInteractiveContext(this))
        return;
    QStringList matrices = matrixNames();
    if ((int)matrices.count() <= 0) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no matrices available in this project.</h4>"
                                "<p><h4>Please create a matrix and try again!</h4>"));
        return;
    }

    DataSetDialog *ad = new DataSetDialog(tr("Matrix") + " :", this);
    ad->setAttribute(Qt::WA_DeleteOnClose);
    connect(ad, SIGNAL(options(const QString &)), this, SLOT(insert3DMatrixPlot(const QString &)));

    ad->setWindowTitle(tr("Choose matrix to plot"));
    ad->setCurveNames(matrices);
    ad->exec();
}

void ApplicationWindow::insert3DMatrixPlot(const QString &matrix_name)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->addMatrixData(matrix(matrix_name));
        emit modified();
    }
}

void ApplicationWindow::insertNew3DData(const QString &colName)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->insertNewData(table(colName), colName);
        emit modified();
    }
}

void ApplicationWindow::change3DData(const QString &colName)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->changeDataColumn(table(colName), colName);
        emit modified();
    }
}

void ApplicationWindow::editSurfacePlot()
{
    if (isNonInteractiveContext(this))
        return;
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        Graph3D *g = (Graph3D *)d_workspace.activeSubWindow();

        SurfaceDialog *sd = new SurfaceDialog(this);
        sd->setAttribute(Qt::WA_DeleteOnClose);
        connect(sd,
                SIGNAL(options(const QString &, double, double, double, double, double, double)), g,
                SLOT(insertFunction(const QString &, double, double, double, double, double,
                                    double)));
        connect(sd, SIGNAL(clearFunctionsList()), this, SLOT(clearSurfaceFunctionsList()));

        sd->insertFunctionsList(surfaceFunc);
        if (g->hasData()) {
            sd->setFunction(g->formula());
            sd->setLimits(g->xStart(), g->xStop(), g->yStart(), g->yStop(), g->zStart(),
                          g->zStop());
        }
        sd->exec();
    }
}

void ApplicationWindow::newSurfacePlot()
{
    if (isNonInteractiveContext(this))
        return;
    SurfaceDialog *sd = new SurfaceDialog(this);
    sd->setAttribute(Qt::WA_DeleteOnClose);
    connect(sd, SIGNAL(options(const QString &, double, double, double, double, double, double)),
            this, SLOT(newPlot3D(const QString &, double, double, double, double, double, double)));
    connect(sd, SIGNAL(clearFunctionsList()), this, SLOT(clearSurfaceFunctionsList()));

    sd->insertFunctionsList(surfaceFunc);
    sd->exec();
}

Graph3D *ApplicationWindow::newPlot3D(const QString &formula, double xl, double xr, double yl,
                                      double yr, double zl, double zr)
{
    QString label = generateUniqueName(tr("Graph"));

    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->addFunction(formula, xl, xr, yl, yr, zl, zr);
    plot->resize(500, 400);
    plot->setWindowTitle(label);
    plot->setName(label);
    customPlot3D(plot);
    plot->update();

    initPlot3D(plot);

    emit modified();
    return plot;
}

void ApplicationWindow::updateSurfaceFuncList(const QString &s)
{
    surfaceFunc.removeAll(s);
    surfaceFunc.push_front(s);
    while ((int)surfaceFunc.size() > 10)
        surfaceFunc.pop_back();
}

Graph3D *ApplicationWindow::newPlot3D(const QString &caption, const QString &formula, double xl,
                                      double xr, double yl, double yr, double zl, double zr)
{
    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->addFunction(formula, xl, xr, yl, yr, zl, zr);
    plot->update();

    QString label = caption;
    while (alreadyUsedName(label))
        label = generateUniqueName(tr("Graph"));

    plot->setWindowTitle(label);
    plot->setName(label);
    initPlot3D(plot);
    return plot;
}

Graph3D *ApplicationWindow::dataPlot3D(Table *table, const QString &colName)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QString label = generateUniqueName(tr("Graph"));
    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->addData(table, colName);
    plot->resize(500, 400);
    plot->setWindowTitle(label);
    plot->setName(label);

    customPlot3D(plot);
    plot->update();
    initPlot3D(plot);

    emit modified();
    QApplication::restoreOverrideCursor();
    return plot;
}

namespace {
static int resolvePlot3DColIndex(Table *w, const QString &token)
{
    if (!w || token.isEmpty())
        return -1;
    if (!w->name().isEmpty() && token.startsWith(w->name() + QLatin1Char('_'))) {
        const int idx = w->colIndex(token.mid(w->name().length() + 1));
        if (idx >= 0 && idx < w->numCols())
            return idx;
    }
    const int idx = w->colIndex(token);
    return (idx >= 0 && idx < w->numCols()) ? idx : -1;
}
} // namespace

Graph3D *ApplicationWindow::dataPlot3D(const QString &caption, const QString &formula, double xl,
                                       double xr, double yl, double yr, double zl, double zr)
{
    const int sepXY = formula.indexOf(QLatin1String("(X),"));
    if (sepXY <= 0)
        return 0;

    const QString xToken = formula.left(sepXY).trimmed();
    QString yToken = formula.mid(sepXY + 4).trimmed();
    if (yToken.endsWith(QLatin1String("(Y)")))
        yToken.chop(3);
    yToken = yToken.trimmed();

    Table *w = table(xToken);
    if (!w && !yToken.isEmpty())
        w = table(yToken);
    if (!w)
        return 0;

    const int xIdx = resolvePlot3DColIndex(w, xToken);
    const int yIdx = resolvePlot3DColIndex(w, yToken);
    if (xIdx < 0 || yIdx < 0)
        return 0;

    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->addData(w, w->colName(xIdx), w->colName(yIdx), xl, xr, yl, yr, zl, zr);
    plot->update();

    QString label = caption;
    while (alreadyUsedName(label))
        label = generateUniqueName(tr("Graph"));

    plot->setWindowTitle(label);
    plot->setName(label);
    initPlot3D(plot);

    return plot;
}

Graph3D *ApplicationWindow::newPlot3D()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QString label = generateUniqueName(tr("Graph"));

    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->resize(500, 400);
    plot->setWindowTitle(label);
    plot->setName(label);

    customPlot3D(plot);
    initPlot3D(plot);

    emit modified();
    QApplication::restoreOverrideCursor();
    return plot;
}

Graph3D *ApplicationWindow::dataPlotXYZ(Table *table, const QString &zColName, int type)
{
    if (!table)
        return 0;
    int zCol = table->colIndex(zColName);
    if (zCol < 0 || zCol >= table->numCols())
        return 0;
    int yCol = table->colY(zCol);
    int xCol = table->colX(zCol);
    return dataPlotXYZ(table, xCol, yCol, zCol, type);
}

Graph3D *ApplicationWindow::dataPlotXYZ(Table *table, int xCol, int yCol, int zCol, int type)
{
    if (!table)
        return 0;
    if (xCol < 0 || xCol >= table->numCols() || yCol < 0 || yCol >= table->numCols()
        || zCol < 0 || zCol >= table->numCols())
        return 0;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QString label = generateUniqueName(tr("Graph"));

    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->addData(table, xCol, yCol, zCol, type);
    plot->resize(500, 400);
    plot->setWindowTitle(label);
    plot->setName(label);

    customPlot3D(plot);
    plot->update();
    initPlot3D(plot);

    emit modified();
    QApplication::restoreOverrideCursor();
    return plot;
}

Graph3D *ApplicationWindow::dataPlotXYZ(const QString &caption, const QString &formula, double xl,
                                        double xr, double yl, double yr, double zl, double zr)
{
    const int sepXY = formula.indexOf(QLatin1String("(X),"));
    const int sepYZ = (sepXY >= 0) ? formula.indexOf(QLatin1String("(Y),"), sepXY + 4) : -1;
    if (sepXY <= 0 || sepYZ <= sepXY + 4)
        return 0;

    const QString xToken = formula.left(sepXY).trimmed();
    const QString yToken = formula.mid(sepXY + 4, sepYZ - (sepXY + 4)).trimmed();
    QString zToken = formula.mid(sepYZ + 4).trimmed();
    if (zToken.endsWith(QLatin1String("(Z)")))
        zToken.chop(3);
    zToken = zToken.trimmed();

    Table *w = table(xToken);
    if (!w && !yToken.isEmpty())
        w = table(yToken);
    if (!w && !zToken.isEmpty())
        w = table(zToken);
    if (!w)
        return 0;

    const int xCol = resolvePlot3DColIndex(w, xToken);
    const int yCol = resolvePlot3DColIndex(w, yToken);
    const int zCol = resolvePlot3DColIndex(w, zToken);
    if (xCol < 0 || yCol < 0 || zCol < 0)
        return 0;

    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->addData(w, xCol, yCol, zCol, xl, xr, yl, yr, zl, zr);
    plot->update();

    QString label = caption;
    while (alreadyUsedName(label))
        label = generateUniqueName(tr("Graph"));

    plot->setWindowTitle(label);
    plot->setName(label);
    initPlot3D(plot);
    return plot;
}

void ApplicationWindow::customPlot3D(Graph3D *plot)
{
    plot->setDataColors(QColor(COLORVALUE(plot3DColors[4])), QColor(COLORVALUE(plot3DColors[0])));
    plot->updateColors(QColor(COLORVALUE(plot3DColors[2])), QColor(COLORVALUE(plot3DColors[6])),
                       QColor(COLORVALUE(plot3DColors[5])), QColor(COLORVALUE(plot3DColors[1])),
                       QColor(COLORVALUE(plot3DColors[7])), QColor(COLORVALUE(plot3DColors[3])));

    plot->setResolution(plot3DResolution);
    plot->showColorLegend(showPlot3DLegend);
    plot->setSmoothMesh(smooth3DMesh);
    plot->setOrtho(orthogonal3DPlots);
    if (showPlot3DProjection)
        plot->setFloorData();

    plot->setNumbersFont(plot3DNumbersFont);
    plot->setXAxisLabelFont(plot3DAxesFont);
    plot->setYAxisLabelFont(plot3DAxesFont);
    plot->setZAxisLabelFont(plot3DAxesFont);
    plot->setTitleFont(plot3DTitleFont);
}

void ApplicationWindow::initPlot3D(Graph3D *plot)
{
    d_workspace.addSubWindow(plot);
    current_folder->addWindow(plot);
    plot->setFolder(current_folder);
    connectSurfacePlot(plot);

    plot->setWindowIcon(IconLoader::load("plot-3d-ribbon"));
    if (isTabbedWorkspaceView())
        plot->showMaximized();
    else
        plot->show();
    plot->setFocus();

    addListViewItem(plot);

    if (!graph_3D_tools->isEnabled())
        graph_3D_tools->setEnabled(true);

    customMenu(plot);
    customToolBars(plot);
}

void ApplicationWindow::polishGraph(Graph *g, int style)
{
    // E1: bar charts and histograms keep the configured (inward) tick direction like every
    // other plot type; they are no longer forced to outward ticks.
    if (style == Graph::HorizontalBars) {
        // 横棒では下軸に Y の値、左軸に X（分類）が来る。旧 SciDAVis の setAxisTitle は
        // 0=下・1=左の番号付けだったが、今の Graph::setAxisTitle は QwtPlot の軸番号
        // （0=yLeft・1=yRight）を取るので、名前で指定する。数字のままだと右軸に仮の題
        // 「X Axis Title」が付き、開き直すと消えていた。
        g->setAxisTitle(QwtPlot::xBottom, tr("Y Axis Title"));
        g->setAxisTitle(QwtPlot::yLeft, tr("X Axis Title"));
    }
    // E1: apply the "preset for new graphs" (default ISIJ 85 mm) once the graph window has been
    // fully built. Pie, ternary and polar plots are skipped inside the request.
    if (g)
        MultiLayer::requestNewGraphStylePreset(g);
}

MultiLayer *ApplicationWindow::multilayerPlot(const QString &caption)
{
    MultiLayer *ml = new MultiLayer("", &d_workspace, 0);
    ml->setAttribute(Qt::WA_DeleteOnClose);
    QString label = caption;
    initMultilayerPlot(ml, label.replace(QRegExp("_"), "-"));
    return ml;
}

MultiLayer *ApplicationWindow::newGraph(const QString &caption)
{
    MultiLayer *ml = multilayerPlot(generateUniqueName(caption));
    if (ml) {
        Graph *g = ml->addLayer();
        setPreferences(g);
        g->newLegend();
        g->setAutoscaleFonts(false);
        g->setIgnoreResizeEvents(false);
        ml->arrangeLayers(false, false);
        g->setAutoscaleFonts(autoScaleFonts); // restore user defined fonts behaviour
        g->setIgnoreResizeEvents(!autoResizeLayers);
        finalizeLastGraphSettings(ml);
        customMenu(ml);
    }
    return ml;
}

static void disambiguateMultiTableLegendLabels(Graph *ag)
{
    if (!ag || ag->isPiePlot() || ag->isTernaryPlot() || !ag->legend())
        return;

    ApplicationWindow *ownerApp = nullptr;
    for (QWidget *p = ag; p != nullptr; p = p->parentWidget()) {
        if (ApplicationWindow *aw = qobject_cast<ApplicationWindow *>(p)) {
            ownerApp = aw;
            break;
        }
        if (ApplicationWindow *aw = qobject_cast<ApplicationWindow *>(p->window())) {
            ownerApp = aw;
            break;
        }
    }
    WorkbookManager *wm = ownerApp ? WorkbookManager::existing(ownerApp) : nullptr;

    auto tableDisplayLabel = [wm](Table *t) -> QString {
        if (!t)
            return QString();
        if (wm) {
            const QString shLbl = wm->sheetDisplayLabel(t).trimmed();
            if (!shLbl.isEmpty())
                return shLbl;
        }
        const QString wl = t->windowLabel().trimmed();
        if (!wl.isEmpty() && wl.length() <= 28 && !wl.contains(QLatin1Char('/'))
            && !wl.contains(QLatin1Char('\\')) && !wl.contains(QLatin1Char(';'))
            && !wl.contains(QLatin1Char('['))) {
            return wl;
        }
        return t->name();
    };

    QSet<Table *> distinctTables;
    for (int ci = 0; ci < ag->curves(); ++ci) {
        if (ag->curveType(ci) == Graph::ErrorBars)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(ag->curve(ci));
        if (dc && dc->table() && dc->testItemAttribute(QwtPlotItem::Legend))
            distinctTables.insert(dc->table());
    }
    if (distinctTables.size() < 2)
        return;

    Legend *leg = ag->legend();
    QStringList lines = leg->text().split(QLatin1Char('\n'));
    struct EntryRef {
        int lineIdx = -1;
        int curveIdx = -1;
        Table *table = nullptr;
        QString label;
    };
    QVector<EntryRef> entries;
    for (int li = 0; li < lines.size(); ++li) {
        const QString &ln = lines.at(li);
        if (!ln.startsWith(QLatin1String("\\c{")))
            continue;
        const int closeBrace = ln.indexOf(QLatin1Char('}'), 3);
        if (closeBrace < 4)
            continue;
        bool ok = false;
        const int oneBased = ln.mid(3, closeBrace - 3).toInt(&ok);
        const int ci = oneBased - 1;
        if (!ok || ci < 0 || ci >= ag->curves() || ag->curveType(ci) == Graph::ErrorBars)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(ag->curve(ci));
        if (!dc || !dc->table())
            continue;
        const QString curLbl = ln.mid(closeBrace + 1).trimmed();
        if (curLbl.isEmpty())
            continue;
        // Only disambiguate automatic column-based labels (leave user-typed custom text intact)
        if (!ag->curveCustomLabel(ci).isEmpty())
            continue;
        const QString autoLbl = ag->columnCurveLabel(ci).trimmed();
        Column *yc = dc->table()->column(dc->table()->colIndex(dc->yColumnName()));
        const QString shortLbl = yc ? yc->name().trimmed() : QString();
        const QString longLbl = yc ? yc->longName().trimmed() : QString();
        if (curLbl != autoLbl && curLbl != shortLbl && curLbl != longLbl
            && curLbl != dc->yColumnName().trimmed()) {
            continue;
        }
        entries.append(EntryRef{ li, ci, dc->table(), curLbl });
    }
    if (entries.size() < 2)
        return;

    QHash<QString, QVector<int>> groups;
    for (int idx = 0; idx < entries.size(); ++idx)
        groups[entries.at(idx).label.toLower()].append(idx);

    bool changed = false;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const QVector<int> &idxs = it.value();
        if (idxs.size() < 2)
            continue;
        QSet<Table *> grpTables;
        for (int idx : idxs)
            grpTables.insert(entries.at(idx).table);
        if (grpTables.size() < 2)
            continue;

        // Check if display labels themselves collide (e.g. Sheet1 in Book1 vs Sheet1 in Book2)
        QSet<QString> dispSet;
        bool dispCollision = false;
        for (int idx : idxs) {
            const QString d = tableDisplayLabel(entries.at(idx).table).toLower();
            if (dispSet.contains(d))
                dispCollision = true;
            dispSet.insert(d);
        }

        for (int idx : idxs) {
            const EntryRef &er = entries.at(idx);
            const QString tblDisp =
                    dispCollision ? er.table->name() : tableDisplayLabel(er.table);
            if (tblDisp.isEmpty() || er.label.contains(tblDisp, Qt::CaseInsensitive))
                continue;
            const QString updated =
                    QStringLiteral("\\c{%1}%2 (%3)").arg(er.curveIdx + 1).arg(tblDisp, er.label);
            if (lines[er.lineIdx] != updated) {
                lines[er.lineIdx] = updated;
                changed = true;
            }
        }
    }
    if (changed) {
        leg->setText(lines.join(QLatin1Char('\n')));
        ag->replot();
    }
}

namespace ApplicationWindowInternal {
void applyWorksheetMetadataToGraph(Graph *ag, Table *fallbackTable,
                                   const QStringList &fallbackCols, int style)
{
    Q_UNUSED(fallbackTable);
    Q_UNUSED(fallbackCols);
    Q_UNUSED(style);
    if (!ag || ag->isPiePlot() || ag->isTernaryPlot())
        return;

    // Ensure all 4 axis title fonts strictly keep "Arial"
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QFont af = ag->axisTitleFont(a);
        if (af.family() != QLatin1String("Arial")) {
            af.setFamily(QStringLiteral("Arial"));
            ag->setAxisTitleFont(a, af);
        }
    }

    ag->refreshAutoAxisTitlesFromColumns();
    if (ag->legend())
        ag->reconstructLegend();
    disambiguateMultiTableLegendLabels(ag);
    ag->updatePlot();

    // Auto-apply categorical X tick labels when the plotted X column is ColumnMode::Text
    if (style != Graph::Box && style != Graph::Histogram) {
        Column *textXCol = nullptr;
        Table *textXTbl = nullptr;
        for (int i = 0; i < ag->curves(); ++i) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(ag->curve(i))) {
                Table *ct = dc->table();
                if (!ct)
                    continue;
                const int xIdx = ct->colIndex(dc->xColumnName());
                if (xIdx >= 0) {
                    Column *xc = ct->column(xIdx);
                    if (xc && xc->columnMode() == SciDAVis::ColumnMode::Text) {
                        textXCol = xc;
                        textXTbl = ct;
                        break;
                    }
                }
            }
        }
        if (!textXCol && fallbackTable) {
            const int fxIdx = fallbackTable->firstXCol();
            if (fxIdx >= 0) {
                Column *xc = fallbackTable->column(fxIdx);
                if (xc && xc->columnMode() == SciDAVis::ColumnMode::Text) {
                    textXCol = xc;
                    textXTbl = fallbackTable;
                }
            }
        }
        if (textXCol && textXTbl) {
            int lastValid = -1;
            const int nRows = textXCol->rowCount();
            for (int r = 0; r < nRows; ++r) {
                if (!textXCol->isInvalid(r) && !textXCol->textAt(r).trimmed().isEmpty())
                    lastValid = r;
            }
            if (lastValid >= 0) {
                QStringList catLabels;
                catLabels.reserve(lastValid + 1);
                for (int r = 0; r <= lastValid; ++r)
                    catLabels.append(textXCol->textAt(r).trimmed());
                const int catAxis =
                        (style == Graph::HorizontalBars) ? QwtPlot::yLeft : QwtPlot::xBottom;
                ag->setLabelsTextFormat(catAxis, textXCol, 0, lastValid);
                ag->setCategoryLabels(catAxis, catLabels);
                ag->replot();
            }
        }
    }

    if ((style == Graph::VerticalBars || style == Graph::HorizontalBars
         || style == Graph::Histogram)
        && ag->legend()) {
        ag->placeLegendAnchor(Graph::LegendAutoAvoidData);
    }
}
} // namespace ApplicationWindowInternal

static void finalizePieGraphLegendAndLayout(Graph *ag, Table *w, const QStringList &colList,
                                            int startRow = 0, int endRow = -1)
{
    if (!ag || !ag->isPiePlot() || !ag->legend())
        return;

    const QString existingLeg = ag->legend()->text();
    if (!existingLeg.contains(QLatin1Char('%')) && w && !colList.isEmpty()) {
        const int ycol = w->colIndex(colList.first());
        Column *yColPtr = (ycol >= 0) ? w->column(ycol) : nullptr;
        if (yColPtr) {
            const int rStart = qMax(0, startRow);
            const int rEnd = (endRow < 0 || endRow >= yColPtr->rowCount())
                    ? (yColPtr->rowCount() - 1)
                    : endRow;
            int labelCol = -1;
            for (int c = 0; c < w->numCols(); ++c) {
                if (c == ycol || !w->column(c))
                    continue;
                if (w->column(c)->columnMode() == SciDAVis::ColumnMode::Text
                    || w->colPlotDesignation(c) == SciDAVis::X) {
                    labelCol = c;
                    break;
                }
            }
            Column *lblColPtr = (labelCol >= 0) ? w->column(labelCol) : nullptr;
            const auto yColType = yColPtr->columnMode();

            QVector<double> vals;
            QStringList names;
            double sum = 0.0;
            for (int r = rStart; r <= rEnd && r < yColPtr->rowCount(); ++r) {
                if (yColPtr->isInvalid(r) || yColPtr->isMasked(r))
                    continue;
                double val = 0.0;
                if (yColType == SciDAVis::ColumnMode::Text) {
                    bool ok = false;
                    val = QLocale().toDouble(yColPtr->textAt(r), &ok);
                    if (!ok)
                        continue;
                } else {
                    val = yColPtr->valueAt(r);
                }
                if (!std::isfinite(val) || val <= 0.0)
                    continue;
                vals.append(val);
                sum += val;
                QString sLbl;
                if (lblColPtr && r < lblColPtr->rowCount() && !lblColPtr->isInvalid(r)) {
                    sLbl = (lblColPtr->columnMode() == SciDAVis::ColumnMode::Text)
                            ? lblColPtr->textAt(r).trimmed()
                            : QLocale::c().toString(lblColPtr->valueAt(r), 'g', 6);
                }
                if (sLbl.isEmpty())
                    sLbl = QString::number(vals.size());
                names.append(sLbl);
            }
            if (!vals.isEmpty() && sum > 0.0 && std::isfinite(sum)) {
                QStringList lines;
                lines.reserve(vals.size());
                for (int i = 0; i < vals.size(); ++i) {
                    const double pct = (vals[i] / sum) * 100.0;
                    lines.append(QStringLiteral("\\p{%1} %2 (%3%)")
                                         .arg(i + 1)
                                         .arg(names[i], QLocale::c().toString(pct, 'f', 1)));
                }
                ag->legend()->setText(lines.join(QLatin1Char('\n')));
            }
        }
    }

    ag->placeLegendAnchor(Graph::LegendTopCenter, nullptr, 6);
    ag->replot();
}

MultiLayer *ApplicationWindow::multilayerPlot(Table *w, const QStringList &colList, int style,
                                              int startRow, int endRow,
                                              const QString &xColName)
{ // used when plotting selected columns
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    MultiLayer *g = new MultiLayer("", &d_workspace, 0);
    g->setAttribute(Qt::WA_DeleteOnClose);

    initMultilayerPlot(g, generateUniqueName(tr("Graph")));

    Graph *ag = g->addLayer();
    if (!ag)
        return 0;

    setPreferences(ag);
    ag->insertCurvesList(w, colList, style, defaultCurveLineWidth, defaultSymbolSize, startRow,
                         endRow, xColName);

    polishGraph(ag, style);
    ag->newLegend();
    applyWorksheetMetadataToGraph(ag, w, colList, style);
    g->arrangeLayers(false, false);
    if (style == Graph::Pie || ag->isPiePlot())
        finalizePieGraphLegendAndLayout(ag, w, colList, startRow, endRow);
    else if (!ag->isTernaryPlot() && !ag->isPolarPlot())
        finalizeLastGraphSettings(g);
    g->applyPendingNewGraphStylePreset(); // E1: apply the new-graph preset now
    if (w)
        ToriUx::dismissPlotThisDataBar(w);
    customMenu(g);

    emit modified();
    QApplication::restoreOverrideCursor();
    return g;
}

MultiLayer *ApplicationWindow::multilayerPlot(int c, int r, int style)
{ // used when plotting from the panel menu
    Table *w = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (!w)
        return 0;
    if (!validFor2DPlot(w, style))
        return 0;

    QStringList list;
    switch (style) {
    case Graph::Histogram:
    case Graph::Pie:
    case Graph::Box:
        list = w->selectedColumns();
        if (list.isEmpty())
            list = w->YColumns();
        if (list.isEmpty() && w->numCols() > 0)
            list << w->colName(w->numCols() - 1);
        break;
    default:
        list = w->selectedYColumns();
        if (list.isEmpty())
            list = w->YColumns();
        break;
    }

    int curves = (int)list.count();
    if (r < 0)
        r = curves;

    MultiLayer *g = new MultiLayer("", &d_workspace, 0);
    g->setAttribute(Qt::WA_DeleteOnClose);

    initMultilayerPlot(g, generateUniqueName(tr("Graph")));

    int layers = c * r;
    if (curves < layers) {
        for (int i = 0; i < curves; i++) {
            Graph *ag = g->addLayer();
            if (ag) {
                setPreferences(ag);
                ag->insertCurvesList(w, QStringList(list[i]), style, defaultCurveLineWidth,
                                     defaultSymbolSize);
                ag->newLegend();
                ag->setAutoscaleFonts(false); // in order to avoid to small fonts
                ag->setIgnoreResizeEvents(false);
                polishGraph(ag, style);
                applyWorksheetMetadataToGraph(ag, w, QStringList(list[i]), style);
            }
        }
    } else {
        for (int i = 0; i < layers; i++) {
            Graph *ag = g->addLayer();
            if (ag) {
                QStringList lst;
                lst << list[i];
                setPreferences(ag);
                ag->insertCurvesList(w, lst, style, defaultCurveLineWidth, defaultSymbolSize);
                ag->newLegend();
                ag->setAutoscaleFonts(false); // in order to avoid to small fonts
                ag->setIgnoreResizeEvents(false);
                polishGraph(ag, style);
                applyWorksheetMetadataToGraph(ag, w, lst, style);
            }
        }
    }
    g->setRows(r);
    g->setCols(c);
    g->arrangeLayers(false, false);
    QWidgetList lst = g->graphPtrs();
    for (int li = 0; li < lst.size(); ++li) {
        Graph *ag = static_cast<Graph *>(lst.at(li));
        ag->setAutoscaleFonts(autoScaleFonts); // restore user defined fonts behaviour
        ag->setIgnoreResizeEvents(!autoResizeLayers);
        if (style == Graph::Pie || ag->isPiePlot()) {
            const QStringList pieCol =
                    (li < list.size()) ? (QStringList() << list.at(li)) : QStringList();
            finalizePieGraphLegendAndLayout(ag, w, pieCol, 0, -1);
        }
    }
    g->applyPendingNewGraphStylePreset(); // E1: apply the new-graph preset now
    if (w)
        ToriUx::dismissPlotThisDataBar(w);
    customMenu(g);
    emit modified();
    return g;
}

MultiLayer *ApplicationWindow::multilayerPlot(const QStringList &colList)
{ // used when plotting from wizard
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    MultiLayer *g = new MultiLayer("", &d_workspace, 0);
    g->setAttribute(Qt::WA_DeleteOnClose);

    initMultilayerPlot(g, generateUniqueName(tr("Graph")));

    Graph *ag = g->addLayer();
    setPreferences(ag);
    polishGraph(ag, defaultCurveStyle);
    int curves = (int)colList.count();
    int errorBars = 0;
    for (int i = 0; i < curves; i++) {
        if (colList[i].contains("(yErr)") || colList[i].contains("(xErr)"))
            errorBars++;
    }

    for (int i = 0; i < curves; i++) {
        QString s = colList[i];
        int pos = s.indexOf(":", 0);
        QString caption = s.left(pos) + "_";
        Table *w = (Table *)table(caption);
        if (w)
            ToriUx::dismissPlotThisDataBar(w);

        int posX = s.indexOf("(X)", pos);
        QString xColName = caption + s.mid(pos + 2, posX - pos - 2);
        int xCol = w->colIndex(xColName);

        posX = s.indexOf(",", posX);
        int posY = s.indexOf("(Y)", posX);
        QString yColName = caption + s.mid(posX + 2, posY - posX - 2);

        if (s.contains("(yErr)") || s.contains("(xErr)")) {
            posY = s.indexOf(",", posY);
            int posErr, errType;
            if (s.contains("(yErr)")) {
                errType = QwtErrorPlotCurve::Vertical;
                posErr = s.indexOf("(yErr)", posY);
            } else {
                errType = QwtErrorPlotCurve::Horizontal;
                posErr = s.indexOf("(xErr)", posY);
            }

            QString errColName = caption + s.mid(posY + 2, posErr - posY - 2);
            ag->addErrorBars(xColName, yColName, w, errColName, errType);
        } else
            ag->insertCurve(w, xCol, yColName, defaultCurveStyle);

        CurveLayout cl = ag->initCurveLayout(defaultCurveStyle, curves - errorBars);
        cl.lWidth = defaultCurveLineWidth;
        cl.sSize = defaultSymbolSize;
        ag->updateCurveLayout(i, &cl);
    }
    ag->newLegend();
    applyWorksheetMetadataToGraph(ag, nullptr, QStringList(), defaultCurveStyle);
    ag->updatePlot();
    g->arrangeLayers(true, false);
    finalizeLastGraphSettings(g);
    g->applyPendingNewGraphStylePreset(); // E1: apply the new-graph preset now
    customMenu(g);
    emit modified();
    QApplication::restoreOverrideCursor();
    return g;
}

void ApplicationWindow::initBareMultilayerPlot(MultiLayer *g, const QString &name)
{ // FIXME: workaround, init without unnecessary g->show()
    QString label = name;
    while (alreadyUsedName(label))
        label = generateUniqueName(tr("Graph"));

    connectMultilayerPlot(g);

    g->setWindowTitle(label);
    g->setName(label);
    g->setWindowIcon(IconLoader::load("new-graph"));
    g->setScaleLayersOnPrint(d_scale_plots_on_print);
    g->printCropmarks(d_print_cropmarks);
    if (g->undoStack())
        g->undoStack()->setUndoLimit(qMax(1, undoLimit));

    d_workspace.addSubWindow(g);
    current_folder->addWindow(g);
    g->setFolder(current_folder);
    addListViewItem(g);
}

void ApplicationWindow::initMultilayerPlot(MultiLayer *g, const QString &name)
{
    Table *srcTbl = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (g && srcTbl) {
        g->setProperty("_tg_initial_source_table",
                       QVariant::fromValue(static_cast<QObject *>(srcTbl)));
    }
    initBareMultilayerPlot(g, name);
    applyLastGraphSettings(g);
    const bool isSplitLayout = d_modern_ribbon
            && (d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph);
    if (isTabbedWorkspaceView()) {
        g->showMaximized();
    } else if (isSplitLayout) {
        g->show();
    } else {
        applyLastGraphWindowGeometry(g);
    }
    g->setFocus();
}

void ApplicationWindow::setPreferences(Graph *g)
{
    if (!g->isPiePlot()) {
        if (allAxesOn) {
            QVector<bool> axesOn(QwtPlot::axisCnt);
            axesOn.fill(true);
            g->enableAxes(axesOn);
            if (g->plotWidget() && g->plotWidget()->axisScaleDraw(QwtPlot::xTop))
                g->plotWidget()->axisScaleDraw(QwtPlot::xTop)->enableComponent(QwtAbstractScaleDraw::Labels, false);
            if (g->plotWidget() && g->plotWidget()->axisScaleDraw(QwtPlot::yRight))
                g->plotWidget()->axisScaleDraw(QwtPlot::yRight)->enableComponent(QwtAbstractScaleDraw::Labels, false);
            g->setTopAxisTitle("");
            g->setRightAxisTitle("");
            g->updateSecondaryAxis(QwtPlot::xTop);
            g->updateSecondaryAxis(QwtPlot::yRight);
        }

        QList<int> ticksList;
        ticksList << majTicksStyle << majTicksStyle << majTicksStyle << majTicksStyle;
        g->setMajorTicksType(ticksList);
        ticksList.clear();
        ticksList << minTicksStyle << minTicksStyle << minTicksStyle << minTicksStyle;
        g->setMinorTicksType(ticksList);

        g->setTicksLength(minTicksLength, majTicksLength);
        g->setAxesLinewidth(axesLineWidth);
        g->drawAxesBackbones(drawBackbones);
    }

    g->initFonts(plotAxesFont, plotNumbersFont);
    g->setTextMarkerDefaults(legendFrameStyle, plotLegendFont, legendTextColor, legendBackground);
    g->setArrowDefaults(defaultArrowLineWidth, defaultArrowColor, defaultArrowLineStyle,
                        defaultArrowHeadLength, defaultArrowHeadAngle, defaultArrowHeadFill);
    g->initTitle(titleOn, plotTitleFont);
    g->drawCanvasFrame(canvasFrameOn, canvasFrameWidth);
    g->plotWidget()->setMargin(defaultPlotMargin);
    g->enableAutoscaling(autoscale2DPlots);
    g->setAutoscaleFonts(autoScaleFonts);
    g->setIgnoreResizeEvents(!autoResizeLayers);
    g->setAntialiasing(antialiasing2DPlots);

    if (d_restored_graph_settings.valid && !g->isPiePlot() && !g->isTernaryPlot()
        && !g->isPolarPlot()) {
        const SavedGraphSettings &s = d_restored_graph_settings;
        if (s.unitFormat == 1) {
            g->setProperty("_toriBracketUnitFmt", false);
            g->setAxisUnitFormat(Column::UnitFormat::ISIJSlash);
        } else if (s.unitFormat == 2) {
            g->setProperty("_toriBracketUnitFmt", true);
            g->setAxisUnitFormat(Column::UnitFormat::Paren);
        } else {
            g->setProperty("_toriBracketUnitFmt", false);
            g->setAxisUnitFormat(Column::UnitFormat::Paren);
        }
        if (Grid *gr = g->grid()) {
            gr->enableX(s.gridMajorX);
            gr->enableY(s.gridMajorY);
            gr->enableXMin(s.gridMinorX);
            gr->enableYMin(s.gridMinorY);
        }
    }
}

void ApplicationWindow::addErrorBars()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g)
        return;

    if (!g->curves()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("There are no curves available on this plot!"));
        return;
    }

    if (g->isPiePlot()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This functionality is not available for pie plots!"));
        return;
    }

    ErrDialog *ed = new ErrDialog(this);
    ed->setAttribute(Qt::WA_DeleteOnClose);
    connect(ed, SIGNAL(options(const QString &, int, const QString &, int)), this,
            SLOT(defineErrorBars(const QString &, int, const QString &, int)));
    connect(ed, SIGNAL(options(const QString &, const QString &, int)), this,
            SLOT(defineErrorBars(const QString &, const QString &, int)));

    ed->setCurveNames(g->analysableCurvesList());
    ed->setSrcTables(tableList());
    ed->exec();
}

void ApplicationWindow::defineErrorBars(const QString &name, int type, const QString &percent,
                                        int direction)
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    Table *w = table(name);
    if (!w || !w->d_future_table) { // user defined function
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(
                    this, tr("Error bars error"),
                    tr("This feature is not available for user defined function curves!"));
        }
        return;
    }

    DataCurve *master_curve = dynamic_cast<DataCurve *>(g->curve(name));
    if (!master_curve)
        return;
    QString xColName = master_curve->xColumnName();
    if (xColName.isEmpty())
        return;

    const QString srcColName = (direction == QwtErrorPlotCurve::Horizontal) ? xColName : name;
    const int srcColIdx = w->colIndex(srcColName);
    Column *data = (srcColIdx >= 0) ? w->column(srcColIdx) : nullptr;
    if (!data)
        return;

    const QString baseLabel = w->colLabel(srcColIdx)
            + (direction == QwtErrorPlotCurve::Horizontal ? QStringLiteral("_xErr")
                                                          : QStringLiteral("_yErr"));
    QString uniqueColName = baseLabel;
    int suffix = 2;
    const QStringList existingCols = w->colNames();
    while (existingCols.contains(uniqueColName)
           || w->colIndex(QString(w->name()) + QStringLiteral("_") + uniqueColName) >= 0) {
        uniqueColName = baseLabel + QStringLiteral("_") + QString::number(suffix++);
    }

    Column *errors = new Column(uniqueColName, SciDAVis::ColumnMode::Numeric);
    errors->setPlotDesignation(direction == QwtErrorPlotCurve::Horizontal ? SciDAVis::xErr
                                                                          : SciDAVis::yErr);

    const int rows = data->rowCount();
    if (type == 0) {
        const double fraction = percent.toDouble() / 100.0;
        for (int i = 0; i < rows; i++)
            errors->setValueAt(i, std::abs(data->valueAt(i) * fraction));
    } else if (type == 1 && rows > 0) {
        double average = 0.0;
        double dev = 0.0;
        for (int i = 0; i < rows; i++)
            average += data->valueAt(i);
        average /= rows;
        for (int i = 0; i < rows; i++)
            dev += pow(data->valueAt(i) - average, 2);
        dev = sqrt(dev / rows);
        for (int i = 0; i < rows; i++)
            errors->setValueAt(i, dev);
    }

    const QString beforeSnap = ml->captureUndoSnapshot();
    w->d_future_table->addChild(errors);
    const QString fullErrColName = QString(w->name()) + QStringLiteral("_") + errors->name();
    g->addErrorBars(xColName, name, w, fullErrColName, direction);
    g->updatePlot();
    ml->pushUndoSnapshot(tr("Add Error Bars"), beforeSnap);
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
        d_graph_inspector_dock->setTargetMultiLayer(ml, true);
    emit modified();
}

void ApplicationWindow::defineErrorBars(const QString &curveName, const QString &errColumnName,
                                        int direction)
{
    Table *w = table(curveName);
    if (!w) { // user defined function --> no worksheet available
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(
                    this, tr("Error"),
                    tr("This feature is not available for user defined function curves!"));
        }
        return;
    }

    Table *errTable = table(errColumnName);
    if (!errTable || w->numRows() != errTable->numRows()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("The selected columns have different numbers of rows!"));

            addErrorBars();
        }
        return;
    }

    int errCol = errTable->colIndex(errColumnName);
    if (errCol < 0 || !errTable->d_future_table->column(errCol)
        || errTable->d_future_table->column(errCol)->dataType() != SciDAVis::TypeDouble) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("You can only define error bars for numeric columns."));
            addErrorBars();
        }
        return;
    }

    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    const QString beforeSnap = ml->captureUndoSnapshot();
    g->addErrorBars(curveName, errTable, errColumnName, direction);
    g->updatePlot();
    ml->pushUndoSnapshot(tr("Add Error Bars"), beforeSnap);
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
        d_graph_inspector_dock->setTargetMultiLayer(ml, true);
    emit modified();
}

void ApplicationWindow::removeCurves(const QString &name)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (!w)
            continue;
        if (w->inherits("MultiLayer")) {
            QWidgetList lst = ((MultiLayer *)w)->graphPtrs();
            foreach (QWidget *widget, lst) {
                if (widget)
                    ((Graph *)widget)->removeCurves(name);
            }
        } else if (w->inherits("Graph3D")) {
            Graph3D *g3 = static_cast<Graph3D *>(w);
            if (graph3DUsesColumnExact(g3, name))
                g3->clearData();
        }
    }
    QApplication::restoreOverrideCursor();
}

void ApplicationWindow::updateCurves(Table *t, const QString &name)
{
    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (!w)
            continue;
        if (w->inherits("MultiLayer")) {
            QWidgetList graphsList = ((MultiLayer *)w)->graphPtrs();
            for (int k = 0; k < (int)graphsList.count(); k++) {
                Graph *g = (Graph *)graphsList.at(k);
                if (g)
                    g->updateCurvesData(t, name);
            }
        } else if (w->inherits("Graph3D")) {
            Graph3D *g3 = static_cast<Graph3D *>(w);
            if (graph3DUsesColumnExact(g3, name, t))
                g3->updateData(t);
        }
    }
}

void ApplicationWindow::showCurvesDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    if (((MultiLayer *)d_workspace.activeSubWindow())->isEmpty()) {
        QMessageBox::warning(this, tr("Error"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        return;
    }

    Graph *g = ((MultiLayer *)d_workspace.activeSubWindow())->activeGraph();
    if (!g)
        return;

    if (g->isPiePlot()) {
        QMessageBox::warning(this, tr("Error"),
                             tr("This functionality is not available for pie plots!"));
    } else {
        MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
        CurvesDialog *crvDialog = new CurvesDialog(this);
        crvDialog->setAttribute(Qt::WA_DeleteOnClose);
        crvDialog->setProperty("targetMultiLayerPtr",
                               QVariant::fromValue(static_cast<QObject *>(ml)));
        crvDialog->setGraph(g);
        crvDialog->resize(d_add_curves_dialog_size);
        crvDialog->show();
    }
}

void ApplicationWindow::showPlotAssociations(int curve)
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;

    Graph *g = ((MultiLayer *)w)->activeGraph();
    if (!g)
        return;

    AssociationsDialog *ad = new AssociationsDialog(this, Qt::WindowStaysOnTopHint);
    ad->setAttribute(Qt::WA_DeleteOnClose);
    ad->setProperty("targetMultiLayerPtr",
                    QVariant::fromValue(static_cast<QObject *>(static_cast<MultiLayer *>(w))));
    ad->setGraph(g);
    ad->initTablesList(tableList(), curve);
    ad->exec();
}

void ApplicationWindow::showTitleDialog()
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w)
        return;

    if (w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        Graph *g = ml->activeGraph();
        if (g) {
            ModalMultiLayerUndoHelper undoHelper(ml, tr("Edit Title"));
            TextDialog *td = new TextDialog(TextDialog::AxisTitle, this, Qt::Widget);
            td->setAttribute(Qt::WA_DeleteOnClose);
            connect(td, SIGNAL(changeFont(const QFont &)), g, SLOT(setTitleFont(const QFont &)));
            connect(td, SIGNAL(changeText(const QString &)), g, SLOT(setTitle(const QString &)));
            connect(td, SIGNAL(changeColor(const QColor &)), g,
                    SLOT(setTitleColor(const QColor &)));
            connect(td, SIGNAL(changeAlignment(int)), g, SLOT(setTitleAlignment(int)));

            QwtText t = g->plotWidget()->title();
            td->setText(t.text());
            td->setFont(t.font());
            td->setTextColor(t.color());
            td->setAlignment(t.renderFlags());
            td->exec();
            undoHelper.commitIfChanged();
        }
    } else if (w->inherits("Graph3D")) {
        openPlot3dDialogConfigured([](Plot3DDialog *pd) { pd->showTitleTab(); });
    }
}

void ApplicationWindow::showXAxisTitleDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g) {
        ModalMultiLayerUndoHelper undoHelper(ml, tr("Edit Axis Title"));
        TextDialog *td = new TextDialog(TextDialog::AxisTitle, this, Qt::Widget);
        td->setAttribute(Qt::WA_DeleteOnClose);
        connect(td, SIGNAL(changeFont(const QFont &)), g, SLOT(setXAxisTitleFont(const QFont &)));
        connect(td, SIGNAL(changeText(const QString &)), g, SLOT(setXAxisTitle(const QString &)));
        connect(td, SIGNAL(changeColor(const QColor &)), g,
                SLOT(setXAxisTitleColor(const QColor &)));
        connect(td, SIGNAL(changeAlignment(int)), g, SLOT(setXAxisTitleAlignment(int)));

        QStringList t = g->scalesTitles();
        td->setText(t[0]);
        td->setFont(g->axisTitleFont(2));
        td->setTextColor(g->axisTitleColor(2));
        td->setAlignment(g->axisTitleAlignment(2));
        td->setWindowTitle(tr("X Axis Title"));
        td->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::showYAxisTitleDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g) {
        ModalMultiLayerUndoHelper undoHelper(ml, tr("Edit Axis Title"));
        TextDialog *td = new TextDialog(TextDialog::AxisTitle, this, Qt::Widget);
        td->setAttribute(Qt::WA_DeleteOnClose);
        connect(td, SIGNAL(changeFont(const QFont &)), g, SLOT(setYAxisTitleFont(const QFont &)));
        connect(td, SIGNAL(changeText(const QString &)), g, SLOT(setYAxisTitle(const QString &)));
        connect(td, SIGNAL(changeColor(const QColor &)), g,
                SLOT(setYAxisTitleColor(const QColor &)));
        connect(td, SIGNAL(changeAlignment(int)), g, SLOT(setYAxisTitleAlignment(int)));

        QStringList t = g->scalesTitles();
        td->setText(t[1]);
        td->setFont(g->axisTitleFont(0));
        td->setTextColor(g->axisTitleColor(0));
        td->setAlignment(g->axisTitleAlignment(0));
        td->setWindowTitle(tr("Y Axis Title"));
        td->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::showRightAxisTitleDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g) {
        ModalMultiLayerUndoHelper undoHelper(ml, tr("Edit Axis Title"));
        TextDialog *td = new TextDialog(TextDialog::AxisTitle, this, Qt::Widget);
        td->setAttribute(Qt::WA_DeleteOnClose);
        connect(td, SIGNAL(changeFont(const QFont &)), g,
                SLOT(setRightAxisTitleFont(const QFont &)));
        connect(td, SIGNAL(changeText(const QString &)), g,
                SLOT(setRightAxisTitle(const QString &)));
        connect(td, SIGNAL(changeColor(const QColor &)), g,
                SLOT(setRightAxisTitleColor(const QColor &)));
        connect(td, SIGNAL(changeAlignment(int)), g, SLOT(setRightAxisTitleAlignment(int)));

        QStringList t = g->scalesTitles();
        td->setText(t[3]);
        td->setFont(g->axisTitleFont(1));
        td->setTextColor(g->axisTitleColor(1));
        td->setAlignment(g->axisTitleAlignment(1));
        td->setWindowTitle(tr("Right Axis Title"));
        td->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::showTopAxisTitleDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g) {
        ModalMultiLayerUndoHelper undoHelper(ml, tr("Edit Axis Title"));
        TextDialog *td = new TextDialog(TextDialog::AxisTitle, this, Qt::Widget);
        td->setAttribute(Qt::WA_DeleteOnClose);
        connect(td, SIGNAL(changeFont(const QFont &)), g, SLOT(setTopAxisTitleFont(const QFont &)));
        connect(td, SIGNAL(changeText(const QString &)), g, SLOT(setTopAxisTitle(const QString &)));
        connect(td, SIGNAL(changeColor(const QColor &)), g,
                SLOT(setTopAxisTitleColor(const QColor &)));
        connect(td, SIGNAL(changeAlignment(int)), g, SLOT(setTopAxisTitleAlignment(int)));

        QStringList t = g->scalesTitles();
        td->setText(t[2]);
        td->setFont(g->axisTitleFont(3));
        td->setTextColor(g->axisTitleColor(3));
        td->setAlignment(g->axisTitleAlignment(3));
        td->setWindowTitle(tr("Top Axis Title"));
        td->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::plot2VerticalLayers()
{
    multilayerPlot(1, 2, defaultCurveStyle);
}

void ApplicationWindow::plot2HorizontalLayers()
{
    multilayerPlot(2, 1, defaultCurveStyle);
}

void ApplicationWindow::plot4Layers()
{
    multilayerPlot(2, 2, defaultCurveStyle);
}

void ApplicationWindow::plotStackedLayers()
{
    multilayerPlot(1, -1, defaultCurveStyle);
}

void ApplicationWindow::plotStackedHistograms()
{
    multilayerPlot(1, -1, Graph::Histogram);
}

void ApplicationWindow::showGeneralPlotDialog()
{
    MyWidget *plot = (MyWidget *)d_workspace.activeSubWindow();
    if (!plot)
        return;

    if (plot->inherits("MultiLayer") && ((MultiLayer *)plot)->layers())
        showPlotDialog();
    else if (plot->inherits("Graph3D")) {
        openPlot3dDialogConfigured([](Plot3DDialog *gd) { gd->showGeneralTab(); });
    }
}

AxesDialog *ApplicationWindow::openAxesDialogConfigured(
        const std::function<void(AxesDialog *)> &configure)
{
    if (isNonInteractiveContext(this))
        return nullptr;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return nullptr;
    MultiLayer *ml = static_cast<MultiLayer *>(w);
    if (ml->isEmpty())
        return nullptr;
    Graph *g = ml->activeGraph();
    if (!g)
        return nullptr;

    auto &ad = addChild<AxesDialog>();
    ad.setAttribute(Qt::WA_DeleteOnClose, true);
    connect(&ad, &QDialog::finished, &ad, &QObject::deleteLater);
    ad.setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    ad.setGraph(g);
    ad.setCurrentScale(QwtScaleDraw::BottomScale);
    if (configure)
        configure(&ad);
    QPointer<AxesDialog> guard(&ad);
    ad.exec();
    if (guard && guard->isVisible() && configure)
        configure(guard.data());
    return guard.data();
}

void ApplicationWindow::showAxisDialog()
{
    QWidget *plot = (QWidget *)d_workspace.activeSubWindow();
    if (!plot)
        return;

    if (plot->inherits("MultiLayer") && ((MultiLayer *)plot)->layers()) {
        openAxesDialogConfigured([](AxesDialog *gd) { gd->showAxesPage(); });
    } else if (plot->inherits("Graph3D")) {
        openPlot3dDialogConfigured([](Plot3DDialog *gd) { gd->showAxisTab(); });
    }
}

void ApplicationWindow::showGridDialog()
{
    openAxesDialogConfigured([](AxesDialog *gd) { gd->showGridPage(); });
}

QDialog *ApplicationWindow::showScaleDialog()
{
    if (isNonInteractiveContext(this))
        return 0;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w)
        return 0;

    if (w->inherits("MultiLayer")) {
        return openAxesDialogConfigured(nullptr);
    } else if (w->inherits("Graph3D"))
        return showPlot3dDialog();

    return 0;
}

AxesDialog *ApplicationWindow::showScalePageFromAxisDialog(int axisPos)
{
    return openAxesDialogConfigured([axisPos](AxesDialog *gd) { gd->setCurrentScale(axisPos); });
}

AxesDialog *ApplicationWindow::showAxisPageFromAxisDialog(int axisPos)
{
    return openAxesDialogConfigured([axisPos](AxesDialog *gd) {
        gd->showAxesPage();
        gd->setCurrentScale(axisPos);
    });
}

Plot3DDialog *ApplicationWindow::openPlot3dDialogConfigured(
        const std::function<void(Plot3DDialog *)> &configure)
{
    if (isNonInteractiveContext(this))
        return nullptr;
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        Graph3D *g = (Graph3D *)d_workspace.activeSubWindow();
        if (!g->hasData()) {
            QApplication::restoreOverrideCursor();
            QMessageBox::warning(this, tr("Warning"),
                                 tr("Not available for empty 3D surface plots!"));
            return nullptr;
        }

        Plot3DDialog *pd = new Plot3DDialog(this);
        pd->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(pd, &QDialog::finished, pd, &QObject::deleteLater);
        pd->setPlot(g);

        connect(pd,
                SIGNAL(updateColors(const QColor &, const QColor &, const QColor &, const QColor &,
                                    const QColor &, const QColor &)),
                g,
                SLOT(updateColors(const QColor &, const QColor &, const QColor &, const QColor &,
                                  const QColor &, const QColor &)));
        connect(pd, SIGNAL(setDataColorMap(const QString &)), g,
                SLOT(setDataColorMap(const QString &)));
        connect(pd, SIGNAL(updateDataColors(const QColor &, const QColor &)), g,
                SLOT(setDataColors(const QColor &, const QColor &)));
        connect(pd, SIGNAL(updateTitle(const QString &, const QColor &, const QFont &)), g,
                SLOT(updateTitle(const QString &, const QColor &, const QFont &)));
        connect(pd, SIGNAL(updateResolution(int)), g, SLOT(setResolution(int)));
        connect(pd, SIGNAL(showColorLegend(bool)), g, SLOT(showColorLegend(bool)));
        connect(pd, SIGNAL(setOrtho(bool)), g, SLOT(setOrtho(bool)));
        connect(pd, SIGNAL(updateLabel(int, const QString &, const QFont &)), g,
                SLOT(updateLabel(int, const QString &, const QFont &)));
        connect(pd, SIGNAL(updateScale(int, const QStringList &)), g,
                SLOT(updateScale(int, const QStringList &)));
        connect(pd, SIGNAL(adjustLabels(int)), g, SLOT(adjustLabels(int)));
        connect(pd, SIGNAL(updateTickLength(int, double, double)), g,
                SLOT(updateTickLength(int, double, double)));
        connect(pd, SIGNAL(setNumbersFont(const QFont &)), g, SLOT(setNumbersFont(const QFont &)));
        connect(pd, SIGNAL(updateMeshLineWidth(int)), g, SLOT(setMeshLineWidth(int)));
        connect(pd, SIGNAL(updateBars(double)), g, SLOT(updateBars(double)));
        connect(pd, SIGNAL(updatePoints(double, bool)), g, SLOT(updatePoints(double, bool)));
        connect(pd, SIGNAL(showWorksheet()), g, SLOT(showWorksheet()));
        connect(pd, SIGNAL(updateZoom(double)), g, SLOT(updateZoom(double)));
        connect(pd, SIGNAL(updateScaling(double, double, double)), g,
                SLOT(updateScaling(double, double, double)));
        connect(pd, SIGNAL(updateCones(double, int)), g, SLOT(updateCones(double, int)));
        connect(pd, SIGNAL(updateCross(double, double, bool, bool)), g,
                SLOT(updateCross(double, double, bool, bool)));

        pd->setMeshLineWidth(g->meshLineWidth());
        pd->setDataColors(g->minDataColor(), g->maxDataColor());
        pd->setColors(g->titleColor(), g->meshColor(), g->axesColor(), g->numColor(),
                      g->labelColor(), g->bgColor(), g->gridColor());

        pd->setTitle(g->plotTitle());
        pd->setTitleFont(g->titleFont());

        pd->setZoom(g->zoom());
        pd->setScaling(g->xScale(), g->yScale(), g->zScale());
        pd->setResolution(g->resolution());
        pd->showLegend(g->isLegendOn());
        pd->setOrthogonal(g->isOrthogonal());
        pd->setAxesLabels(g->axesLabels());
        pd->setAxesTickLengths(g->axisTickLengths());
        pd->setAxesFonts(g->xAxisLabelFont(), g->yAxisLabelFont(), g->zAxisLabelFont());
        pd->setScales(g->scaleLimits());
        pd->setLabelsDistance(g->labelsDistance());
        pd->setNumbersFonts(g->numbersFont());

        if (g->coordStyle() == Qwt3D::NOCOORD)
            pd->disableAxesOptions();

        Qwt3D::PLOTSTYLE style = g->plotStyle();
        Graph3D::PointStyle pt = g->pointType();

        if (style == Qwt3D::USER) {
            switch (pt) {
            case Graph3D::None:
                break;

            case Graph3D::Dots:
                pd->disableMeshOptions();
                pd->initPointsOptionsStack();
                pd->showPointsTab(g->pointsSize(), g->smoothPoints());
                break;

            case Graph3D::VerticalBars:
                pd->showBarsTab(g->barsRadius());
                break;

            case Graph3D::HairCross:
                pd->disableMeshOptions();
                pd->initPointsOptionsStack();
                pd->showCrossHairTab(g->crossHairRadius(), g->crossHairLinewidth(),
                                     g->smoothCrossHair(), g->boxedCrossHair());
                break;

            case Graph3D::Cones:
                pd->disableMeshOptions();
                pd->initPointsOptionsStack();
                pd->showConesTab(g->coneRadius(), g->coneQuality());
                break;
            }
        } else if (style == Qwt3D::FILLED)
            pd->disableMeshOptions();
        else if (style == Qwt3D::HIDDENLINE || style == Qwt3D::WIREFRAME)
            pd->disableLegend();

        if (g->grids() == 0)
            pd->disableGridOptions();

        if (g->userFunction())
            pd->customWorksheetBtn(QString());
        else if (g->getTable())
            pd->customWorksheetBtn(tr("&Worksheet"));
        else if (g->matrix())
            pd->customWorksheetBtn(tr("&Matrix"));

        if (configure)
            configure(pd);
        QPointer<Plot3DDialog> guard(pd);
        pd->exec();
        return guard.data();
    }
    return nullptr;
}

QDialog *ApplicationWindow::showPlot3dDialog()
{
    return openPlot3dDialogConfigured(nullptr);
}

void ApplicationWindow::showPlotDialog(int curveKey)
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w)
        return;

    if (w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        PlotDialog *pd = new PlotDialog(d_extended_plot_dialog, this);
        pd->setAttribute(Qt::WA_DeleteOnClose);
        pd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
        pd->setMultiLayer(ml);
        pd->insertColumnsList(columnsList());
        if (curveKey >= 0) {
            Graph *g = ml->activeGraph();
            if (g)
                pd->selectCurve(g->curveIndex(curveKey));
        }
        pd->initFonts(plotTitleFont, plotAxesFont, plotNumbersFont, plotLegendFont);
        pd->showAll(d_extended_plot_dialog);
        pd->show();
    }
}

void ApplicationWindow::showCurvePlotDialog()
{
    showPlotDialog(actionShowCurvePlotDialog->data().toInt());
}

void ApplicationWindow::showAllCurves()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    const QString before = ml->captureUndoSnapshot();
    for (int i = 0; i < g->curves(); i++)
        g->showCurve(i);
    g->replot();
    ml->pushUndoSnapshot(tr("Show All Curves"), before);
    emit modified();
}

void ApplicationWindow::hideOtherCurves()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    const QString before = ml->captureUndoSnapshot();
    int curveKey = actionHideOtherCurves->data().toInt();
    for (int i = 0; i < g->curves(); i++)
        g->showCurve(i, false);

    g->showCurve(g->curveIndex(curveKey));
    g->replot();
    ml->pushUndoSnapshot(tr("Hide Other Curves"), before);
    emit modified();
}

void ApplicationWindow::hideCurve()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    const QString before = ml->captureUndoSnapshot();
    int curveKey = actionHideCurve->data().toInt();
    g->showCurve(g->curveIndex(curveKey), false);
    ml->pushUndoSnapshot(tr("Hide Curve"), before);
    emit modified();
}

void ApplicationWindow::removeCurve()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    const QString before = ml->captureUndoSnapshot();
    int curveKey = actionRemoveCurve->data().toInt();
    g->removeCurve(g->curveIndex(curveKey));
    g->updatePlot();
    for (QDialog *dlg : findChildren<QDialog *>()) {
        if (!dlg || dlg->isModal())
            continue;
        if (qobject_cast<PlotDialog *>(dlg) || qobject_cast<CurveRangeDialog *>(dlg)
            || qobject_cast<CurvesDialog *>(dlg) || qobject_cast<FunctionDialog *>(dlg)) {
            const QVariant tp = dlg->property("targetMultiLayerPtr");
            if (!tp.isValid() || storedTargetPointer(tp) == static_cast<QObject *>(ml)) {
                invalidateModelessGraphDialogs(ml);
                break;
            }
        }
    }
    ml->pushUndoSnapshot(tr("Remove Curve"), before);
    emit modified();
}

void ApplicationWindow::showCurveWorksheet(Graph *g, int curveIndex)
{
    if (!g)
        return;

    QwtPlotItem *it = g->plotItem(curveIndex);
    if (!it)
        return;

    if (auto sp = dynamic_cast<Spectrogram *>(it)) {
        if (sp->matrix()) {
            Matrix *m = sp->matrix();
            if (m->isMinimized() || m->isHidden())
                m->showNormal();
            activateSubWindow(m);
            m->raise();
            m->setFocus();
        }
    } else if (PlotCurve *pc = dynamic_cast<PlotCurve *>(it)) {
        int clickedRow = -1;
        if (actionShowCurveWorksheet) {
            const QVariant prop = actionShowCurveWorksheet->property("_targetRow");
            if (prop.isValid())
                clickedRow = prop.toInt();
            actionShowCurveWorksheet->setProperty("_targetRow", -1);
        }
        if (pc->type() == Graph::Function) {
            g->createTable(pc);
        } else {
            const QString colTitle = it->title().text();
            showTable(colTitle);
            DataCurve *dc = dynamic_cast<DataCurve *>(pc);
            Table *t = dc ? dc->table() : table(colTitle);
            if (!t)
                t = table(colTitle);
            if (t) {
                const int colIdx = t->colIndex(colTitle);
                if (colIdx >= 0) {
                    if (clickedRow >= 0 && clickedRow < t->numRows()) {
                        t->deselectAll();
                        t->setCellSelected(clickedRow, colIdx, true);
                        t->goToCell(clickedRow, colIdx);
                    } else if (dc && dc->startRow() > 0 && dc->startRow() < t->numRows()) {
                        t->goToCell(dc->startRow(), colIdx);
                    }
                }
            }
        }
    }
}

void ApplicationWindow::showCurveWorksheet()
{
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (!g)
        return;

    int curveKey = actionShowCurveWorksheet->data().toInt();
    showCurveWorksheet(g, g->curveIndex(curveKey));
}

void ApplicationWindow::zoomIn()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"
                                    "<p><h4>Please add a layer and try again!</h4>"));
        }
        btnPointer->setChecked(true);
        return;
    }

    if ((Graph *)plot->activeGraph()->isPiePlot()) {
        if (btnZoomIn->isChecked() && !isNonInteractiveContext(this))
            QMessageBox::warning(this, tr("Warning"),
                                 tr("This functionality is not available for pie plots!"));
        btnPointer->setChecked(true);
        return;
    }

    QWidgetList graphsList = plot->graphPtrs();
    foreach (QWidget *widget, graphsList) {
        Graph *g = (Graph *)widget;
        if (!g->isPiePlot())
            g->zoom(true);
    }
}

void ApplicationWindow::zoomOut()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty() || (Graph *)plot->activeGraph()->isPiePlot())
        return;

    ((Graph *)plot->activeGraph())->zoomOut();
    btnPointer->setChecked(true);
}

void ApplicationWindow::setAutoScale()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"));
        }
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (g) {
        const QString before = plot->captureUndoSnapshot();
        g->setAutoScale();
        plot->pushUndoSnapshot(tr("Autoscale Axes"), before);
        emit modified();
    }
}

void ApplicationWindow::removePoints()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        btnPointer->setChecked(true);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g || !g->validCurvesDataSize()) {
        btnPointer->setChecked(true);
        return;
    }

    if (g->isPiePlot()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This functionality is not available for pie plots!"));
        btnPointer->setChecked(true);
        return;
    } else {
        switch (QMessageBox::warning(this, tr("ToriGraph"),
                                     tr("This will modify the data in the worksheets!\nAre you "
                                        "sure you want to continue?"),
                                     tr("Continue"), tr("Cancel"), 0, 1)) {
        case 0:
            g->setActiveTool(new DataPickerTool(g, this, DataPickerTool::Remove, d_status_info,
                                                SLOT(setText(const QString &))));
            break;

        case 1:
            btnPointer->setChecked(true);
            break;
        }
    }
}

void ApplicationWindow::movePoints()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        btnPointer->setChecked(true);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g || !g->validCurvesDataSize()) {
        btnPointer->setChecked(true);
        return;
    }

    if (g->isPiePlot()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This functionality is not available for pie plots!"));

        btnPointer->setChecked(true);
        return;
    } else {
        switch (QMessageBox::warning(this, tr("ToriGraph"),
                                     tr("This will modify the data in the worksheets!\nAre you "
                                        "sure you want to continue?"),
                                     tr("Continue"), tr("Cancel"), 0, 1)) {
        case 0:
            if (g) {
                g->setActiveTool(new DataPickerTool(g, this, DataPickerTool::Move, d_status_info,
                                                    SLOT(setText(const QString &))));
            }
            break;

        case 1:
            btnPointer->setChecked(true);
            break;
        }
    }
}

void ApplicationWindow::showScreenReader()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"
                                    "<p><h4>Please add a layer and try again!</h4>"));
        }
        btnPointer->setChecked(true);
        return;
    }

    QWidgetList graphsList = plot->graphPtrs();
    foreach (QWidget *w, graphsList)
        ((Graph *)w)
                ->setActiveTool(new ScreenPickerTool((Graph *)w, d_status_info,
                                                     SLOT(setText(const QString &))));
}

void ApplicationWindow::showRangeSelectors()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("There are no plot layers available in this window!"));
        }
        btnPointer->setChecked(true);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g)
        return;

    if (!g->curves()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("There are no curves available on this plot!"));
        }
        btnPointer->setChecked(true);
        return;
    } else if (g->isPiePlot()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("This functionality is not available for pie plots!"));
        }
        btnPointer->setChecked(true);
        return;
    }

    g->enableRangeSelectors(d_status_info, SLOT(setText(const QString &)));
}

void ApplicationWindow::showCursor()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"
                                    "<p><h4>Please add a layer and try again!</h4>"));
        }
        btnPointer->setChecked(true);
        return;
    }

    if ((Graph *)plot->activeGraph()->isPiePlot()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("This functionality is not available for pie plots!"));
        }

        btnPointer->setChecked(true);
        return;
    }

    QWidgetList graphsList = plot->graphPtrs();
    foreach (QWidget *w, graphsList)
        if (!((Graph *)w)->isPiePlot() && ((Graph *)w)->validCurvesDataSize())
            ((Graph *)w)
                    ->setActiveTool(new DataPickerTool((Graph *)w, this, DataPickerTool::Display,
                                                       d_status_info,
                                                       SLOT(setText(const QString &))));
}

void ApplicationWindow::newLegend()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"
                                    "<p><h4>Please add a layer and try again!</h4>"));
        }
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (g) {
        const QString before = plot->captureUndoSnapshot();
        if (g->legend())
            g->reconstructLegend();
        else
            g->newLegend();
        plot->pushUndoSnapshot(tr("New Legend"), before);
        emit modified();
    }
}

void ApplicationWindow::addTimeStamp()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"
                                    "<p><h4>Please add a layer and try again!</h4>"));
        }
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (g) {
        const QString before = plot->captureUndoSnapshot();
        g->addTimeStamp();
        plot->pushUndoSnapshot(tr("Add Time Stamp"), before);
        emit modified();
    }
}

void ApplicationWindow::disableAddText()
{
    actionAddText->setChecked(false);
    showTextDialog();
}

void ApplicationWindow::addText()
{
    if (isNonInteractiveContext(this))
        return;
    if (!btnPointer->isChecked())
        btnPointer->setChecked(true);

    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();

    // C9: no "new layer or active layer?" question. Text goes on the active layer at the
    // clicked position and is typed in place (CanvasPicker::drawTextMarker opens the inline
    // editor). A page without layers gets a text layer, which was the only sensible answer there.
    if (plot->isEmpty()) {
        plot->addTextLayer(legendFrameStyle, plotLegendFont, legendTextColor, legendBackground);
        actionAddText->setChecked(false);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g) {
        actionAddText->setChecked(false);
        return;
    }
    g->drawText(true);
    statusBar()->showMessage(
            tr("Click on the graph where the text should go, then type it (Esc to cancel)."),
            8000);
    refreshToolModeIndicator();
}

void ApplicationWindow::addImage()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g)
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
            QFileDialog::getOpenFileName(this, tr("Insert image from file"), imagesDirPath, filter);
    if (!fn.isEmpty()) {
        QFileInfo fi(fn);
        imagesDirPath = fi.absolutePath();

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        const QString before = plot->captureUndoSnapshot();
        g->addImage(fn);
        plot->pushUndoSnapshot(tr("Add Image"), before);
        QApplication::restoreOverrideCursor();
        emit modified();
    }
}

void ApplicationWindow::drawLine()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"
                                    "<p><h4>Please add a layer and try again!</h4>"));
        }

        btnPointer->setChecked(true);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (g) {
        g->drawLine(true);
        emit modified();
    }
}

void ApplicationWindow::drawArrow()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("<h4>There are no plot layers available in this window.</h4>"
                                    "<p><h4>Please add a layer and try again!</h4>"));
        }

        btnPointer->setChecked(true);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (g) {
        g->drawLine(true, 1);
        emit modified();
    }
}

void ApplicationWindow::showImageDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g) {
        ImageMarker *im = (ImageMarker *)g->selectedMarkerPtr();
        if (!im)
            return;

        ModalMultiLayerUndoHelper undoHelper(ml, tr("Edit Annotation"));
        ImageDialog *id = new ImageDialog(this);
        id->setAttribute(Qt::WA_DeleteOnClose);
        connect(id, SIGNAL(setGeometry(int, int, int, int)), g,
                SLOT(updateImageMarker(int, int, int, int)));
        id->setWindowIcon(QPixmap(":/appicon"));
        id->setOrigin(im->origin());
        id->setSize(im->size());
        id->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::showLayerDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("There are no plot layers available in this window."));
        return;
    }

    LayerDialog *id = new LayerDialog(this);
    id->setAttribute(Qt::WA_DeleteOnClose);
    id->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(plot)));
    id->setMultiLayer(plot);
    id->exec();
}

void ApplicationWindow::showPlotGeometryDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    Graph *g = plot->activeGraph();
    if (g) {
        ModalMultiLayerUndoHelper undoHelper(plot, tr("Layer Geometry"));
        ImageDialog *id = new ImageDialog(this);
        id->setAttribute(Qt::WA_DeleteOnClose);
        connect(id, SIGNAL(setGeometry(int, int, int, int)), plot,
                SLOT(setGraphGeometry(int, int, int, int)));
        id->setWindowIcon(QPixmap(":/appicon"));
        id->setWindowTitle(tr("Layer Geometry"));
        id->setOrigin(g->pos());
        id->setSize(g->plotWidget()->size());
        id->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::showTextDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g) {
        Legend *m = (Legend *)g->selectedMarkerPtr();
        if (!m)
            return;

        const QString undoLbl = (m == g->legend()) ? tr("Modify Legend") : tr("Edit Annotation");
        ModalMultiLayerUndoHelper undoHelper(ml, undoLbl);
        TextDialog *td = new TextDialog(TextDialog::TextMarker, this, Qt::Widget);
        td->setAttribute(Qt::WA_DeleteOnClose);
        connect(td,
                SIGNAL(values(const QString &, int, int, const QFont &, const QColor &,
                              const QColor &)),
                g,
                SLOT(updateTextMarker(const QString &, int, int, const QFont &, const QColor &,
                                      const QColor &)));

        td->setWindowIcon(QPixmap(":/appicon"));
        td->setText(m->text());
        td->setFont(m->font());
        td->setTextColor(m->textColor());
        td->setBackgroundColor(m->backgroundColor());
        td->setBackgroundType(m->frameStyle());
        td->setAngle(m->angle());
        td->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::showLineDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g) {
        ArrowMarker *lm = (ArrowMarker *)g->selectedMarkerPtr();
        if (!lm)
            return;

        ModalMultiLayerUndoHelper undoHelper(ml, tr("Edit Annotation"));
        LineDialog *ld = new LineDialog(lm, this);
        ld->setAttribute(Qt::WA_DeleteOnClose);
        ld->exec();
        undoHelper.commitIfChanged();
    }
}

void ApplicationWindow::showPlotWizard()
{
    if (isNonInteractiveContext(this))
        return;

    if (tableWindows().count() > 0) {
        PlotWizard *pw = new PlotWizard(this, Qt::Widget);
        pw->setAttribute(Qt::WA_DeleteOnClose);
        connect(pw, SIGNAL(plot(const QStringList &)), this,
                SLOT(multilayerPlot(const QStringList &)));

        pw->insertTablesList(tableWindows());
        // TODO: string list -> Column * list
        pw->setColumnsList(columnsList());
        pw->changeColumnsList(tableWindows()[0]);
        pw->exec();
    } else
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no tables available in this project.</h4>"
                                "<p><h4>Please create a table and try again!</h4>"));
}

void ApplicationWindow::setCurveFullRange()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g)
        return;

    const QString before = ml->captureUndoSnapshot();
    int curveKey = actionCurveFullRange->data().toInt();
    g->setCurveFullRange(g->curveIndex(curveKey));
    ml->pushUndoSnapshot(tr("Change Curve Range"), before);
    emit modified();
}

void ApplicationWindow::showCurveRangeDialog()
{
    if (isNonInteractiveContext(this))
        return;

    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (!g)
        return;

    int curveKey = actionEditCurveRange->data().toInt();
    showCurveRangeDialog(g, g->curveIndex(curveKey));
}

CurveRangeDialog *ApplicationWindow::showCurveRangeDialog(Graph *g, int curve)
{
    if (!g || isNonInteractiveContext(this))
        return 0;

    CurveRangeDialog *crd = new CurveRangeDialog(this);
    crd->setAttribute(Qt::WA_DeleteOnClose);
    if (MultiLayer *ml = g->multiLayerParent())
        crd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    crd->setCurveToModify(g, curve);
    crd->show();
    return crd;
}

void ApplicationWindow::showFunctionDialog()
{
    if (isNonInteractiveContext(this))
        return;

    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (!g)
        return;

    int curveKey = actionEditFunction->data().toInt();
    showFunctionDialog(g, g->curveIndex(curveKey));
}

void ApplicationWindow::showFunctionDialog(Graph *g, int curve)
{
    if (!g || isNonInteractiveContext(this))
        return;

    FunctionDialog *fd = functionDialog();
    if (!fd)
        return;
    if (MultiLayer *ml = g->multiLayerParent())
        fd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    fd->setWindowTitle(tr("Edit function"));
    fd->setCurveToModify(g, curve);
}

FunctionDialog *ApplicationWindow::functionDialog()
{
    if (isNonInteractiveContext(this))
        return 0;

    FunctionDialog *fd = new FunctionDialog(this);
    fd->setAttribute(Qt::WA_DeleteOnClose);
    connect(fd, SIGNAL(clearParamFunctionsList()), this, SLOT(clearParamFunctionsList()));
    connect(fd, SIGNAL(clearPolarFunctionsList()), this, SLOT(clearPolarFunctionsList()));

    fd->insertParamFunctionsList(xFunctions, yFunctions);
    fd->insertPolarFunctionsList(rFunctions, thetaFunctions);
    fd->show();
    fd->activateWindow();
    return fd;
}

void ApplicationWindow::addFunctionCurve()
{
    if (isNonInteractiveContext(this))
        return;

    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;

    if (((MultiLayer *)w)->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        return;
    }

    Graph *g = ((MultiLayer *)w)->activeGraph();
    if (g) {
        FunctionDialog *fd = functionDialog();
        if (fd) {
            fd->setProperty("targetMultiLayerPtr",
                            QVariant::fromValue(static_cast<QObject *>(static_cast<MultiLayer *>(w))));
            fd->setGraph(g);
        }
    }
}

void ApplicationWindow::updateFunctionLists(int type, QStringList &formulas)
{
    int maxListSize = 10;
    if (type == 2) {
        rFunctions.removeAll(formulas[0]);
        rFunctions.push_front(formulas[0]);

        thetaFunctions.removeAll(formulas[1]);
        thetaFunctions.push_front(formulas[1]);

        while ((int)rFunctions.size() > maxListSize)
            rFunctions.pop_back();
        while ((int)thetaFunctions.size() > maxListSize)
            thetaFunctions.pop_back();
    } else if (type == 1) {
        xFunctions.removeAll(formulas[0]);
        xFunctions.push_front(formulas[0]);

        yFunctions.removeAll(formulas[1]);
        yFunctions.push_front(formulas[1]);

        while ((int)xFunctions.size() > maxListSize)
            xFunctions.pop_back();
        while ((int)yFunctions.size() > maxListSize)
            yFunctions.pop_back();
    }
}

bool ApplicationWindow::newFunctionPlot(int type, QStringList &formulas, const QString &var,
                                        QList<double> &ranges, int points)
{
    MultiLayer *ml = newGraph();
    if (!ml)
        return false;

    if (!ml->activeGraph()->addFunctionCurve(this, type, formulas, var, ranges, points))
        return false;

    updateFunctionLists(type, formulas);
    return true;
}

void ApplicationWindow::setFramed3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->setFramed();
        actionShowAxisDialog->setEnabled(true);
    }
}

void ApplicationWindow::setBoxed3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->setBoxed();
        actionShowAxisDialog->setEnabled(true);
    }
}

void ApplicationWindow::removeAxes3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->setNoAxes();
        actionShowAxisDialog->setEnabled(false);
    }
}

void ApplicationWindow::removeGrid3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setNoGrid();
}

void ApplicationWindow::setHiddenLineGrid3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setHiddenLineGrid();
}

void ApplicationWindow::setPoints3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setPointsMesh();
}

void ApplicationWindow::setCones3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setConesMesh();
}

void ApplicationWindow::setCrosses3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setCrossMesh();
}

void ApplicationWindow::setBars3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setBarsPlot();
}

void ApplicationWindow::setLineGrid3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setLineGrid();
}

void ApplicationWindow::setFilledMesh3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setFilledMesh();
}

void ApplicationWindow::setFloorData3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setFloorData();
}

void ApplicationWindow::setFloorIso3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setFloorIsolines();
}

void ApplicationWindow::setEmptyFloor3DPlot()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setEmptyFloor();
}

void ApplicationWindow::setFrontGrid3DPlot(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setFrontGrid(on);
}

void ApplicationWindow::setBackGrid3DPlot(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setBackGrid(on);
}

void ApplicationWindow::setFloorGrid3DPlot(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setFloorGrid(on);
}

void ApplicationWindow::setCeilGrid3DPlot(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setCeilGrid(on);
}

void ApplicationWindow::setRightGrid3DPlot(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setRightGrid(on);
}

void ApplicationWindow::setLeftGrid3DPlot(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->setLeftGrid(on);
}

void ApplicationWindow::pickPlotStyle(QAction *action)
{
    if (!action)
        return;

    if (action == polygon) {
        removeGrid3DPlot();
    } else if (action == filledmesh) {
        setFilledMesh3DPlot();
    } else if (action == wireframe) {
        setLineGrid3DPlot();
    } else if (action == hiddenline) {
        setHiddenLineGrid3DPlot();
    } else if (action == pointstyle) {
        setPoints3DPlot();
    } else if (action == conestyle) {
        setCones3DPlot();
    } else if (action == crossHairStyle) {
        setCrosses3DPlot();
    } else if (action == barstyle) {
        setBars3DPlot();
    }
    emit modified();
}

void ApplicationWindow::pickCoordSystem(QAction *action)
{
    if (!action)
        return;

    if (action == Box || action == Frame) {
        if (action == Box)
            setBoxed3DPlot();
        if (action == Frame)
            setFramed3DPlot();
        grids->setEnabled(true);
    } else if (action == None) {
        removeAxes3DPlot();
        grids->setEnabled(false);
    }

    emit modified();
}

void ApplicationWindow::pickFloorStyle(QAction *action)
{
    if (!action)
        return;

    if (action == floordata) {
        setFloorData3DPlot();
    } else if (action == flooriso) {
        setFloorIso3DPlot();
    } else {
        setEmptyFloor3DPlot();
    }

    emit modified();
}

void ApplicationWindow::pixelLineProfile()
{
    if (isNonInteractiveContext(this))
        return;

    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (!g)
        return;

    bool ok;
    int res = QInputDialog::getInt(this, tr("Set the number of pixels to average"),
                                   tr("Number of averaged pixels"), 1, 1, 2000, 2, &ok);
    if (!ok)
        return;

    LineProfileTool *lpt = new LineProfileTool(g, res);
    connect(lpt, SIGNAL(createTablePlot(const QString &, const QString &, QList<Column *>)), this,
            SLOT(newWrksheetPlot(const QString &, const QString &, QList<Column *>)));
    g->setActiveTool(lpt);
}

void ApplicationWindow::intensityTable()
{
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (g)
        g->showIntensityTable();
}

void ApplicationWindow::autoArrangeLayers()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)d_workspace.activeSubWindow();
    plot->setMargins(5, 5, 5, 5);
    plot->setSpacing(5, 5);
    plot->arrangeLayers(true, false);
}

void ApplicationWindow::addLayer()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    QPointer<MultiLayer> plot = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!plot)
        return;
    if (isNonInteractiveContext(this)) {
        const QString before = plot->captureUndoSnapshot();
        setPreferences(plot->addLayer());
        plot->arrangeLayers(true, false);
        plot->pushUndoSnapshot(tr("Add Layer"), before);
        emit modified();
        return;
    }
    const int choice = QMessageBox::information(
            this, tr("Guess best origin for the new layer?"),
            tr("Do you want ToriGraph to guess the best position for the new layer?\n Warning: this "
               "will rearrange existing layers!"),
            tr("&Guess"), tr("&Top-left corner"), tr("&Cancel"), 0, 2);
    if (!plot)
        return;
    switch (choice) {
    case 0: {
        const QString before = plot->captureUndoSnapshot();
        setPreferences(plot->addLayer());
        plot->arrangeLayers(true, false);
        plot->pushUndoSnapshot(tr("Add Layer"), before);
        emit modified();
    } break;

    case 1: {
        const QString before = plot->captureUndoSnapshot();
        const QSize cvSize = plot->canvasWidget() ? plot->canvasWidget()->size() : plot->size();
        setPreferences(plot->addLayer(0, 0, cvSize.width(), cvSize.height()));
        plot->pushUndoSnapshot(tr("Add Layer"), before);
        emit modified();
    } break;

    case 2:
        return;
    }
}

void ApplicationWindow::deleteLayer()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (isNonInteractiveContext(this)) {
        ml->removeLayer();
        return;
    }
    ml->confirmRemoveLayer();
}

void ApplicationWindow::pickPointerCursor()
{
    btnPointer->setChecked(true);
}

void ApplicationWindow::pickDataTool(QAction *action)
{
    if (!action)
        return;

    MultiLayer *m = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!m)
        return;

    Graph *g = m->activeGraph();
    if (!g)
        return;

    g->disableTools();

    if (action == btnCursor)
        showCursor();
    else if (action == btnSelect)
        showRangeSelectors();
    else if (action == btnPicker)
        showScreenReader();
    else if (action == btnMovePoints)
        movePoints();
    else if (action == btnRemovePoints)
        removePoints();
    else if (action == btnMaskPoints)
        maskDataPointsTool();
    else if (action == btnUnmaskPoints)
        unmaskDataPointsTool();
    else if (action == btnZoomIn)
        zoomIn();
    else if (action == btnZoomOut)
        zoomOut();
    else if (action == btnArrow)
        drawArrow();
    else if (action == btnLine)
        drawLine();
}

void ApplicationWindow::connectSurfacePlot(Graph3D *plot)
{
    connect(plot, SIGNAL(showTitleBarMenu()), this, SLOT(showWindowTitleBarMenu()));
    connect(plot, SIGNAL(showContextMenu()), this, SLOT(showWindowContextMenu()));
    connect(plot, SIGNAL(showOptionsDialog()), this, SLOT(showPlot3dDialog()));
    connect(plot, SIGNAL(closedWindow(MyWidget *)), this, SLOT(closeWindow(MyWidget *)));
    connect(plot, SIGNAL(hiddenWindow(MyWidget *)), this, SLOT(hideWindow(MyWidget *)));
    connect(plot, SIGNAL(statusChanged(MyWidget *)), this, SLOT(updateWindowStatus(MyWidget *)));
    connect(plot, SIGNAL(modified()), this, SIGNAL(modified()));
    connect(plot, SIGNAL(custom3DActions(MyWidget *)), this, SLOT(custom3DActions(MyWidget *)));

    plot->askOnCloseEvent(confirmClosePlot3D);
}

void ApplicationWindow::connectMultilayerPlot(MultiLayer *g)
{
    connect(g, SIGNAL(showTitleBarMenu()), this, SLOT(showWindowTitleBarMenu()));
    connect(g, SIGNAL(showTextDialog()), this, SLOT(showTextDialog()));
    connect(g, SIGNAL(showPlotDialog(int)), this, SLOT(showPlotDialog(int)));
    connect(g, SIGNAL(showScaleDialog(int)), this, SLOT(showScalePageFromAxisDialog(int)));
    connect(g, SIGNAL(showAxisDialog(int)), this, SLOT(showAxisPageFromAxisDialog(int)));
    connect(g, SIGNAL(showCurveContextMenu(int)), this, SLOT(showCurveContextMenu(int)));
    connect(g, SIGNAL(showWindowContextMenu()), this, SLOT(showWindowContextMenu()));
    connect(g, SIGNAL(showCurvesDialog()), this, SLOT(showCurvesDialog()));
    connect(g, SIGNAL(drawLineEnded(bool)), btnPointer, SLOT(setChecked(bool)));
    connect(g, SIGNAL(drawTextOff()), this, SLOT(disableAddText()));
    connect(g, SIGNAL(showXAxisTitleDialog()), this, SLOT(showXAxisTitleDialog()));
    connect(g, SIGNAL(showYAxisTitleDialog()), this, SLOT(showYAxisTitleDialog()));
    connect(g, SIGNAL(showRightAxisTitleDialog()), this, SLOT(showRightAxisTitleDialog()));
    connect(g, SIGNAL(showTopAxisTitleDialog()), this, SLOT(showTopAxisTitleDialog()));
    connect(g, SIGNAL(showMarkerPopupMenu()), this, SLOT(showMarkerPopupMenu()));
    connect(g, SIGNAL(closedWindow(MyWidget *)), this, SLOT(closeWindow(MyWidget *)));
    connect(g, SIGNAL(hiddenWindow(MyWidget *)), this, SLOT(hideWindow(MyWidget *)));
    connect(g, SIGNAL(statusChanged(MyWidget *)), this, SLOT(updateWindowStatus(MyWidget *)));
    connect(g, SIGNAL(cursorInfo(const QString &)), d_status_info, SLOT(setText(const QString &)));
    connect(g, SIGNAL(showImageDialog()), this, SLOT(showImageDialog()));
    connect(g, SIGNAL(createTable(const QString &, const QString &, QList<Column *>)), this,
            SLOT(newTable(const QString &, const QString &, QList<Column *>)));
    connect(g, SIGNAL(viewTitleDialog()), this, SLOT(showTitleDialog()));
    connect(g, SIGNAL(modifiedWindow(MyWidget *)), this, SLOT(modifiedProject(MyWidget *)));
    connect(g, SIGNAL(modifiedPlot()), this, SLOT(modifiedProject()));
    connect(g, SIGNAL(showLineDialog()), this, SLOT(showLineDialog()));
    connect(g, SIGNAL(showGeometryDialog()), this, SLOT(showPlotGeometryDialog()));
    connect(g, SIGNAL(pasteMarker()), this, SLOT(pasteSelection()));
    connect(g, SIGNAL(showGraphContextMenu()), this, SLOT(showGraphContextMenu()));
    connect(g, SIGNAL(showLayerButtonContextMenu()), this, SLOT(showLayerButtonContextMenu()));
    connect(g, SIGNAL(createIntensityTable(const QString &)), this,
            SLOT(importImage(const QString &)));
    connect(g, SIGNAL(setPointerCursor()), this, SLOT(pickPointerCursor()));

    if (QUndoStack *us = g->undoStack()) {
        connect(us, &QUndoStack::canUndoChanged, this,
                [this](bool) { updateUndoRedoActionsForActiveWindow(); });
        connect(us, &QUndoStack::canRedoChanged, this,
                [this](bool) { updateUndoRedoActionsForActiveWindow(); });
        connect(us, &QUndoStack::indexChanged, this,
                [this](int) { updateUndoRedoActionsForActiveWindow(); });
    }

    g->askOnCloseEvent(confirmClosePlot2D);
    g->removeEventFilter(this);
    g->installEventFilter(this);
}

Graph3D *ApplicationWindow::openMatrixPlot3D(const QString &caption, const QString &matrix_name,
                                             double xl, double xr, double yl, double yr, double zl,
                                             double zr)
{
    QString name = matrix_name;
    name.remove("matrix<", Qt::CaseSensitive);
    name.remove(">");
    Matrix *m = matrix(name);
    if (!m)
        return 0;

    QString label = caption;
    while (alreadyUsedName(label))
        label = generateUniqueName(tr("Graph"));

    Graph3D *plot = new Graph3D("", &d_workspace, 0, Qt::Widget);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->setWindowTitle(label);
    plot->setName(label);
    plot->addMatrixData(m, xl, xr, yl, yr, zl, zr);
    plot->update();

    initPlot3D(plot);
    return plot;
}

void ApplicationWindow::plot3DMatrix(int style)
{
    Matrix *mat = qobject_cast<Matrix *>(d_workspace.activeSubWindow());
    if (!mat)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString label = generateUniqueName(tr("Graph"));

    Graph3D *plot = new Graph3D("", &d_workspace, 0);
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->addMatrixData(mat);
    plot->customPlotStyle(style);
    customPlot3D(plot);
    plot->update();

    plot->resize(500, 400);
    plot->setWindowTitle(label);
    plot->setName(label);
    initPlot3D(plot);

    emit modified();
    QApplication::restoreOverrideCursor();
}

void ApplicationWindow::plotGrayScale()
{
    if (Matrix *mat = qobject_cast<Matrix *>(d_workspace.activeSubWindow()))
        plotSpectrogram(mat, Graph::GrayMap);
}

MultiLayer *ApplicationWindow::plotGrayScale(Matrix *m)
{
    if (!m)
        return 0;

    return plotSpectrogram(m, Graph::GrayMap);
}

void ApplicationWindow::plotContour()
{
    if (Matrix *mat = qobject_cast<Matrix *>(d_workspace.activeSubWindow()))
        plotSpectrogram(mat, Graph::ContourMap);
}

MultiLayer *ApplicationWindow::plotContour(Matrix *m)
{
    if (!m)
        return 0;

    return plotSpectrogram(m, Graph::ContourMap);
}

void ApplicationWindow::plotColorMap()
{
    if (Matrix *mat = qobject_cast<Matrix *>(d_workspace.activeSubWindow()))
        plotSpectrogram(mat, Graph::ColorMap);
}

MultiLayer *ApplicationWindow::plotColorMap(Matrix *m)
{
    if (!m)
        return 0;

    return plotSpectrogram(m, Graph::ColorMap);
}

MultiLayer *ApplicationWindow::plotSpectrogram(Matrix *m, Graph::CurveType type)
{
    const bool nonInteractive = isNonInteractiveContext(this);
    if (!nonInteractive)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    MultiLayer *g = multilayerPlot(generateUniqueName(tr("Graph")));
    Graph *plot = g->addLayer();
    setPreferences(plot);

    plot->plotSpectrogram(m, type);
    if (isTabbedWorkspaceView())
        g->showMaximized();
    else
        g->showNormal();

    emit modified();
    if (!nonInteractive)
        QApplication::restoreOverrideCursor();
    return g;
}

/*!
  Turns 3D animation on or off
  */
void ApplicationWindow::toggle3DAnimation(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D"))
        ((Graph3D *)d_workspace.activeSubWindow())->animate(on);
}

/*!
  Turns perspective mode on or off
  */
void ApplicationWindow::togglePerspective(bool on)
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->setOrtho(!on);
    }
}

/*!
  Resets rotation of 3D plots to default values
  */
void ApplicationWindow::resetRotation()
{
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("Graph3D")) {
        ((Graph3D *)d_workspace.activeSubWindow())->setRotation(30, 0, 15);
    }
}

/*!
  Finds best layout for the 3D plot
  */
void ApplicationWindow::fitFrameToLayer()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Graph3D"))
        return;

    ((Graph3D *)d_workspace.activeSubWindow())->findBestLayout();
}

bool ApplicationWindow::validFor3DPlot(Table *table)
{
    if (!table)
        return false;
    if (table->numCols() < 2) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("You need at least two columns (X, Y, and Z) for a 3D plot!")
                                          + QStringLiteral("<p>")
                                          + tr("Add columns via Table -> Add Column or paste data "
                                               "from Excel."));
        }
        return false;
    }
    if (table->firstSelectedColumn() < 0
        || table->colPlotDesignation(table->firstSelectedColumn()) != SciDAVis::Z) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(
                    this, tr("Error"),
                    tr("Please select a Z column for this operation!") + QStringLiteral("<p>")
                            + tr("Right-click a column header -> \"Set Column(s) As\" -> \"Z\", "
                                 "select it, and try again."));
        }
        return false;
    }
    if (table->noXColumn()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("You need to define an X column first!") + QStringLiteral("<p>")
                                          + tr("Right-click the X column header -> \"Set Column(s) "
                                               "As\" -> \"X\"."));
        }
        return false;
    }
    if (table->noYColumn()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("You need to define a Y column first!") + QStringLiteral("<p>")
                                          + tr("Right-click the Y column header -> \"Set Column(s) "
                                               "As\" -> \"Y\"."));
        }
        return false;
    }
    return true;
}

// Where to fix a missing X / Y column (B7: error messages must name the place to act).
static QString xColumnHowToHint()
{
    return ApplicationWindow::tr("Right-click the header of the column to use as X and choose "
                                 "\"Set Column(s) As\" > \"X\", then plot again.");
}

static QString yColumnHowToHint()
{
    return ApplicationWindow::tr("Click the header of the Y column(s) to plot (Ctrl+click for "
                                 "several). A column becomes Y via right-click > \"Set Column(s) "
                                 "As\" > \"Y\".");
}

bool ApplicationWindow::validFor2DPlot(Table *table, int type)
{
    if (!table)
        return false;

    switch (type) {
    case Graph::Histogram:
    case Graph::Pie:
    case Graph::Box:
        if (table->selectedColumnCount() < 1 && table->YColumns().isEmpty()
            && table->numCols() < 1) {
            if (!isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Error"),
                                     tr("Please select a column to plot!") + QStringLiteral("<p>")
                                             + tr("Click a column header in the worksheet to "
                                                  "select it, then press the plot button again."));
            return false;
        }
        break;
    case Graph::VectXYXY:
    case Graph::VectXYAM:
        if (table->numCols() < 4) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("You need at least four columns for this operation!"));
            }
            return false;
        } else if (table->noXColumn()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("Please set a default X column for this table, first!")
                                              + QStringLiteral("<p>") + xColumnHowToHint());
            }
            return false;
        } else if (table->selectedColumnCount(SciDAVis::Y) < 2 && table->YColumns().size() < 2) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::warning(this, tr("Error"),
                                     tr("Please select a Y column to plot!") + QStringLiteral("<p>")
                                             + yColumnHowToHint());
            }
            return false;
        }
        break;
    default:
        if (table->numCols() < 2) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("You need at least two columns for this operation!")
                                              + QStringLiteral("<p>")
                                              + tr("Paste X and Y data from Excel (Ctrl+V) or add "
                                                   "a column (Home > More New > Add Column)."));
            }
            return false;
        } else if (table->noXColumn()) {
            if (!isNonInteractiveContext(this)) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("Please set a default X column for this table, first!")
                                              + QStringLiteral("<p>") + xColumnHowToHint());
            }
            return false;
        } else if (table->selectedColumnCount(SciDAVis::Y) < 1 && table->YColumns().isEmpty()) {
            if (!isNonInteractiveContext(this))
                QMessageBox::warning(this, tr("Error"),
                                     tr("Please select a Y column to plot!") + QStringLiteral("<p>")
                                             + yColumnHowToHint());
            return false;
        }
        break;
    }

    return true;
}

namespace {
static QStringList collectAllDrawableColumnsInOrder(Table *table)
{
    QStringList cols;
    if (!table)
        return cols;
    for (int i = 0; i < table->numCols(); ++i) {
        const SciDAVis::PlotDesignation pd = table->colPlotDesignation(i);
        if (pd == SciDAVis::Y || pd == SciDAVis::yErr || pd == SciDAVis::xErr)
            cols << table->colName(i);
    }
    return cols;
}
} // namespace

void ApplicationWindow::selectPlotType(int type)
{
    if (!d_workspace.activeSubWindow())
        return;

    Table *table = qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (table) {
        // Validation, empty-table guidance and column collection live in plotColumnsFromTable()
        // so that the ribbon (which may resolve the source table of a graph) behaves the same.
        plotColumnsFromTable(table, type);
        return;
    }

    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (ml) {
        Graph *g = ml->activeGraph();
        if (g && g->curves() > 0) {
            const QString before = ml->captureUndoSnapshot();
            g->setCurveType(g->curves() - 1, (Graph::CurveType)type);
            ml->pushUndoSnapshot(tr("Change Curve Plot Type"), before);
        }
    }
}

MultiLayer *ApplicationWindow::addTableSelectionToActiveOrRecentGraph(Table *srcTable, int style)
{
    Table *table = srcTable ? srcTable : qobject_cast<Table *>(d_workspace.activeSubWindow());
    if (!table)
        return nullptr;

    const int plotStyle = (style >= 0) ? style : defaultCurveStyle;
    const QString blocker = plotBlockerForTable(table, plotStyle);
    if (!blocker.isEmpty()) {
        if (!isNonInteractiveContext(this))
            QMessageBox::information(this, tr("Nothing to plot yet"), blocker);
        else if (statusBar())
            statusBar()->showMessage(
                    QString(blocker).replace(QStringLiteral("<p>"), QStringLiteral(" ")), 8000);
        return nullptr;
    }
    if (!validFor2DPlot(table, plotStyle))
        return nullptr;

    const int startRow = (table->selectedRowCount(false) >= 2) ? table->firstSelectedRow(false) : 0;
    const int endRow = (table->selectedRowCount(false) >= 2) ? table->lastSelectedRow(false) : -1;

    QStringList cols;
    if (plotStyle == Graph::Histogram || plotStyle == Graph::Pie || plotStyle == Graph::Box) {
        cols = table->selectedColumns();
        if (cols.isEmpty())
            cols = table->YColumns();
        if (cols.isEmpty() && table->numCols() > 0)
            cols << table->colName(table->numCols() - 1);
    } else {
        cols = (table->selectedColumnCount(SciDAVis::Y) >= 1)
                ? table->drawableColumnSelection()
                : collectAllDrawableColumnsInOrder(table);
    }
    if (cols.isEmpty())
        return nullptr;

    MultiLayer *targetMl = nullptr;
    const QList<MyWidget *> folderWins =
            current_folder ? current_folder->windowsList() : windowsList();
    const QList<QMdiSubWindow *> hist = d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
    for (int i = hist.size() - 1; i >= 0; --i) {
        MultiLayer *cand = qobject_cast<MultiLayer *>(hist.at(i));
        if (cand && !cand->isEmpty() && folderWins.contains(cand)) {
            targetMl = cand;
            break;
        }
    }
    if (!targetMl) {
        for (int i = folderWins.size() - 1; i >= 0; --i) {
            MyWidget *mw = folderWins.at(i);
            if (mw && mw->inherits("MultiLayer")) {
                MultiLayer *cand = static_cast<MultiLayer *>(mw);
                if (!cand->isEmpty()) {
                    targetMl = cand;
                    break;
                }
            }
        }
    }

    if (!targetMl || !targetMl->activeGraph() || targetMl->activeGraph()->isPiePlot()
        || plotStyle == Graph::Pie)
        return multilayerPlot(table, cols, plotStyle, startRow, endRow);

    Graph *ag = targetMl->activeGraph();
    const QString before = targetMl->captureUndoSnapshot();
    invalidateModelessGraphDialogs(targetMl);
    ag->insertCurvesList(table, cols, plotStyle, defaultCurveLineWidth, defaultSymbolSize, startRow,
                         endRow);
    if (ag->hasLegend())
        ag->reconstructLegend();
    else
        ag->newLegend();
    applyWorksheetMetadataToGraph(ag, table, cols, plotStyle);
    ag->refreshAutoAxisTitlesFromColumns();
    ag->setAutoScale();
    ag->updatePlot();
    targetMl->pushUndoSnapshot(tr("Add Curves from Worksheet"), before);
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == targetMl)
        d_graph_inspector_dock->setTargetMultiLayer(targetMl, true);
    activateSubWindow(targetMl);
    emit modified();
    if (statusBar()) {
        statusBar()->showMessage(
                tr("Added %1 column(s) from %2 to %3.")
                        .arg(cols.size())
                        .arg(table->name(), targetMl->name()),
                3000);
    }
    return targetMl;
}

void ApplicationWindow::refreshAllGraphsAutoAxisTitles()
{
    const QList<MyWidget *> windows = windowsList();
    for (MyWidget *w : windows) {
        if (w && w->inherits("MultiLayer")) {
            MultiLayer *ml = static_cast<MultiLayer *>(w);
            const QWidgetList graphs = ml->graphPtrs();
            for (QWidget *gw : graphs) {
                Graph *g = static_cast<Graph *>(gw);
                if (g)
                    g->refreshAutoAxisTitlesFromColumns();
            }
        }
    }
}

void ApplicationWindow::showFillBetweenCurvesDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        CurveMathTools::showFillBetweenDialog(this, g);
}

void ApplicationWindow::showStackOffsetsDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g || !g->plotWidget())
        return;
    MultiLayer *ml = g->multiLayerParent();
    const QString beforeUndoSnap = ml ? ml->captureUndoSnapshot() : QString();

    QList<PlotCurve *> targetCurves;
    for (int i = 0; i < g->curves(); ++i) {
        const int ct = g->curveType(i);
        if (ct == Graph::ErrorBars || ct == Graph::Pie || ct == Graph::Box
            || ct == Graph::ColorMap || ct == Graph::GrayMap || ct == Graph::ContourMap)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(g->curve(i));
        if (pc && pc->isVisible() && pc->dataSize() > 0)
            targetCurves.append(pc);
    }
    if (targetCurves.isEmpty())
        return;

    struct CurveStackSnapshot
    {
        PlotCurve *pc = nullptr;
        double dx = 0.0;
        double dy = 0.0;
        bool norm01 = false;
    };

    QVector<CurveStackSnapshot> initialCurves;
    initialCurves.reserve(targetCurves.size());
    bool anyCurrentlyStacked = false;
    for (PlotCurve *pc : targetCurves) {
        CurveStackSnapshot snap;
        snap.pc = pc;
        snap.dx = pc->stackOffsetX();
        snap.dy = pc->stackOffsetY();
        snap.norm01 = pc->stackNormalize01();
        if (qAbs(snap.dx) > 1e-12 || qAbs(snap.dy) > 1e-12 || snap.norm01)
            anyCurrentlyStacked = true;
        initialCurves.push_back(snap);
    }

    Plot *plt = g->plotWidget();
    struct AxisScaleSnapshot
    {
        bool autoScale = true;
        double lo = 0.0;
        double hi = 1.0;
    };
    AxisScaleSnapshot initialAxes[QwtPlot::axisCnt];
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        initialAxes[a].autoScale = plt->axisAutoScale(a);
        const QwtScaleDiv *sd = plt->axisScaleDiv(a);
        if (sd) {
            initialAxes[a].lo = sd->lowerBound();
            initialAxes[a].hi = sd->upperBound();
        }
    }

    const QVariant origPropMode = g->property("stackOffsetMode");
    const QVariant origPropYPct = g->property("yStackOffsetPercent");
    const QVariant origPropXPct = g->property("xStackOffsetPercent");
    const QVariant origPropNorm = g->property("normalizeStackZeroOne");
    const QVariant origPropAuto = g->property("stackAutoscale");
    const QVariant origPropRev = g->property("stackReverseOrder");

    // Reconstruct active Graph's current stack state on open
    int initMode = 0;
    double initYPct = 0.0;
    double initXPct = 0.0;
    bool initNorm = false;
    bool initAutoScale = origPropAuto.isValid() ? origPropAuto.toBool() : true;
    bool initRevOrder = false;

    if (anyCurrentlyStacked && origPropYPct.isValid()) {
        initMode = origPropMode.isValid() ? qBound(0, origPropMode.toInt(), 1) : 0;
        initYPct = qBound(-100.0, origPropYPct.toDouble(), 200.0);
        initXPct = origPropXPct.isValid() ? qBound(-100.0, origPropXPct.toDouble(), 200.0) : 0.0;
        initNorm = origPropNorm.isValid() ? origPropNorm.toBool() : targetCurves.first()->stackNormalize01();
        initRevOrder = origPropRev.isValid() ? origPropRev.toBool() : false;
    } else if (anyCurrentlyStacked) {
        initNorm = targetCurves.first()->stackNormalize01();
        const int m = targetCurves.size();
        if (m >= 2) {
            initRevOrder = (qAbs(targetCurves.first()->stackOffsetY()) > 1e-9
                            && qAbs(targetCurves.last()->stackOffsetY()) <= 1e-9);
            PlotCurve *baseCurve = initRevOrder ? targetCurves.last() : targetCurves.first();
            PlotCurve *secondCurve = initRevOrder ? targetCurves[m - 2] : targetCurves[1];

            double sumSpanY = 0.0;
            int validSpans = 0;
            double maxSpanX = 0.0;
            for (PlotCurve *pc : targetCurves) {
                DataCurve *dc = dynamic_cast<DataCurve *>(pc);
                bool first = true;
                double loX = 0.0, hiX = 0.0, loY = 0.0, hiY = 0.0;
                for (int j = 0; j < pc->dataSize(); ++j) {
                    if (dc && dc->isPointMasked(j))
                        continue;
                    const double xv = pc->x(j) - pc->stackOffsetX();
                    const double yv = pc->y(j) - pc->stackOffsetY();
                    if (!std::isfinite(xv) || !std::isfinite(yv))
                        continue;
                    if (first) {
                        loX = hiX = xv;
                        loY = hiY = yv;
                        first = false;
                    } else {
                        loX = qMin(loX, xv);
                        hiX = qMax(hiX, xv);
                        loY = qMin(loY, yv);
                        hiY = qMax(hiY, yv);
                    }
                }
                if (!first) {
                    sumSpanY += qMax(0.0, hiY - loY);
                    ++validSpans;
                    maxSpanX = qMax(maxSpanX, hiX - loX);
                }
            }
            const double avgSpanY =
                    (validSpans > 0 && sumSpanY > 1e-12) ? (sumSpanY / double(validSpans)) : 1.0;
            const double refSpanX = (maxSpanX > 1e-12) ? maxSpanX : 1.0;
            const double dxStep = secondCurve->stackOffsetX() - baseCurve->stackOffsetX();
            const double dyStep = secondCurve->stackOffsetY() - baseCurve->stackOffsetY();
            initXPct = qBound(-100.0, (dxStep / refSpanX) * 100.0, 200.0);
            initYPct = qBound(-100.0, (dyStep / avgSpanY) * 100.0, 200.0);
            initMode = 0;
        }
    }

    auto syncCurveErrorBars = [](PlotCurve *pc) {
        if (DataCurve *dc = dynamic_cast<DataCurve *>(pc)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->loadData(); // master's transformed points, errors per table row
            }
        }
    };

    auto restoreInitialSnapshot = [&]() {
        for (const CurveStackSnapshot &snap : initialCurves) {
            snap.pc->setStackNormalize01(snap.norm01);
            snap.pc->setStackOffset(snap.dx, snap.dy);
            syncCurveErrorBars(snap.pc);
        }
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (initialAxes[a].autoScale)
                plt->setAxisAutoScale(a);
            else
                plt->setAxisScale(a, initialAxes[a].lo, initialAxes[a].hi);
        }
        g->setProperty("stackOffsetMode", origPropMode);
        g->setProperty("yStackOffsetPercent", origPropYPct);
        g->setProperty("xStackOffsetPercent", origPropXPct);
        g->setProperty("normalizeStackZeroOne", origPropNorm);
        g->setProperty("stackAutoscale", origPropAuto);
        g->setProperty("stackReverseOrder", origPropRev);
        g->updatePlot();
    };

    auto applyStackToGraph = [&](int mode, double yPct, double xPct, bool norm01,
                                 bool autoRescale, bool revOrder) {
        QList<PlotCurve *> ordered = targetCurves;
        if (revOrder)
            std::reverse(ordered.begin(), ordered.end());

        for (PlotCurve *pc : ordered) {
            pc->setStackOffset(0.0, 0.0);
            pc->setStackNormalize01(norm01);
        }

        const int m = ordered.size();
        if (m >= 2) {
            QVector<double> minY(m, 0.0), maxY(m, 0.0);
            double sumSpanY = 0.0;
            int validSpansY = 0;
            double maxSpanX = 0.0;

            for (int k = 0; k < m; ++k) {
                PlotCurve *pc = ordered[k];
                DataCurve *dc = dynamic_cast<DataCurve *>(pc);
                const int n = pc->dataSize();
                bool first = true;
                double loX = 0.0, hiX = 0.0, loY = 0.0, hiY = 0.0;
                for (int j = 0; j < n; ++j) {
                    if (dc && dc->isPointMasked(j))
                        continue;
                    const double xv = pc->x(j);
                    const double yv = pc->y(j);
                    if (!std::isfinite(xv) || !std::isfinite(yv))
                        continue;
                    if (first) {
                        loX = hiX = xv;
                        loY = hiY = yv;
                        first = false;
                    } else {
                        loX = qMin(loX, xv);
                        hiX = qMax(hiX, xv);
                        loY = qMin(loY, yv);
                        hiY = qMax(hiY, yv);
                    }
                }
                minY[k] = loY;
                maxY[k] = hiY;
                if (!first) {
                    sumSpanY += qMax(0.0, hiY - loY);
                    ++validSpansY;
                    maxSpanX = qMax(maxSpanX, hiX - loX);
                }
            }

            const double avgSpanY =
                    (validSpansY > 0 && sumSpanY > 1e-12) ? (sumSpanY / double(validSpansY)) : 1.0;
            const double refSpanX = (maxSpanX > 1e-12) ? maxSpanX : 1.0;
            const double stepX = (qAbs(xPct) > 1e-12) ? (refSpanX * (xPct / 100.0)) : 0.0;

            double prevTop = maxY[0];
            for (int k = 1; k < m; ++k) {
                const double dx = double(k) * stepX;
                double dy = 0.0;
                if (mode == 1) {
                    dy = (prevTop - minY[k]) + (yPct / 100.0) * avgSpanY;
                    prevTop = maxY[k] + dy;
                } else {
                    dy = double(k) * avgSpanY * (yPct / 100.0);
                }
                ordered[k]->setStackOffset(dx, dy);
            }
        }

        for (PlotCurve *pc : ordered)
            syncCurveErrorBars(pc);

        if (autoRescale) {
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                plt->setAxisAutoScale(a);
        } else {
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (!initialAxes[a].autoScale)
                    plt->setAxisScale(a, initialAxes[a].lo, initialAxes[a].hi);
            }
        }
        g->updatePlot();
    };

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Interactive Waterfall / Spectra Stacker"));
    dlg.setMinimumWidth(480);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *modeCombo = new QComboBox(&dlg);
    modeCombo->addItem(tr("Uniform Step (% of Span)"), 0);
    modeCombo->addItem(tr("Auto Gap (% above Previous Curve Peak)"), 1);
    modeCombo->setCurrentIndex(initMode);
    form->addRow(tr("Offset Mode:"), modeCombo);

    QHBoxLayout *yRow = new QHBoxLayout();
    QSlider *ySlider = new QSlider(Qt::Horizontal, &dlg);
    ySlider->setRange(-1000, 2000);
    ySlider->setSingleStep(10);
    ySlider->setPageStep(100);
    ySlider->setValue(qRound(initYPct * 10.0));
    QDoubleSpinBox *ySpin = new QDoubleSpinBox(&dlg);
    ySpin->setRange(-100.0, 200.0);
    ySpin->setSingleStep(1.0);
    ySpin->setDecimals(1);
    ySpin->setSuffix(" %");
    ySpin->setValue(initYPct);
    yRow->addWidget(ySlider, 1);
    yRow->addWidget(ySpin);
    form->addRow(tr("Y Offset (%):"), yRow);

    QHBoxLayout *xRow = new QHBoxLayout();
    QSlider *xSlider = new QSlider(Qt::Horizontal, &dlg);
    xSlider->setRange(-1000, 2000);
    xSlider->setSingleStep(10);
    xSlider->setPageStep(100);
    xSlider->setValue(qRound(initXPct * 10.0));
    QDoubleSpinBox *xSpin = new QDoubleSpinBox(&dlg);
    xSpin->setRange(-100.0, 200.0);
    xSpin->setSingleStep(1.0);
    xSpin->setDecimals(1);
    xSpin->setSuffix(" %");
    xSpin->setValue(initXPct);
    xRow->addWidget(xSlider, 1);
    xRow->addWidget(xSpin);
    form->addRow(tr("X Offset (%):"), xRow);

    vbox->addLayout(form);

    QCheckBox *normCheck =
            new QCheckBox(tr("Normalize [0, 1] (Scale Each Curve to Unit Span)"), &dlg);
    normCheck->setChecked(initNorm);
    vbox->addWidget(normCheck);

    QCheckBox *autoScaleCheck =
            new QCheckBox(tr("Auto-Rescale Axes (Individual Autoscale)"), &dlg);
    autoScaleCheck->setChecked(initAutoScale);
    vbox->addWidget(autoScaleCheck);

    QCheckBox *revOrderCheck =
            new QCheckBox(tr("Reverse Curve Order (Top-to-Bottom)"), &dlg);
    revOrderCheck->setChecked(initRevOrder);
    vbox->addWidget(revOrderCheck);

    QCheckBox *livePreviewCheck = new QCheckBox(tr("Live Preview on Canvas"), &dlg);
    livePreviewCheck->setChecked(true);
    vbox->addWidget(livePreviewCheck);

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *resetBtn = new QPushButton(tr("Reset (0%, 0%)"), &dlg);
    btnRow->addWidget(resetBtn);
    btnRow->addStretch();
    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnRow->addWidget(btns);
    vbox->addLayout(btnRow);

    auto refreshLivePreview = [&]() {
        if (!livePreviewCheck->isChecked())
            return;
        applyStackToGraph(modeCombo->currentData().toInt(), ySpin->value(), xSpin->value(),
                          normCheck->isChecked(), autoScaleCheck->isChecked(),
                          revOrderCheck->isChecked());
    };

    QObject::connect(ySlider, &QSlider::valueChanged, &dlg, [&](int val) {
        ySpin->blockSignals(true);
        ySpin->setValue(double(val) / 10.0);
        ySpin->blockSignals(false);
        refreshLivePreview();
    });
    QObject::connect(ySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double val) {
                         ySlider->blockSignals(true);
                         ySlider->setValue(qRound(val * 10.0));
                         ySlider->blockSignals(false);
                         refreshLivePreview();
                     });
    QObject::connect(xSlider, &QSlider::valueChanged, &dlg, [&](int val) {
        xSpin->blockSignals(true);
        xSpin->setValue(double(val) / 10.0);
        xSpin->blockSignals(false);
        refreshLivePreview();
    });
    QObject::connect(xSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                     [&](double val) {
                         xSlider->blockSignals(true);
                         xSlider->setValue(qRound(val * 10.0));
                         xSlider->blockSignals(false);
                         refreshLivePreview();
                     });
    QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) { refreshLivePreview(); });
    QObject::connect(normCheck, &QCheckBox::toggled, &dlg, [&](bool) { refreshLivePreview(); });
    QObject::connect(autoScaleCheck, &QCheckBox::toggled, &dlg,
                     [&](bool) { refreshLivePreview(); });
    QObject::connect(revOrderCheck, &QCheckBox::toggled, &dlg,
                     [&](bool) { refreshLivePreview(); });
    QObject::connect(livePreviewCheck, &QCheckBox::toggled, &dlg, [&](bool checked) {
        if (checked)
            refreshLivePreview();
        else
            restoreInitialSnapshot();
    });

    QObject::connect(resetBtn, &QPushButton::clicked, &dlg, [&]() {
        modeCombo->blockSignals(true);
        modeCombo->setCurrentIndex(0);
        modeCombo->blockSignals(false);
        ySpin->blockSignals(true);
        ySpin->setValue(0.0);
        ySpin->blockSignals(false);
        ySlider->blockSignals(true);
        ySlider->setValue(0);
        ySlider->blockSignals(false);
        xSpin->blockSignals(true);
        xSpin->setValue(0.0);
        xSpin->blockSignals(false);
        xSlider->blockSignals(true);
        xSlider->setValue(0);
        xSlider->blockSignals(false);
        normCheck->blockSignals(true);
        normCheck->setChecked(false);
        normCheck->blockSignals(false);
        revOrderCheck->blockSignals(true);
        revOrderCheck->setChecked(false);
        revOrderCheck->blockSignals(false);
        refreshLivePreview();
    });

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const int finalMode = modeCombo->currentData().toInt();
        const double finalY = ySpin->value();
        const double finalX = xSpin->value();
        const bool finalNorm = normCheck->isChecked();
        const bool finalAuto = autoScaleCheck->isChecked();
        const bool finalRev = revOrderCheck->isChecked();

        applyStackToGraph(finalMode, finalY, finalX, finalNorm, finalAuto, finalRev);
        g->setProperty("stackOffsetMode", finalMode);
        g->setProperty("yStackOffsetPercent", finalY);
        g->setProperty("xStackOffsetPercent", finalX);
        g->setProperty("normalizeStackZeroOne", finalNorm);
        g->setProperty("stackAutoscale", finalAuto);
        g->setProperty("stackReverseOrder", finalRev);
        emit g->modifiedGraph();
        if (ml)
            ml->pushUndoSnapshot(tr("Waterfall / Y-Offset Stack"), beforeUndoSnap);
        emit modified();
    } else {
        restoreInitialSnapshot();
    }
}

void ApplicationWindow::showWaterfallStackOffsetDialog()
{
    showStackOffsetsDialog();
}

void ApplicationWindow::resetActiveGraphStackOffsets()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;
    MultiLayer *ml = g->multiLayerParent();
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    g->resetStackOffsets();
    g->setProperty("stackOffsetMode", 0);
    g->setProperty("yStackOffsetPercent", 0.0);
    g->setProperty("xStackOffsetPercent", 0.0);
    g->setProperty("normalizeStackZeroOne", false);
    g->setProperty("stackReverseOrder", false);
    if (ml)
        ml->pushUndoSnapshot(tr("Reset Stack Offsets"), before);
    emit modified();
}

void ApplicationWindow::invalidateModelessGraphDialogs(MultiLayer *ml)
{
    invalidateModelessGraphDialogs(ml, nullptr);
}

void ApplicationWindow::invalidateModelessGraphDialogs(MultiLayer *ml, QDialog *excludeDlg)
{
    if (!ml)
        return;

    clearLastCopiedLayerIfReferencing(ml);

    if (d_active_peak_analyzer_dlg && d_active_peak_analyzer_dlg.data() != excludeDlg) {
        const QVariant peakTargetVar = d_active_peak_analyzer_dlg->property("targetMultiLayerPtr");
        const QObject *peakTargetObj =
                storedTargetPointer(peakTargetVar);
        if (!peakTargetVar.isValid() || peakTargetObj == static_cast<QObject *>(ml)) {
            d_active_peak_analyzer_dlg->blockSignals(true);
            d_active_peak_analyzer_dlg->disconnect();
            d_active_peak_analyzer_dlg->hide();
            d_active_peak_analyzer_dlg->deleteLater();
            d_active_peak_analyzer_dlg = nullptr;
        }
    }

    QWidget *activeModal = QApplication::activeModalWidget();
    const QList<QDialog *> childDialogs = findChildren<QDialog *>();
    for (QDialog *dlg : childDialogs) {
        if (!dlg || dlg == excludeDlg || dlg->isModal())
            continue;
        if (activeModal && (activeModal == dlg || dlg->isAncestorOf(activeModal)))
            continue;
        bool hasActiveModalChild = false;
        for (QDialog *sub : dlg->findChildren<QDialog *>()) {
            if (sub && sub != dlg && sub->isVisible() && sub->isModal()) {
                hasActiveModalChild = true;
                break;
            }
        }
        if (hasActiveModalChild)
            continue;
        const QVariant targetVar = dlg->property("targetMultiLayerPtr");
        bool shouldInvalidate = false;
        if (targetVar.isValid()) {
            shouldInvalidate = (storedTargetPointer(targetVar) == static_cast<QObject *>(ml));
        } else if (dlg->testAttribute(Qt::WA_DeleteOnClose)) {
            shouldInvalidate = (qobject_cast<PlotDialog *>(dlg) != nullptr
                                || qobject_cast<CurvesDialog *>(dlg) != nullptr
                                || qobject_cast<CurveRangeDialog *>(dlg) != nullptr
                                || qobject_cast<LayerDialog *>(dlg) != nullptr
                                || qobject_cast<FitDialog *>(dlg) != nullptr
                                || qobject_cast<FunctionDialog *>(dlg) != nullptr
                                || qobject_cast<ExpDecayDialog *>(dlg) != nullptr
                                || qobject_cast<InterpolationDialog *>(dlg) != nullptr
                                || qobject_cast<PolynomFitDialog *>(dlg) != nullptr
                                || qobject_cast<IntDialog *>(dlg) != nullptr
                                || qobject_cast<SmoothCurveDialog *>(dlg) != nullptr
                                || qobject_cast<FilterDialog *>(dlg) != nullptr
                                || qobject_cast<FFTDialog *>(dlg) != nullptr);
        }
        if (shouldInvalidate) {
            dlg->blockSignals(true);
            dlg->disconnect();
            dlg->hide();
            dlg->deleteLater();
        }
    }
}

bool ApplicationWindow::handleColumnMimeDrop(const QMimeData *mimeData, MultiLayer *targetMl,
                                             Graph *targetGraph, int targetAxisId,
                                             bool replaceExisting)
{
    if (!mimeData || !mimeData->hasFormat(QLatin1String(MultiLayer::kColumnMimeType)))
        return false;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(
            mimeData->data(QLatin1String(MultiLayer::kColumnMimeType)), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject obj = doc.object();
    const QString tblName = obj.value(QStringLiteral("table")).toString().trimmed();

    Table *tbl = nullptr;
    if (!tblName.isEmpty()) {
        for (MyWidget *mw : windowsList()) {
            if (Table *cand = qobject_cast<Table *>(mw)) {
                if (cand->name() == tblName) {
                    tbl = cand;
                    break;
                }
            }
        }
        if (!tbl) {
            for (MyWidget *hw : hiddenWindows) {
                if (Table *cand = qobject_cast<Table *>(hw)) {
                    if (cand->name() == tblName) {
                        tbl = cand;
                        break;
                    }
                }
            }
        }
    }
    if (!tbl) {
        const QJsonArray fullArr = obj.value(QStringLiteral("full_columns")).toArray();
        if (!fullArr.isEmpty())
            tbl = table(fullArr.first().toString());
        if (!tbl) {
            const QJsonArray colArr = obj.value(QStringLiteral("columns")).toArray();
            if (!colArr.isEmpty())
                tbl = table(colArr.first().toString());
        }
    }
    if (!tbl || tbl->numCols() <= 0 || tbl->numRows() <= 0)
        return false;

    replaceExisting = replaceExisting || obj.value(QStringLiteral("replace")).toBool(false);
    const bool useRightY = (targetAxisId == QwtPlot::yRight)
            || obj.value(QStringLiteral("right_y")).toBool(false);

    auto resolveColIdx = [tbl](const QString &rawRef) -> int {
        const QString ref = rawRef.trimmed();
        if (ref.isEmpty())
            return -1;
        for (int c = 0; c < tbl->numCols(); ++c) {
            if (tbl->colLabel(c) == ref)
                return c;
        }
        int idx = tbl->colIndex(ref);
        if (idx >= 0 && idx < tbl->numCols())
            return idx;
        idx = tbl->colIndex(QString(tbl->name()) + QStringLiteral("_") + ref);
        if (idx >= 0 && idx < tbl->numCols())
            return idx;
        return -1;
    };

    int xColIdx = -1;
    if (obj.contains(QStringLiteral("x_index"))) {
        const int xi = obj.value(QStringLiteral("x_index")).toInt(-1);
        if (xi >= 0 && xi < tbl->numCols())
            xColIdx = xi;
    }
    if (xColIdx < 0)
        xColIdx = resolveColIdx(obj.value(QStringLiteral("x_full_column")).toString());
    if (xColIdx < 0)
        xColIdx = resolveColIdx(obj.value(QStringLiteral("x_column")).toString());
    if (xColIdx < 0)
        xColIdx = resolveColIdx(obj.value(QStringLiteral("x_col")).toString());

    QVector<int> rawColIndices;
    auto appendUniqueIdx = [](QVector<int> &vec, int idx) {
        if (idx >= 0 && !vec.contains(idx))
            vec.append(idx);
    };

    const QJsonArray colsArr = obj.value(QStringLiteral("columns")).toArray();
    for (const QJsonValue &v : colsArr) {
        if (v.isDouble())
            appendUniqueIdx(rawColIndices, v.toInt(-1));
        else
            appendUniqueIdx(rawColIndices, resolveColIdx(v.toString()));
    }
    if (rawColIndices.isEmpty()) {
        const QJsonArray fullArr = obj.value(QStringLiteral("full_columns")).toArray();
        for (const QJsonValue &v : fullArr)
            appendUniqueIdx(rawColIndices, resolveColIdx(v.toString()));
    }
    if (rawColIndices.isEmpty()) {
        const QJsonArray namesArr = obj.value(QStringLiteral("column_names")).toArray();
        for (const QJsonValue &v : namesArr)
            appendUniqueIdx(rawColIndices, resolveColIdx(v.toString()));
    }
    if (rawColIndices.isEmpty()) {
        const QJsonArray idxArr = obj.value(QStringLiteral("column_indices")).toArray();
        for (const QJsonValue &v : idxArr) {
            const int ci = v.toInt(-1);
            if (ci >= 0 && ci < tbl->numCols())
                appendUniqueIdx(rawColIndices, ci);
        }
    }

    QVector<int> errColIndices;
    const QJsonArray errArr = obj.value(QStringLiteral("err_columns")).toArray();
    for (const QJsonValue &v : errArr)
        appendUniqueIdx(errColIndices, resolveColIdx(v.toString()));

    bool hasNonXPlotCol = false;
    for (int ci : rawColIndices) {
        const int desig = tbl->colPlotDesignation(ci);
        if (desig != SciDAVis::X && desig != SciDAVis::xErr && desig != SciDAVis::yErr) {
            hasNonXPlotCol = true;
            break;
        }
    }

    QVector<int> yColIndices;
    for (int ci : rawColIndices) {
        const int desig = tbl->colPlotDesignation(ci);
        if ((desig == SciDAVis::xErr || desig == SciDAVis::yErr) && rawColIndices.size() > 1)
            appendUniqueIdx(errColIndices, ci);
        else if (desig == SciDAVis::X && hasNonXPlotCol && xColIdx < 0)
            continue;
        else
            appendUniqueIdx(yColIndices, ci);
    }

    if (yColIndices.isEmpty() && errColIndices.isEmpty())
        return false;

    // Pure error-bar drop onto an existing graph layer
    if (yColIndices.isEmpty() && !errColIndices.isEmpty() && targetMl && !replaceExisting) {
        Graph *ag = targetGraph ? targetGraph : targetMl->activeGraph();
        if (!ag)
            ag = targetMl->layer(1);
        if (ag && ag->curves() > 0) {
            QString targetMaster;
            for (int k = ag->curves() - 1; k >= 0; --k) {
                if (auto *dc = dynamic_cast<DataCurve *>(ag->curve(k))) {
                    if (!dynamic_cast<QwtErrorPlotCurve *>(dc) && dc->table() == tbl) {
                        targetMaster = dc->title().text();
                        break;
                    }
                }
            }
            if (targetMaster.isEmpty()) {
                for (int k = ag->curves() - 1; k >= 0; --k) {
                    if (auto *dc = dynamic_cast<DataCurve *>(ag->curve(k))) {
                        if (!dynamic_cast<QwtErrorPlotCurve *>(dc)) {
                            targetMaster = dc->title().text();
                            break;
                        }
                    }
                }
            }
            if (!targetMaster.isEmpty()) {
                const QString before = targetMl->captureUndoSnapshot();
                bool anyAdded = false;
                for (int eIdx : errColIndices) {
                    const QString errFull =
                            QString(tbl->name()) + QStringLiteral("_") + tbl->colLabel(eIdx);
                    const int dir = (tbl->colPlotDesignation(eIdx) == SciDAVis::xErr)
                            ? QwtErrorPlotCurve::Horizontal
                            : QwtErrorPlotCurve::Vertical;
                    if (ag->addErrorBars(targetMaster, tbl, errFull, dir))
                        anyAdded = true;
                }
                if (anyAdded) {
                    ag->updatePlot();
                    targetMl->pushUndoSnapshot(tr("Drop Error Bars"), before);
                    if (d_graph_inspector_dock)
                        d_graph_inspector_dock->setTargetMultiLayer(targetMl, true);
                    emit modified();
                    return true;
                }
            }
        }
        // Fallback: plot error columns as Y if no master curve exists
        yColIndices = errColIndices;
        errColIndices.clear();
    }

    if (yColIndices.isEmpty())
        return false;

    const QJsonArray xColsArr = obj.value(QStringLiteral("x_columns")).toArray();
    const bool hasExplicitStyle = obj.contains(QStringLiteral("style"));
    int style = hasExplicitStyle ? obj.value(QStringLiteral("style")).toInt(defaultCurveStyle)
                                 : defaultCurveStyle;

    bool createdNewGraph = false;
    if (!targetMl) {
        targetMl = new MultiLayer("", &d_workspace, nullptr);
        targetMl->setAttribute(Qt::WA_DeleteOnClose);
        initMultilayerPlot(targetMl, generateUniqueName(tr("Graph")));
        targetGraph = targetMl->addLayer();
        if (!targetGraph)
            return false;
        setPreferences(targetGraph);
        polishGraph(targetGraph, style);
        createdNewGraph = true;
    } else {
        if (!targetGraph)
            targetGraph = targetMl->activeGraph() ? targetMl->activeGraph() : targetMl->layer(1);
        if (!targetGraph) {
            targetGraph = targetMl->addLayer();
            if (!targetGraph)
                return false;
            setPreferences(targetGraph);
        }
    }

    if (targetGraph->linkedParentLayer())
        targetGraph = targetGraph->linkedParentLayer();

    const QString beforeSnap = createdNewGraph ? QString() : targetMl->captureUndoSnapshot();

    if (replaceExisting && !createdNewGraph) {
        invalidateModelessGraphDialogs(targetMl);
        for (int k = targetGraph->curves() - 1; k >= 0; --k)
            targetGraph->removeCurve(k);
    }

    int inheritedLineWidth = defaultCurveLineWidth;
    int inheritedSymSize = defaultSymbolSize;
    if (!createdNewGraph && !replaceExisting && targetGraph) {
        for (int ci = 0; ci < targetGraph->curves(); ++ci) {
            const int ct = targetGraph->curveType(ci);
            if (ct == Graph::Line || ct == Graph::Scatter || ct == Graph::LineSymbols
                || ct == Graph::Spline || ct == Graph::VerticalSteps
                || ct == Graph::HorizontalSteps) {
                if (!hasExplicitStyle)
                    style = ct;
                if (const QwtPlotCurve *ec = targetGraph->curve(ci)) {
                    if (ec->pen().width() > 0)
                        inheritedLineWidth = ec->pen().width();
                    if (ec->symbol().size().width() > 0)
                        inheritedSymSize = ec->symbol().size().width();
                }
                break;
            }
        }
    }

    QStringList insertedFullCols;
    for (int pos = 0; pos < yColIndices.size(); ++pos) {
        const int yIdx = yColIndices[pos];
        if (yIdx < 0 || yIdx >= tbl->numCols())
            continue;
        int curXIdx = xColIdx;
        if (curXIdx < 0 && pos < xColsArr.size()) {
            const QJsonValue &xv = xColsArr.at(pos);
            curXIdx = xv.isDouble() ? xv.toInt(-1) : resolveColIdx(xv.toString());
        }
        if (curXIdx < 0 || curXIdx >= tbl->numCols())
            curXIdx = tbl->colX(yIdx);
        if (curXIdx < 0 || curXIdx >= tbl->numCols())
            curXIdx = tbl->firstXCol();
        if (curXIdx < 0 || curXIdx >= tbl->numCols()) {
            curXIdx = (yColIndices.size() == 1 && yIdx == 0 && tbl->numCols() > 1) ? 1 : 0;
        }
        const QString curXFullCol =
                QString(tbl->name()) + QStringLiteral("_") + tbl->colLabel(curXIdx);
        const QString yFullCol = QString(tbl->name()) + QStringLiteral("_") + tbl->colLabel(yIdx);
        if (!targetGraph->insertCurve(tbl, curXFullCol, yFullCol, style))
            continue;

        const int newCurveIdx = targetGraph->curves() - 1;
        int dataCurveOrder = 0;
        for (int ci = 0; ci < newCurveIdx; ++ci) {
            if (targetGraph->curveType(ci) != Graph::ErrorBars)
                ++dataCurveOrder;
        }
        CurveLayout cl = targetGraph->initCurveLayout(style, dataCurveOrder);
        cl.sSize = inheritedSymSize;
        cl.lWidth = inheritedLineWidth;
        targetGraph->updateCurveLayout(newCurveIdx, &cl);

        if (useRightY) {
            if (QwtPlotCurve *cv = targetGraph->curve(newCurveIdx))
                cv->setAxis(cv->xAxis(), QwtPlot::yRight);
        }
        insertedFullCols.append(yFullCol);
    }

    if (insertedFullCols.isEmpty()) {
        if (createdNewGraph && targetMl) {
            targetMl->askOnCloseEvent(false);
            closeWindow(targetMl);
        } else if (replaceExisting && targetMl && !beforeSnap.isEmpty()) {
            targetMl->restoreSnapshotInPlace(this, beforeSnap);
        }
        return false;
    }

    if (useRightY && targetGraph->plotWidget()) {
        targetGraph->enableAxis(QwtPlot::yRight, true);
        QStringList tickLbls = targetGraph->enabledTickLabels();
        if (tickLbls.size() > QwtPlot::yRight) {
            tickLbls[QwtPlot::yRight] = QStringLiteral("1");
            targetGraph->setEnabledTickLabels(tickLbls);
        }
        targetGraph->setAxisTicksLength(QwtPlot::yRight, ScaleDraw::In, ScaleDraw::In,
                                        targetGraph->minorTickLength(),
                                        targetGraph->majorTickLength());
        QFont rf = targetGraph->axisTitleFont(QwtPlot::yRight);
        rf.setFamily(QStringLiteral("Arial"));
        targetGraph->setAxisTitleFont(QwtPlot::yRight, rf);
        QFont rtf = targetGraph->axisFont(QwtPlot::yRight);
        rtf.setFamily(QStringLiteral("Arial"));
        targetGraph->setAxisFont(QwtPlot::yRight, rtf);
    }

    if (!errColIndices.isEmpty()) {
        const QString masterCol = insertedFullCols.last();
        for (int eIdx : errColIndices) {
            if (eIdx < 0 || eIdx >= tbl->numCols())
                continue;
            const QString errFull =
                    QString(tbl->name()) + QStringLiteral("_") + tbl->colLabel(eIdx);
            const int dir = (tbl->colPlotDesignation(eIdx) == SciDAVis::xErr)
                    ? QwtErrorPlotCurve::Horizontal
                    : QwtErrorPlotCurve::Vertical;
            targetGraph->addErrorBars(masterCol, tbl, errFull, dir);
        }
    }

    if (createdNewGraph) {
        targetGraph->newLegend();
        applyWorksheetMetadataToGraph(targetGraph, tbl, insertedFullCols, style);
        if (useRightY) {
            int rightCurveCount = 0;
            int rightCurveIdx = -1;
            for (int ci = 0; ci < targetGraph->curves(); ++ci) {
                if (targetGraph->curveType(ci) == Graph::ErrorBars)
                    continue;
                if (const QwtPlotCurve *cv = targetGraph->curve(ci)) {
                    if (cv->yAxis() == QwtPlot::yRight) {
                        ++rightCurveCount;
                        rightCurveIdx = ci;
                    }
                }
            }
            if (rightCurveCount == 1 && rightCurveIdx >= 0)
                targetGraph->syncAxisColorToCurve(QwtPlot::yRight, rightCurveIdx);
        }
        targetGraph->setAutoScale();
        targetMl->arrangeLayers(false, false);
        if (targetMl->undoStack())
            targetMl->undoStack()->clear();
        customMenu(targetMl);
    } else {
        if (replaceExisting || useRightY)
            applyWorksheetMetadataToGraph(targetGraph, tbl, insertedFullCols, style);
        else
            targetGraph->refreshAutoAxisTitlesFromColumns();

        if (useRightY) {
            int rightCurveCount = 0;
            int rightCurveIdx = -1;
            for (int ci = 0; ci < targetGraph->curves(); ++ci) {
                if (targetGraph->curveType(ci) == Graph::ErrorBars)
                    continue;
                if (const QwtPlotCurve *cv = targetGraph->curve(ci)) {
                    if (cv->yAxis() == QwtPlot::yRight) {
                        ++rightCurveCount;
                        rightCurveIdx = ci;
                    }
                }
            }
            if (rightCurveCount == 1 && rightCurveIdx >= 0)
                targetGraph->syncAxisColorToCurve(QwtPlot::yRight, rightCurveIdx);
        }

        if (targetGraph->legend())
            targetGraph->reconstructLegend();
        else
            targetGraph->newLegend();
        disambiguateMultiTableLegendLabels(targetGraph);

        if (replaceExisting) {
            targetGraph->setAutoScale();
        } else {
            if (useRightY) {
                if (!targetGraph->isAxisUserLocked(QwtPlot::yRight))
                    targetGraph->setAutoScaleAxis(QwtPlot::yRight);
            } else {
                if (!targetGraph->isAxisUserLocked(QwtPlot::yLeft))
                    targetGraph->setAutoScaleAxis(QwtPlot::yLeft);
            }
            if (!targetGraph->isAxisUserLocked(QwtPlot::xBottom))
                targetGraph->setAutoScaleAxis(QwtPlot::xBottom);
        }
        targetGraph->updatePlot();
        targetGraph->replot();
        targetMl->pushUndoSnapshot(
                replaceExisting ? tr("Replace Plotted Columns") : tr("Drop Columns onto Graph"),
                beforeSnap);
    }

    targetMl->setActiveGraph(targetGraph);
    d_workspace.setActiveSubWindow(targetMl);
    if (d_modern_ribbon
        && d_modern_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
        d_modern_ribbon->applySplitWorkspaceGeometry();
    }
    if (d_graph_inspector_dock)
        d_graph_inspector_dock->setTargetMultiLayer(targetMl, true);
    updateGraphInspectorResponsiveness();
    updateUndoRedoActionsForActiveWindow();
    emit modified();
    return true;
}

void ApplicationWindow::plotStackedBars()
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
        showUxHint(tr("Please open or select a worksheet before creating a stacked bar chart."));
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
        showUxHint(tr("The worksheet '%1' has no Y columns to plot.").arg(t->name()));
        return;
    }

    int startRow = (t->selectedRowCount(false) >= 2) ? t->firstSelectedRow(false) : 0;
    int endRow = (t->selectedRowCount(false) >= 2) ? t->lastSelectedRow(false) : (t->numRows() - 1);
    if (startRow < 0 || endRow < startRow) {
        startRow = 0;
        endRow = qMax(0, t->numRows() - 1);
    }

    MultiLayer *ml = multilayerPlot(t, yCols, Graph::VerticalBars, startRow, endRow);
    if (!ml)
        return;
    if (Graph *ag = ml->activeGraph()) {
        for (int ci = 0; ci < ag->curves(); ++ci) {
            if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(ag->curve(ci))) {
                bc->setStacked(true);
                ag->setBarsGap(ci, 30, 0);
            }
        }
        ag->setAutoScale();
        ag->replot();
        emit ml->modifiedPlot();
    }
    if (ml->undoStack())
        ml->undoStack()->clear();
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
        d_graph_inspector_dock->setTargetMultiLayer(ml, true);
    updateUndoRedoActionsForActiveWindow();
    emit modified();
}

void ApplicationWindow::plotPolarData()
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
    if (!t || t->numCols() < 2 || t->numRows() < 1) {
        showUxHint(tr("Polar plot requires a worksheet with at least one angle column and one radius column."));
        return;
    }

    int thetaCol = t->firstXCol();
    if (thetaCol < 0 || thetaCol >= t->numCols())
        thetaCol = 0;

    QStringList selY = t->selectedYColumns();
    if (selY.isEmpty())
        selY = t->YColumns();
    if (selY.isEmpty()) {
        for (int c = 0; c < t->numCols(); ++c) {
            if (c != thetaCol)
                selY << t->colName(c);
        }
    }
    if (selY.isEmpty()) {
        showUxHint(tr("No numeric radius columns were found in '%1'.").arg(t->name()));
        return;
    }

    bool inRadians = false;
    QList<int> radiusCols;
    if (!isNonInteractiveContext(this) && t->numCols() >= 2) {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Polar Plot"));
        dlg.setMinimumWidth(380);
        QVBoxLayout *vbox = new QVBoxLayout(&dlg);
        QFormLayout *form = new QFormLayout();
        QComboBox *cbTheta = new QComboBox(&dlg);
        for (int c = 0; c < t->numCols(); ++c)
            cbTheta->addItem(t->colLabel(c), c);
        cbTheta->setCurrentIndex(qBound(0, thetaCol, t->numCols() - 1));
        QComboBox *cbUnits = new QComboBox(&dlg);
        cbUnits->addItem(tr("Degrees (0–360°)"), false);
        cbUnits->addItem(tr("Radians (0–2π)"), true);
        form->addRow(tr("Angle Column (θ):"), cbTheta);
        form->addRow(tr("Angle Units:"), cbUnits);
        vbox->addLayout(form);

        vbox->addWidget(new QLabel(tr("Radius Column(s) (r):"), &dlg));
        QListWidget *rList = new QListWidget(&dlg);
        for (int c = 0; c < t->numCols(); ++c) {
            QListWidgetItem *it = new QListWidgetItem(t->colLabel(c), rList);
            it->setData(Qt::UserRole, c);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            const bool isSelY = selY.contains(t->colName(c));
            it->setCheckState((isSelY && c != thetaCol) ? Qt::Checked : Qt::Unchecked);
        }
        if (rList->count() > 0) {
            bool anyChecked = false;
            for (int i = 0; i < rList->count(); ++i)
                anyChecked |= (rList->item(i)->checkState() == Qt::Checked);
            if (!anyChecked && rList->count() > 1)
                rList->item(1)->setCheckState(Qt::Checked);
        }
        vbox->addWidget(rList, 1);

        QDialogButtonBox *btns =
                new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vbox->addWidget(btns);
        if (dlg.exec() != QDialog::Accepted)
            return;

        thetaCol = cbTheta->currentData().toInt();
        inRadians = cbUnits->currentData().toBool();
        for (int i = 0; i < rList->count(); ++i) {
            if (rList->item(i)->checkState() == Qt::Checked)
                radiusCols << rList->item(i)->data(Qt::UserRole).toInt();
        }
    } else {
        for (const QString &yn : selY) {
            const int ci = t->colIndex(yn);
            if (ci >= 0)
                radiusCols << ci;
        }
    }

    if (radiusCols.isEmpty()) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Polar Plot"),
                                 tr("Please select at least one radius column."));
        }
        return;
    }

    const int startRow = (t->selectedRowCount(false) >= 2) ? t->firstSelectedRow(false) : 0;
    const int endRow = (t->selectedRowCount(false) >= 2) ? t->lastSelectedRow(false)
                                                         : (t->numRows() - 1);

    MultiLayer *ml = multilayerPlot(generateUniqueName(tr("Polar")));
    if (!ml)
        return;
    Graph *ag = ml->addLayer();
    if (!ag)
        return;
    setPreferences(ag);
    int plottedSeries = 0;
    for (int rc : radiusCols) {
        QVector<double> thVec, rVec;
        for (int row = qMax(0, startRow); row <= qMin(endRow, t->numRows() - 1); ++row) {
            if (t->text(row, thetaCol).trimmed().isEmpty()
                || t->text(row, rc).trimmed().isEmpty()) {
                continue;
            }
            const double th = t->cell(row, thetaCol);
            const double rv = t->cell(row, rc);
            if (std::isfinite(th) && std::isfinite(rv)) {
                thVec.append(th);
                rVec.append(rv);
            }
        }
        if (thVec.isEmpty())
            continue;
        Column *rcObj = t->column(rc);
        const QString lbl = (rcObj && !rcObj->longName().trimmed().isEmpty())
                ? rcObj->formattedAxisTitle()
                : t->colLabel(rc);
        ag->setupPolarPlot(thVec, rVec, lbl, inRadians);
        ++plottedSeries;
    }

    if (plottedSeries == 0) {
        ml->askOnCloseEvent(false);
        closeWindow(ml);
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Polar Plot"),
                                 tr("No valid finite (theta, r) numeric pairs were found."));
        }
        return;
    }
    ToriUx::dismissPlotThisDataBar(t);
    if (!ag->hasLegend())
        ag->newLegend();
    ag->placeLegendAnchor(Graph::LegendTopRight);
    ag->replot();
    ml->arrangeLayers(false, false);
    if (ml->undoStack())
        ml->undoStack()->clear();
    customMenu(ml);
    activateSubWindow(ml);
    if (d_graph_inspector_dock)
        d_graph_inspector_dock->setTargetMultiLayer(ml, true);
    emit modified();
}
