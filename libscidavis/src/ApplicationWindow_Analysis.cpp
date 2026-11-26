/***************************************************************************
        File                 : ApplicationWindow_Analysis.cpp
        Project              : SciDAVis
        Description          : SciDAVis's main window (Peak Analyzer, curve math, fitting, gadgets, signal processing, stats & recalc)
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

QVector<QPointF> normalizeSortedAnchors(const QVector<QPointF> &anchors)
{
    QVector<QPointF> valid;
    valid.reserve(anchors.size());
    for (const QPointF &pt : anchors) {
        if (std::isfinite(pt.x()) && std::isfinite(pt.y()))
            valid.push_back(pt);
    }
    std::sort(valid.begin(), valid.end(),
              [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });

    QVector<QPointF> out;
    out.reserve(valid.size());
    for (int i = 0; i < valid.size();) {
        double xSum = valid[i].x();
        double ySum = valid[i].y();
        int cnt = 1;
        int j = i + 1;
        while (j < valid.size() && std::abs(valid[j].x() - valid[i].x()) <= 1e-12) {
            xSum += valid[j].x();
            ySum += valid[j].y();
            ++cnt;
            ++j;
        }
        out.push_back(QPointF(xSum / double(cnt), ySum / double(cnt)));
        i = j;
    }
    return out;
}

void evaluateAnchorBaselineSamples(const QVector<QPointF> &rawAnchors, bool cubicSpline,
                                   double xMin, double xMax, int numSamples,
                                   QVector<double> &outX, QVector<double> &outY)
{
    outX.clear();
    outY.clear();
    const QVector<QPointF> anchors = normalizeSortedAnchors(rawAnchors);
    const int m = anchors.size();
    if (m == 0 || numSamples < 2 || !std::isfinite(xMin) || !std::isfinite(xMax))
        return;

    if (xMax < xMin)
        std::swap(xMin, xMax);
    if (std::abs(xMax - xMin) <= 1e-15) {
        xMin -= 0.5;
        xMax += 0.5;
    }

    outX.resize(numSamples);
    outY.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
        outX[i] = xMin + (xMax - xMin) * (double(i) / double(numSamples - 1));

    if (m == 1) {
        outY.fill(anchors[0].y());
        return;
    }

    std::vector<double> ax(static_cast<size_t>(m)), ay(static_cast<size_t>(m));
    for (int k = 0; k < m; ++k) {
        ax[size_t(k)] = anchors[k].x();
        ay[size_t(k)] = anchors[k].y();
    }

    if (m >= 3 && cubicSpline) {
        gsl_error_handler_t *oldHandler = gsl_set_error_handler_off();
        gsl_interp_accel *acc = gsl_interp_accel_alloc();
        gsl_spline *sp = gsl_spline_alloc(gsl_interp_cspline, size_t(m));
        if (acc && sp && gsl_spline_init(sp, ax.data(), ay.data(), size_t(m)) == GSL_SUCCESS) {
            const double dLeft = gsl_spline_eval_deriv(sp, ax.front(), acc);
            const double dRight = gsl_spline_eval_deriv(sp, ax.back(), acc);
            for (int i = 0; i < numSamples; ++i) {
                const double xi = outX[i];
                if (xi <= ax.front())
                    outY[i] = ay.front() + dLeft * (xi - ax.front());
                else if (xi >= ax.back())
                    outY[i] = ay.back() + dRight * (xi - ax.back());
                else
                    outY[i] = gsl_spline_eval(sp, xi, acc);
            }
            gsl_spline_free(sp);
            gsl_interp_accel_free(acc);
            gsl_set_error_handler(oldHandler);
            return;
        }
        if (sp)
            gsl_spline_free(sp);
        if (acc)
            gsl_interp_accel_free(acc);
        gsl_set_error_handler(oldHandler);
    }

    for (int i = 0; i < numSamples; ++i) {
        const double xi = outX[i];
        if (xi <= ax.front()) {
            const double dx0 = ax[1] - ax[0];
            const double slope = (std::abs(dx0) > 1e-15) ? ((ay[1] - ay[0]) / dx0) : 0.0;
            outY[i] = ay[0] + slope * (xi - ax[0]);
        } else if (xi >= ax.back()) {
            const double dxEnd = ax[size_t(m - 1)] - ax[size_t(m - 2)];
            const double slope =
                    (std::abs(dxEnd) > 1e-15) ? ((ay[size_t(m - 1)] - ay[size_t(m - 2)]) / dxEnd) : 0.0;
            outY[i] = ay.back() + slope * (xi - ax.back());
        } else {
            auto it = std::upper_bound(ax.begin(), ax.end(), xi);
            size_t k1 = size_t(std::distance(ax.begin(), it));
            if (k1 == 0)
                k1 = 1;
            if (k1 >= size_t(m))
                k1 = size_t(m - 1);
            const size_t k0 = k1 - 1;
            const double dxSeg = ax[k1] - ax[k0];
            const double frac = (std::abs(dxSeg) > 1e-15) ? ((xi - ax[k0]) / dxSeg) : 0.0;
            outY[i] = ay[k0] + frac * (ay[k1] - ay[k0]);
        }
    }
}

} // anonymous namespace

void ApplicationWindow::correlate()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *t = (Table *)d_workspace.activeSubWindow();
    QStringList s = t->selectedColumns();
    if ((int)s.count() != 2) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Please select two columns for this operation!"));
        }
        return;
    }

    Correlation *cor = new Correlation(this, t, s[0], s[1]);
    cor->run();
    delete cor;
}

void ApplicationWindow::autoCorrelate()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *t = (Table *)d_workspace.activeSubWindow();
    QStringList s = t->selectedColumns();
    if ((int)s.count() != 1) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Please select exactly one columns for this operation!"));
        }
        return;
    }

    Correlation *cor = new Correlation(this, t, s[0], s[0]);
    cor->run();
    delete cor;
}

void ApplicationWindow::convolute()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *t = (Table *)d_workspace.activeSubWindow();
    QStringList s = t->selectedColumns();
    if ((int)s.count() != 2) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Please select two columns for this operation:\n the first "
                                    "represents the signal and the second the response function!"));
        }
        return;
    }

    Convolution *cv = new Convolution(this, t, s[0], s[1]);
    cv->run();
    delete cv;
}

void ApplicationWindow::deconvolute()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;

    Table *t = (Table *)d_workspace.activeSubWindow();
    QStringList s = t->selectedColumns();
    if ((int)s.count() != 2) {
        if (!isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Please select two columns for this operation:\n the first "
                                    "represents the signal and the second the response function!"));
        }
        return;
    }

    Deconvolution *dcv = new Deconvolution(this, t, s[0], s[1]);
    dcv->run();
    delete dcv;
}

void ApplicationWindow::showColStatistics()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;
    Table *t = (Table *)d_workspace.activeSubWindow();

    if (int(t->selectedColumns().count()) > 0) {
        QList<int> targets;
        for (int i = 0; i < t->numCols(); i++)
            if (t->isColumnSelected(i, false))
                targets << i;
        newTableStatistics(t, TableStatistics::StatColumn, targets)->showNormal();
    } else if (!isNonInteractiveContext(this))
        QMessageBox::warning(this, tr("Column selection error"),
                             tr("Please select a column first!"));
}

void ApplicationWindow::showRowStatistics()
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("Table"))
        return;
    Table *t = (Table *)d_workspace.activeSubWindow();

    if (t->numSelectedRows() > 0) {
        QList<int> targets;
        for (int i = 0; i < t->numRows(); i++)
            if (t->isRowSelected(i, false))
                targets << i;
        newTableStatistics(t, TableStatistics::StatRow, targets)->showNormal();
    } else if (!isNonInteractiveContext(this))
        QMessageBox::warning(this, tr("Row selection error"), tr("Please select a row first!"));
}

void ApplicationWindow::showExpGrowthDialog()
{
    showExpDecayDialog(-1);
}

void ApplicationWindow::showExpDecayDialog()
{
    showExpDecayDialog(1);
}

void ApplicationWindow::showExpDecayDialog(int type)
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g || !g->validCurvesDataSize())
        return;

    ExpDecayDialog *edd = new ExpDecayDialog(type, this);
    edd->setAttribute(Qt::WA_DeleteOnClose);
    edd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    connect(g, SIGNAL(destroyed()), edd, SLOT(close()));

    edd->setGraph(g);
    edd->show();
}

void ApplicationWindow::showTwoExpDecayDialog()
{
    showExpDecayDialog(2);
}

void ApplicationWindow::showExpDecay3Dialog()
{
    showExpDecayDialog(3);
}

void ApplicationWindow::showFitDialog()
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w)
        return;

    MultiLayer *plot = 0;
    if (w->inherits("MultiLayer"))
        plot = (MultiLayer *)w;
    else if (w->inherits("Table"))
        plot = multilayerPlot((Table *)w, ((Table *)w)->drawableColumnSelection(),
                              Graph::LineSymbols);

    if (!plot)
        return;

    Graph *g = (Graph *)plot->activeGraph();
    if (!g || !g->validCurvesDataSize())
        return;

    FitDialog *fd = new FitDialog(this);
    fd->setAttribute(Qt::WA_DeleteOnClose);
    fd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(plot)));
    connect(fd, SIGNAL(clearFunctionsList()), this, SLOT(clearFitFunctionsList()));
    connect(fd, SIGNAL(saveFunctionsList(const QStringList &)), this,
            SLOT(saveFitFunctionsList(const QStringList &)));
    connect(plot, SIGNAL(destroyed()), fd, SLOT(close()));

    fd->addUserFunctions(fitFunctions);
    fd->setGraph(g);
    fd->setSrcTables(tableList());
    fd->exec();
}

void ApplicationWindow::showFilterDialog(int filter)
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (g && g->validCurvesDataSize()) {
        FilterDialog *fd = new FilterDialog(filter, this);
        fd->setAttribute(Qt::WA_DeleteOnClose);
        fd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
        fd->setGraph(g);
        fd->exec();
    }
}

void ApplicationWindow::lowPassFilterDialog()
{
    showFilterDialog(FFTFilter::LowPass);
}

void ApplicationWindow::highPassFilterDialog()
{
    showFilterDialog(FFTFilter::HighPass);
}

void ApplicationWindow::bandPassFilterDialog()
{
    showFilterDialog(FFTFilter::BandPass);
}

void ApplicationWindow::bandBlockFilterDialog()
{
    showFilterDialog(FFTFilter::BandBlock);
}

void ApplicationWindow::showFFTDialog()
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w)
        return;

    FFTDialog *sd = 0;
    if (w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        Graph *g = ml->activeGraph();
        if (g && g->validCurvesDataSize()) {
            sd = new FFTDialog(FFTDialog::onGraph, this);
            sd->setAttribute(Qt::WA_DeleteOnClose);
            sd->setProperty("targetMultiLayerPtr",
                            QVariant::fromValue(static_cast<QObject *>(ml)));
            sd->setGraph(g);
        }
    } else if (w->inherits("Table")) {
        sd = new FFTDialog(FFTDialog::onTable, this);
        sd->setAttribute(Qt::WA_DeleteOnClose);
        sd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(nullptr)));
        sd->setTable((Table *)w);
    }

    if (sd)
        sd->exec();
}

void ApplicationWindow::showSmoothDialog(int m)
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g || !g->validCurvesDataSize())
        return;

    SmoothCurveDialog *sd = new SmoothCurveDialog(m, this);
    sd->setAttribute(Qt::WA_DeleteOnClose);
    sd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    sd->setGraph(g);
    sd->exec();
}

void ApplicationWindow::showSmoothSavGolDialog()
{
    showSmoothDialog(SmoothFilter::SavitzkyGolay);
}

void ApplicationWindow::showSmoothFFTDialog()
{
    showSmoothDialog(SmoothFilter::FFT);
}

void ApplicationWindow::showSmoothAverageDialog()
{
    showSmoothDialog(SmoothFilter::Average);
}

void ApplicationWindow::showInterpolationDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g || !g->validCurvesDataSize())
        return;

    InterpolationDialog *id = new InterpolationDialog(this);
    id->setAttribute(Qt::WA_DeleteOnClose);
    id->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    connect(g, SIGNAL(destroyed()), id, SLOT(close()));
    id->setGraph(g);
    id->show();
}

void ApplicationWindow::showFitPolynomDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g || !g->validCurvesDataSize())
        return;

    PolynomFitDialog *pfd = new PolynomFitDialog(this);
    pfd->setAttribute(Qt::WA_DeleteOnClose);
    pfd->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    connect(g, SIGNAL(destroyed()), pfd, SLOT(close()));
    pfd->setGraph(g);
    pfd->show();
}

void ApplicationWindow::fitLinear()
{
    analysis("fitLinear");
}

void ApplicationWindow::showIntegrationDialog()
{
    if (isNonInteractiveContext(this))
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml->activeGraph();
    if (!g || !g->validCurvesDataSize())
        return;

    IntDialog *id = new IntDialog(this);
    id->setAttribute(Qt::WA_DeleteOnClose);
    id->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    connect(g, SIGNAL(destroyed()), id, SLOT(close()));
    id->setGraph(g);
    id->show();
}

void ApplicationWindow::fitSigmoidal()
{
    analysis("fitSigmoidal");
}

void ApplicationWindow::fitGauss()
{
    analysis("fitGauss");
}

void ApplicationWindow::fitLorentz()

{
    analysis("fitLorentz");
}

void ApplicationWindow::differentiate()
{
    analysis("differentiate");
}

void ApplicationWindow::showDataSetDialog(const QString &whichFit)
{
    if (isNonInteractiveContext(this))
        return;

    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (!g)
        return;

    DataSetDialog *ad = new DataSetDialog(tr("Curve") + ": ", this);
    ad->setAttribute(Qt::WA_DeleteOnClose);
    ad->setGraph(g);
    ad->setOperationType(whichFit);
    ad->exec();
}

void ApplicationWindow::analyzeCurve(Graph *g, const QString &whichFit, const QString &curveTitle)
{
    if (whichFit == "fitLinear" || whichFit == "fitApparentLinear" || whichFit == "fitSigmoidal"
        || whichFit == "fitGauss" || whichFit == "fitLorentz") {
        Fit *fitter = 0;
        if (whichFit == "fitLinear")
            fitter = new LinearFit(this, g);
        else if (whichFit == "fitApparentLinear") {
            LinearFit *lf = new LinearFit(this, g);
            lf->setApparentFit(true);
            fitter = lf;
        } else if (whichFit == "fitSigmoidal")
            fitter = new SigmoidalFit(this, g);
        else if (whichFit == "fitGauss")
            fitter = new GaussFit(this, g);
        else if (whichFit == "fitLorentz")
            fitter = new LorentzFit(this, g);

        if (fitter->setDataFromCurve(curveTitle)) {
            if (whichFit != "fitLinear" && whichFit != "fitApparentLinear")
                fitter->guessInitialValues();

            fitter->scaleErrors(fit_scale_errors);
            fitter->setOutputPrecision(fit_output_precision);

            if (whichFit == "fitLinear" && d_2_linear_fit_points)
                fitter->generateFunction(generateUniformFitPoints, 2);
            else
                fitter->generateFunction(generateUniformFitPoints, fitPoints);

            const QStringList curvesBefore = g->curvesList();
            fitter->fit();
            if (pasteFitResultsToPlot)
                fitter->showLegend();

            if (whichFit == "fitLinear"
                && RecalcManager::instance(this)->recalcMode() != RecalcManager::Disabled) {
                const QStringList curvesAfter = g->curvesList();
                QString outCurveTitle;
                for (int i = curvesAfter.size() - 1; i >= 0; --i) {
                    if (!curvesBefore.contains(curvesAfter[i])) {
                        outCurveTitle = curvesAfter[i];
                        break;
                    }
                }
                DataCurve *srcDc = dynamic_cast<DataCurve *>(g->curve(curveTitle));
                if (srcDc && srcDc->table() && !outCurveTitle.isEmpty()) {
                    RecalcManager::instance(this)->registerLink(
                            this, g, srcDc->table(), curveTitle, outCurveTitle,
                            RecalcManager::OpLinearFit);
                }
            }
            delete fitter;
        }
    } else if (whichFit == "differentiate") {
        Differentiation *diff = new Differentiation(this, g, curveTitle);
        diff->run();
        delete diff;
    }
}

void ApplicationWindow::analysis(const QString &whichFit)
{
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml)
        return;

    Graph *g = ml->activeGraph();
    if (!g || !g->validCurvesDataSize())
        return;

    QString curve_title = g->selectedCurveTitle();
    if (!curve_title.isNull()) {
        analyzeCurve(g, whichFit, curve_title);
        return;
    }

    QStringList lst = g->analysableCurvesList();
    if (lst.count() == 1) {
        const QwtPlotCurve *c = g->curve(lst[0]);
        if (c)
            analyzeCurve(g, whichFit, lst[0]);
    } else if (isNonInteractiveContext(this) && !lst.isEmpty()) {
        if (g->curve(lst[0]))
            analyzeCurve(g, whichFit, lst[0]);
    } else
        showDataSetDialog(whichFit);
}

void ApplicationWindow::deleteFitTables()
{
    QList<MyWidget *> *mLst = new QList<MyWidget *>();
    QList<MyWidget *> windows = windowsList();
    for (int i = 0; i < int(windows.count()); i++) {
        if (windows.at(i)->inherits("MultiLayer"))
            mLst->append(windows.at(i));
    }

    foreach (MyWidget *ml, *mLst) {
        if (ml->inherits("MultiLayer")) {
            QWidgetList lst = ((MultiLayer *)ml)->graphPtrs();
            foreach (QWidget *widget, lst) {
                QList<QwtPlotCurve *> curves = ((Graph *)widget)->fitCurvesList();
                foreach (QwtPlotCurve *c, curves) {
                    if (((PlotCurve *)c)->type() != Graph::Function) {
                        Table *t = ((DataCurve *)c)->table();
                        if (!t)
                            continue;

                        t->askOnCloseEvent(false);
                        t->close();
                    }
                }
            }
        }
    }
    delete mLst;
}

void ApplicationWindow::translateCurveHor()
{
    if (isNonInteractiveContext(this))
        return;

    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)w;
    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        btnPointer->setChecked(true);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g)
        return;

    if (g->isPiePlot()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This functionality is not available for pie plots!"));

        btnPointer->setChecked(true);
        return;
    } else if (g->validCurvesDataSize()) {
        btnPointer->setChecked(true);
        g->setActiveTool(new TranslateCurveTool(g, this, TranslateCurveTool::Horizontal,
                                                d_status_info, SLOT(setText(const QString &))));
    }
}

void ApplicationWindow::translateCurveVert()
{
    if (isNonInteractiveContext(this))
        return;

    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;

    MultiLayer *plot = (MultiLayer *)w;
    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        btnPointer->setChecked(true);
        return;
    }

    Graph *g = (Graph *)plot->activeGraph();
    if (!g)
        return;

    if (g->isPiePlot()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This functionality is not available for pie plots!"));

        btnPointer->setChecked(true);
        return;
    } else if (g->validCurvesDataSize()) {
        btnPointer->setChecked(true);
        g->setActiveTool(new TranslateCurveTool(g, this, TranslateCurveTool::Vertical,
                                                d_status_info, SLOT(setText(const QString &))));
    }
}

void ApplicationWindow::fitMultiPeakGauss()
{
    fitMultiPeak((int)MultiPeakFit::Gauss);
}

void ApplicationWindow::fitMultiPeakLorentz()
{
    fitMultiPeak((int)MultiPeakFit::Lorentz);
}

void ApplicationWindow::fitMultiPeakGaussAmp()
{
    fitMultiPeak((int)MultiPeakFit::GaussAmp);
}

void ApplicationWindow::fitMultiPeakPseudoVoigt()
{
    fitMultiPeak((int)MultiPeakFit::PseudoVoigt);
}

void ApplicationWindow::fitMultiPeakPearsonVII()
{
    fitMultiPeak((int)MultiPeakFit::PearsonVII);
}

void ApplicationWindow::fitAutoPeakGauss()
{
    runAutoPeakFit((int)MultiPeakFit::Gauss, 0, true, true, true, false);
}

void ApplicationWindow::fitAutoPeakLorentz()
{
    runAutoPeakFit((int)MultiPeakFit::Lorentz, 0, true, true, true, false);
}

void ApplicationWindow::fitAutoPeakPseudoVoigt()
{
    runAutoPeakFit((int)MultiPeakFit::PseudoVoigt, 0, true, true, true, false);
}

void ApplicationWindow::runAutoPeakFit(int profile, int numPeaks, bool autoCount,
                                       bool plotPeakCurves, bool createSummary,
                                       bool createResiduals, const QString &curveTitle,
                                       double prominence, double voigtMu, double pearsonM,
                                       int baselineMode, double asymAlpha, double tailTau,
                                       bool plotBaseline, bool integratePeaks)
{
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;

    MultiLayer *plot = static_cast<MultiLayer *>(w);
    if (plot->isEmpty())
        return;

    Graph *g = static_cast<Graph *>(plot->activeGraph());
    if (!g || !g->validCurvesDataSize() || g->isPiePlot())
        return;

    QString targetCurve = curveTitle;
    if (targetCurve.isEmpty())
        targetCurve = g->selectedCurveTitle();
    if (targetCurve.isEmpty()) {
        QStringList lst = g->analysableCurvesList();
        for (const QString &cName : lst) {
            const QwtPlotCurve *c = g->curve(cName);
            if (c && static_cast<const PlotCurve *>(c)->type() != Graph::Function) {
                targetCurve = cName;
                break;
            }
        }
        if (targetCurve.isEmpty() && !lst.isEmpty())
            targetCurve = lst.first();
    }
    if (targetCurve.isEmpty())
        return;

    int initialPeaks = (numPeaks > 0) ? numPeaks : 1;
    MultiPeakFit *fitter = new MultiPeakFit(
            this, g, static_cast<MultiPeakFit::PeakProfile>(profile), initialPeaks);
    fitter->setLorentzFraction(voigtMu);
    fitter->setPearsonShape(pearsonM);
    fitter->setAsymmetryFactor(asymAlpha);
    fitter->setTailFactor(tailTau);
    fitter->setBaselineMode(static_cast<MultiPeakFit::BaselineMode>(baselineMode));
    fitter->enablePeakCurves(plotPeakCurves);
    fitter->setPeakCurvesColor(peakCurvesColor);
    fitter->scaleErrors(fit_scale_errors);
    fitter->setOutputPrecision(fit_output_precision);
    fitter->generateFunction(generateUniformFitPoints, fitPoints);

    if (fitter->setDataFromCurve(targetCurve)) {
        if (autoCount || numPeaks <= 0) {
            int maxP = (numPeaks > 0) ? numPeaks : 10;
            fitter->autoDetectAndUpdateNumPeaks(maxP, prominence);
        } else {
            fitter->setNumPeaks(numPeaks);
            fitter->autoDetectPeaks(numPeaks, prominence);
        }
        fitter->fit();
        if (plotBaseline)
            fitter->insertBaselineCurve();
        if (pasteFitResultsToPlot)
            fitter->showLegend();
        if (createSummary)
            fitter->createPeakSummaryTable();
        if (integratePeaks)
            fitter->integratePeaksToTable();
        if (createResiduals)
            fitter->createResidualsTable();
        g->replot();
    }
    delete fitter;
}

void ApplicationWindow::showPeakAnalyzerDialog(int defaultProfile)
{
    if (isNonInteractiveContext(this))
        return;

    QWidget *w = d_workspace.activeSubWindow() ? d_workspace.activeSubWindow()
                                               : d_workspace.currentSubWindow();

    MultiLayer *plot = nullptr;
    if (w && w->inherits("MultiLayer")) {
        plot = static_cast<MultiLayer *>(w);
    } else if (w && w->inherits("Table")) {
        Table *t = static_cast<Table *>(w);
        QStringList cols = t->drawableColumnSelection();
        if (!cols.isEmpty())
            plot = multilayerPlot(t, cols, Graph::LineSymbols);
    }
    if (!plot) {
        const QList<QMdiSubWindow *> swList = d_workspace.subWindowList();
        for (int i = swList.size() - 1; i >= 0; --i) {
            if (QMdiSubWindow *sw = swList.at(i)) {
                if (sw->inherits("MultiLayer")) {
                    MultiLayer *cand = static_cast<MultiLayer *>(sw);
                    if (cand && !cand->isEmpty() && cand->activeGraph()
                        && cand->activeGraph()->validCurvesDataSize()) {
                        plot = cand;
                        break;
                    }
                }
            }
        }
    }

    if (!plot)
        return;

    if (plot->isEmpty()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("<h4>There are no plot layers available in this window.</h4>"
                                "<p><h4>Please add a layer and try again!</h4>"));
        btnPointer->setChecked(true);
        return;
    }

    Graph *g = static_cast<Graph *>(plot->activeGraph());
    if (!g || !g->validCurvesDataSize())
        return;

    if (g->isPiePlot()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This functionality is not available for pie plots!"));
        return;
    }

    QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    if (d_active_peak_analyzer_dlg) {
        d_active_peak_analyzer_dlg->close();
        d_active_peak_analyzer_dlg = nullptr;
    }

    QDialog *dlg = new QDialog(this, Qt::Window);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setModal(false);
    if (MultiLayer *ml = g->multiLayerParent())
        dlg->setProperty("targetMultiLayerPtr", QVariant::fromValue(static_cast<QObject *>(ml)));
    dlg->setWindowTitle(tr("Peak Analyzer"));
    dlg->setMinimumSize(620, 540);
    d_active_peak_analyzer_dlg = dlg;

    enum {
        ComboUserAnchorsLinear = 100,
        ComboUserAnchorsSpline = 101
    };

    struct PeakAnalyzerSession
    {
        QPointer<ApplicationWindow> app;
        QPointer<Graph> g;
        QPointer<QDialog> dlg;
        QPointer<QWidget> canvas;

        QComboBox *curveCombo = nullptr;
        QComboBox *goalCombo = nullptr;
        QComboBox *baselineCombo = nullptr;
        QLineEdit *anchorEdit = nullptr;
        QTabWidget *tabs = nullptr;
        QDialogButtonBox *buttonBox = nullptr;
        QCheckBox *anchorSplineCheck = nullptr;
        QPushButton *autoSeedAnchorsBtn = nullptr;
        QPushButton *pickAnchorsBtn = nullptr;
        QPushButton *clearAnchorsBtn = nullptr;
        QCheckBox *overwriteBaselineCheck = nullptr;
        QLabel *anchorHintLabel = nullptr;

        QSpinBox *peaksSpin = nullptr;
        QCheckBox *autoCountCheck = nullptr;
        QDoubleSpinBox *prominenceSpin = nullptr;
        QLabel *detectedLabel = nullptr;
        QRadioButton *autoModeRadio = nullptr;
        QRadioButton *manualModeRadio = nullptr;
        QLabel *manualPickHintLabel = nullptr;
        QPushButton *clearPickedPeaksBtn = nullptr;
        QTableWidget *peakTable = nullptr;

        QComboBox *profileCombo = nullptr;
        QDoubleSpinBox *voigtMuSpin = nullptr;
        QCheckBox *freeLorentzMuCheck = nullptr;
        QDoubleSpinBox *pearsonMSpin = nullptr;
        QDoubleSpinBox *asymAlphaSpin = nullptr;
        QDoubleSpinBox *tailTauSpin = nullptr;
        bool prevProfileWasDS = false;
        QCheckBox *plotPeaksCheck = nullptr;
        QCheckBox *plotBaselineCheck = nullptr;
        QCheckBox *summaryTableCheck = nullptr;
        QCheckBox *integrateTableCheck = nullptr;
        QCheckBox *residualsTableCheck = nullptr;

        struct DoubletLinkSpec
        {
            int priPeak = 1;
            int secPeak = 2;
            double ratio = 0.5;
            double splitDx = 3.67;
            bool shareWidth = true;
        };
        QVector<DoubletLinkSpec> doubletSpecs;
        QLabel *doubletSummaryLabel = nullptr;

        QwtPlotCurve *liveBaselineCurve = nullptr;
        QwtPlotCurve *anchorMarkersCurve = nullptr;
        QwtPlotCurve *activeAnchorMarkerCurve = nullptr;
        QwtPlotCurve *pickedPeakMarkersCurve = nullptr;
        PeakAnchorEventFilter *eventFilterObj = nullptr;

        QVector<QPointF> anchors;
        int selectedAnchorIdx = -1;
        int draggingAnchorIdx = -1;
        bool dragNewlyAdded = false;
        QPoint pressPos;
        QPoint lastMousePos;
        bool hasLastMousePos = false;
        bool syncingAnchorEdit = false;
        bool suppressAutoSeedOnce = false;
        bool cleanedUp = false;
        int subToolAction = 0;

        std::function<void()> refreshPeakTableFn;
        std::function<void(double, double)> addManualPeakFn;
        std::function<void(double)> removeNearestManualPeakFn;

        bool isAnchorMode() const
        {
            if (!baselineCombo)
                return false;
            const int d = baselineCombo->currentData().toInt();
            return d == ComboUserAnchorsLinear || d == ComboUserAnchorsSpline
                    || d == (int)MultiPeakFit::BaselineAnchors;
        }

        int effectiveBaselineMode() const
        {
            if (!baselineCombo)
                return (int)MultiPeakFit::BaselineConstant;
            const int d = baselineCombo->currentData().toInt();
            if (d == ComboUserAnchorsLinear || d == ComboUserAnchorsSpline)
                return (int)MultiPeakFit::BaselineAnchors;
            return d;
        }

        bool useCubicSpline() const
        {
            if (!baselineCombo)
                return anchorSplineCheck ? anchorSplineCheck->isChecked() : true;
            const int d = baselineCombo->currentData().toInt();
            if (d == ComboUserAnchorsLinear)
                return false;
            if (d == ComboUserAnchorsSpline)
                return true;
            return anchorSplineCheck ? anchorSplineCheck->isChecked() : true;
        }

        const QwtPlotCurve *targetPlotCurve() const
        {
            if (!g || !curveCombo)
                return nullptr;
            return g->curve(curveCombo->currentText());
        }

        int targetXAxis() const
        {
            const QwtPlotCurve *tc = targetPlotCurve();
            return tc ? tc->xAxis() : QwtPlot::xBottom;
        }

        int targetYAxis() const
        {
            const QwtPlotCurve *tc = targetPlotCurve();
            return tc ? tc->yAxis() : QwtPlot::yLeft;
        }

        bool targetCurveBounds(double &xMin, double &xMax) const
        {
            const QwtPlotCurve *tc = targetPlotCurve();
            if (!tc || tc->dataSize() < 1)
                return false;
            const DataCurve *dc = dynamic_cast<const DataCurve *>(tc);
            bool first = true;
            for (int i = 0; i < tc->dataSize(); ++i) {
                if (dc && dc->isPointMasked(i))
                    continue;
                const double xv = tc->x(i);
                const double yv = tc->y(i);
                if (!std::isfinite(xv) || !std::isfinite(yv))
                    continue;
                if (first) {
                    xMin = xMax = xv;
                    first = false;
                } else {
                    xMin = qMin(xMin, xv);
                    xMax = qMax(xMax, xv);
                }
            }
            return !first;
        }

        double snapYToTargetCurve(double xQuery, double fallbackY) const
        {
            const QwtPlotCurve *tc = targetPlotCurve();
            if (!tc || tc->dataSize() < 1)
                return fallbackY;
            const DataCurve *dc = dynamic_cast<const DataCurve *>(tc);
            QVector<QPointF> pts;
            pts.reserve(tc->dataSize());
            for (int i = 0; i < tc->dataSize(); ++i) {
                if (dc && dc->isPointMasked(i))
                    continue;
                const double xv = tc->x(i);
                const double yv = tc->y(i);
                if (std::isfinite(xv) && std::isfinite(yv))
                    pts.push_back(QPointF(xv, yv));
            }
            if (pts.isEmpty())
                return fallbackY;
            std::sort(pts.begin(), pts.end(),
                      [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
            if (xQuery <= pts.first().x())
                return pts.first().y();
            if (xQuery >= pts.last().x())
                return pts.last().y();
            auto it = std::lower_bound(
                    pts.begin(), pts.end(), xQuery,
                    [](const QPointF &pt, double val) { return pt.x() < val; });
            if (it == pts.begin())
                return it->y();
            if (it == pts.end())
                return pts.last().y();
            const QPointF &p1 = *it;
            const QPointF &p0 = *(it - 1);
            const double dx = p1.x() - p0.x();
            if (std::abs(dx) <= 1e-15)
                return 0.5 * (p0.y() + p1.y());
            const double t = (xQuery - p0.x()) / dx;
            return p0.y() + t * (p1.y() - p0.y());
        }

        int findNearestAnchorPx(const QPoint &pos, double maxDistPx) const
        {
            if (!g || !g->plotWidget() || anchors.isEmpty())
                return -1;
            Plot *plt = g->plotWidget();
            const int xAx = targetXAxis();
            const int yAx = targetYAxis();
            int bestIdx = -1;
            double bestDist = maxDistPx;
            for (int i = 0; i < anchors.size(); ++i) {
                const double px = plt->transform(xAx, anchors[i].x());
                const double py = plt->transform(yAx, anchors[i].y());
                const double dist = std::hypot(px - double(pos.x()), py - double(pos.y()));
                if (dist <= bestDist) {
                    bestDist = dist;
                    bestIdx = i;
                }
            }
            return bestIdx;
        }

        void parseAnchorsFromText()
        {
            anchors.clear();
            if (!anchorEdit)
                return;
            const QString raw = anchorEdit->text().trimmed();
            if (raw.isEmpty()) {
                selectedAnchorIdx = -1;
                return;
            }
            const QStringList items = raw.split(QRegExp("[;\\n]+"), Qt::SkipEmptyParts);
            for (const QString &item : items) {
                const QStringList xy = item.trimmed().split(QRegExp("[,\\s]+"), Qt::SkipEmptyParts);
                if (xy.size() >= 2) {
                    bool okX = false, okY = false;
                    const double xv = xy[0].toDouble(&okX);
                    const double yv = xy[1].toDouble(&okY);
                    if (okX && okY && std::isfinite(xv) && std::isfinite(yv))
                        anchors.append(QPointF(xv, yv));
                }
            }
            anchors = normalizeSortedAnchors(anchors);
            if (selectedAnchorIdx >= anchors.size())
                selectedAnchorIdx = anchors.size() - 1;
        }

        void syncTextFromAnchors()
        {
            if (!anchorEdit)
                return;
            QStringList parts;
            parts.reserve(anchors.size());
            for (const QPointF &pt : anchors) {
                parts << QString("%1,%2")
                                 .arg(pt.x(), 0, 'g', 6)
                                 .arg(pt.y(), 0, 'g', 6);
            }
            syncingAnchorEdit = true;
            anchorEdit->setText(parts.join("; "));
            syncingAnchorEdit = false;
        }

        void ensureOverlayCurves()
        {
            if (!g || !g->plotWidget() || cleanedUp)
                return;
            Plot *plt = g->plotWidget();
            const int xAx = targetXAxis();
            const int yAx = targetYAxis();

            if (!liveBaselineCurve) {
                liveBaselineCurve = new QwtPlotCurve();
                liveBaselineCurve->setTitle(QStringLiteral("__tg_transient_ui_preview__"));
                liveBaselineCurve->setItemAttribute(QwtPlotItem::Legend, false);
                liveBaselineCurve->setItemAttribute(QwtPlotItem::AutoScale, false);
                liveBaselineCurve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                liveBaselineCurve->setStyle(QwtPlotCurve::Lines);
                liveBaselineCurve->setPen(QPen(QColor(215, 48, 39), 2.0, Qt::DashLine));
                liveBaselineCurve->setZ(998.0);
                liveBaselineCurve->attach(plt);
            }
            liveBaselineCurve->setAxis(xAx, yAx);

            if (!anchorMarkersCurve) {
                anchorMarkersCurve = new QwtPlotCurve();
                anchorMarkersCurve->setTitle(QStringLiteral("__tg_transient_ui_preview__"));
                anchorMarkersCurve->setItemAttribute(QwtPlotItem::Legend, false);
                anchorMarkersCurve->setItemAttribute(QwtPlotItem::AutoScale, false);
                anchorMarkersCurve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                anchorMarkersCurve->setStyle(QwtPlotCurve::NoCurve);
                anchorMarkersCurve->setSymbol(
                        QwtSymbol(QwtSymbol::Diamond, QBrush(QColor(255, 210, 48)),
                                  QPen(QColor(175, 25, 25), 2), QSize(11, 11)));
                anchorMarkersCurve->setZ(999.0);
                anchorMarkersCurve->attach(plt);
            }
            anchorMarkersCurve->setAxis(xAx, yAx);

            if (!activeAnchorMarkerCurve) {
                activeAnchorMarkerCurve = new QwtPlotCurve();
                activeAnchorMarkerCurve->setTitle(QStringLiteral("__tg_transient_ui_preview__"));
                activeAnchorMarkerCurve->setItemAttribute(QwtPlotItem::Legend, false);
                activeAnchorMarkerCurve->setItemAttribute(QwtPlotItem::AutoScale, false);
                activeAnchorMarkerCurve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                activeAnchorMarkerCurve->setStyle(QwtPlotCurve::NoCurve);
                activeAnchorMarkerCurve->setSymbol(
                        QwtSymbol(QwtSymbol::Ellipse, QBrush(Qt::NoBrush),
                                  QPen(QColor(0, 120, 215), 2), QSize(17, 17)));
                activeAnchorMarkerCurve->setZ(1000.0);
                activeAnchorMarkerCurve->attach(plt);
            }
            activeAnchorMarkerCurve->setAxis(xAx, yAx);

            if (!pickedPeakMarkersCurve) {
                pickedPeakMarkersCurve = new QwtPlotCurve();
                pickedPeakMarkersCurve->setTitle(QStringLiteral("__tg_transient_ui_preview__"));
                pickedPeakMarkersCurve->setItemAttribute(QwtPlotItem::Legend, false);
                pickedPeakMarkersCurve->setItemAttribute(QwtPlotItem::AutoScale, false);
                pickedPeakMarkersCurve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                pickedPeakMarkersCurve->setStyle(QwtPlotCurve::NoCurve);
                pickedPeakMarkersCurve->setSymbol(
                        QwtSymbol(QwtSymbol::UTriangle, QBrush(QColor(16, 185, 129)),
                                  QPen(QColor(6, 95, 70), 2), QSize(12, 12)));
                pickedPeakMarkersCurve->setZ(1001.0);
                pickedPeakMarkersCurve->attach(plt);
            }
            pickedPeakMarkersCurve->setAxis(xAx, yAx);
        }

        void updateCanvasOverlays()
        {
            if (!g || !g->plotWidget() || cleanedUp)
                return;

            const bool showAnchors = isAnchorMode() && !anchors.isEmpty();
            bool showPickedPeaks = false;
            QVector<double> px, py;
            if (manualModeRadio && manualModeRadio->isChecked() && peakTable
                && peakTable->rowCount() > 0) {
                for (int r = 0; r < peakTable->rowCount(); ++r) {
                    if (!peakTable->item(r, 1))
                        continue;
                    bool okX = false;
                    const double xv = peakTable->item(r, 1)->text().toDouble(&okX);
                    if (okX && std::isfinite(xv)) {
                        px.append(xv);
                        py.append(snapYToTargetCurve(xv, 0.0));
                    }
                }
                showPickedPeaks = !px.isEmpty();
            }

            if (!showAnchors && !showPickedPeaks) {
                if (liveBaselineCurve)
                    liveBaselineCurve->setVisible(false);
                if (anchorMarkersCurve)
                    anchorMarkersCurve->setVisible(false);
                if (activeAnchorMarkerCurve)
                    activeAnchorMarkerCurve->setVisible(false);
                if (pickedPeakMarkersCurve)
                    pickedPeakMarkersCurve->setVisible(false);
                g->plotWidget()->replot();
                return;
            }

            ensureOverlayCurves();

            if (pickedPeakMarkersCurve) {
                if (showPickedPeaks) {
                    pickedPeakMarkersCurve->setData(px.constData(), py.constData(), px.size());
                    pickedPeakMarkersCurve->setVisible(true);
                } else {
                    pickedPeakMarkersCurve->setVisible(false);
                }
            }

            if (!showAnchors) {
                if (liveBaselineCurve)
                    liveBaselineCurve->setVisible(false);
                if (anchorMarkersCurve)
                    anchorMarkersCurve->setVisible(false);
                if (activeAnchorMarkerCurve)
                    activeAnchorMarkerCurve->setVisible(false);
                g->plotWidget()->replot();
                return;
            }

            QVector<double> mx(anchors.size()), my(anchors.size());
            for (int i = 0; i < anchors.size(); ++i) {
                mx[i] = anchors[i].x();
                my[i] = anchors[i].y();
            }
            anchorMarkersCurve->setData(mx.constData(), my.constData(), anchors.size());
            anchorMarkersCurve->setVisible(true);

            if (selectedAnchorIdx >= 0 && selectedAnchorIdx < anchors.size()) {
                const double sx = anchors[selectedAnchorIdx].x();
                const double sy = anchors[selectedAnchorIdx].y();
                activeAnchorMarkerCurve->setData(&sx, &sy, 1);
                activeAnchorMarkerCurve->setVisible(true);
            } else {
                activeAnchorMarkerCurve->setVisible(false);
            }

            double xMin = anchors.first().x();
            double xMax = anchors.last().x();
            double cMin = 0.0, cMax = 0.0;
            if (targetCurveBounds(cMin, cMax)) {
                xMin = qMin(xMin, cMin);
                xMax = qMax(xMax, cMax);
            }
            QVector<double> bx, by;
            evaluateAnchorBaselineSamples(anchors, useCubicSpline(), xMin, xMax, 256, bx, by);
            if (!bx.isEmpty()) {
                liveBaselineCurve->setData(bx.constData(), by.constData(), bx.size());
                liveBaselineCurve->setVisible(true);
            } else {
                liveBaselineCurve->setVisible(false);
            }

            g->plotWidget()->replot();
        }

        void cleanup()
        {
            if (cleanedUp)
                return;
            cleanedUp = true;
            draggingAnchorIdx = -1;

            if (canvas && eventFilterObj)
                canvas->removeEventFilter(eventFilterObj);
            if (dlg && eventFilterObj)
                dlg->removeEventFilter(eventFilterObj);
            if (canvas)
                canvas->unsetCursor();

            if (g && g->plotWidget()) {
                if (liveBaselineCurve) {
                    liveBaselineCurve->detach();
                    delete liveBaselineCurve;
                }
                if (anchorMarkersCurve) {
                    anchorMarkersCurve->detach();
                    delete anchorMarkersCurve;
                }
                if (activeAnchorMarkerCurve) {
                    activeAnchorMarkerCurve->detach();
                    delete activeAnchorMarkerCurve;
                }
                if (pickedPeakMarkersCurve) {
                    pickedPeakMarkersCurve->detach();
                    delete pickedPeakMarkersCurve;
                }
                g->plotWidget()->replot();
            }
            liveBaselineCurve = nullptr;
            anchorMarkersCurve = nullptr;
            activeAnchorMarkerCurve = nullptr;
            pickedPeakMarkersCurve = nullptr;
        }
    };

    std::shared_ptr<PeakAnalyzerSession> st = std::make_shared<PeakAnalyzerSession>();
    st->app = this;
    st->g = g;
    st->dlg = dlg;
    if (g->plotWidget())
        st->canvas = g->plotWidget()->canvas();

    QObject::connect(g, &QObject::destroyed, dlg, [st]() {
        // QwtPlotDict deletes attached QwtPlotItems when Plot is destroyed
        st->liveBaselineCurve = nullptr;
        st->anchorMarkersCurve = nullptr;
        st->activeAnchorMarkerCurve = nullptr;
        st->pickedPeakMarkersCurve = nullptr;
        st->g = nullptr;
        st->canvas = nullptr;
        st->cleanedUp = true;
        if (st->dlg)
            st->dlg->reject();
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(dlg);

    // Top Target Curve bar
    QHBoxLayout *topCurveRow = new QHBoxLayout();
    topCurveRow->addWidget(new QLabel(tr("Target Curve:"), dlg));
    QComboBox *curveCombo = new QComboBox(dlg);
    st->curveCombo = curveCombo;
    curveCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    curveCombo->addItems(curves);
    QString selectedCurve = g->selectedCurveTitle();
    if (!selectedCurve.isEmpty()) {
        int idx = curveCombo->findText(selectedCurve);
        if (idx >= 0)
            curveCombo->setCurrentIndex(idx);
    } else {
        for (int i = 0; i < curves.size(); ++i) {
            const QwtPlotCurve *c = g->curve(curves[i]);
            if (c && static_cast<const PlotCurve *>(c)->type() != Graph::Function) {
                curveCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    topCurveRow->addWidget(curveCombo, 1);
    QPushButton *batchPeakBtn = new QPushButton(tr("Batch Peak Analysis (All Curves)..."), dlg);
    QPushButton *peak2DBtn = new QPushButton(tr("2D Peak Analysis (Matrix / Image)..."), dlg);
    topCurveRow->addWidget(batchPeakBtn);
    topCurveRow->addWidget(peak2DBtn);
    mainLayout->addLayout(topCurveRow);

    QObject::connect(batchPeakBtn, &QPushButton::clicked, dlg, [st]() {
        st->subToolAction = 1;
        if (st->dlg)
            st->dlg->reject();
    });
    QObject::connect(peak2DBtn, &QPushButton::clicked, dlg, [st]() {
        st->subToolAction = 2;
        if (st->dlg)
            st->dlg->reject();
    });

    QTabWidget *tabs = new QTabWidget(dlg);
    st->tabs = tabs;
    mainLayout->addWidget(tabs, 1);

    // =========================================================================
    // TAB 1: Goal & Baseline
    // =========================================================================
    QWidget *tabGoalBaseline = new QWidget(dlg);
    QVBoxLayout *tab1Layout = new QVBoxLayout(tabGoalBaseline);

    QGroupBox *goalGroup = new QGroupBox(tr("Analysis Goal"), tabGoalBaseline);
    QFormLayout *goalForm = new QFormLayout(goalGroup);
    QComboBox *goalCombo = new QComboBox(goalGroup);
    st->goalCombo = goalCombo;
    goalCombo->addItem(tr("Fit Peaks (Pro) - Deconvolution & Nonlinear Fit"), 0);
    goalCombo->addItem(tr("Integrate Peaks - Numerical Peak Area Integration"), 1);
    goalCombo->addItem(tr("Find Peaks - Peak Detection & Summary Table"), 2);
    goalCombo->addItem(tr("Subtract Baseline - Subtract Baseline from Curve"), 3);
    goalCombo->addItem(tr("Create Baseline - Plot Baseline Curve on Graph"), 4);
    goalForm->addRow(tr("Goal:"), goalCombo);
    tab1Layout->addWidget(goalGroup);

    QGroupBox *baseGroup = new QGroupBox(tr("Baseline Mode && Subtraction"), tabGoalBaseline);
    QVBoxLayout *baseVBox = new QVBoxLayout(baseGroup);
    QFormLayout *baseForm = new QFormLayout();
    QComboBox *baselineCombo = new QComboBox(baseGroup);
    st->baselineCombo = baselineCombo;
    baselineCombo->addItem(tr("Constant (Minimum / Fitted y0)"),
                           (int)MultiPeakFit::BaselineConstant);
    baselineCombo->addItem(tr("Zero (y = 0 Fixed)"), (int)MultiPeakFit::BaselineZero);
    baselineCombo->addItem(tr("Straight Line (End-to-End Boundary)"),
                           (int)MultiPeakFit::BaselineLinear);
    baselineCombo->addItem(tr("ALS / Moving Minimum Envelope (Curved XRD/Raman)"),
                           (int)MultiPeakFit::BaselineALS);
    baselineCombo->addItem(tr("User Anchors (Linear)"), int(ComboUserAnchorsLinear));
    baselineCombo->addItem(tr("User Anchors (Cubic Spline)"), int(ComboUserAnchorsSpline));
    baseForm->addRow(tr("Baseline Mode:"), baselineCombo);

    QLineEdit *anchorEdit = new QLineEdit(baseGroup);
    st->anchorEdit = anchorEdit;
    anchorEdit->setPlaceholderText("x1,y1; x2,y2; x3,y3; ...");
    baseForm->addRow(tr("Anchor Points (x1,y1; x2,y2; ...):"), anchorEdit);
    baseVBox->addLayout(baseForm);

    QHBoxLayout *anchorOptRow = new QHBoxLayout();
    QCheckBox *anchorSplineCheck = new QCheckBox(tr("Cubic Spline Interpolation"), baseGroup);
    st->anchorSplineCheck = anchorSplineCheck;
    anchorSplineCheck->setChecked(true);
    QPushButton *pickAnchorsBtn =
            new QPushButton(tr("Pick / Drag Anchors on Graph"), baseGroup);
    st->pickAnchorsBtn = pickAnchorsBtn;
    pickAnchorsBtn->setCheckable(true);
    QPushButton *autoSeedAnchorsBtn =
            new QPushButton(tr("Auto-Seed Anchors from Local Minima"), baseGroup);
    st->autoSeedAnchorsBtn = autoSeedAnchorsBtn;
    QPushButton *clearAnchorsBtn = new QPushButton(tr("Clear Anchors"), baseGroup);
    st->clearAnchorsBtn = clearAnchorsBtn;
    anchorOptRow->addWidget(anchorSplineCheck);
    anchorOptRow->addWidget(pickAnchorsBtn);
    anchorOptRow->addWidget(autoSeedAnchorsBtn);
    anchorOptRow->addWidget(clearAnchorsBtn);
    anchorOptRow->addStretch();
    baseVBox->addLayout(anchorOptRow);

    QLabel *anchorHintLabel = new QLabel(
            tr("Click on graph: add anchor (snaps to curve; Shift+Click for free Y) | Drag: move | Right-click / Delete: remove"),
            baseGroup);
    st->anchorHintLabel = anchorHintLabel;
    anchorHintLabel->setWordWrap(true);
    anchorHintLabel->setStyleSheet("color: #555555; font-size: 9.5pt;");
    anchorHintLabel->setVisible(false);
    baseVBox->addWidget(anchorHintLabel);

    auto seedAnchorsFromCurve = [st]() {
        if (!st->app || !st->g || !st->curveCombo)
            return;
        MultiPeakFit tmp(st->app, st->g, MultiPeakFit::Gauss, 1);
        if (!tmp.setDataFromCurve(st->curveCombo->currentText()) || tmp.dataSize() < 2)
            return;
        const int n = tmp.dataSize();
        const auto &px = tmp.x();
        const auto &py = tmp.y();
        const int segments = qMin(6, qMax(2, n / 8));
        st->anchors.clear();
        for (int s = 0; s < segments; ++s) {
            int i0 = (s * n) / segments;
            int i1 = ((s + 1) * n) / segments - 1;
            if (i1 < i0)
                i1 = i0;
            int bestIdx = (s == 0) ? 0 : ((s == segments - 1) ? (n - 1) : i0);
            double minY = py[bestIdx];
            for (int i = i0; i <= i1; ++i) {
                if (py[i] < minY) {
                    minY = py[i];
                    bestIdx = i;
                }
            }
            st->anchors.push_back(QPointF(px[bestIdx], py[bestIdx]));
        }
        st->anchors = normalizeSortedAnchors(st->anchors);
        st->selectedAnchorIdx = st->anchors.isEmpty() ? -1 : 0;
        st->syncTextFromAnchors();
        st->updateCanvasOverlays();
    };

    auto updateBaselineAnchorUi = [st, seedAnchorsFromCurve]() {
        const bool isAnchors = st->isAnchorMode();
        const int cData = st->baselineCombo->currentData().toInt();
        if (cData == ComboUserAnchorsLinear || cData == ComboUserAnchorsSpline) {
            st->anchorSplineCheck->blockSignals(true);
            st->anchorSplineCheck->setChecked(cData == ComboUserAnchorsSpline);
            st->anchorSplineCheck->blockSignals(false);
        }
        st->anchorEdit->setEnabled(isAnchors);
        st->anchorSplineCheck->setEnabled(isAnchors);
        st->autoSeedAnchorsBtn->setEnabled(isAnchors);
        st->clearAnchorsBtn->setEnabled(isAnchors);
        // Keep pickAnchorsBtn always clickable so clicking it from any mode auto-switches to User Anchors!
        st->pickAnchorsBtn->setEnabled(true);

        if (!isAnchors && st->pickAnchorsBtn->isChecked()) {
            st->pickAnchorsBtn->setChecked(false);
        }
        if (isAnchors && st->anchors.isEmpty() && st->anchorEdit->text().trimmed().isEmpty()
            && !st->suppressAutoSeedOnce) {
            seedAnchorsFromCurve();
        }
        st->suppressAutoSeedOnce = false;
        st->updateCanvasOverlays();
    };

    // Install interactive canvas + dialog event filter (anchors AND manual peak picking)
    PeakAnchorEventFilter *filterObj = new PeakAnchorEventFilter(dlg);
    st->eventFilterObj = filterObj;
    filterObj->onTeardown = [st]() { st->cleanup(); };
    filterObj->handler = [st](QObject *watched, QEvent *ev) -> bool {
        if (st->cleanedUp || !st->g || !st->g->plotWidget())
            return false;

        const bool anchorPickActive = st->pickAnchorsBtn && st->pickAnchorsBtn->isChecked();
        const bool manualPeakPickActive = !anchorPickActive && st->manualModeRadio
                && st->manualModeRadio->isChecked() && st->tabs && st->tabs->currentIndex() == 1;
        if (!anchorPickActive && !manualPeakPickActive)
            return false;

        if (ev->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
            if (st->anchorEdit && st->anchorEdit->hasFocus())
                return false;
            if (ke->key() == Qt::Key_Escape) {
                if (anchorPickActive)
                    st->pickAnchorsBtn->setChecked(false);
                else if (manualPeakPickActive && st->autoModeRadio)
                    st->autoModeRadio->setChecked(true);
                return true;
            }
            if (anchorPickActive
                && (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace)) {
                if (!st->anchors.isEmpty()) {
                    int remIdx = st->selectedAnchorIdx;
                    if (remIdx < 0 || remIdx >= st->anchors.size()) {
                        remIdx = st->hasLastMousePos
                                ? st->findNearestAnchorPx(st->lastMousePos, 1e9)
                                : (st->anchors.size() - 1);
                    }
                    if (remIdx >= 0 && remIdx < st->anchors.size()) {
                        st->anchors.removeAt(remIdx);
                        if (st->selectedAnchorIdx >= st->anchors.size())
                            st->selectedAnchorIdx = st->anchors.size() - 1;
                        st->draggingAnchorIdx = -1;
                        st->syncTextFromAnchors();
                        st->updateCanvasOverlays();
                        if (st->refreshPeakTableFn)
                            st->refreshPeakTableFn();
                        return true;
                    }
                }
            }
            return false;
        }

        if (watched != st->canvas)
            return false;

        if (manualPeakPickActive) {
            if (ev->type() == QEvent::ContextMenu)
                return true;
            Plot *plt = st->g->plotWidget();
            const int xAx = st->targetXAxis();
            if (ev->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent *>(ev);
                const double xv = plt->invTransform(xAx, me->pos().x());
                if (me->button() == Qt::LeftButton && std::isfinite(xv)) {
                    const double yv = st->snapYToTargetCurve(xv, 0.0);
                    if (st->addManualPeakFn)
                        st->addManualPeakFn(xv, yv);
                    return true;
                }
                if (me->button() == Qt::RightButton && std::isfinite(xv)) {
                    if (st->removeNearestManualPeakFn)
                        st->removeNearestManualPeakFn(xv);
                    return true;
                }
            } else if (ev->type() == QEvent::MouseMove && st->canvas) {
                st->canvas->setCursor(Qt::CrossCursor);
            }
            return false;
        }

        if (ev->type() == QEvent::ContextMenu)
            return true;

        Plot *plt = st->g->plotWidget();
        const int xAx = st->targetXAxis();
        const int yAx = st->targetYAxis();

        if (ev->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            st->lastMousePos = me->pos();
            st->hasLastMousePos = true;
            if (st->canvas)
                st->canvas->setFocus();

            if (me->button() == Qt::LeftButton) {
                const int hit = st->findNearestAnchorPx(me->pos(), 12.0);
                if (hit >= 0) {
                    st->selectedAnchorIdx = hit;
                    st->draggingAnchorIdx = hit;
                    st->dragNewlyAdded = false;
                    st->pressPos = me->pos();
                    if (st->canvas)
                        st->canvas->setCursor(Qt::ClosedHandCursor);
                    st->updateCanvasOverlays();
                } else {
                    const double xv = plt->invTransform(xAx, me->pos().x());
                    const double freeY = plt->invTransform(yAx, me->pos().y());
                    const bool freeMode = (me->modifiers() & Qt::ShiftModifier) != 0;
                    const double yv = freeMode ? freeY : st->snapYToTargetCurve(xv, freeY);
                    if (std::isfinite(xv) && std::isfinite(yv)) {
                        const QPointF addedPt(xv, yv);
                        st->anchors.push_back(addedPt);
                        st->anchors = normalizeSortedAnchors(st->anchors);
                        int newIdx = 0;
                        double bestDx = std::numeric_limits<double>::infinity();
                        for (int i = 0; i < st->anchors.size(); ++i) {
                            const double dx = std::abs(st->anchors[i].x() - xv);
                            if (dx < bestDx) {
                                bestDx = dx;
                                newIdx = i;
                            }
                        }
                        st->selectedAnchorIdx = newIdx;
                        st->draggingAnchorIdx = newIdx;
                        st->dragNewlyAdded = !freeMode;
                        st->pressPos = me->pos();
                        st->syncTextFromAnchors();
                        st->updateCanvasOverlays();
                    }
                }
                return true;
            }

            if (me->button() == Qt::RightButton) {
                int hit = st->findNearestAnchorPx(me->pos(), 28.0);
                if (hit >= 0 && hit < st->anchors.size()) {
                    st->anchors.removeAt(hit);
                    if (st->selectedAnchorIdx >= st->anchors.size())
                        st->selectedAnchorIdx = st->anchors.size() - 1;
                    st->draggingAnchorIdx = -1;
                    st->syncTextFromAnchors();
                    st->updateCanvasOverlays();
                    if (st->refreshPeakTableFn)
                        st->refreshPeakTableFn();
                }
                return true;
            }
        } else if (ev->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            st->lastMousePos = me->pos();
            st->hasLastMousePos = true;

            if (st->draggingAnchorIdx >= 0 && st->draggingAnchorIdx < st->anchors.size()) {
                if (st->dragNewlyAdded && (me->pos() - st->pressPos).manhattanLength() < 4)
                    return true;
                st->dragNewlyAdded = false;

                const double xv = plt->invTransform(xAx, me->pos().x());
                const double freeY = plt->invTransform(yAx, me->pos().y());
                const double yv = (me->modifiers() & Qt::ShiftModifier)
                        ? st->snapYToTargetCurve(xv, freeY)
                        : freeY;
                if (std::isfinite(xv) && std::isfinite(yv)) {
                    st->anchors[st->draggingAnchorIdx] = QPointF(xv, yv);
                    std::sort(st->anchors.begin(), st->anchors.end(),
                              [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
                    int trackedIdx = 0;
                    double bestMetric = std::numeric_limits<double>::infinity();
                    for (int i = 0; i < st->anchors.size(); ++i) {
                        const double m = std::hypot(st->anchors[i].x() - xv,
                                                    st->anchors[i].y() - yv);
                        if (m < bestMetric) {
                            bestMetric = m;
                            trackedIdx = i;
                        }
                    }
                    st->draggingAnchorIdx = trackedIdx;
                    st->selectedAnchorIdx = trackedIdx;
                    st->syncTextFromAnchors();
                    st->updateCanvasOverlays();
                }
                return true;
            } else if (st->canvas) {
                const int hoverIdx = st->findNearestAnchorPx(me->pos(), 12.0);
                st->canvas->setCursor(hoverIdx >= 0 ? Qt::OpenHandCursor : Qt::CrossCursor);
            }
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton && st->draggingAnchorIdx >= 0) {
                st->draggingAnchorIdx = -1;
                st->dragNewlyAdded = false;
                if (st->canvas) {
                    const int hoverIdx = st->findNearestAnchorPx(me->pos(), 12.0);
                    st->canvas->setCursor(hoverIdx >= 0 ? Qt::OpenHandCursor : Qt::CrossCursor);
                }
                st->syncTextFromAnchors();
                st->updateCanvasOverlays();
                if (st->refreshPeakTableFn)
                    st->refreshPeakTableFn();
                return true;
            }
        }
        return false;
    };

    dlg->installEventFilter(filterObj);
    if (st->canvas)
        st->canvas->installEventFilter(filterObj);

    QObject::connect(pickAnchorsBtn, &QPushButton::toggled, dlg,
                     [st, updateBaselineAnchorUi](bool checked) {
                         st->draggingAnchorIdx = -1;
                         if (checked) {
                             if (!st->isAnchorMode()) {
                                 st->suppressAutoSeedOnce = true;
                                 const int targetData = st->anchorSplineCheck->isChecked()
                                         ? int(ComboUserAnchorsSpline)
                                         : int(ComboUserAnchorsLinear);
                                 const int comboIdx = st->baselineCombo->findData(targetData);
                                 if (comboIdx >= 0)
                                     st->baselineCombo->setCurrentIndex(comboIdx);
                             }
                             st->pickAnchorsBtn->setText(
                                     ApplicationWindow::tr("Stop Picking Anchors (Active)"));
                             st->pickAnchorsBtn->setStyleSheet(
                                     "QPushButton { background-color: #ffe8a3; font-weight: bold; }");
                             st->anchorHintLabel->setVisible(true);
                             if (st->g)
                                 st->g->disableTools();
                             if (st->app && st->app->btnPointer)
                                 st->app->btnPointer->setChecked(true);
                             if (st->canvas) {
                                 st->canvas->setMouseTracking(true);
                                 st->canvas->setFocusPolicy(Qt::StrongFocus);
                                 st->canvas->setCursor(Qt::CrossCursor);
                                 st->canvas->setFocus();
                             }
                         } else {
                             st->pickAnchorsBtn->setText(
                                     ApplicationWindow::tr("Pick / Drag Anchors on Graph"));
                             st->pickAnchorsBtn->setStyleSheet(QString());
                             st->anchorHintLabel->setVisible(false);
                             if (st->canvas)
                                 st->canvas->unsetCursor();
                         }
                         updateBaselineAnchorUi();
                     });

    QObject::connect(clearAnchorsBtn, &QPushButton::clicked, dlg, [st]() {
        st->anchors.clear();
        st->selectedAnchorIdx = -1;
        st->draggingAnchorIdx = -1;
        st->syncTextFromAnchors();
        st->updateCanvasOverlays();
        if (st->refreshPeakTableFn)
            st->refreshPeakTableFn();
    });

    updateBaselineAnchorUi();

    QHBoxLayout *baseBtnRow = new QHBoxLayout();
    QPushButton *previewBaselineBtn =
            new QPushButton(tr("Preview Baseline on Graph"), baseGroup);
    QPushButton *subtractBaselineBtn =
            new QPushButton(tr("Subtract Baseline Now"), baseGroup);
    QCheckBox *overwriteBaselineCheck =
            new QCheckBox(tr("Overwrite original data column (destructive)"), baseGroup);
    st->overwriteBaselineCheck = overwriteBaselineCheck;
    overwriteBaselineCheck->setChecked(false);
    overwriteBaselineCheck->setToolTip(
            tr("By default, subtraction creates a new column (<Name>_Sub) so raw data is kept."));
    baseBtnRow->addWidget(previewBaselineBtn);
    baseBtnRow->addWidget(subtractBaselineBtn);
    baseBtnRow->addWidget(overwriteBaselineCheck);
    baseBtnRow->addStretch();
    baseVBox->addLayout(baseBtnRow);
    tab1Layout->addWidget(baseGroup);
    tab1Layout->addStretch();

    tabs->addTab(tabGoalBaseline, tr("1. Goal && Baseline"));

    auto addProfileItems = [](QComboBox *cb, bool includeInheritDefault) {
        if (includeInheritDefault)
            cb->addItem(tr("Default (Tab 3)"), -1);
        cb->addItem(tr("Gaussian (Area)"), (int)MultiPeakFit::Gauss);
        cb->addItem(tr("Gaussian (Amplitude)"), (int)MultiPeakFit::GaussAmp);
        cb->addItem(tr("Lorentzian"), (int)MultiPeakFit::Lorentz);
        cb->addItem(tr("Pseudo-Voigt"), (int)MultiPeakFit::PseudoVoigt);
        cb->addItem(tr("Pearson VII"), (int)MultiPeakFit::PearsonVII);
        cb->addItem(tr("Voigt (Thompson-Cox-Hastings)"), (int)MultiPeakFit::Voigt);
        cb->addItem(tr("BiGaussian (Asymmetric)"), (int)MultiPeakFit::BiGaussian);
        cb->addItem(tr("EMG (Exponentially Modified Gaussian)"), (int)MultiPeakFit::EMG);
        cb->addItem(tr("Log-Normal"), (int)MultiPeakFit::LogNormal);
        cb->addItem(tr("Asym2Sig (Asymmetric Double Sigmoidal)"),
                    (int)MultiPeakFit::Asym2Sig);
        cb->addItem(tr("Doniach-Sunjic (XPS Asymmetric)"),
                    (int)MultiPeakFit::DoniachSunjic);
    };

    // =========================================================================
    // TAB 2: Find Peaks & Peak List
    // =========================================================================
    QWidget *tabFindPeaks = new QWidget(dlg);
    QVBoxLayout *tab2Layout = new QVBoxLayout(tabFindPeaks);

    QGroupBox *detectGroup = new QGroupBox(tr("Peak Detection Settings"), tabFindPeaks);
    QVBoxLayout *detectLayout = new QVBoxLayout(detectGroup);

    QHBoxLayout *countRow = new QHBoxLayout();
    countRow->addWidget(new QLabel(tr("Number of Peaks:"), detectGroup));
    QSpinBox *peaksSpin = new QSpinBox(detectGroup);
    st->peaksSpin = peaksSpin;
    peaksSpin->setRange(1, 20);
    peaksSpin->setValue(2);
    countRow->addWidget(peaksSpin);

    QCheckBox *autoCountCheck = new QCheckBox(tr("Auto"), detectGroup);
    st->autoCountCheck = autoCountCheck;
    autoCountCheck->setChecked(true);
    countRow->addWidget(autoCountCheck);

    countRow->addWidget(new QLabel(tr("Min Prominence:"), detectGroup));
    QDoubleSpinBox *prominenceSpin = new QDoubleSpinBox(detectGroup);
    st->prominenceSpin = prominenceSpin;
    prominenceSpin->setRange(0.01, 0.90);
    prominenceSpin->setSingleStep(0.02);
    prominenceSpin->setDecimals(2);
    prominenceSpin->setValue(0.08);
    countRow->addWidget(prominenceSpin);

    QPushButton *findPeaksBtn = new QPushButton(tr("Auto-Find Peaks"), detectGroup);
    countRow->addWidget(findPeaksBtn);

    QPushButton *secondDerivPeaksBtn =
            new QPushButton(tr("2nd Derivative (Hidden / Shoulder Peaks)"), detectGroup);
    countRow->addWidget(secondDerivPeaksBtn);

    QLabel *detectedLabel = new QLabel(detectGroup);
    st->detectedLabel = detectedLabel;
    countRow->addWidget(detectedLabel);
    countRow->addStretch();
    detectLayout->addLayout(countRow);

    QHBoxLayout *pickRow = new QHBoxLayout();
    QRadioButton *autoModeRadio =
            new QRadioButton(tr("Use Peak List Below (Auto / Editable Centers && Widths)"), detectGroup);
    QRadioButton *manualModeRadio =
            new QRadioButton(tr("Interactive Peak Picking on Graph (Preserves All Settings)"),
                             detectGroup);
    st->autoModeRadio = autoModeRadio;
    st->manualModeRadio = manualModeRadio;
    autoModeRadio->setChecked(true);
    pickRow->addWidget(autoModeRadio);
    pickRow->addWidget(manualModeRadio);
    QPushButton *clearPickedPeaksBtn = new QPushButton(tr("Clear Peaks"), detectGroup);
    st->clearPickedPeaksBtn = clearPickedPeaksBtn;
    clearPickedPeaksBtn->setVisible(false);
    pickRow->addWidget(clearPickedPeaksBtn);
    pickRow->addStretch();
    detectLayout->addLayout(pickRow);

    QLabel *manualPickHintLabel = new QLabel(
            tr("Click peak centers directly on the graph (Right-click to remove nearest). "
               "Baseline, anchors, and shape parameters on Tabs 1 & 3 are preserved."),
            detectGroup);
    st->manualPickHintLabel = manualPickHintLabel;
    manualPickHintLabel->setWordWrap(true);
    manualPickHintLabel->setStyleSheet("color: #0f766e; font-size: 9.5pt;");
    manualPickHintLabel->setVisible(false);
    detectLayout->addWidget(manualPickHintLabel);

    tab2Layout->addWidget(detectGroup);

    QGroupBox *listGroup = new QGroupBox(tr("Detected / Initial Peak List (Editable)"), tabFindPeaks);
    QVBoxLayout *listLayout = new QVBoxLayout(listGroup);

    QTableWidget *peakTable = new QTableWidget(0, 6, listGroup);
    st->peakTable = peakTable;
    QStringList headers;
    headers << tr("Peak #") << tr("Center (xc)") << tr("Height") << tr("FWHM (w)")
            << tr("Est. Area") << tr("Profile");
    peakTable->setHorizontalHeaderLabels(headers);
    peakTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    peakTable->horizontalHeader()->setStretchLastSection(false);
    peakTable->verticalHeader()->setVisible(false);
    peakTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    listLayout->addWidget(peakTable, 1);

    QHBoxLayout *tableBtnRow = new QHBoxLayout();
    QPushButton *addPeakRowBtn = new QPushButton(tr("+ Add Peak"), listGroup);
    QPushButton *removePeakRowBtn = new QPushButton(tr("- Remove Peak"), listGroup);
    tableBtnRow->addWidget(addPeakRowBtn);
    tableBtnRow->addWidget(removePeakRowBtn);
    tableBtnRow->addStretch();
    listLayout->addLayout(tableBtnRow);

    tab2Layout->addWidget(listGroup, 1);
    tabs->addTab(tabFindPeaks, tr("2. Find Peaks && Peak List"));

    // =========================================================================
    // TAB 3: Fit & Output
    // =========================================================================
    QWidget *tabFitOutput = new QWidget(dlg);
    QVBoxLayout *tab3Layout = new QVBoxLayout(tabFitOutput);

    QGroupBox *profileGroup = new QGroupBox(tr("Peak Profile && Shape Parameters"), tabFitOutput);
    QFormLayout *profileForm = new QFormLayout(profileGroup);

    QComboBox *profileCombo = new QComboBox(profileGroup);
    st->profileCombo = profileCombo;
    addProfileItems(profileCombo, false);
    int profIdx = profileCombo->findData(defaultProfile);
    if (profIdx >= 0)
        profileCombo->setCurrentIndex(profIdx);
    profileForm->addRow(tr("Peak Profile:"), profileCombo);

    QDoubleSpinBox *voigtMuSpin = new QDoubleSpinBox(profileGroup);
    st->voigtMuSpin = voigtMuSpin;
    voigtMuSpin->setRange(0.0, 1.0);
    voigtMuSpin->setSingleStep(0.05);
    voigtMuSpin->setDecimals(3);
    voigtMuSpin->setValue(0.5);
    profileForm->addRow(tr("Pseudo-Voigt / Voigt Lorentz Fraction (\xCE\xBC):"), voigtMuSpin);

    QCheckBox *freeLorentzMuCheck =
            new QCheckBox(tr("Optimize Lorentz fraction \xCE\xBC freely during fit"), profileGroup);
    st->freeLorentzMuCheck = freeLorentzMuCheck;
    freeLorentzMuCheck->setChecked(false);
    profileForm->addRow(QString(), freeLorentzMuCheck);

    QDoubleSpinBox *pearsonMSpin = new QDoubleSpinBox(profileGroup);
    st->pearsonMSpin = pearsonMSpin;
    pearsonMSpin->setRange(0.5, 50.0);
    pearsonMSpin->setSingleStep(0.5);
    pearsonMSpin->setDecimals(2);
    pearsonMSpin->setValue(2.0);
    profileForm->addRow(tr("Pearson VII Shape Parameter (m):"), pearsonMSpin);

    QDoubleSpinBox *asymAlphaSpin = new QDoubleSpinBox(profileGroup);
    st->asymAlphaSpin = asymAlphaSpin;
    st->prevProfileWasDS = (defaultProfile == (int)MultiPeakFit::DoniachSunjic);
    if (st->prevProfileWasDS) {
        asymAlphaSpin->setRange(0.0, 0.849);
        asymAlphaSpin->setSingleStep(0.02);
        asymAlphaSpin->setDecimals(3);
        asymAlphaSpin->setValue(0.15);
    } else {
        asymAlphaSpin->setRange(0.05, 10.0);
        asymAlphaSpin->setSingleStep(0.1);
        asymAlphaSpin->setDecimals(3);
        asymAlphaSpin->setValue(1.5);
    }
    profileForm->addRow(tr("Asymmetry Factor (\xCE\xB1, BiGaussian / Doniach-Sunjic):"),
                        asymAlphaSpin);

    QDoubleSpinBox *tailTauSpin = new QDoubleSpinBox(profileGroup);
    st->tailTauSpin = tailTauSpin;
    tailTauSpin->setRange(0.05, 10.0);
    tailTauSpin->setSingleStep(0.1);
    tailTauSpin->setDecimals(3);
    tailTauSpin->setValue(1.2);
    profileForm->addRow(tr("Exponential Tail Factor (\xCF\x84, EMG / Asym2Sig):"), tailTauSpin);

    tab3Layout->addWidget(profileGroup);

    QGroupBox *doubletGroup =
            new QGroupBox(tr("Spin-Orbit Doublet Constraints (XPS / Spectroscopy)"), tabFitOutput);
    QVBoxLayout *doubletLayout = new QVBoxLayout(doubletGroup);
    QHBoxLayout *doubletRow = new QHBoxLayout();
    QSpinBox *dblPriSpin = new QSpinBox(doubletGroup);
    dblPriSpin->setRange(1, 20);
    dblPriSpin->setValue(1);
    QSpinBox *dblSecSpin = new QSpinBox(doubletGroup);
    dblSecSpin->setRange(1, 20);
    dblSecSpin->setValue(2);
    QDoubleSpinBox *dblRatioSpin = new QDoubleSpinBox(doubletGroup);
    dblRatioSpin->setRange(0.01, 10.0);
    dblRatioSpin->setDecimals(3);
    dblRatioSpin->setSingleStep(0.05);
    dblRatioSpin->setValue(0.5);
    QDoubleSpinBox *dblSplitSpin = new QDoubleSpinBox(doubletGroup);
    dblSplitSpin->setRange(-5000.0, 5000.0);
    dblSplitSpin->setDecimals(3);
    dblSplitSpin->setSingleStep(0.1);
    dblSplitSpin->setValue(3.67);
    QCheckBox *dblShareWidthCheck = new QCheckBox(tr("Share FWHM"), doubletGroup);
    dblShareWidthCheck->setChecked(true);
    QPushButton *addDoubletBtn = new QPushButton(tr("+ Link Doublet"), doubletGroup);
    QPushButton *clearDoubletsBtn = new QPushButton(tr("Clear"), doubletGroup);

    doubletRow->addWidget(new QLabel(tr("Primary #:"), doubletGroup));
    doubletRow->addWidget(dblPriSpin);
    doubletRow->addWidget(new QLabel(tr("Secondary #:"), doubletGroup));
    doubletRow->addWidget(dblSecSpin);
    doubletRow->addWidget(new QLabel(tr("Area Ratio:"), doubletGroup));
    doubletRow->addWidget(dblRatioSpin);
    doubletRow->addWidget(new QLabel(tr("Split \xCE\x94x:"), doubletGroup));
    doubletRow->addWidget(dblSplitSpin);
    doubletRow->addWidget(dblShareWidthCheck);
    doubletRow->addWidget(addDoubletBtn);
    doubletRow->addWidget(clearDoubletsBtn);
    doubletLayout->addLayout(doubletRow);

    QLabel *doubletSummaryLabel = new QLabel(tr("Active doublet links: None"), doubletGroup);
    st->doubletSummaryLabel = doubletSummaryLabel;
    doubletSummaryLabel->setWordWrap(true);
    doubletSummaryLabel->setStyleSheet("color: #475569; font-size: 9pt;");
    doubletLayout->addWidget(doubletSummaryLabel);
    tab3Layout->addWidget(doubletGroup);

    auto syncDoubletLabel = [st]() {
        if (!st->doubletSummaryLabel)
            return;
        if (st->doubletSpecs.isEmpty()) {
            st->doubletSummaryLabel->setText(tr("Active doublet links: None"));
            return;
        }
        QStringList items;
        for (const auto &d : st->doubletSpecs) {
            items << QStringLiteral("P%1 -> P%2 (ratio=%3, \xCE\x94x=%4%5)")
                             .arg(d.secPeak)
                             .arg(d.priPeak)
                             .arg(d.ratio, 0, 'g', 4)
                             .arg(d.splitDx, 0, 'g', 5)
                             .arg(d.shareWidth ? QStringLiteral(", shared w") : QString());
        }
        st->doubletSummaryLabel->setText(tr("Active doublet links: ") + items.join(QStringLiteral("; ")));
    };
    QObject::connect(addDoubletBtn, &QPushButton::clicked, dlg,
                     [st, dblPriSpin, dblSecSpin, dblRatioSpin, dblSplitSpin, dblShareWidthCheck,
                      syncDoubletLabel]() {
                         const int pri = dblPriSpin->value();
                         const int sec = dblSecSpin->value();
                         if (pri == sec)
                             return;
                         PeakAnalyzerSession::DoubletLinkSpec spec;
                         spec.priPeak = pri;
                         spec.secPeak = sec;
                         spec.ratio = dblRatioSpin->value();
                         spec.splitDx = dblSplitSpin->value();
                         spec.shareWidth = dblShareWidthCheck->isChecked();
                         bool replaced = false;
                         for (int i = 0; i < st->doubletSpecs.size(); ++i) {
                             if (st->doubletSpecs[i].secPeak == sec) {
                                 st->doubletSpecs[i] = spec;
                                 replaced = true;
                                 break;
                             }
                         }
                         if (!replaced)
                             st->doubletSpecs.append(spec);
                         syncDoubletLabel();
                     });
    QObject::connect(clearDoubletsBtn, &QPushButton::clicked, dlg, [st, syncDoubletLabel]() {
        st->doubletSpecs.clear();
        syncDoubletLabel();
    });

    auto updateProfileControls = [st]() {
        const int p = st->profileCombo->currentData().toInt();
        bool hasVoigtPeak = (p == (int)MultiPeakFit::PseudoVoigt || p == (int)MultiPeakFit::Voigt);
        bool hasPearsonPeak = (p == (int)MultiPeakFit::PearsonVII);
        bool hasAsymPeak =
                (p == (int)MultiPeakFit::BiGaussian || p == (int)MultiPeakFit::DoniachSunjic);
        bool hasTailPeak = (p == (int)MultiPeakFit::EMG || p == (int)MultiPeakFit::Asym2Sig);
        bool hasDsPeak = (p == (int)MultiPeakFit::DoniachSunjic);
        if (st->peakTable) {
            for (int r = 0; r < st->peakTable->rowCount(); ++r) {
                if (auto *cb = qobject_cast<QComboBox *>(st->peakTable->cellWidget(r, 5))) {
                    const int rp = cb->currentData().toInt();
                    if (rp == (int)MultiPeakFit::PseudoVoigt || rp == (int)MultiPeakFit::Voigt)
                        hasVoigtPeak = true;
                    if (rp == (int)MultiPeakFit::PearsonVII)
                        hasPearsonPeak = true;
                    if (rp == (int)MultiPeakFit::BiGaussian || rp == (int)MultiPeakFit::DoniachSunjic)
                        hasAsymPeak = true;
                    if (rp == (int)MultiPeakFit::EMG || rp == (int)MultiPeakFit::Asym2Sig)
                        hasTailPeak = true;
                    if (rp == (int)MultiPeakFit::DoniachSunjic)
                        hasDsPeak = true;
                }
            }
        }
        st->voigtMuSpin->setEnabled(hasVoigtPeak);
        if (st->freeLorentzMuCheck)
            st->freeLorentzMuCheck->setEnabled(hasVoigtPeak);
        st->pearsonMSpin->setEnabled(hasPearsonPeak);
        st->asymAlphaSpin->setEnabled(hasAsymPeak);
        st->tailTauSpin->setEnabled(hasTailPeak);

        const bool isPrimaryDS = (p == (int)MultiPeakFit::DoniachSunjic) || hasDsPeak;
        if (isPrimaryDS && !st->prevProfileWasDS) {
            QSignalBlocker blk(st->asymAlphaSpin);
            st->asymAlphaSpin->setRange(0.0, 0.849);
            st->asymAlphaSpin->setSingleStep(0.02);
            if (st->asymAlphaSpin->value() >= 0.85 || st->asymAlphaSpin->value() == 1.5)
                st->asymAlphaSpin->setValue(0.15);
            st->prevProfileWasDS = true;
        } else if (!isPrimaryDS && st->prevProfileWasDS) {
            QSignalBlocker blk(st->asymAlphaSpin);
            st->asymAlphaSpin->setRange(0.05, 10.0);
            st->asymAlphaSpin->setSingleStep(0.1);
            if (st->asymAlphaSpin->value() < 0.30)
                st->asymAlphaSpin->setValue(1.5);
            st->prevProfileWasDS = false;
        }
    };
    QObject::connect(profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
                     [st, updateProfileControls](int) {
                         updateProfileControls();
                         if (st->refreshPeakTableFn)
                             st->refreshPeakTableFn();
                     });
    QObject::connect(voigtMuSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dlg,
                     [st](double) {
                         if (st->refreshPeakTableFn)
                             st->refreshPeakTableFn();
                     });
    QObject::connect(pearsonMSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dlg,
                     [st](double) {
                         if (st->refreshPeakTableFn)
                             st->refreshPeakTableFn();
                     });
    QObject::connect(asymAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dlg,
                     [st](double) {
                         if (st->refreshPeakTableFn)
                             st->refreshPeakTableFn();
                     });
    QObject::connect(tailTauSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dlg,
                     [st](double) {
                         if (st->refreshPeakTableFn)
                             st->refreshPeakTableFn();
                     });
    updateProfileControls();

    QGroupBox *outputGroup = new QGroupBox(tr("Output && Curve Options"), tabFitOutput);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    QCheckBox *plotPeaksCheck =
            new QCheckBox(tr("Plot Individual Deconvoluted Peak Curves"), outputGroup);
    st->plotPeaksCheck = plotPeaksCheck;
    plotPeaksCheck->setChecked(true);
    QCheckBox *plotBaselineCheck =
            new QCheckBox(tr("Plot Baseline Curve on Graph"), outputGroup);
    st->plotBaselineCheck = plotBaselineCheck;
    plotBaselineCheck->setChecked(true);
    QCheckBox *summaryTableCheck =
            new QCheckBox(tr("Generate Peak Summary Table"), outputGroup);
    st->summaryTableCheck = summaryTableCheck;
    summaryTableCheck->setChecked(true);
    QCheckBox *integrateTableCheck =
            new QCheckBox(tr("Generate Integrated Peak Area Table"), outputGroup);
    st->integrateTableCheck = integrateTableCheck;
    integrateTableCheck->setChecked(true);
    QCheckBox *residualsTableCheck =
            new QCheckBox(tr("Generate Residuals Table"), outputGroup);
    st->residualsTableCheck = residualsTableCheck;
    residualsTableCheck->setChecked(false);
    outputLayout->addWidget(plotPeaksCheck);
    outputLayout->addWidget(plotBaselineCheck);
    outputLayout->addWidget(summaryTableCheck);
    outputLayout->addWidget(integrateTableCheck);
    outputLayout->addWidget(residualsTableCheck);
    tab3Layout->addWidget(outputGroup);
    tab3Layout->addStretch();

    tabs->addTab(tabFitOutput, tr("3. Fit && Output"));

    auto addPeakRowToTable = [st, addProfileItems, updateProfileControls](
                                     int row, int peakIdx, double xc, double height, double fwhm,
                                     double area, int perPeakProfile) {
        QTableWidgetItem *idxItem = new QTableWidgetItem(QString::number(peakIdx));
        idxItem->setFlags(idxItem->flags() & ~Qt::ItemIsEditable);
        st->peakTable->setItem(row, 0, idxItem);
        st->peakTable->setItem(row, 1, new QTableWidgetItem(QString::number(xc, 'g', 6)));
        QTableWidgetItem *hItem = new QTableWidgetItem(QString::number(height, 'g', 6));
        hItem->setFlags(hItem->flags() & ~Qt::ItemIsEditable);
        st->peakTable->setItem(row, 2, hItem);
        st->peakTable->setItem(row, 3, new QTableWidgetItem(QString::number(fwhm, 'g', 6)));
        QTableWidgetItem *aItem = new QTableWidgetItem(QString::number(area, 'g', 6));
        aItem->setFlags(aItem->flags() & ~Qt::ItemIsEditable);
        st->peakTable->setItem(row, 4, aItem);

        QComboBox *rowProfCombo = new QComboBox(st->peakTable);
        addProfileItems(rowProfCombo, true);
        const int rIdx = rowProfCombo->findData(perPeakProfile);
        if (rIdx >= 0)
            rowProfCombo->setCurrentIndex(rIdx);
        QObject::connect(rowProfCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         st->dlg, [updateProfileControls](int) { updateProfileControls(); });
        st->peakTable->setCellWidget(row, 5, rowProfCombo);
    };

    auto configureShapeParamsOnFitter = [st](MultiPeakFit &f) {
        f.setLorentzFraction(st->voigtMuSpin->value());
        f.setPearsonShape(st->pearsonMSpin->value());
        f.setAsymmetryFactor(st->asymAlphaSpin->value());
        f.setTailFactor(st->tailTauSpin->value());
    };

    auto applyBaselineSubtractionSafe = [st](MultiPeakFit &bf, const QString &curveName,
                                             bool overwriteSource) -> bool {
        if (!st->app || !st->g)
            return false;
        const std::vector<double> sortedBase = bf.computeBaseline();
        const auto &bx = bf.x();
        if (bx.empty() || sortedBase.size() != bx.size())
            return false;

        QVector<QPair<double, double>> basePts;
        basePts.reserve(int(bx.size()));
        for (size_t k = 0; k < bx.size(); ++k)
            basePts.append(qMakePair(bx[k], sortedBase[k]));
        std::sort(basePts.begin(), basePts.end(),
                  [](const QPair<double, double> &a, const QPair<double, double> &b) {
                      return a.first < b.first;
                  });
        auto evalBaseAtX = [&basePts](double xv) -> double {
            if (basePts.isEmpty())
                return 0.0;
            if (xv <= basePts.first().first)
                return basePts.first().second;
            if (xv >= basePts.last().first)
                return basePts.last().second;
            auto it = std::lower_bound(
                    basePts.begin(), basePts.end(), xv,
                    [](const QPair<double, double> &p, double v) { return p.first < v; });
            if (it == basePts.end())
                return basePts.last().second;
            if (std::abs(it->first - xv) <= 1e-12 * qMax(1.0, std::abs(xv)))
                return it->second;
            if (it == basePts.begin())
                return it->second;
            auto prev = it - 1;
            const double dx = it->first - prev->first;
            const double frac = (std::abs(dx) > 1e-15) ? ((xv - prev->first) / dx) : 0.0;
            return prev->second + frac * (it->second - prev->second);
        };

        DataCurve *dc = dynamic_cast<DataCurve *>(st->g->curve(curveName));
        if (!dc || !dc->table())
            return false;
        Table *t = dc->table();
        if (!t->d_future_table)
            return false;

        int xIdx = t->colIndex(dc->xColumnName());
        Column *xCol = (xIdx >= 0) ? t->column(xIdx) : t->column(dc->xColumnName());
        int yIdx = t->colIndex(dc->yColumnName());
        if (yIdx < 0)
            yIdx = t->colIndex(dc->title().text());
        Column *srcYCol = (yIdx >= 0) ? t->column(yIdx) : nullptr;
        if (!srcYCol)
            return false;

        // Box plots / histograms show values that are not table rows (quartiles, bins...):
        // there is no row to write a point back to, neither in place nor in a new column.
        bool anyRow = false;
        for (int pt = 0; pt < dc->dataSize() && !anyRow; ++pt)
            anyRow = (dc->tableRow(pt) >= 0);
        if (!anyRow) {
            if (st->app->statusBar()) {
                st->app->statusBar()->showMessage(
                        ApplicationWindow::tr("The points of curve \"%1\" do not correspond to "
                                              "worksheet rows; the result cannot be written to "
                                              "the worksheet.")
                                .arg(curveName),
                        8000);
            }
            return false;
        }

        // The curve shows its data with the stack offset (and possibly the 0-1 normalisation)
        // applied, and the baseline was found on those shown values; the table holds the
        // original values. Both the point and the baseline under it are mapped back to the
        // original coordinates, so the table gets y_raw - baseline_raw (no offset baked in, and
        // the normalisation's scale undone). The fixed zero baseline means y = 0 of the data.
        // All values are computed before the first write, so a refusal leaves the table as is.
        const bool zeroBaseline = (bf.baselineMode() == MultiPeakFit::BaselineZero);
        QVector<QPair<int, double>> writes;
        writes.reserve(dc->dataSize());
        for (int pt = 0; pt < dc->dataSize(); ++pt) {
            const int row = dc->tableRow(pt); // -1: the point is not a table row
            if (row < 0 || row >= t->numRows())
                continue;
            if ((xCol && xCol->isMasked(row)) || srcYCol->isMasked(row))
                continue;
            const double xv = dc->x(pt);
            const double yv = dc->y(pt);
            if (!std::isfinite(xv) || !std::isfinite(yv))
                continue;
            double rawY = 0.0;
            double rawBase = 0.0;
            if (!dc->displayToRaw(xv, yv, nullptr, &rawY)
                || (!zeroBaseline && !dc->displayToRaw(xv, evalBaseAtX(xv), nullptr, &rawBase))) {
                if (st->app->statusBar()) {
                    st->app->statusBar()->showMessage(
                            ApplicationWindow::tr("The original values of curve \"%1\" cannot be "
                                                  "recovered from its 0-1 normalization; the "
                                                  "result cannot be written to the worksheet.")
                                    .arg(curveName),
                            8000);
                }
                return false;
            }
            const double outY = rawY - rawBase;
            if (std::isfinite(outY))
                writes.append(qMakePair(row, outY));
        }

        if (overwriteSource) {
            if (!ApplicationWindow::isNonInteractiveContext(st->app.data())) {
                if (QMessageBox::question(
                            st->dlg ? static_cast<QWidget *>(st->dlg.data())
                                    : static_cast<QWidget *>(st->app.data()),
                            ApplicationWindow::tr("Overwrite Original Column?"),
                            ApplicationWindow::tr("Overwrite worksheet column \"%1\" in-place with "
                                                  "baseline-subtracted values? (This can be undone "
                                                  "with Ctrl+Z.)")
                                    .arg(srcYCol->name()),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                    != QMessageBox::Yes) {
                    return false;
                }
            }
            t->d_future_table->beginMacro(ApplicationWindow::tr("Subtract Baseline"));
            for (const QPair<int, double> &w : writes)
                srcYCol->setValueAt(w.first, w.second);
            t->d_future_table->endMacro();
            st->g->replot();
            if (st->app && st->app->statusBar()) {
                st->app->statusBar()->showMessage(
                        ApplicationWindow::tr("Baseline subtracted from \"%1\".")
                                .arg(srcYCol->name()),
                        8000);
            }
            return true;
        }

        // Non-destructive default: create a new Y column (<Name>_Sub) and plot it
        const QStringList existingCols = t->colNames();
        QString baseColName = srcYCol->name() + QStringLiteral("_Sub");
        QString newColName = baseColName;
        int suffix = 2;
        while (existingCols.contains(newColName))
            newColName = baseColName + QString::number(suffix++);

        Column *outCol = new Column(newColName, SciDAVis::ColumnMode::Numeric);
        outCol->setPlotDesignation(SciDAVis::Y);
        const QString srcLn = srcYCol->longName().trimmed();
        outCol->setLongName(srcLn.isEmpty()
                                    ? (srcYCol->name() + ApplicationWindow::tr(" (Baseline Subtracted)"))
                                    : (srcLn + ApplicationWindow::tr(" (Baseline Subtracted)")));
        outCol->setUnits(srcYCol->units());
        if (auto *srcOut = dynamic_cast<Double2StringFilter *>(srcYCol->outputFilter())) {
            if (auto *dstOut = dynamic_cast<Double2StringFilter *>(outCol->outputFilter())) {
                dstOut->setNumericFormat(srcOut->numericFormat());
                dstOut->setNumDigits(srcOut->numDigits());
            }
        }
        if (t->numRows() > 0)
            outCol->setValueAt(t->numRows() - 1, std::numeric_limits<double>::quiet_NaN());
        for (const QPair<int, double> &w : writes)
            outCol->setValueAt(w.first, w.second);

        t->d_future_table->beginMacro(ApplicationWindow::tr("Subtract Baseline"));
        t->d_future_table->addChild(outCol);
        t->d_future_table->endMacro();

        const QString xFullName =
                (xIdx >= 0) ? t->colName(xIdx) : dc->xColumnName();
        const int newIdx = t->colIndex(newColName);
        const QString yFullName =
                (newIdx >= 0) ? t->colName(newIdx) : (QString(t->name()) + QLatin1Char('_') + newColName);
        st->g->insertCurve(t, xFullName, yFullName, dc->type());
        st->g->replot();
        if (st->curveCombo && st->curveCombo->findText(yFullName) < 0)
            st->curveCombo->addItem(yFullName);
        if (st->app && st->app->statusBar()) {
            st->app->statusBar()->showMessage(
                    ApplicationWindow::tr("Created baseline-subtracted column \"%1\" (original "
                                          "data kept).")
                            .arg(newColName),
                    10000);
        }
        return true;
    };

    // Populate Peak Table helper
    auto refreshPeakTable = [st, addPeakRowToTable, configureShapeParamsOnFitter]() {
        if (!st->app || !st->g)
            return;
        if (st->manualModeRadio && st->manualModeRadio->isChecked()) {
            st->updateCanvasOverlays();
            return;
        }
        QVector<int> existingRowProfs;
        for (int r = 0; r < st->peakTable->rowCount(); ++r) {
            if (auto *cb = qobject_cast<QComboBox *>(st->peakTable->cellWidget(r, 5)))
                existingRowProfs.append(cb->currentData().toInt());
            else
                existingRowProfs.append(-1);
        }

        MultiPeakFit previewFitter(
                st->app, st->g,
                static_cast<MultiPeakFit::PeakProfile>(st->profileCombo->currentData().toInt()), 1);
        configureShapeParamsOnFitter(previewFitter);
        const int bMode = st->effectiveBaselineMode();
        previewFitter.setBaselineMode(static_cast<MultiPeakFit::BaselineMode>(bMode));
        if (bMode == (int)MultiPeakFit::BaselineAnchors)
            previewFitter.setBaselineAnchors(st->anchors, st->useCubicSpline());
        int est = 1;
        if (previewFitter.setDataFromCurve(st->curveCombo->currentText())) {
            if (st->autoCountCheck->isChecked()) {
                est = previewFitter.autoDetectAndUpdateNumPeaks(20, st->prominenceSpin->value());
                st->peaksSpin->blockSignals(true);
                st->peaksSpin->setValue(qBound(1, est, 20));
                st->peaksSpin->blockSignals(false);
            } else {
                est = st->peaksSpin->value();
                previewFitter.setNumPeaks(est);
                previewFitter.autoDetectPeaks(est, st->prominenceSpin->value());
            }
            st->detectedLabel->setText(ApplicationWindow::tr("(Detected: %1)").arg(est));
            const std::vector<MultiPeakFit::PeakSummaryRow> rows =
                    previewFitter.initialPeakSummary();
            st->peakTable->setRowCount(int(rows.size()));
            for (int i = 0; i < int(rows.size()); ++i) {
                const int keepProf = (i < existingRowProfs.size()) ? existingRowProfs.at(i) : -1;
                addPeakRowToTable(i, rows[i].index, rows[i].center, rows[i].height, rows[i].fwhm,
                                  rows[i].area, keepProf);
            }
        }
        st->updateCanvasOverlays();
    };
    st->refreshPeakTableFn = refreshPeakTable;

    st->addManualPeakFn = [st, addPeakRowToTable](double xv, double yv) {
        if (!st->peakTable)
            return;
        double cMin = 0.0, cMax = 0.0;
        st->targetCurveBounds(cMin, cMax);
        const double wGuess = (cMax > cMin) ? qMax(1e-6, (cMax - cMin) * 0.05) : 1.0;
        st->autoCountCheck->blockSignals(true);
        st->autoCountCheck->setChecked(false);
        st->autoCountCheck->blockSignals(false);
        const int r = st->peakTable->rowCount();
        st->peakTable->insertRow(r);
        addPeakRowToTable(r, r + 1, xv, yv, wGuess, yv * wGuess, -1);
        st->peaksSpin->blockSignals(true);
        st->peaksSpin->setValue(qBound(1, st->peakTable->rowCount(), 20));
        st->peaksSpin->blockSignals(false);
        st->detectedLabel->setText(
                ApplicationWindow::tr("(Picked: %1)").arg(st->peakTable->rowCount()));
        st->updateCanvasOverlays();
    };
    st->removeNearestManualPeakFn = [st](double xv) {
        if (!st->peakTable || st->peakTable->rowCount() <= 0)
            return;
        int bestRow = -1;
        double bestDist = std::numeric_limits<double>::infinity();
        for (int r = 0; r < st->peakTable->rowCount(); ++r) {
            if (!st->peakTable->item(r, 1))
                continue;
            bool ok = false;
            const double px = st->peakTable->item(r, 1)->text().toDouble(&ok);
            if (ok && std::abs(px - xv) < bestDist) {
                bestDist = std::abs(px - xv);
                bestRow = r;
            }
        }
        if (bestRow >= 0) {
            st->peakTable->removeRow(bestRow);
            for (int i = 0; i < st->peakTable->rowCount(); ++i) {
                if (st->peakTable->item(i, 0))
                    st->peakTable->item(i, 0)->setText(QString::number(i + 1));
            }
            st->peaksSpin->blockSignals(true);
            st->peaksSpin->setValue(qBound(1, st->peakTable->rowCount(), 20));
            st->peaksSpin->blockSignals(false);
            st->detectedLabel->setText(
                    ApplicationWindow::tr("(Picked: %1)").arg(st->peakTable->rowCount()));
            st->updateCanvasOverlays();
        }
    };

    QObject::connect(manualModeRadio, &QRadioButton::toggled, dlg, [st](bool checked) {
        if (st->manualPickHintLabel)
            st->manualPickHintLabel->setVisible(checked);
        if (st->clearPickedPeaksBtn)
            st->clearPickedPeaksBtn->setVisible(checked);
        if (checked && st->pickAnchorsBtn && st->pickAnchorsBtn->isChecked())
            st->pickAnchorsBtn->setChecked(false);
        if (st->canvas) {
            if (checked && st->tabs && st->tabs->currentIndex() == 1)
                st->canvas->setCursor(Qt::CrossCursor);
            else if (!st->pickAnchorsBtn || !st->pickAnchorsBtn->isChecked())
                st->canvas->unsetCursor();
        }
        st->updateCanvasOverlays();
    });
    QObject::connect(tabs, &QTabWidget::currentChanged, dlg, [st](int idx) {
        if (!st->canvas)
            return;
        if (idx == 1 && st->manualModeRadio && st->manualModeRadio->isChecked()
            && (!st->pickAnchorsBtn || !st->pickAnchorsBtn->isChecked())) {
            st->canvas->setCursor(Qt::CrossCursor);
        } else if (!st->pickAnchorsBtn || !st->pickAnchorsBtn->isChecked()) {
            st->canvas->unsetCursor();
        }
    });
    QObject::connect(clearPickedPeaksBtn, &QPushButton::clicked, dlg, [st]() {
        st->peakTable->setRowCount(0);
        st->detectedLabel->setText(ApplicationWindow::tr("(Picked: 0)"));
        st->updateCanvasOverlays();
    });

    QObject::connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
                     [st, seedAnchorsFromCurve, refreshPeakTable](int) {
                         if (st->isAnchorMode() && !st->pickAnchorsBtn->isChecked())
                             seedAnchorsFromCurve();
                         else
                             st->updateCanvasOverlays();
                         refreshPeakTable();
                     });
    QObject::connect(baselineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
                     [updateBaselineAnchorUi, refreshPeakTable](int) {
                         updateBaselineAnchorUi();
                         refreshPeakTable();
                     });
    QObject::connect(autoSeedAnchorsBtn, &QPushButton::clicked, dlg,
                     [seedAnchorsFromCurve, refreshPeakTable]() {
                         seedAnchorsFromCurve();
                         refreshPeakTable();
                     });
    QObject::connect(anchorEdit, &QLineEdit::textEdited, dlg, [st](const QString &) {
        if (st->syncingAnchorEdit)
            return;
        st->parseAnchorsFromText();
        st->updateCanvasOverlays();
    });
    QObject::connect(anchorEdit, &QLineEdit::editingFinished, dlg, [st, refreshPeakTable]() {
        if (st->syncingAnchorEdit)
            return;
        st->parseAnchorsFromText();
        st->updateCanvasOverlays();
        refreshPeakTable();
    });
    QObject::connect(anchorSplineCheck, &QCheckBox::toggled, dlg,
                     [st, refreshPeakTable](bool checked) {
                         if (st->isAnchorMode()) {
                             const int wantData = checked ? int(ComboUserAnchorsSpline)
                                                          : int(ComboUserAnchorsLinear);
                             const int idx = st->baselineCombo->findData(wantData);
                             if (idx >= 0 && st->baselineCombo->currentIndex() != idx) {
                                 st->baselineCombo->blockSignals(true);
                                 st->baselineCombo->setCurrentIndex(idx);
                                 st->baselineCombo->blockSignals(false);
                             }
                         }
                         st->updateCanvasOverlays();
                         refreshPeakTable();
                     });
    QObject::connect(prominenceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dlg,
                     [refreshPeakTable](double) { refreshPeakTable(); });
    QObject::connect(autoCountCheck, &QCheckBox::toggled, dlg,
                     [refreshPeakTable](bool checked) {
                         if (checked)
                             refreshPeakTable();
                     });
    QObject::connect(peaksSpin, QOverload<int>::of(&QSpinBox::valueChanged), dlg,
                     [st, refreshPeakTable](int) {
                         if (st->peaksSpin->hasFocus())
                             st->autoCountCheck->setChecked(false);
                         refreshPeakTable();
                     });
    QObject::connect(findPeaksBtn, &QPushButton::clicked, dlg, [st, refreshPeakTable]() {
        st->autoModeRadio->setChecked(true);
        st->autoCountCheck->setChecked(true);
        refreshPeakTable();
    });
    QObject::connect(secondDerivPeaksBtn, &QPushButton::clicked, dlg,
                     [st, addPeakRowToTable, configureShapeParamsOnFitter]() {
                         if (!st->app || !st->g)
                             return;
                         st->autoModeRadio->setChecked(true);
                         MultiPeakFit sdFitter(
                                 st->app, st->g,
                                 static_cast<MultiPeakFit::PeakProfile>(
                                         st->profileCombo->currentData().toInt()),
                                 1);
                         configureShapeParamsOnFitter(sdFitter);
                         const int bMode = st->effectiveBaselineMode();
                         sdFitter.setBaselineMode(static_cast<MultiPeakFit::BaselineMode>(bMode));
                         if (bMode == (int)MultiPeakFit::BaselineAnchors)
                             sdFitter.setBaselineAnchors(st->anchors, st->useCubicSpline());
                         if (sdFitter.setDataFromCurve(st->curveCombo->currentText())) {
                             const QVector<double> sdCenters = sdFitter.detectPeaksSecondDerivative(
                                     9, st->prominenceSpin->value());
                             if (!sdCenters.isEmpty()) {
                                 st->autoCountCheck->blockSignals(true);
                                 st->autoCountCheck->setChecked(false);
                                 st->autoCountCheck->blockSignals(false);
                                 st->peaksSpin->blockSignals(true);
                                 st->peaksSpin->setValue(qBound(1, sdCenters.size(), 20));
                                 st->peaksSpin->blockSignals(false);

                                 sdFitter.setNumPeaks(sdCenters.size());
                                 std::vector<double> stdCenters(sdCenters.begin(), sdCenters.end());
                                 sdFitter.seedPeaksFromCenters(stdCenters,
                                                               st->prominenceSpin->value());
                                 st->detectedLabel->setText(
                                         ApplicationWindow::tr("(Detected: %1)")
                                                 .arg(sdCenters.size()));

                                 const std::vector<MultiPeakFit::PeakSummaryRow> rows =
                                         sdFitter.initialPeakSummary();
                                 st->peakTable->setRowCount(int(rows.size()));
                                 for (int i = 0; i < int(rows.size()); ++i) {
                                     addPeakRowToTable(i, rows[i].index, rows[i].center,
                                                       rows[i].height, rows[i].fwhm, rows[i].area,
                                                       -1);
                                 }
                             }
                         }
                     });
    QObject::connect(addPeakRowBtn, &QPushButton::clicked, dlg, [st, addPeakRowToTable]() {
        st->autoCountCheck->setChecked(false);
        const int r = st->peakTable->rowCount();
        st->peakTable->insertRow(r);
        double xcGuess = 0.0, wGuess = 1.0;
        if (r > 0 && st->peakTable->item(r - 1, 1) && st->peakTable->item(r - 1, 3)) {
            xcGuess = st->peakTable->item(r - 1, 1)->text().toDouble()
                    + st->peakTable->item(r - 1, 3)->text().toDouble();
            wGuess = st->peakTable->item(r - 1, 3)->text().toDouble();
        }
        addPeakRowToTable(r, r + 1, xcGuess, 1.0, wGuess, 1.0, -1);
        st->peaksSpin->blockSignals(true);
        st->peaksSpin->setValue(st->peakTable->rowCount());
        st->peaksSpin->blockSignals(false);
    });
    QObject::connect(removePeakRowBtn, &QPushButton::clicked, dlg, [st, updateProfileControls]() {
        if (st->peakTable->rowCount() <= 1)
            return;
        st->autoCountCheck->setChecked(false);
        int curRow = st->peakTable->currentRow();
        if (curRow < 0)
            curRow = st->peakTable->rowCount() - 1;
        st->peakTable->removeRow(curRow);
        for (int i = 0; i < st->peakTable->rowCount(); ++i) {
            if (st->peakTable->item(i, 0))
                st->peakTable->item(i, 0)->setText(QString::number(i + 1));
        }
        st->peaksSpin->blockSignals(true);
        st->peaksSpin->setValue(st->peakTable->rowCount());
        st->peaksSpin->blockSignals(false);
        updateProfileControls();
        st->updateCanvasOverlays();
    });

    // Baseline Preview & Subtract actions on Tab 1
    QObject::connect(previewBaselineBtn, &QPushButton::clicked, dlg, [st]() {
        if (!st->app || !st->g)
            return;
        st->parseAnchorsFromText();
        MultiPeakFit bf(st->app, st->g, MultiPeakFit::Gauss, 1);
        const int bMode = st->effectiveBaselineMode();
        bf.setBaselineMode(static_cast<MultiPeakFit::BaselineMode>(bMode));
        if (bMode == (int)MultiPeakFit::BaselineAnchors)
            bf.setBaselineAnchors(st->anchors, st->useCubicSpline());
        if (bf.setDataFromCurve(st->curveCombo->currentText())) {
            bf.insertBaselineCurve();
            st->g->replot();
        }
    });
    QObject::connect(subtractBaselineBtn, &QPushButton::clicked, dlg,
                     [st, applyBaselineSubtractionSafe, refreshPeakTable]() {
                         if (!st->app || !st->g)
                             return;
                         st->parseAnchorsFromText();
                         MultiPeakFit bf(st->app, st->g, MultiPeakFit::Gauss, 1);
                         const int bMode = st->effectiveBaselineMode();
                         bf.setBaselineMode(static_cast<MultiPeakFit::BaselineMode>(bMode));
                         if (bMode == (int)MultiPeakFit::BaselineAnchors)
                             bf.setBaselineAnchors(st->anchors, st->useCubicSpline());
                         if (bf.setDataFromCurve(st->curveCombo->currentText())) {
                             const bool overwrite = st->overwriteBaselineCheck
                                     && st->overwriteBaselineCheck->isChecked();
                             if (applyBaselineSubtractionSafe(bf, st->curveCombo->currentText(),
                                                              overwrite)) {
                                 st->updateCanvasOverlays();
                                 refreshPeakTable();
                             }
                         }
                     });

    refreshPeakTable();

    QDialogButtonBox *buttonBox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    st->buttonBox = buttonBox;
    if (QPushButton *okBtn = buttonBox->button(QDialogButtonBox::Ok))
        okBtn->setText(tr("Run Peak Fit"));

    auto updateGoalTabsUi = [st](int goal) {
        const bool isBaselineOnly = (goal == 3 || goal == 4);
        if (st->tabs) {
            st->tabs->setTabEnabled(1, !isBaselineOnly);
            st->tabs->setTabEnabled(2, !isBaselineOnly);
            if (isBaselineOnly && st->tabs->currentIndex() != 0)
                st->tabs->setCurrentIndex(0);
        }
        if (st->buttonBox) {
            if (QPushButton *okBtn = st->buttonBox->button(QDialogButtonBox::Ok)) {
                switch (goal) {
                case 1:
                    okBtn->setText(tr("Integrate Peaks"));
                    break;
                case 2:
                    okBtn->setText(tr("Find Peaks"));
                    break;
                case 3:
                    okBtn->setText(tr("Subtract Baseline"));
                    break;
                case 4:
                    okBtn->setText(tr("Create Baseline Curve"));
                    break;
                default:
                    okBtn->setText(tr("Run Peak Fit"));
                    break;
                }
            }
        }
    };
    QObject::connect(goalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
                     [st, updateGoalTabsUi](int) {
                         updateGoalTabsUi(st->goalCombo->currentData().toInt());
                     });
    updateGoalTabsUi(goalCombo->currentData().toInt());

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, dlg, [dlg, st]() {
        const int goal = st->goalCombo ? st->goalCombo->currentData().toInt() : 0;
        if (goal == 0 && st->manualModeRadio && st->manualModeRadio->isChecked() && st->peakTable
            && st->peakTable->rowCount() == 0) {
            if (st->tabs)
                st->tabs->setCurrentIndex(1);
            QMessageBox::information(
                    dlg, tr("Peak Analyzer"),
                    tr("Click at least one peak center on the graph (or switch to Auto peak "
                       "detection) before running the fit."));
            return;
        }
        dlg->accept();
    });
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    QObject::connect(dlg, &QDialog::accepted, this, [st, applyBaselineSubtractionSafe]() {
        if (!st->app || !st->g) {
            st->cleanup();
            return;
        }
        ApplicationWindow *mw = st->app.data();
        Graph *gr = st->g.data();
        st->parseAnchorsFromText();
        const QVector<QPointF> finalAnchors = st->anchors;
        const bool cubicSpline = st->useCubicSpline();
        const int goal = st->goalCombo->currentData().toInt();
        const int chosenProfile = st->profileCombo->currentData().toInt();
        const int baseMode = st->effectiveBaselineMode();
        const QString targetCurve = st->curveCombo->currentText();
        const bool overwriteBaseline =
                st->overwriteBaselineCheck && st->overwriteBaselineCheck->isChecked();
        const int peaksSpinVal = st->peaksSpin->value();
        const bool autoCount = st->autoCountCheck->isChecked();
        const double prominence = st->prominenceSpin->value();
        double voigtMu = st->voigtMuSpin->value();
        const bool optimizeFreeMu =
                st->freeLorentzMuCheck && st->freeLorentzMuCheck->isEnabled()
                && st->freeLorentzMuCheck->isChecked();
        const double pearsonM = st->pearsonMSpin->value();
        const double asymAlpha = st->asymAlphaSpin->value();
        const double tailTau = st->tailTauSpin->value();
        const bool plotPeaks = st->plotPeaksCheck->isChecked();
        const bool plotBaseline = st->plotBaselineCheck->isChecked();
        const bool summaryTbl = st->summaryTableCheck->isChecked();
        const bool integrateTbl = st->integrateTableCheck->isChecked();
        const bool residualsTbl = st->residualsTableCheck->isChecked();

        QVector<double> customCenters;
        QVector<double> customWidths;
        QVector<int> customRowProfiles;
        bool hasAnyCustomProfile = false;
        for (int i = 0; i < st->peakTable->rowCount(); ++i) {
            if (st->peakTable->item(i, 1)) {
                bool okC = false;
                const double xc = st->peakTable->item(i, 1)->text().toDouble(&okC);
                const double wVal = st->peakTable->item(i, 3)
                        ? st->peakTable->item(i, 3)->text().toDouble()
                        : 0.0;
                if (okC && std::isfinite(xc)) {
                    customCenters.append(xc);
                    customWidths.append(wVal);
                    int rp = -1;
                    if (auto *cb = qobject_cast<QComboBox *>(st->peakTable->cellWidget(i, 5)))
                        rp = cb->currentData().toInt();
                    customRowProfiles.append(rp);
                    if (rp >= 0 && rp != chosenProfile)
                        hasAnyCustomProfile = true;
                }
            }
        }
        const QVector<PeakAnalyzerSession::DoubletLinkSpec> doubletSpecs = st->doubletSpecs;

        // Clean up temporary canvas overlays/event-filters before inserting permanent fit curves
        st->cleanup();

        const int nPeaks = !customCenters.isEmpty() ? customCenters.size() : peaksSpinVal;
        auto configureFitterAll = [&](MultiPeakFit &f, double muVal) {
            f.setLorentzFraction(muVal);
            f.setPearsonShape(pearsonM);
            f.setAsymmetryFactor(asymAlpha);
            f.setTailFactor(tailTau);
            f.setBaselineMode(static_cast<MultiPeakFit::BaselineMode>(baseMode));
            if (baseMode == (int)MultiPeakFit::BaselineAnchors)
                f.setBaselineAnchors(finalAnchors, cubicSpline);
            if (!customCenters.isEmpty()) {
                f.setNumPeaks(customCenters.size());
                std::vector<double> stdCenters(customCenters.begin(), customCenters.end());
                f.seedPeaksFromCenters(stdCenters, prominence);
                f.setInitialPeakWidths(customWidths);
            } else if (autoCount) {
                f.autoDetectAndUpdateNumPeaks(20, prominence);
            } else {
                f.setNumPeaks(nPeaks);
                f.autoDetectPeaks(nPeaks, prominence);
            }
            if (hasAnyCustomProfile && !customRowProfiles.isEmpty()) {
                QVector<MultiPeakFit::PeakProfile> perPeak;
                perPeak.reserve(customRowProfiles.size());
                for (int rp : customRowProfiles) {
                    perPeak.append(static_cast<MultiPeakFit::PeakProfile>(
                            rp >= 0 ? rp : chosenProfile));
                }
                f.setPerPeakProfiles(perPeak);
            }
            for (const auto &dbl : doubletSpecs) {
                if (dbl.priPeak >= 1 && dbl.secPeak >= 1 && dbl.priPeak != dbl.secPeak
                    && dbl.priPeak <= f.peaks() && dbl.secPeak <= f.peaks()) {
                    f.addPeakDoubletConstraint(dbl.secPeak - 1, dbl.priPeak - 1, dbl.ratio,
                                               dbl.splitDx, dbl.shareWidth);
                }
            }
        };

        if (goal == 0 && optimizeFreeMu) {
            if (const QwtPlotCurve *srcCurve = gr->curve(targetCurve)) {
                const int nPts = srcCurve->dataSize();
                if (nPts >= 3) {
                    QVector<double> xv(nPts), yv(nPts);
                    for (int k = 0; k < nPts; ++k) {
                        xv[k] = srcCurve->x(k);
                        yv[k] = srcCurve->y(k);
                    }
                    std::unique_ptr<Graph> probeGr(new Graph(nullptr, nullptr, Qt::Widget));
                    QwtPlotCurve *probeCurve = new QwtPlotCurve(targetCurve);
                    probeCurve->setData(xv.constData(), yv.constData(), nPts);
                    probeGr->insertPlotItem(probeCurve, Graph::Line);

                    const bool savedLogFlag = mw->writeFitResultsToLog;
                    mw->writeFitResultsToLog = false;
                    auto evalMuChi2 = [&](double muCand) -> double {
                        MultiPeakFit pf(mw, probeGr.get(),
                                        static_cast<MultiPeakFit::PeakProfile>(chosenProfile),
                                        qMax(1, nPeaks));
                        pf.setSilent(true);
                        pf.generateFunction(false, 2);
                        pf.enablePeakCurves(false);
                        if (!pf.setDataFromCurve(targetCurve))
                            return std::numeric_limits<double>::infinity();
                        configureFitterAll(pf, muCand);
                        pf.fit();
                        const double c2 = pf.chiSquare();
                        return (std::isfinite(c2) && c2 >= 0.0)
                                ? c2
                                : std::numeric_limits<double>::infinity();
                    };

                    double a = 0.0, b = 1.0;
                    const double phi = 0.6180339887498949;
                    double c = b - phi * (b - a);
                    double d = a + phi * (b - a);
                    double fc = evalMuChi2(c);
                    double fd = evalMuChi2(d);
                    for (int iter = 0; iter < 10; ++iter) {
                        if (fc < fd) {
                            b = d;
                            d = c;
                            fd = fc;
                            c = b - phi * (b - a);
                            fc = evalMuChi2(c);
                        } else {
                            a = c;
                            c = d;
                            fc = fd;
                            d = a + phi * (b - a);
                            fd = evalMuChi2(d);
                        }
                    }
                    double bestMu = 0.5 * (a + b);
                    double bestChi = evalMuChi2(bestMu);
                    for (double ep : { 0.0, voigtMu, 1.0 }) {
                        const double ec = evalMuChi2(ep);
                        if (ec < bestChi) {
                            bestChi = ec;
                            bestMu = ep;
                        }
                    }
                    mw->writeFitResultsToLog = savedLogFlag;
                    if (std::isfinite(bestChi))
                        voigtMu = bestMu;
                }
            }
        }

        MultiPeakFit *fitter = new MultiPeakFit(
                mw, gr, static_cast<MultiPeakFit::PeakProfile>(chosenProfile), qMax(1, nPeaks));
        fitter->enablePeakCurves(plotPeaks);
        fitter->setPeakCurvesColor(mw->peakCurvesColor);
        fitter->scaleErrors(mw->fit_scale_errors);
        fitter->setOutputPrecision(mw->fit_output_precision);
        fitter->generateFunction(mw->generateUniformFitPoints, mw->fitPoints);

        if (fitter->setDataFromCurve(targetCurve)) {
            configureFitterAll(*fitter, voigtMu);

            if (goal == 4) {
                fitter->insertBaselineCurve();
            } else if (goal == 3) {
                applyBaselineSubtractionSafe(*fitter, targetCurve, overwriteBaseline);
            } else if (goal == 2) {
                if (plotBaseline)
                    fitter->insertBaselineCurve();
                fitter->createPeakSummaryTable();
            } else if (goal == 1) {
                if (plotBaseline)
                    fitter->insertBaselineCurve();
                fitter->integratePeaksToTable();
                if (summaryTbl)
                    fitter->createPeakSummaryTable();
            } else {
                fitter->fit();
                if (optimizeFreeMu && mw->writeFitResultsToLog) {
                    mw->updateLog(tr("Optimized Lorentz fraction (\xCE\xBC) = %1\n")
                                          .arg(voigtMu, 0, 'g', 6));
                }
                if (plotBaseline)
                    fitter->insertBaselineCurve();
                if (mw->pasteFitResultsToPlot)
                    fitter->showLegend();
                if (summaryTbl)
                    fitter->createPeakSummaryTable();
                if (integrateTbl)
                    fitter->integratePeaksToTable();
                if (residualsTbl)
                    fitter->createResidualsTable();
            }
            gr->replot();
            emit mw->modified();
        }
        delete fitter;
    });

    QObject::connect(dlg, &QDialog::rejected, this, [st]() {
        ApplicationWindow *mw = st->app.data();
        Graph *gr = st->g.data();
        const int action = st->subToolAction;
        st->cleanup();
        if (mw && action == 1 && gr) {
            QPointer<Graph> grGuard(gr);
            QTimer::singleShot(0, mw, [mw, grGuard]() {
                if (grGuard)
                    MultiPeakFit::showBatchPeakDialog(mw, grGuard.data());
            });
        } else if (mw && action == 2) {
            QTimer::singleShot(0, mw, [mw]() {
                GraphStatsAndSurfaceFit::show2DPeakDialog(mw);
            });
        }
    });

    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void ApplicationWindow::showPeakFitDialog(int defaultProfile)
{
    showPeakAnalyzerDialog(defaultProfile);
}

void ApplicationWindow::fitMultiPeak(int profile)
{
    showPeakAnalyzerDialog(profile);
}

// ============================================================================
// Phase 3: OriginPro Graph Analysis Parity (7 Domains) - UI Integration Slots
// ============================================================================
namespace {

Matrix *activeOrChosenMatrix(ApplicationWindow *app, QMdiArea &workspace)
{
    QWidget *w = workspace.activeSubWindow();
    if (w && w->inherits("Matrix"))
        return static_cast<Matrix *>(w);

    QList<Matrix *> matrices;
    QStringList names;
    for (MyWidget *mw : app->windowsList()) {
        if (mw && mw->inherits("Matrix")) {
            matrices.append(static_cast<Matrix *>(mw));
            names.append(mw->name());
        }
    }
    if (matrices.isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(app)) {
            QMessageBox::information(
                    app, ApplicationWindow::tr("2D FFT / IFFT (Matrix)"),
                    ApplicationWindow::tr("Please create or activate a Matrix window first."));
        }
        return nullptr;
    }
    if (matrices.size() == 1 || ApplicationWindow::isNonInteractiveContext(app))
        return matrices.first();

    bool ok = false;
    QString chosen = QInputDialog::getItem(app, ApplicationWindow::tr("2D FFT / IFFT (Matrix)"),
                                           ApplicationWindow::tr("Select Matrix:"), names, 0,
                                           false, &ok);
    if (!ok || chosen.isEmpty())
        return nullptr;
    int idx = names.indexOf(chosen);
    return (idx >= 0 && idx < matrices.size()) ? matrices[idx] : nullptr;
}

} // anonymous namespace

void ApplicationWindow::maskDataPointsTool()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;

    if (btnMaskPoints) {
        dataTools->blockSignals(true);
        btnMaskPoints->setChecked(true);
        dataTools->blockSignals(false);
    }
    g->setActiveTool(new DataPickerTool(g, this, DataPickerTool::Mask, d_status_info,
                                        SLOT(setText(const QString &))));
}

void ApplicationWindow::unmaskDataPointsTool()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;

    if (btnUnmaskPoints) {
        dataTools->blockSignals(true);
        btnUnmaskPoints->setChecked(true);
        dataTools->blockSignals(false);
    }
    g->setActiveTool(new DataPickerTool(g, this, DataPickerTool::Unmask, d_status_info,
                                        SLOT(setText(const QString &))));
}

void ApplicationWindow::maskRegionGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::MaskRegion);
}

void ApplicationWindow::unmaskRegionGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::UnmaskRegion);
}

void ApplicationWindow::clearAllMasksOnGraph()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;

    const int clearedCurves = g->clearAllCurveMasksWithUndo();
    if (d_status_info && clearedCurves > 0)
        d_status_info->setText(
                tr("Cleared all masked points across %1 curve(s).").arg(clearedCurves));
}

void ApplicationWindow::attachVerticalCursorGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    VerticalCursorTool::attachCursor(this, g, true);
}

void ApplicationWindow::attachRegionStatsGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::RegionStats);
}

void ApplicationWindow::attachQuickFitGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::QuickFit);
}

void ApplicationWindow::attachQuickPeaksGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::QuickPeaks);
}

void ApplicationWindow::attachQuickSigmoidalGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::QuickSigmoidal);
}

void ApplicationWindow::attachIntegrateGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::Integrate);
}

void ApplicationWindow::attachInterpolateGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::Interpolate);
}

void ApplicationWindow::attachFFTGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::FFT);
}

void ApplicationWindow::attachRiseTimeGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::RiseTime);
}

void ApplicationWindow::attachClusterGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::Cluster);
}

void ApplicationWindow::clearActiveGadget()
{
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;

    MultiLayer *ml = static_cast<MultiLayer *>(w);
    for (QWidget *gw : ml->graphPtrs()) {
        Graph *lg = static_cast<Graph *>(gw);
        if (lg) {
            RoiGadgetTool::clearGadget(lg);
            lg->setActiveTool(nullptr);
            lg->replot();
        }
    }
    if (btnPointer)
        btnPointer->setChecked(true);
}

void ApplicationWindow::showSimpleCurveMathDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        CurveMathTools::showCurveMathDialog(this, g);
}

void ApplicationWindow::showSubtractStraightLineDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        CurveMathTools::showSubtractLineDialog(this, g);
}

void ApplicationWindow::showAverageMultipleCurvesDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        CurveMathTools::showAverageCurvesDialog(this, g);
}

void ApplicationWindow::showNormalizeCurvesDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        CurveMathTools::showNormalizeDialog(this, g);
}

void ApplicationWindow::showCurveIntersectionsDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        CurveMathTools::showIntersectionLookupDialog(this, g);
}

void ApplicationWindow::calculatePolygonLoopArea()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;

    const bool nonInteractive = isNonInteractiveContext(this);
    QString targetCurve = g->selectedCurveTitle();
    if (targetCurve.isEmpty()) {
        const QStringList curves = g->analysableCurvesList();
        if (curves.isEmpty())
            return;
        if (curves.size() == 1 || nonInteractive) {
            targetCurve = curves.first();
        } else {
            bool ok = false;
            targetCurve = QInputDialog::getItem(
                    this, tr("Polygon / Hysteresis Loop Area & Arc Length"), tr("Target Curve:"),
                    curves, 0, false, &ok);
            if (!ok || targetCurve.isEmpty())
                return;
        }
    }

    const double areaAbs = CurveMathTools::polygonArea(g, targetCurve, false);
    const double areaSigned = CurveMathTools::polygonArea(g, targetCurve, true);
    const double arcLen = CurveMathTools::curveArcLength(g, targetCurve);

    const QString report =
            QString("[%1 - %2]\n"
                    "  Curve: %3\n"
                    "  Enclosed Polygon / Loop Area |A| = %4\n"
                    "  Signed Shoelace Area         A   = %5\n"
                    "  Total Curve Arc Length       L   = %6\n"
                    "------------------------------------------------------------\n")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                         tr("Polygon / Hysteresis Loop Area & Arc Length"), targetCurve,
                         QString::number(areaAbs, 'g', fit_output_precision),
                         QString::number(areaSigned, 'g', fit_output_precision),
                         QString::number(arcLen, 'g', fit_output_precision));
    updateLog(report);
    if (d_status_info) {
        d_status_info->setText(
                tr("%1: |Area| = %2, Arc Length = %3")
                        .arg(targetCurve, QString::number(areaAbs, 'g', 6),
                             QString::number(arcLen, 'g', 6)));
    }
    if (!nonInteractive) {
        QMessageBox::information(
                this, tr("Polygon / Hysteresis Loop Area & Arc Length"),
                tr("Target Curve: %1\n\n|Area| = %2\nSigned Area = %3\nArc Length = %4")
                        .arg(targetCurve, QString::number(areaAbs, 'g', fit_output_precision),
                             QString::number(areaSigned, 'g', fit_output_precision),
                             QString::number(arcLen, 'g', fit_output_precision)));
    }
}

void ApplicationWindow::fitApparentLinear()
{
    analysis("fitApparentLinear");
}

void ApplicationWindow::showYorkLinearFitDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;

    const QStringList curves = g->analysableCurvesList();
    if (curves.isEmpty())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Linear Fit with X & Y Errors (York)..."));
    dlg.setMinimumWidth(440);

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();

    QComboBox *curveCombo = new QComboBox(&dlg);
    curveCombo->addItems(curves);
    const QString selCurve = g->selectedCurveTitle();
    if (!selCurve.isEmpty()) {
        const int idx = curveCombo->findText(selCurve);
        if (idx >= 0)
            curveCombo->setCurrentIndex(idx);
    }
    form->addRow(tr("Target Curve:"), curveCombo);

    QComboBox *xErrCombo = new QComboBox(&dlg);
    QComboBox *yErrCombo = new QComboBox(&dlg);
    form->addRow(tr("X Error Column (\xCF\x83x):"), xErrCombo);
    form->addRow(tr("Y Error Column (\xCF\x83y):"), yErrCombo);

    QCheckBox *confBandCheck = new QCheckBox(tr("Plot 95% Confidence Band"), &dlg);
    QCheckBox *predBandCheck = new QCheckBox(tr("Plot 95% Prediction Band"), &dlg);
    QCheckBox *summaryBoxCheck = new QCheckBox(tr("Insert Fit Summary Box on Graph"), &dlg);
    summaryBoxCheck->setChecked(true);
    form->addRow(QString(), confBandCheck);
    form->addRow(QString(), predBandCheck);
    form->addRow(QString(), summaryBoxCheck);
    vbox->addLayout(form);

    auto populateErrorCombos = [=]() {
        xErrCombo->clear();
        yErrCombo->clear();
        xErrCombo->addItem(tr("<None / Unit Weights>"), QString());
        yErrCombo->addItem(tr("<None / Unit Weights>"), QString());

        DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(curveCombo->currentText()));
        if (dc && dc->table()) {
            Table *t = dc->table();
            for (int c = 0; c < t->numCols(); ++c) {
                const QString fullColName = t->colName(c);
                xErrCombo->addItem(fullColName, fullColName);
                yErrCombo->addItem(fullColName, fullColName);
                if (t->colPlotDesignation(c) == SciDAVis::xErr && xErrCombo->currentIndex() == 0)
                    xErrCombo->setCurrentIndex(xErrCombo->count() - 1);
                else if (t->colPlotDesignation(c) == SciDAVis::yErr
                         && yErrCombo->currentIndex() == 0)
                    yErrCombo->setCurrentIndex(yErrCombo->count() - 1);
            }
        }
    };
    QObject::connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [=](int) { populateErrorCombos(); });
    populateErrorCombos();

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    LinearFit *lf = new LinearFit(this, g);
    lf->scaleErrors(fit_scale_errors);
    lf->setOutputPrecision(fit_output_precision);
    lf->generateFunction(generateUniformFitPoints, d_2_linear_fit_points ? 2 : fitPoints);
    lf->enableConfidenceBand(confBandCheck->isChecked(), 0.95);
    lf->enablePredictionBand(predBandCheck->isChecked(), 0.95);

    if (lf->setDataFromCurve(curveCombo->currentText())) {
        if (lf->fitWithXYErrors(xErrCombo->currentData().toString(),
                                yErrCombo->currentData().toString())) {
            if (pasteFitResultsToPlot)
                lf->showLegend();
            if (summaryBoxCheck->isChecked())
                lf->insertFitSummaryBoxOnGraph();
            g->replot();
            emit modified();
        }
    }
    delete lf;
}

void ApplicationWindow::showGlobalCurveFitDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        NonLinearFit::showGlobalFitDialog(this, g);
}

void ApplicationWindow::showImplicitODRFitDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        NonLinearFit::showImplicitFitDialog(this, g);
}

void ApplicationWindow::showModelComparisonDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        NonLinearFit::showModelComparisonDialog(this, g);
}

void ApplicationWindow::showSurfaceFit3DDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showSurfaceFitDialog(this);
}

void ApplicationWindow::showBatchPeakAnalysisDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        MultiPeakFit::showBatchPeakDialog(this, g);
}

void ApplicationWindow::show2DPeakAnalysisDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::show2DPeakDialog(this);
}

void ApplicationWindow::showSmoothLOWESSDialog()
{
    showSmoothDialog(SmoothFilter::LOWESS);
}

void ApplicationWindow::showSmoothWhittakerDialog()
{
    showSmoothDialog(SmoothFilter::Whittaker);
}

void ApplicationWindow::showSmoothMedianDialog()
{
    showSmoothDialog(SmoothFilter::Median);
}

void ApplicationWindow::showIirDigitalFilterDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showIirFilterDialog(this, g);
}

void ApplicationWindow::showStftSpectrogramDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showStftDialog(this, g);
}

void ApplicationWindow::showHilbertEnvelopeDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showHilbertDialog(this, g);
}

void ApplicationWindow::showWaveletTransformDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showWaveletDialog(this, g);
}

void ApplicationWindow::showWelchPsdCoherenceDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showPsdCoherenceDialog(this, g);
}

void ApplicationWindow::runMatrixFFT2D()
{
    Matrix *mat = activeOrChosenMatrix(this, d_workspace);
    if (mat)
        AdvancedSignalProc::fft2D(this, mat, false, true);
}

void ApplicationWindow::runMatrixIFFT2D()
{
    Matrix *mat = activeOrChosenMatrix(this, d_workspace);
    if (mat)
        AdvancedSignalProc::fft2D(this, mat, true, true);
}

void ApplicationWindow::showConfidenceEllipseDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        GraphStatsAndSurfaceFit::showConfidenceEllipseDialog(this, g);
}

void ApplicationWindow::showGroupComparisonDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = nullptr;
    QWidget *w = d_workspace.activeSubWindow();
    if (w && w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        if (!ml->isEmpty())
            g = static_cast<Graph *>(ml->activeGraph());
    }
    GraphStatsAndSurfaceFit::showGroupComparisonDialog(this, g);
}

void ApplicationWindow::showROCCurveDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showROCDialog(this);
}

void ApplicationWindow::showKaplanMeierDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showKaplanMeierDialog(this);
}

void ApplicationWindow::showPCABiplotDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showPCADialog(this);
}

namespace {
static QList<Table *> collectAllProjectTables(ApplicationWindow *app,
                                              const QList<MyWidget *> &hiddenList)
{
    QList<Table *> out;
    if (!app)
        return out;
    QList<MyWidget *> allWins = app->windowsList();
    for (MyWidget *hw : hiddenList) {
        if (hw && !allWins.contains(hw))
            allWins.append(hw);
    }
    for (MyWidget *mw : allWins) {
        if (Table *t = qobject_cast<Table *>(mw)) {
            if (!out.contains(t))
                out.append(t);
        }
    }
    return out;
}

static int recalculateAllTableFormulasBounded(ApplicationWindow *app, const QList<Table *> &tables)
{
    if (!app || tables.isEmpty())
        return 0;

    QVector<QPair<QPointer<Table>, int>> formulaCols;
    for (Table *t : tables) {
        if (!t)
            continue;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col || col->formulaIntervals().isEmpty())
                continue;
            bool hasFormula = false;
            const QList<Interval<int>> ivs = col->formulaIntervals();
            for (const Interval<int> &iv : ivs) {
                if (!col->formula(iv.start()).trimmed().isEmpty()) {
                    hasFormula = true;
                    break;
                }
            }
            if (hasFormula)
                formulaCols.append(qMakePair(QPointer<Table>(t), c));
        }
    }
    if (formulaCols.isEmpty())
        return 0;

    const int maxPasses = qMin(formulaCols.size() + 1, 8);
    QSet<QString> updatedKeys;
    for (int pass = 0; pass < maxPasses; ++pass) {
        bool anyChanged = false;
        for (const auto &entry : formulaCols) {
            Table *t = entry.first.data();
            if (!t)
                continue;
            const int c = entry.second;
            if (c < 0 || c >= t->numCols())
                continue;
            Column *col = t->column(c);
            if (!col)
                continue;
            bool colChanged = false;
            const QMetaObject::Connection conn = QObject::connect(
                    col, &AbstractColumn::dataChanged, [&colChanged]() { colChanged = true; });
            const bool prevAuto = t->autoRecalcFormulas();
            t->setAutoRecalcFormulas(false);
            const bool ok = t->recalculate(c, false);
            QObject::disconnect(conn);
            t = entry.first.data();
            if (!t)
                continue;
            t->setAutoRecalcFormulas(prevAuto);
            t = entry.first.data();
            if (!t)
                continue;
            if (ok && c < t->numCols()) {
                const QString key = t->name() + QLatin1Char('_') + t->colLabel(c);
                if (pass == 0 || colChanged)
                    updatedKeys.insert(key);
                if (colChanged) {
                    anyChanged = true;
                    app->updateCurves(t, t->colName(c));
                }
            }
        }
        if (!anyChanged)
            break;
    }
    return updatedKeys.size();
}
} // namespace

void ApplicationWindow::setRecalcModeAuto()
{
    RecalcManager::instance(this)->setRecalcMode(RecalcManager::Auto);
    syncRecalcMenuState();
}

void ApplicationWindow::setRecalcModeManual()
{
    RecalcManager::instance(this)->setRecalcMode(RecalcManager::Manual);
    syncRecalcMenuState();
}

void ApplicationWindow::setRecalcModeDisabled()
{
    RecalcManager::instance(this)->setRecalcMode(RecalcManager::Disabled);
    syncRecalcMenuState();
}

void ApplicationWindow::syncRecalcMenuState()
{
    const RecalcManager::RecalcMode mode = RecalcManager::instance(this)->recalcMode();
    if (actionRecalcAuto)
        actionRecalcAuto->setChecked(mode == RecalcManager::Auto);
    if (actionRecalcManual)
        actionRecalcManual->setChecked(mode == RecalcManager::Manual);
    if (actionRecalcDisabled)
        actionRecalcDisabled->setChecked(mode == RecalcManager::Disabled);

    const bool autoFormula = (mode == RecalcManager::Auto);
    const QList<Table *> allTables = collectAllProjectTables(this, hiddenWindows);
    QList<QPointer<Table>> safeTables;
    safeTables.reserve(allTables.size());
    for (Table *t : allTables) {
        if (t)
            safeTables.append(QPointer<Table>(t));
    }
    bool anyNewlyEnabledAuto = false;
    for (const QPointer<Table> &tp : safeTables) {
        Table *t = tp.data();
        if (!t)
            continue;
        if (autoFormula && !t->autoRecalcFormulas())
            anyNewlyEnabledAuto = true;
        t->setAutoRecalcFormulas(autoFormula);
    }
    if (anyNewlyEnabledAuto) {
        QList<Table *> liveTables;
        liveTables.reserve(safeTables.size());
        for (const QPointer<Table> &tp : safeTables) {
            if (Table *t = tp.data())
                liveTables.append(t);
        }
        recalculateAllTableFormulasBounded(this, liveTables);
    }
}

void ApplicationWindow::recalculatePendingLinksNow()
{
    const QList<Table *> allTables = collectAllProjectTables(this, hiddenWindows);
    QList<QPointer<Table>> safeTables;
    safeTables.reserve(allTables.size());
    for (Table *t : allTables) {
        if (t)
            safeTables.append(QPointer<Table>(t));
    }
    QList<Table *> liveTables;
    liveTables.reserve(safeTables.size());
    for (const QPointer<Table> &tp : safeTables) {
        if (Table *t = tp.data())
            liveTables.append(t);
    }
    const int updatedFormulas = recalculateAllTableFormulasBounded(this, liveTables);
    const int updatedLinks = RecalcManager::instance(this)->recalculateAll();
    if (d_status_info) {
        d_status_info->setText(
                tr("Recalculated %1 linked analysis output(s).").arg(updatedFormulas + updatedLinks));
    }
}

void ApplicationWindow::repeatLinearFitForAllCurves()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        RecalcManager::repeatForAllCurves(this, g, RecalcManager::OpLinearFit);
}

void ApplicationWindow::repeatNormalizationForAllCurves()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        RecalcManager::repeatForAllCurves(this, g, RecalcManager::OpNormalize);
}

void ApplicationWindow::showManageDialogThemes()
{
    if (isNonInteractiveContext(this))
        return;
    DialogThemeManager::showManageThemesDialog(this);
}

void ApplicationWindow::attachDifferentiateGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::Differentiate);
}

void ApplicationWindow::attachIntersectGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::Intersect);
}

void ApplicationWindow::attachIntegrate2DGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;
    if (btnPointer)
        btnPointer->setChecked(true);
    RoiGadgetTool::attachGadget(this, g, RoiGadgetTool::Integrate2D);
}

void ApplicationWindow::showPiecewiseLinearFitDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        PiecewiseAndRobustFit::showPiecewiseFitDialog(this, g);
}

void ApplicationWindow::showRobustLinearFitDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        PiecewiseAndRobustFit::showRobustFitDialog(this, g);
}

void ApplicationWindow::showCompareDatasetsFitDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        PiecewiseAndRobustFit::showCompareDatasetsDialog(this, g);
}

void ApplicationWindow::showMultipleLinearRegressionDialog()
{
    if (isNonInteractiveContext(this))
        return;
    PiecewiseAndRobustFit::showMLRDialog(this);
}

void ApplicationWindow::showSavGolSmoothDerivativeDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showSavGolDerivativeDialog(this, g);
}

void ApplicationWindow::showPeakEnvelopeAnalysisDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showPeakEnvelopeDialog(this, g);
}

void ApplicationWindow::showDecimateResampleDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showDecimateDialog(this, g);
}

void ApplicationWindow::showNormalityTestDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = nullptr;
    QWidget *w = d_workspace.activeSubWindow();
    if (w && w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        if (!ml->isEmpty())
            g = static_cast<Graph *>(ml->activeGraph());
    }
    GraphStatsAndSurfaceFit::showNormalityDialog(this, g);
}

void ApplicationWindow::showBlandAltmanPlotDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showBlandAltmanDialog(this);
}

void ApplicationWindow::showTwoWayANOVAPlotDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showTwoWayANOVADialog(this);
}

void ApplicationWindow::showGraphDigitizerDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphDigitizer::showDigitizerDialog(this);
}

void ApplicationWindow::attachDeltaCursorGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (!g)
        return;
    DeltaCursorTool::attach(this, g);
}

void ApplicationWindow::showLogisticRegressionDialog()
{
    if (isNonInteractiveContext(this))
        return;
    PiecewiseAndRobustFit::showLogisticRegressionDialog(this);
}

void ApplicationWindow::showTensileYieldDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showTensileYieldDialog(this);
}

void ApplicationWindow::showArrheniusPlotDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showArrheniusDialog(this);
}

void ApplicationWindow::showTaucPlotDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showTaucDialog(this);
}

void ApplicationWindow::attachImageProfileGadget()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;
    RoiGadgetTool::clearGadget(g);
    if (btnPointer)
        btnPointer->setChecked(true);
    g->setActiveTool(new ImageProfileGadgetTool(g, this));
}

void ApplicationWindow::showMatrixFilter2DDialog()
{
    if (isNonInteractiveContext(this))
        return;
    AdvancedSignalProc::showMatrixFilter2DDialog(this);
}

void ApplicationWindow::showFourierSelfDeconvolveDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, true);
    if (g)
        AdvancedSignalProc::showFourierSelfDeconvolveDialog(this, g);
}

void ApplicationWindow::showEISAnalyzerDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showEISAnalyzerDialog(this);
}

void ApplicationWindow::showSampleGalleryDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showSampleGalleryDialog(this);
}

void ApplicationWindow::showKissingerDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showKissingerDialog(this);
}

void ApplicationWindow::showWorkHardeningDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showWorkHardeningDialog(this);
}

void ApplicationWindow::showXRDAnalyzerDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showXRDAnalyzerDialog(this);
}

void ApplicationWindow::showVaspPlotterDialog()
{
    if (isNonInteractiveContext(this))
        return;
    CurveMathTools::showVaspPlotterDialog(this);
}

void ApplicationWindow::showPassingBablokDemingDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showPassingBablokDemingDialog(this);
}

void ApplicationWindow::showWeibullPlotDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showWeibullDialog(this);
}

void ApplicationWindow::showOutlierTestDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showOutlierTestDialog(this);
}

void ApplicationWindow::showNonparametricTestsDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *g = nullptr;
    QWidget *w = d_workspace.activeSubWindow();
    if (w && w->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(w);
        if (!ml->isEmpty())
            g = static_cast<Graph *>(ml->activeGraph());
    }
    GraphStatsAndSurfaceFit::showNonparametricDialog(this, g);
}

void ApplicationWindow::showPairedComparisonPlotDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showPairedComparisonDialog(this);
}

void ApplicationWindow::showCorrelationMatrixDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showCorrelationMatrixDialog(this);
}

void ApplicationWindow::showClusteringDialog()
{
    if (isNonInteractiveContext(this))
        return;
    GraphStatsAndSurfaceFit::showClusteringDialog(this);
}

void ApplicationWindow::quickLinearFitSelection()
{
    QWidget *aw = d_workspace.activeSubWindow();
    if (MultiLayer *ml = qobject_cast<MultiLayer *>(aw)) {
        Graph *g = ml->activeGraph();
        if (!g || !g->validCurvesDataSize()) {
            showUxHint(tr("Please select or activate a graph with plottable curve data first."));
            return;
        }
        QString targetCurve = g->selectedCurveTitle();
        if (targetCurve.isNull()) {
            const QStringList lst = g->analysableCurvesList();
            if (lst.isEmpty())
                return;
            targetCurve = lst.first();
        }
        analyzeCurve(g, QStringLiteral("fitLinear"), targetCurve);
        emit ml->modifiedPlot();
        return;
    }

    Table *t = qobject_cast<Table *>(aw);
    if (!t) {
        showUxHint(tr("Quick Linear Fit works on an active graph curve or selected worksheet columns."));
        return;
    }
    if (t->noXColumn()) {
        showUxHint(tr("Worksheet '%1' needs an X column for Quick Linear Fit.").arg(t->name()));
        return;
    }
    QStringList yCols = t->selectedYColumns();
    if (yCols.isEmpty())
        yCols = t->YColumns();
    if (yCols.isEmpty()) {
        showUxHint(tr("Please select a Y column in '%1' for Quick Linear Fit.").arg(t->name()));
        return;
    }

    const int startRow = (t->selectedRowCount(false) >= 2) ? t->firstSelectedRow(false) : 0;
    const int endRow = (t->selectedRowCount(false) >= 2) ? t->lastSelectedRow(false)
                                                         : qMax(0, t->numRows() - 1);
    MultiLayer *ml = multilayerPlot(t, QStringList() << yCols.first(), Graph::Scatter, startRow,
                                    endRow);
    if (!ml)
        return;
    Graph *ag = ml->activeGraph();
    if (!ag)
        return;
    const QStringList curves = ag->analysableCurvesList();
    if (!curves.isEmpty())
        analyzeCurve(ag, QStringLiteral("fitLinear"), curves.first());
    emit ml->modifiedPlot();
    activateSubWindow(ml);
    emit modified();
}
