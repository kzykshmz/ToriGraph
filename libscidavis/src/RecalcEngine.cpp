/***************************************************************************
    File                 : RecalcEngine.cpp
    Project              : ToriGraph (SciDAVis OriginPro 2021 Parity)
    Description          : OriginPro Dynamic Recalculation Engine ("Green Lock"),
                           Batch Repeat for All Curves, and Dialog Theme Manager
 ***************************************************************************/
#include "RecalcEngine.h"

#include "AdvancedSignalProc.h"
#include "ApplicationWindow.h"
#include "CurveMathTools.h"
#include "Differentiation.h"
#include "FunctionCurve.h"
#include "Graph.h"
#include "I18nJapanese.h"
#include "MultiLayer.h"
#include "MultiPeakFit.h"
#include "NonLinearFit.h"
#include "PiecewiseAndRobustFit.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "PolynomialFit.h"
#include "SmoothFilter.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>

#include <QByteArray>
#include <QComboBox>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIODevice>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace {

QString resolveGraphCurveTitle(Graph *g, const QString &curveTitle)
{
    if (!g || g->curves() <= 0)
        return curveTitle;
    if (!curveTitle.isEmpty()) {
        if (g->curve(curveTitle))
            return curveTitle;
        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            if (!c)
                continue;
            QString t = c->title().text();
            if (t.compare(curveTitle, Qt::CaseInsensitive) == 0
                || t.endsWith("_" + curveTitle, Qt::CaseInsensitive))
                return t;
        }
    }
    QString sel = g->selectedCurveTitle();
    if (!sel.isEmpty() && g->curve(sel))
        return sel;
    QStringList analysable = g->analysableCurvesList();
    if (!analysable.isEmpty())
        return analysable.first();
    return curveTitle;
}

QwtPlotCurve *createNormalizedCurveOnGraph(ApplicationWindow *app, Graph *g,
                                           const QString &srcCurveTitle,
                                           const QVariantMap &params)
{
    if (!app || !g)
        return nullptr;
    QwtPlotCurve *src = g->curve(srcCurveTitle);
    if (!src || src->dataSize() < 2)
        return nullptr;

    const int rawN = src->dataSize();
    auto *dcSrc = dynamic_cast<DataCurve *>(src);
    Column *xMaskCol = nullptr;
    Column *yMaskCol = nullptr;
    if (dcSrc && dcSrc->table()) {
        int xi = dcSrc->table()->colIndex(dcSrc->xColumnName());
        int yi = dcSrc->table()->colIndex(dcSrc->yColumnName());
        if (xi >= 0)
            xMaskCol = dcSrc->table()->column(xi);
        if (yi >= 0)
            yMaskCol = dcSrc->table()->column(yi);
    }

    std::vector<double> xs;
    std::vector<double> ys;
    xs.reserve(static_cast<size_t>(rawN));
    ys.reserve(static_cast<size_t>(rawN));
    for (int i = 0; i < rawN; ++i) {
        if (dcSrc) {
            int row = dcSrc->tableRow(i);
            if (row >= 0
                && ((xMaskCol && xMaskCol->isMasked(row))
                    || (yMaskCol && yMaskCol->isMasked(row))))
                continue;
        }
        double vx = src->x(i);
        double vy = src->y(i);
        if (std::isfinite(vx) && std::isfinite(vy)) {
            xs.push_back(vx);
            ys.push_back(vy);
        }
    }
    const int n = static_cast<int>(xs.size());
    if (n < 2)
        return nullptr;

    // Sort by X (stable) so that the area of the unit-area mode is the true trapezoidal area,
    // and share the normalisation kernel with the Normalize dialog so both give the same values.
    {
        std::vector<size_t> ord(xs.size());
        std::iota(ord.begin(), ord.end(), size_t(0));
        std::stable_sort(ord.begin(), ord.end(), [&](size_t a, size_t b) { return xs[a] < xs[b]; });
        std::vector<double> xsS(xs.size()), ysS(ys.size());
        for (size_t i = 0; i < ord.size(); ++i) {
            xsS[i] = xs[ord[i]];
            ysS[i] = ys[ord[i]];
        }
        xs.swap(xsS);
        ys.swap(ysS);
    }

    // Modes: 0 = Range [0, 1], 1 = Max = 1, 2 = Z-Score, 3 = Unit Area, 4 = Range [-1, 1]
    int mode = params.value(QStringLiteral("mode"), 0).toInt();
    QString modeStr = params.value(QStringLiteral("method")).toString().trimmed().toLower();
    if (modeStr == QStringLiteral("max"))
        mode = 1;
    else if (modeStr == QStringLiteral("zscore"))
        mode = 2;
    else if (modeStr == QStringLiteral("unitarea") || modeStr == QStringLiteral("area"))
        mode = 3;

    CurveMathTools::NormMode cmMode = CurveMathTools::Range0To1;
    switch (mode) {
    case 1:
        cmMode = CurveMathTools::DivideByMax;
        break;
    case 2:
        cmMode = CurveMathTools::ZScore;
        break;
    case 3:
        cmMode = CurveMathTools::UnitArea;
        break;
    case 4:
        cmMode = CurveMathTools::RangeMinus1To1;
        break;
    default:
        cmMode = CurveMathTools::Range0To1;
        break;
    }
    std::vector<double> yn = CurveMathTools::normalizeValues(xs, ys, cmMode);

    QString tblName = app->generateUniqueName(QStringLiteral("Norm"));
    Column *colX = new Column(QStringLiteral("1"), SciDAVis::ColumnMode::Numeric);
    Column *colY = new Column(QStringLiteral("Norm"), SciDAVis::ColumnMode::Numeric);
    colX->setPlotDesignation(SciDAVis::X);
    colY->setPlotDesignation(SciDAVis::Y);
    for (int i = 0; i < n; ++i) {
        colX->setValueAt(i, xs[static_cast<size_t>(i)]);
        colY->setValueAt(i, yn[static_cast<size_t>(i)]);
    }

    Table *t = app->newHiddenTable(tblName,
                                   QObject::tr("Normalized ") + srcCurveTitle,
                                   QList<Column *>() << colX << colY);
    if (!t)
        return nullptr;

    DataCurve *dc = new DataCurve(t, tblName + "_" + colX->name(), tblName + "_" + colY->name());
    dc->setData(xs.data(), yn.data(), n);
    dc->setPen(QPen(QColor(25, 130, 75), 2));
    g->insertPlotItem(dc, Graph::Line);
    g->updatePlot();
    return dc;
}

void updateFitSummaryTable(Table *resultTable, const QStringList &names,
                           const std::vector<double> &vals, const std::vector<double> &errs,
                           double r2)
{
    if (!resultTable || vals.empty())
        return;
    const int p = static_cast<int>(vals.size());
    if (resultTable->numRows() < p)
        resultTable->setNumRows(p);
    if (resultTable->numCols() < 2)
        resultTable->setNumCols(3);

    for (int i = 0; i < p; ++i) {
        if (i < names.size() && resultTable->column(0)) {
            if (resultTable->column(0)->columnMode() == SciDAVis::ColumnMode::Text)
                resultTable->column(0)->setTextAt(i, names[i]);
        }
        if (resultTable->column(1)
            && resultTable->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric) {
            resultTable->column(1)->setValueAt(i, vals[static_cast<size_t>(i)]);
        }
        if (resultTable->numCols() >= 3 && resultTable->column(2)
            && resultTable->column(2)->columnMode() == SciDAVis::ColumnMode::Numeric
            && i < static_cast<int>(errs.size())) {
            resultTable->column(2)->setValueAt(i, errs[static_cast<size_t>(i)]);
        }
    }
    if (resultTable->numRows() > p && resultTable->column(1)
        && resultTable->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric) {
        if (resultTable->column(0)
            && resultTable->column(0)->columnMode() == SciDAVis::ColumnMode::Text) {
            QString lbl = resultTable->column(0)->textAt(p);
            if (lbl.contains(QStringLiteral("R^2"), Qt::CaseInsensitive) || lbl.isEmpty()) {
                resultTable->column(0)->setTextAt(p, QStringLiteral("R^2"));
                resultTable->column(1)->setValueAt(p, r2);
            }
        }
    }
}

// ============================================================================
// Persistent + In-Memory Theme Store for DialogThemeManager
// ============================================================================

QMap<QString, QMap<QString, QVariantMap>> &themeMemoryCache()
{
    static QMap<QString, QMap<QString, QVariantMap>> s_cache;
    static bool s_loaded = false;
    if (!s_loaded) {
        s_loaded = true;
        QSettings &settings = ApplicationWindow::getSettings();
        settings.beginGroup(QStringLiteral("OriginProDialogThemes"));
        const QStringList toolGroups = settings.childGroups();
        for (const QString &encTool : toolGroups) {
            const QString toolId = QUrl::fromPercentEncoding(encTool.toUtf8());
            settings.beginGroup(encTool);
            const QStringList keys = settings.childKeys();
            for (const QString &encName : keys) {
                const QString themeName = QUrl::fromPercentEncoding(encName.toUtf8());
                QByteArray raw = settings.value(encName).toByteArray();
                if (!raw.isEmpty()) {
                    QDataStream ds(&raw, QIODevice::ReadOnly);
                    ds.setVersion(QDataStream::Qt_5_12);
                    QVariantMap map;
                    ds >> map;
                    if (ds.status() == QDataStream::Ok)
                        s_cache[toolId][themeName] = map;
                }
            }
            settings.endGroup();
        }
        settings.endGroup();
    }
    return s_cache;
}

} // namespace

// ============================================================================
// RecalcManager Implementation
// ============================================================================

RecalcManager::RecalcManager(ApplicationWindow *app) : QObject(nullptr), d_app(app) {}

RecalcManager *RecalcManager::instance(ApplicationWindow *app)
{
    static RecalcManager *s_instance = nullptr;
    if (!s_instance)
        s_instance = new RecalcManager(app);
    else if (app)
        s_instance->d_app = app;
    return s_instance;
}

void RecalcManager::pruneDeadLinks()
{
    for (int i = d_links.size() - 1; i >= 0; --i) {
        if (d_links[i].graph.isNull() || d_links[i].srcTable.isNull())
            d_links.removeAt(i);
    }
}

void RecalcManager::setRecalcMode(RecalcMode mode)
{
    d_mode = mode;
    pruneDeadLinks();

    if (d_mode == Auto && hasDirtyLinks())
        recalculateAll();

    QList<Graph *> graphs;
    for (const RecalcLink &lnk : d_links) {
        if (lnk.graph && !graphs.contains(lnk.graph.data()))
            graphs.append(lnk.graph.data());
    }
    for (Graph *g : graphs)
        updateLockBadge(g);
}

int RecalcManager::findLinkIndexById(int id) const
{
    for (int i = 0; i < d_links.size(); ++i) {
        if (d_links[i].id == id)
            return i;
    }
    return -1;
}

bool RecalcManager::executeLinkById(int id)
{
    const int idx = findLinkIndexById(id);
    if (idx < 0)
        return false;
    RecalcLink copy = d_links[idx];
    if (!executeLink(copy))
        return false;
    const int postIdx = findLinkIndexById(id);
    if (postIdx >= 0) {
        d_links[postIdx].srcCurveTitle = copy.srcCurveTitle;
        d_links[postIdx].outCurveTitle = copy.outCurveTitle;
        d_links[postIdx].extraCurveTitles = copy.extraCurveTitles;
        d_links[postIdx].resultTable = copy.resultTable;
    }
    return true;
}

int RecalcManager::registerLink(ApplicationWindow *app, Graph *g, Table *srcTable,
                                const QString &srcCurveTitle, const QString &outCurveTitle,
                                OpType op, const QVariantMap &params, Table *resultTable)
{
    ApplicationWindow *effApp = app ? app : d_app.data();
    if (effApp)
        d_app = effApp;
    if (!effApp || !g)
        return -1;

    // Never reallocate or prune d_links re-entrantly while executeLink is running
    if (d_recalculating) {
        const QString resolvedSrc = resolveGraphCurveTitle(g, srcCurveTitle);
        for (RecalcLink &existing : d_links) {
            if (existing.graph == g && existing.op == op
                && (existing.srcCurveTitle == resolvedSrc || existing.srcTable == srcTable)) {
                if (!outCurveTitle.isEmpty())
                    existing.outCurveTitle = outCurveTitle;
                if (resultTable)
                    existing.resultTable = resultTable;
                return existing.id;
            }
        }
        return 0;
    }

    pruneDeadLinks();

    QString resolvedSrc = resolveGraphCurveTitle(g, srcCurveTitle);
    Table *effSrcTable = srcTable;
    if (!effSrcTable) {
        if (auto *dc = dynamic_cast<DataCurve *>(g->curve(resolvedSrc)))
            effSrcTable = dc->table();
    }
    if (!effSrcTable)
        return -1;

    QString effOut = outCurveTitle;
    if (effOut.isEmpty() && !g->fitCurvesList().isEmpty()) {
        QwtPlotCurve *lastFit = g->fitCurvesList().last();
        if (lastFit)
            effOut = lastFit->title().text();
    }

    connect(effSrcTable, &Table::modifiedData, this, &RecalcManager::onTableDataModified,
            Qt::UniqueConnection);
    connect(g, &QObject::destroyed, this, &RecalcManager::onGraphDestroyed, Qt::UniqueConnection);

    QPointer<Graph> gGuard(g);

    // Update in place if an identical link already exists
    for (int i = 0; i < d_links.size(); ++i) {
        RecalcLink &existing = d_links[i];
        if (existing.graph == g && existing.srcTable == effSrcTable
            && existing.srcCurveTitle == resolvedSrc && existing.op == op
            && (effOut.isEmpty() || existing.outCurveTitle == effOut
                || existing.desiredOutTitle == outCurveTitle)) {
            const int existingId = existing.id;
            existing.app = effApp;
            existing.params = params;
            existing.resultTable = resultTable;
            if (!outCurveTitle.isEmpty())
                existing.desiredOutTitle = outCurveTitle;
            if (!effOut.isEmpty())
                existing.outCurveTitle = effOut;
            const bool needExec = (d_mode == Auto
                                   && (existing.outCurveTitle.isEmpty()
                                       || g->curve(existing.outCurveTitle) == nullptr));
            if (needExec)
                executeLinkById(existingId);
            if (gGuard)
                updateLockBadge(gGuard.data());
            return existingId;
        }
    }

    RecalcLink link;
    link.id = d_next_id++;
    link.app = effApp;
    link.graph = g;
    link.srcTable = effSrcTable;
    link.srcCurveTitle = resolvedSrc;
    link.outCurveTitle = effOut;
    link.desiredOutTitle = outCurveTitle;
    link.op = op;
    link.params = params;
    link.resultTable = resultTable;
    link.dirty = false;

    const int newId = link.id;
    const bool needExec =
            (d_mode == Auto && (link.outCurveTitle.isEmpty() || g->curve(link.outCurveTitle) == nullptr));
    d_links.append(link);

    // If caller registered a live link before running the operation, materialize the output curve immediately in Auto mode
    if (needExec)
        executeLinkById(newId);

    if (gGuard)
        updateLockBadge(gGuard.data());
    return newId;
}

void RecalcManager::removeLinksForGraph(Graph *g)
{
    if (!g)
        return;
    for (int i = d_links.size() - 1; i >= 0; --i) {
        if (d_links[i].graph == g || d_links[i].graph.isNull())
            d_links.removeAt(i);
    }
    updateLockBadge(g);
}

int RecalcManager::activeLinkCount(Graph *g) const
{
    int count = 0;
    for (const RecalcLink &lnk : d_links) {
        if (lnk.graph.isNull() || lnk.srcTable.isNull())
            continue;
        if (!g || lnk.graph == g)
            ++count;
    }
    return count;
}

bool RecalcManager::hasDirtyLinks(Graph *g) const
{
    for (const RecalcLink &lnk : d_links) {
        if (lnk.graph.isNull() || lnk.srcTable.isNull())
            continue;
        if ((!g || lnk.graph == g) && lnk.dirty)
            return true;
    }
    return false;
}

int RecalcManager::recalculateAll(Graph *filterGraph)
{
    if (d_recalculating)
        return 0;

    pruneDeadLinks();
    int updated = 0;
    QVector<QPointer<Graph>> touchedGraphs;
    QPointer<Graph> filterGuard(filterGraph);

    QVector<int> linkIds;
    linkIds.reserve(d_links.size());
    for (const RecalcLink &lnk : d_links) {
        if (!lnk.graph || !lnk.srcTable)
            continue;
        if (filterGraph && lnk.graph != filterGraph)
            continue;
        linkIds.append(lnk.id);
    }

    for (int id : linkIds) {
        const int idx = findLinkIndexById(id);
        if (idx < 0 || !d_links[idx].graph || !d_links[idx].srcTable)
            continue;
        QPointer<Graph> gPtr = d_links[idx].graph;
        if (executeLinkById(id)) {
            const int postIdx = findLinkIndexById(id);
            if (postIdx >= 0)
                d_links[postIdx].dirty = false;
            ++updated;
            if (gPtr && !touchedGraphs.contains(gPtr))
                touchedGraphs.append(gPtr);
        }
    }

    for (const QPointer<Graph> &g : touchedGraphs) {
        if (g)
            updateLockBadge(g.data());
    }
    if (filterGuard && !touchedGraphs.contains(filterGuard))
        updateLockBadge(filterGuard.data());

    return updated;
}

void RecalcManager::onTableDataModified(Table *t, const QString &colName)
{
    if (d_recalculating || d_mode == Disabled || !t)
        return;

    pruneDeadLinks();

    QVector<int> linkIds;
    linkIds.reserve(d_links.size());
    for (const RecalcLink &lnk : d_links) {
        if (lnk.srcTable != t || lnk.graph.isNull() || lnk.resultTable == t)
            continue;
        linkIds.append(lnk.id);
    }

    QVector<QPointer<Graph>> touchedGraphs;
    for (int id : linkIds) {
        const int idx = findLinkIndexById(id);
        if (idx < 0 || d_links[idx].srcTable != t || d_links[idx].graph.isNull()
            || d_links[idx].resultTable == t)
            continue;

        QPointer<Graph> g = d_links[idx].graph;
        if (!colName.isEmpty() && g)
            g->updateCurvesData(t, colName);

        const int curIdx = findLinkIndexById(id);
        if (curIdx < 0 || !g)
            continue;

        if (d_mode == Manual) {
            d_links[curIdx].dirty = true;
            if (!touchedGraphs.contains(g))
                touchedGraphs.append(g);
        } else if (d_mode == Auto) {
            if (executeLinkById(id)) {
                const int postIdx = findLinkIndexById(id);
                if (postIdx >= 0)
                    d_links[postIdx].dirty = false;
            }
            if (g && !touchedGraphs.contains(g))
                touchedGraphs.append(g);
        }
    }

    for (const QPointer<Graph> &g : touchedGraphs) {
        if (g)
            updateLockBadge(g.data());
    }
}

void RecalcManager::onGraphDestroyed(QObject *obj)
{
    for (int i = d_links.size() - 1; i >= 0; --i) {
        if (d_links[i].graph.isNull() || static_cast<QObject *>(d_links[i].graph.data()) == obj)
            d_links.removeAt(i);
    }
}

bool RecalcManager::executeLink(RecalcLink &link)
{
    if (d_recalculating)
        return false;

    ApplicationWindow *app = link.app ? link.app.data() : d_app.data();
    Graph *g = link.graph.data();
    Table *srcTbl = link.srcTable.data();
    if (!app || !g || !srcTbl)
        return false;

    QScopedValueRollback<bool> recalcGuard(d_recalculating, true);
    QScopedValueRollback<bool> logGuard(app->writeFitResultsToLog, false);
    QScopedValueRollback<bool> pasteGuard(app->pasteFitResultsToPlot, false);

    // Refresh all DataCurves on g backed by srcTbl so Fit/Filter sees up-to-date X/Y values
    for (int c = 0; c < srcTbl->numCols(); ++c)
        g->updateCurvesData(srcTbl, srcTbl->colName(c));

    QString srcCurve = resolveGraphCurveTitle(g, link.srcCurveTitle);
    if (srcCurve.isEmpty() || !g->curve(srcCurve))
        return false;
    link.srcCurveTitle = srcCurve;

    // Preserve previous output curve pen/brush and remove old output curve(s) so curves don't pile up
    bool hasSavedPen = false;
    QPen savedPen;
    Table *orphanHiddenTable = nullptr;

    if (!link.outCurveTitle.isEmpty()) {
        if (QwtPlotCurve *oldOut = g->curve(link.outCurveTitle)) {
            savedPen = oldOut->pen();
            hasSavedPen = true;
            if (auto *oldDc = dynamic_cast<DataCurve *>(oldOut)) {
                Table *bt = oldDc->table();
                const bool isAutoSignalTable =
                        (link.op == OpIIRFilter || link.op == OpSavGolDeriv
                         || link.op == OpWaveletDenoise);
                if (bt && bt != srcTbl && bt != link.resultTable
                    && (app->hidden(bt) || isAutoSignalTable)) {
                    orphanHiddenTable = bt;
                }
            }
            g->removeCurve(link.outCurveTitle);
        }
    }
    for (const QString &extraTitle : link.extraCurveTitles) {
        if (!extraTitle.isEmpty() && g->curve(extraTitle)) {
            Table *extraOrphan = nullptr;
            if (auto *edc = dynamic_cast<DataCurve *>(g->curve(extraTitle))) {
                Table *bt = edc->table();
                if (bt && bt != srcTbl && bt != link.resultTable && app->hidden(bt))
                    extraOrphan = bt;
            }
            g->removeCurve(extraTitle);
            if (extraOrphan) {
                extraOrphan->askOnCloseEvent(false);
                app->closeWindow(extraOrphan);
            }
        }
    }
    link.extraCurveTitles.clear();

    if (orphanHiddenTable) {
        orphanHiddenTable->askOnCloseEvent(false);
        app->closeWindow(orphanHiddenTable);
    }

    const QStringList beforeCurves = g->curvesList();
    QStringList fitParamNames;
    std::vector<double> fitVals;
    std::vector<double> fitErrs;
    double fitR2 = 0.0;

    switch (link.op) {
    case OpLinearFit: {
        LinearFit fitter(app, g, srcCurve);
        if (fitter.error())
            return false;
        int pts = link.params.value(QStringLiteral("points"),
                                    app->d_2_linear_fit_points ? 2 : std::max(2, app->fitPoints))
                          .toInt();
        bool genFn = link.params.value(QStringLiteral("generateFunction"), true).toBool();
        fitter.scaleErrors(app->fit_scale_errors);
        fitter.setOutputPrecision(app->fit_output_precision);
        fitter.generateFunction(genFn, std::max(2, pts));
        fitter.fit();
        fitParamNames = QStringList{ QStringLiteral("B"), QStringLiteral("A") };
        fitVals = fitter.results();
        fitErrs = fitter.errors();
        fitR2 = fitter.rSquare();
        break;
    }
    case OpPolyFit: {
        int order = std::max(1, link.params.value(QStringLiteral("order"), 2).toInt());
        QString cTitle = srcCurve;
        PolynomialFit fitter(app, g, cTitle, order, false);
        if (fitter.error())
            return false;
        int pts = std::max(2, link.params.value(QStringLiteral("points"), app->fitPoints).toInt());
        bool genFn = link.params.value(QStringLiteral("generateFunction"), true).toBool();
        fitter.scaleErrors(app->fit_scale_errors);
        fitter.setOutputPrecision(app->fit_output_precision);
        fitter.generateFunction(genFn, pts);
        fitter.fit();
        fitParamNames = PolynomialFit::generateParameterList(order);
        fitVals = fitter.results();
        fitErrs = fitter.errors();
        fitR2 = fitter.rSquare();
        break;
    }
    case OpNonLinearBuiltIn: {
        NonLinearFit fitter(app, g, srcCurve);
        if (fitter.error())
            return false;
        QString modelName =
                link.params.value(QStringLiteral("model"), QStringLiteral("GaussAmp")).toString();
        if (!fitter.setBuiltInModel(modelName)) {
            QString formula = link.params.value(QStringLiteral("formula")).toString();
            QStringList pList = link.params.value(QStringLiteral("paramNames")).toStringList();
            if (!formula.isEmpty() && !pList.isEmpty()) {
                fitter.setParametersList(pList);
                fitter.setFormula(formula);
            } else {
                fitter.setBuiltInModel(QStringLiteral("GaussAmp"));
            }
        }
        fitter.guessInitialValues();
        fitter.scaleErrors(app->fit_scale_errors);
        fitter.setOutputPrecision(app->fit_output_precision);
        int pts = std::max(20, link.params.value(QStringLiteral("points"), app->fitPoints).toInt());
        fitter.generateFunction(link.params.value(QStringLiteral("generateFunction"), true).toBool(),
                                pts);
        fitter.fit();
        fitParamNames = NonLinearFit::builtInModelParameters(modelName);
        fitVals = fitter.results();
        fitErrs = fitter.errors();
        fitR2 = fitter.rSquare();
        break;
    }
    case OpMultiPeakFit: {
        int profile = link.params.value(QStringLiteral("profile"), int(MultiPeakFit::Gauss)).toInt();
        int peaks = std::max(1, link.params.value(QStringLiteral("peaks"), 2).toInt());
        MultiPeakFit fitter(app, g, static_cast<MultiPeakFit::PeakProfile>(profile), peaks);
        fitter.enablePeakCurves(link.params.value(QStringLiteral("peakCurves"), false).toBool());
        if (!fitter.setDataFromCurve(srcCurve))
            return false;
        double prom = link.params.value(QStringLiteral("prominence"), 0.08).toDouble();
        fitter.autoDetectPeaks(peaks, prom);
        fitter.scaleErrors(app->fit_scale_errors);
        fitter.setOutputPrecision(app->fit_output_precision);
        fitter.generateFunction(true,
                                std::max(50, link.params.value(QStringLiteral("points"), 200).toInt()));
        fitter.fit();
        fitParamNames = MultiPeakFit::generateParameterList(peaks);
        fitVals = fitter.results();
        fitErrs = fitter.errors();
        fitR2 = fitter.rSquare();
        break;
    }
    case OpSmooth: {
        int method = link.params
                             .value(QStringLiteral("method"), int(SmoothFilter::SavitzkyGolay))
                             .toInt();
        SmoothFilter sf(app, g, srcCurve, method);
        if (sf.error())
            return false;
        int pts = std::max(3, link.params.value(QStringLiteral("points"), 5).toInt());
        int leftPts = link.params.value(QStringLiteral("leftPoints"), pts / 2).toInt();
        sf.setSmoothPoints(pts, leftPts);
        sf.setPolynomOrder(
                std::clamp(link.params.value(QStringLiteral("polyOrder"), 2).toInt(), 1, pts - 1));
        sf.run();
        break;
    }
    case OpNormalize: {
        if (!createNormalizedCurveOnGraph(app, g, srcCurve, link.params))
            return false;
        break;
    }
    case OpDifferentiate: {
        Differentiation diff(app, g, srcCurve);
        if (diff.error())
            return false;
        diff.run();
        break;
    }
    case OpRobustFit: {
        int method = link.params
                             .value(QStringLiteral("method"),
                                    int(PiecewiseAndRobustFit::TukeyBisquare))
                             .toInt();
        double tau = link.params.value(QStringLiteral("quantileTau"), 0.5).toDouble();
        auto res = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                app, g, srcCurve,
                static_cast<PiecewiseAndRobustFit::RobustMethod>(method), tau, false);
        if (!res.converged || !res.fittedCurve)
            return false;
        fitParamNames = QStringList{ QStringLiteral("Intercept"), QStringLiteral("Slope") };
        fitVals = { res.intercept, res.slope };
        fitErrs = { res.interceptSE, res.slopeSE };
        fitR2 = 0.0;
        if (res.reportTable) {
            if (!link.resultTable) {
                link.resultTable = res.reportTable;
            } else if (res.reportTable != link.resultTable) {
                res.reportTable->askOnCloseEvent(false);
                app->closeWindow(res.reportTable);
            }
        }
        break;
    }
    case OpPiecewiseFit: {
        int segments = std::clamp(
                link.params
                        .value(QStringLiteral("segments"),
                               link.params.value(QStringLiteral("numSegments"), 2))
                        .toInt(),
                2, 3);
        auto res = PiecewiseAndRobustFit::fitPiecewiseLinear(app, g, srcCurve, segments, false);
        if (!res.converged || !res.fittedCurve)
            return false;
        fitParamNames.clear();
        fitVals.clear();
        fitParamNames << QStringLiteral("Intercept0");
        fitVals.push_back(res.intercept0);
        for (int i = 0; i < res.breakpointsX.size(); ++i) {
            fitParamNames << QStringLiteral("BreakpointX%1").arg(i + 1);
            fitVals.push_back(res.breakpointsX[i]);
        }
        for (int i = 0; i < res.slopes.size(); ++i) {
            fitParamNames << QStringLiteral("Slope%1").arg(i + 1);
            fitVals.push_back(res.slopes[i]);
        }
        fitErrs.assign(fitVals.size(), 0.0);
        fitR2 = res.rSquare;
        if (res.reportTable) {
            if (!link.resultTable) {
                link.resultTable = res.reportTable;
            } else if (res.reportTable != link.resultTable) {
                res.reportTable->askOnCloseEvent(false);
                app->closeWindow(res.reportTable);
            }
        }
        break;
    }
    case OpIIRFilter: {
        int design = link.params
                             .value(QStringLiteral("design"), int(AdvancedSignalProc::Butterworth))
                             .toInt();
        int pass = link.params.value(QStringLiteral("pass"), int(AdvancedSignalProc::LowPass))
                           .toInt();
        int order = std::clamp(link.params.value(QStringLiteral("order"), 4).toInt(), 1, 12);
        double fLow = link.params.value(QStringLiteral("cutoffLowHz"), 10.0).toDouble();
        double fHigh = link.params.value(QStringLiteral("cutoffHighHz"), 25.0).toDouble();
        bool zeroPhase = link.params.value(QStringLiteral("zeroPhase"), true).toBool();
        double rippleDb = link.params.value(QStringLiteral("rippleDb"), 1.0).toDouble();
        if (!AdvancedSignalProc::iirFilter(
                    app, g, srcCurve, static_cast<AdvancedSignalProc::IirDesign>(design),
                    static_cast<AdvancedSignalProc::PassType>(pass), order, fLow, fHigh,
                    zeroPhase, rippleDb)) {
            return false;
        }
        break;
    }
    case OpSavGolDeriv: {
        int derivOrder = std::clamp(link.params.value(QStringLiteral("derivOrder"), 1).toInt(), 1, 4);
        int windowPoints = std::max(3, link.params.value(QStringLiteral("windowPoints"), 9).toInt());
        int polyOrder = std::max(1, link.params.value(QStringLiteral("polyOrder"), 3).toInt());
        if (!AdvancedSignalProc::savitzkyGolayDerivative(app, g, srcCurve, derivOrder, windowPoints,
                                                         polyOrder)) {
            return false;
        }
        break;
    }
    case OpWaveletDenoise: {
        int wtype = link.params
                            .value(QStringLiteral("wavelet"), int(AdvancedSignalProc::Daubechies4))
                            .toInt();
        int levels = std::clamp(link.params.value(QStringLiteral("levels"), 4).toInt(), 1, 10);
        bool soft = link.params.value(QStringLiteral("softThreshold"), true).toBool();
        double scale = link.params.value(QStringLiteral("thresholdScale"), 1.0).toDouble();
        if (!AdvancedSignalProc::waveletDenoiseDWT(
                    app, g, srcCurve, static_cast<AdvancedSignalProc::WaveletType>(wtype), levels,
                    soft, scale)) {
            return false;
        }
        break;
    }
    }

    const QStringList afterCurves = g->curvesList();
    QStringList addedCurves;
    for (const QString &cn : afterCurves) {
        if (!beforeCurves.contains(cn))
            addedCurves.append(cn);
    }

    if (!addedCurves.isEmpty()) {
        QString primaryTitle = addedCurves.first();
        QwtPlotCurve *newPrimary = g->curve(primaryTitle);
        if (newPrimary) {
            if (!link.desiredOutTitle.isEmpty() && link.desiredOutTitle != primaryTitle
                && g->curve(link.desiredOutTitle) == nullptr
                && dynamic_cast<FunctionCurve *>(newPrimary) != nullptr) {
                newPrimary->setTitle(link.desiredOutTitle);
                primaryTitle = link.desiredOutTitle;
            }
            if (hasSavedPen)
                newPrimary->setPen(savedPen);
        }
        link.outCurveTitle = primaryTitle;
        for (int k = 1; k < addedCurves.size(); ++k)
            link.extraCurveTitles.append(addedCurves[k]);
    }

    if (link.resultTable && !fitVals.empty())
        updateFitSummaryTable(link.resultTable.data(), fitParamNames, fitVals, fitErrs, fitR2);

    g->replot();
    return true;
}

void RecalcManager::updateLockBadge(Graph *g)
{
    if (!g || !g->plotWidget() || !g->plotWidget()->canvas())
        return;

    QWidget *canvas = g->plotWidget()->canvas();
    auto *badge = canvas->findChild<QPushButton *>(QStringLiteral("ToriGraphRecalcLockBadge"),
                                                   Qt::FindDirectChildrenOnly);

    const int nActive = activeLinkCount(g);
    if (nActive <= 0 || d_mode == Disabled) {
        if (badge)
            badge->hide();
        g->setProperty("recalcLockBadgeText", QString());
        g->setProperty("recalcLockState", QStringLiteral("Disabled"));
        return;
    }

    if (!badge) {
        badge = new QPushButton(canvas);
        badge->setObjectName(QStringLiteral("ToriGraphRecalcLockBadge"));
        badge->setFocusPolicy(Qt::NoFocus);
        badge->setCursor(Qt::PointingHandCursor);
        QPointer<Graph> safeG(g);
        QObject::connect(badge, &QPushButton::clicked, this, [this, safeG]() {
            if (!safeG)
                return;
            Graph *grp = safeG.data();
            if (hasDirtyLinks(grp)) {
                recalculateAll(grp);
                return;
            }
            QMenu menu(grp);
            QAction *actNow = menu.addAction(QObject::tr("Recalculate Layer Now"));
            menu.addSeparator();
            QAction *actAuto = menu.addAction(QObject::tr("Auto Recalculate (Green Lock)"));
            actAuto->setCheckable(true);
            actAuto->setChecked(d_mode == Auto);
            QAction *actManual = menu.addAction(QObject::tr("Manual Recalculate"));
            actManual->setCheckable(true);
            actManual->setChecked(d_mode == Manual);
            QAction *actDisable = menu.addAction(QObject::tr("Disable Recalculation"));
            actDisable->setCheckable(true);
            actDisable->setChecked(d_mode == Disabled);
            menu.addSeparator();
            QAction *actClear = menu.addAction(QObject::tr("Remove Recalculation Links on Graph"));

            QAction *picked = menu.exec(QCursor::pos());
            if (picked == actNow)
                recalculateAll(grp);
            else if (picked == actAuto)
                setRecalcMode(Auto);
            else if (picked == actManual)
                setRecalcMode(Manual);
            else if (picked == actDisable)
                setRecalcMode(Disabled);
            else if (picked == actClear)
                removeLinksForGraph(grp);
        });
    }

    const bool dirty = hasDirtyLinks(g);
    QString text;
    QString style;
    QString state;

    if (dirty) {
        text = QStringLiteral("[Recalc Pending !]");
        state = QStringLiteral("Pending");
        style = QStringLiteral(
                "QPushButton { background-color: rgba(230, 175, 25, 235); color: #181818; "
                "font-weight: bold; font-size: 9pt; border: 1px solid #8a6400; "
                "border-radius: 4px; padding: 2px 6px; }");
    } else if (d_mode == Auto) {
        text = QStringLiteral("[Auto \xE2\x9C\x93]");
        state = QStringLiteral("Auto");
        style = QStringLiteral(
                "QPushButton { background-color: rgba(34, 139, 34, 225); color: #ffffff; "
                "font-weight: bold; font-size: 9pt; border: 1px solid #145a14; "
                "border-radius: 4px; padding: 2px 6px; }");
    } else {
        text = QStringLiteral("[Manual \xE2\x9C\x93]");
        state = QStringLiteral("Manual");
        style = QStringLiteral(
                "QPushButton { background-color: rgba(65, 105, 155, 220); color: #ffffff; "
                "font-weight: bold; font-size: 9pt; border: 1px solid #2b4a70; "
                "border-radius: 4px; padding: 2px 6px; }");
    }

    badge->setText(text);
    badge->setToolTip(QObject::tr("Dynamic Recalculation (%1 active link(s))").arg(nActive));
    badge->setStyleSheet(style);
    badge->adjustSize();
    badge->move(8, 8);
    badge->show();
    badge->raise();

    g->setProperty("recalcLockBadgeText", text);
    g->setProperty("recalcLockState", state);
}

void RecalcManager::repeatForAllCurves(ApplicationWindow *app, Graph *g, OpType op,
                                       const QVariantMap &params)
{
    RecalcManager *mgr = instance(app);
    ApplicationWindow *effApp = app ? app : mgr->d_app.data();
    if (!effApp || !g)
        return;

    const QList<QwtPlotCurve *> existingFits = g->fitCurvesList();
    QStringList existingOuts;
    for (const RecalcLink &lnk : mgr->d_links) {
        if (lnk.graph == g && !lnk.outCurveTitle.isEmpty())
            existingOuts.append(lnk.outCurveTitle);
    }

    const QStringList candidates = g->analysableCurvesList();
    QStringList sourceCurves;
    for (const QString &ct : candidates) {
        QwtPlotCurve *cv = g->curve(ct);
        if (!cv || existingFits.contains(cv) || existingOuts.contains(ct))
            continue;
        if (dynamic_cast<FunctionCurve *>(cv) != nullptr)
            continue;
        sourceCurves.append(ct);
    }

    static const QColor kBatchColors[] = { QColor(215, 35, 35),  QColor(25, 95, 195),
                                           QColor(25, 145, 70),  QColor(155, 55, 185),
                                           QColor(225, 125, 15), QColor(20, 155, 170) };

    for (int idx = 0; idx < sourceCurves.size(); ++idx) {
        const QString &srcTitle = sourceCurves[idx];
        auto *srcDc = dynamic_cast<DataCurve *>(g->curve(srcTitle));
        Table *srcTbl = srcDc ? srcDc->table() : nullptr;

        const QStringList before = g->curvesList();
        const QColor outColor = kBatchColors[idx % 6];

        switch (op) {
        case OpLinearFit: {
            LinearFit fitter(effApp, g, srcTitle);
            if (!fitter.error()) {
                fitter.setColor(outColor);
                fitter.generateFunction(true,
                                        effApp->d_2_linear_fit_points
                                                ? 2
                                                : std::max(2, effApp->fitPoints));
                fitter.fit();
            }
            break;
        }
        case OpPolyFit: {
            int order = std::max(1, params.value(QStringLiteral("order"), 2).toInt());
            QString cTitle = srcTitle;
            PolynomialFit fitter(effApp, g, cTitle, order, false);
            if (!fitter.error()) {
                fitter.setColor(outColor);
                fitter.generateFunction(true, std::max(2, effApp->fitPoints));
                fitter.fit();
            }
            break;
        }
        case OpNonLinearBuiltIn: {
            NonLinearFit fitter(effApp, g, srcTitle);
            if (!fitter.error()) {
                fitter.setColor(outColor);
                fitter.setBuiltInModel(
                        params.value(QStringLiteral("model"), QStringLiteral("GaussAmp")).toString());
                fitter.guessInitialValues();
                fitter.generateFunction(true, std::max(20, effApp->fitPoints));
                fitter.fit();
            }
            break;
        }
        case OpMultiPeakFit: {
            int profile = params.value(QStringLiteral("profile"), int(MultiPeakFit::Gauss)).toInt();
            int peaks = std::max(1, params.value(QStringLiteral("peaks"), 2).toInt());
            MultiPeakFit fitter(effApp, g, static_cast<MultiPeakFit::PeakProfile>(profile), peaks);
            fitter.enablePeakCurves(false);
            if (fitter.setDataFromCurve(srcTitle)) {
                fitter.setColor(outColor);
                fitter.autoDetectPeaks(peaks, 0.08);
                fitter.generateFunction(true, 200);
                fitter.fit();
            }
            break;
        }
        case OpSmooth: {
            int method =
                    params.value(QStringLiteral("method"), int(SmoothFilter::SavitzkyGolay)).toInt();
            SmoothFilter sf(effApp, g, srcTitle, method);
            if (!sf.error()) {
                sf.setColor(outColor);
                int pts = std::max(3, params.value(QStringLiteral("points"), 5).toInt());
                sf.setSmoothPoints(pts, pts / 2);
                sf.setPolynomOrder(std::clamp(params.value(QStringLiteral("polyOrder"), 2).toInt(),
                                              1, pts - 1));
                sf.run();
            }
            break;
        }
        case OpNormalize: {
            if (QwtPlotCurve *nc = createNormalizedCurveOnGraph(effApp, g, srcTitle, params))
                nc->setPen(QPen(outColor, 2));
            break;
        }
        case OpDifferentiate: {
            Differentiation diff(effApp, g, srcTitle);
            if (!diff.error()) {
                diff.setColor(outColor);
                diff.run();
            }
            break;
        }
        case OpRobustFit: {
            int method = params.value(QStringLiteral("method"),
                                      int(PiecewiseAndRobustFit::TukeyBisquare))
                                 .toInt();
            double tau = params.value(QStringLiteral("quantileTau"), 0.5).toDouble();
            auto res = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                    effApp, g, srcTitle,
                    static_cast<PiecewiseAndRobustFit::RobustMethod>(method), tau, false);
            if (res.fittedCurve)
                res.fittedCurve->setPen(QPen(outColor, 2));
            break;
        }
        case OpPiecewiseFit: {
            int segments = std::clamp(
                    params.value(QStringLiteral("segments"),
                                 params.value(QStringLiteral("numSegments"), 2))
                            .toInt(),
                    2, 3);
            auto res = PiecewiseAndRobustFit::fitPiecewiseLinear(effApp, g, srcTitle, segments,
                                                                 false);
            if (res.fittedCurve)
                res.fittedCurve->setPen(QPen(outColor, 2));
            break;
        }
        case OpIIRFilter: {
            int design = params.value(QStringLiteral("design"),
                                      int(AdvancedSignalProc::Butterworth))
                                 .toInt();
            int pass = params.value(QStringLiteral("pass"), int(AdvancedSignalProc::LowPass))
                               .toInt();
            int order = std::clamp(params.value(QStringLiteral("order"), 4).toInt(), 1, 12);
            double fLow = params.value(QStringLiteral("cutoffLowHz"), 10.0).toDouble();
            double fHigh = params.value(QStringLiteral("cutoffHighHz"), 25.0).toDouble();
            bool zeroPhase = params.value(QStringLiteral("zeroPhase"), true).toBool();
            double rippleDb = params.value(QStringLiteral("rippleDb"), 1.0).toDouble();
            if (QwtPlotCurve *fc = AdvancedSignalProc::iirFilter(
                        effApp, g, srcTitle, static_cast<AdvancedSignalProc::IirDesign>(design),
                        static_cast<AdvancedSignalProc::PassType>(pass), order, fLow, fHigh,
                        zeroPhase, rippleDb)) {
                fc->setPen(QPen(outColor, 2));
            }
            break;
        }
        case OpSavGolDeriv: {
            int derivOrder = std::clamp(params.value(QStringLiteral("derivOrder"), 1).toInt(), 1, 4);
            int windowPoints = std::max(3, params.value(QStringLiteral("windowPoints"), 9).toInt());
            int polyOrder = std::max(1, params.value(QStringLiteral("polyOrder"), 3).toInt());
            if (QwtPlotCurve *sc = AdvancedSignalProc::savitzkyGolayDerivative(
                        effApp, g, srcTitle, derivOrder, windowPoints, polyOrder)) {
                sc->setPen(QPen(outColor, 2));
            }
            break;
        }
        case OpWaveletDenoise: {
            int wtype = params.value(QStringLiteral("wavelet"),
                                     int(AdvancedSignalProc::Daubechies4))
                                .toInt();
            int levels = std::clamp(params.value(QStringLiteral("levels"), 4).toInt(), 1, 10);
            bool soft = params.value(QStringLiteral("softThreshold"), true).toBool();
            double scale = params.value(QStringLiteral("thresholdScale"), 1.0).toDouble();
            if (QwtPlotCurve *wc = AdvancedSignalProc::waveletDenoiseDWT(
                        effApp, g, srcTitle,
                        static_cast<AdvancedSignalProc::WaveletType>(wtype), levels, soft, scale)) {
                wc->setPen(QPen(outColor, 2));
            }
            break;
        }
        }

        const QStringList after = g->curvesList();
        QString newOut;
        for (const QString &cn : after) {
            if (!before.contains(cn)) {
                newOut = cn;
                break;
            }
        }
        if (!newOut.isEmpty() && srcTbl && mgr->recalcMode() != Disabled)
            mgr->registerLink(effApp, g, srcTbl, srcTitle, newOut, op, params);
    }

    g->replot();
}

// ============================================================================
// DialogThemeManager Implementation
// ============================================================================

void DialogThemeManager::saveTheme(const QString &toolId, const QString &themeName,
                                   const QVariantMap &settings)
{
    const QString tid = toolId.trimmed().isEmpty() ? QStringLiteral("General") : toolId.trimmed();
    const QString tname = themeName.trimmed();
    if (tname.isEmpty())
        return;

    themeMemoryCache()[tid][tname] = settings;

    QByteArray encodedData;
    QDataStream ds(&encodedData, QIODevice::WriteOnly);
    ds.setVersion(QDataStream::Qt_5_12);
    ds << settings;

    const QString encTool = QString::fromLatin1(QUrl::toPercentEncoding(tid));
    const QString encName = QString::fromLatin1(QUrl::toPercentEncoding(tname));

    QSettings &qs = ApplicationWindow::getSettings();
    qs.beginGroup(QStringLiteral("OriginProDialogThemes"));
    qs.beginGroup(encTool);
    qs.setValue(encName, encodedData);
    qs.endGroup();
    qs.endGroup();
    qs.sync();
}

QVariantMap DialogThemeManager::loadTheme(const QString &toolId, const QString &themeName)
{
    const QString tid = toolId.trimmed().isEmpty() ? QStringLiteral("General") : toolId.trimmed();
    const QString tname = themeName.trimmed();
    const auto &cache = themeMemoryCache();
    auto itTool = cache.constFind(tid);
    if (itTool == cache.constEnd())
        return QVariantMap();
    return itTool->value(tname);
}

QStringList DialogThemeManager::themeNames(const QString &toolId)
{
    const auto &cache = themeMemoryCache();
    QStringList names;
    if (toolId.trimmed().isEmpty()) {
        for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
            for (auto kt = it.value().cbegin(); kt != it.value().cend(); ++kt)
                names.append(it.key() + QStringLiteral("/") + kt.key());
        }
    } else {
        auto it = cache.constFind(toolId.trimmed());
        if (it != cache.constEnd())
            names = it.value().keys();
    }
    names.sort();
    return names;
}

void DialogThemeManager::deleteTheme(const QString &toolId, const QString &themeName)
{
    const QString tid = toolId.trimmed().isEmpty() ? QStringLiteral("General") : toolId.trimmed();
    const QString tname = themeName.trimmed();
    if (tname.isEmpty())
        return;

    auto &cache = themeMemoryCache();
    if (cache.contains(tid)) {
        cache[tid].remove(tname);
        if (cache[tid].isEmpty())
            cache.remove(tid);
    }

    const QString encTool = QString::fromLatin1(QUrl::toPercentEncoding(tid));
    const QString encName = QString::fromLatin1(QUrl::toPercentEncoding(tname));

    QSettings &qs = ApplicationWindow::getSettings();
    qs.beginGroup(QStringLiteral("OriginProDialogThemes"));
    qs.beginGroup(encTool);
    qs.remove(encName);
    qs.endGroup();
    qs.endGroup();
    qs.sync();
}

void DialogThemeManager::showManageThemesDialog(QWidget *parent, const QString &toolId)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Manage Analysis Dialog Themes"));
    dlg.setMinimumSize(500, 380);
    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);

    QHBoxLayout *topLay = new QHBoxLayout();
    topLay->addWidget(new QLabel(QObject::tr("Filter by Tool:"), &dlg));
    QComboBox *toolCombo = new QComboBox(&dlg);
    toolCombo->addItem(QObject::tr("(All Tools)"), QString());

    auto populateToolCombo = [&]() {
        QString curData = toolCombo->currentData().toString();
        toolCombo->blockSignals(true);
        toolCombo->clear();
        toolCombo->addItem(QObject::tr("(All Tools)"), QString());
        const auto &cache = themeMemoryCache();
        QStringList tids = cache.keys();
        if (!toolId.isEmpty() && !tids.contains(toolId))
            tids.append(toolId);
        tids.sort();
        for (const QString &t : tids) {
            toolCombo->addItem(t, t);
            if ((!curData.isEmpty() && t == curData) || (curData.isEmpty() && t == toolId))
                toolCombo->setCurrentIndex(toolCombo->count() - 1);
        }
        toolCombo->blockSignals(false);
    };
    populateToolCombo();
    topLay->addWidget(toolCombo, 1);
    mainLay->addLayout(topLay);

    QListWidget *themeList = new QListWidget(&dlg);
    mainLay->addWidget(themeList, 1);

    QPlainTextEdit *previewBox = new QPlainTextEdit(&dlg);
    previewBox->setReadOnly(true);
    previewBox->setMaximumHeight(110);
    previewBox->setPlaceholderText(QObject::tr("Select a saved theme to inspect its parameters..."));
    mainLay->addWidget(previewBox);

    auto refreshThemeList = [&]() {
        themeList->clear();
        previewBox->clear();
        QString filterTid = toolCombo->currentData().toString();
        const auto &cache = themeMemoryCache();
        for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
            if (!filterTid.isEmpty() && it.key() != filterTid)
                continue;
            for (auto kt = it.value().cbegin(); kt != it.value().cend(); ++kt) {
                auto *item = new QListWidgetItem(
                        QStringLiteral("[%1] %2").arg(it.key(), kt.key()), themeList);
                item->setData(Qt::UserRole, it.key());
                item->setData(Qt::UserRole + 1, kt.key());
            }
        }
    };
    refreshThemeList();

    QObject::connect(toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&](int) { refreshThemeList(); });

    QObject::connect(themeList, &QListWidget::currentItemChanged,
                     [&](QListWidgetItem *cur, QListWidgetItem *) {
                         previewBox->clear();
                         if (!cur)
                             return;
                         QString tid = cur->data(Qt::UserRole).toString();
                         QString tname = cur->data(Qt::UserRole + 1).toString();
                         QVariantMap map = loadTheme(tid, tname);
                         QStringList lines;
                         for (auto it = map.cbegin(); it != map.cend(); ++it)
                             lines << QStringLiteral("%1 = %2").arg(it.key(), it.value().toString());
                         previewBox->setPlainText(lines.join(QStringLiteral("\n")));
                     });

    QHBoxLayout *actLay = new QHBoxLayout();
    QPushButton *dupBtn = new QPushButton(QObject::tr("Duplicate / Rename..."), &dlg);
    QPushButton *delBtn = new QPushButton(QObject::tr("Delete Selected"), &dlg);
    actLay->addWidget(dupBtn);
    actLay->addWidget(delBtn);
    actLay->addStretch();
    mainLay->addLayout(actLay);

    QObject::connect(dupBtn, &QPushButton::clicked, [&]() {
        QListWidgetItem *cur = themeList->currentItem();
        if (!cur)
            return;
        QString tid = cur->data(Qt::UserRole).toString();
        QString oldName = cur->data(Qt::UserRole + 1).toString();
        bool ok = false;
        QString newName = QInputDialog::getText(&dlg, QObject::tr("Duplicate Theme"),
                                                QObject::tr("New theme name:"), QLineEdit::Normal,
                                                oldName + QStringLiteral("_Copy"), &ok);
        if (ok && !newName.trimmed().isEmpty()) {
            saveTheme(tid, newName.trimmed(), loadTheme(tid, oldName));
            populateToolCombo();
            refreshThemeList();
        }
    });

    QObject::connect(delBtn, &QPushButton::clicked, [&]() {
        QListWidgetItem *cur = themeList->currentItem();
        if (!cur)
            return;
        QString tid = cur->data(Qt::UserRole).toString();
        QString tname = cur->data(Qt::UserRole + 1).toString();
        deleteTheme(tid, tname);
        populateToolCombo();
        refreshThemeList();
    });

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainLay->addWidget(btns);

    dlg.exec();
}
