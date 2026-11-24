/***************************************************************************
        File                 : ApplicationWindow.cpp
        Project              : SciDAVis
    Description          : Shared internal helpers for split ApplicationWindow translation units
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
#ifndef APPLICATION_WINDOW_INTERNAL_H
#define APPLICATION_WINDOW_INTERNAL_H

#include "ApplicationWindow.h"
#include "Graph.h"
#include "Graph3D.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "RecalcEngine.h"
#include "Table.h"

#include <QAction>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QUndoStack>
#include <functional>

namespace ApplicationWindowInternal {

bool &creatingHiddenTableFlag();
QTranslator *sharedBuiltinJaTranslator();
void registerApplicationWindowJapaneseStrings();
QSet<const ApplicationWindow *> &applicationWindowsBeingDestroyed();
int &renamedLookupSuppressionDepth();
void applyWorksheetMetadataToGraph(Graph *ag, Table *fallbackTable = nullptr,
                                   const QStringList &fallbackCols = QStringList(),
                                   int style = -1);

class PeakAnchorEventFilter final : public QObject
{
public:
    std::function<bool(QObject *, QEvent *)> handler;
    std::function<void()> onTeardown;

    explicit PeakAnchorEventFilter(QObject *parent = nullptr) : QObject(parent) {}
    ~PeakAnchorEventFilter() override
    {
        if (onTeardown)
            onTeardown();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (handler)
            return handler(watched, event);
        return QObject::eventFilter(watched, event);
    }
};

inline bool isBeingDestroyed(const ApplicationWindow *app)
{
    const QSet<const ApplicationWindow *> &s = applicationWindowsBeingDestroyed();
    return !s.isEmpty() && s.contains(app);
}

struct ScopedUndoReplayGuard
{
    QPointer<ApplicationWindow> app;
    explicit ScopedUndoReplayGuard(ApplicationWindow *a) : app(a)
    {
        Table::beginUndoRedoReplay();
    }
    ~ScopedUndoReplayGuard()
    {
        Table::endUndoRedoReplay();
        if (!Table::isUndoRedoReplaying() && app && !isBeingDestroyed(app.data())) {
            RecalcManager *rm = RecalcManager::instance(app.data());
            if (rm && rm->recalcMode() == RecalcManager::Auto && rm->hasDirtyLinks()) {
                rm->recalculateAll();
                app->updateUndoRedoActionsForActiveWindow();
            }
        }
    }
    ScopedUndoReplayGuard(const ScopedUndoReplayGuard &) = delete;
    ScopedUndoReplayGuard &operator=(const ScopedUndoReplayGuard &) = delete;
};

inline bool splitGraph3DTableAssoc(const QString &assoc, QString &xSpec, QString &ySpec, QString &zSpec,
                            bool &isXYZ)
{
    xSpec.clear();
    ySpec.clear();
    zSpec.clear();
    isXYZ = false;
    if (assoc.isEmpty())
        return false;

    const int xTagPos = assoc.indexOf(QLatin1String("(X),"));
    if (xTagPos <= 0)
        return false;

    xSpec = assoc.left(xTagPos);
    const QString restAfterX = assoc.mid(xTagPos + 4);

    if (restAfterX.endsWith(QLatin1String("(Z)"))) {
        const int yTagPos = restAfterX.indexOf(QLatin1String("(Y),"));
        if (yTagPos <= 0)
            return false;
        ySpec = restAfterX.left(yTagPos);
        zSpec = restAfterX.mid(yTagPos + 4, restAfterX.length() - (yTagPos + 4) - 3);
        if (xSpec.isEmpty() || ySpec.isEmpty() || zSpec.isEmpty())
            return false;
        isXYZ = true;
        return true;
    }

    if (restAfterX.endsWith(QLatin1String("(Y)"))) {
        ySpec = restAfterX.left(restAfterX.length() - 3);
        if (xSpec.isEmpty() || ySpec.isEmpty())
            return false;
        isXYZ = false;
        return true;
    }

    return false;
}

inline QString joinGraph3DTableAssoc(const QString &xSpec, const QString &ySpec, const QString &zSpec,
                              bool isXYZ)
{
    if (isXYZ) {
        return xSpec + QStringLiteral("(X),") + ySpec + QStringLiteral("(Y),") + zSpec
                + QStringLiteral("(Z)");
    }
    return xSpec + QStringLiteral("(X),") + ySpec + QStringLiteral("(Y)");
}

inline bool graph3DUsesColumnExact(Graph3D *g3, const QString &colFullName,
                            const Table *expectedTable = nullptr)
{
    if (!g3 || colFullName.isEmpty() || g3->userFunction() || g3->matrix())
        return false;
    Table *tbl = g3->getTable();
    if (tbl) {
        if (expectedTable && tbl != expectedTable)
            return false;
        if (!colFullName.startsWith(tbl->name() + QLatin1Char('_')))
            return false;
    }
    QString xSpec, ySpec, zSpec;
    bool isXYZ = false;
    if (!splitGraph3DTableAssoc(g3->formula(), xSpec, ySpec, zSpec, isXYZ))
        return false;
    if (xSpec == colFullName || ySpec == colFullName || (isXYZ && zSpec == colFullName))
        return true;
    if (tbl) {
        const QString bareLabel = colFullName.mid(tbl->name().length() + 1);
        if (!bareLabel.isEmpty()
            && (xSpec == bareLabel || ySpec == bareLabel || (isXYZ && zSpec == bareLabel))) {
            return true;
        }
    }
    return false;
}

struct ScopedDisableRenamedLookup
{
    ScopedDisableRenamedLookup() { ++renamedLookupSuppressionDepth(); }
    ~ScopedDisableRenamedLookup() { --renamedLookupSuppressionDepth(); }
};

inline void updateStatusBarForWindow(QLabel *statusLabel, QMdiSubWindow *w)
{
    if (!statusLabel)
        return;
    if (Table *tbl = qobject_cast<Table *>(w)) {
        statusLabel->setText(
                ApplicationWindow::tr(
                        "Worksheet: %1  (%2 rows × %3 cols)  —  Select columns & click Plot or press Ctrl+K")
                        .arg(tbl->name())
                        .arg(tbl->numRows())
                        .arg(tbl->numCols()));
    } else if (MultiLayer *ml = qobject_cast<MultiLayer *>(w)) {
        Graph *ag = ml->activeGraph();
        const int nCurves = ag ? ag->curves() : 0;
        statusLabel->setText(
                ApplicationWindow::tr(
                        "2D Graph: %1  (Layers: %2 · Curves: %3)  —  Click curve/axis to edit in Inspector")
                        .arg(ml->name())
                        .arg(ml->layers())
                        .arg(nCurves));
    } else if (Matrix *mat = qobject_cast<Matrix *>(w)) {
        statusLabel->setText(ApplicationWindow::tr("Matrix: %1  (%2 × %3)")
                                     .arg(mat->name())
                                     .arg(mat->numRows())
                                     .arg(mat->numCols()));
    } else if (MyWidget *mw = qobject_cast<MyWidget *>(w)) {
        statusLabel->setText(ApplicationWindow::tr("Active: %1").arg(mw->name()));
    } else {
        statusLabel->setText(
                ApplicationWindow::tr("Ready — Press Ctrl+K to search commands"));
    }
}

struct ModalMultiLayerUndoHelper
{
    QPointer<MultiLayer> ml;
    QString label;
    QString beforeSnap;
    int countBefore = 0;
    int indexBefore = 0;

    ModalMultiLayerUndoHelper(MultiLayer *target, const QString &undoLabel)
        : ml(target), label(undoLabel)
    {
        if (ml && !ml->isRestoringSnapshot()) {
            beforeSnap = ml->captureUndoSnapshot();
            if (QUndoStack *us = ml->undoStack()) {
                countBefore = us->count();
                indexBefore = us->index();
            }
        }
    }

    void commitIfChanged()
    {
        if (!ml || beforeSnap.isEmpty() || ml->isRestoringSnapshot())
            return;
        if (QUndoStack *us = ml->undoStack()) {
            if (us->count() != countBefore || us->index() != indexBefore)
                return;
        }
        if (ml->captureUndoSnapshot() != beforeSnap)
            ml->pushUndoSnapshot(label, beforeSnap);
    }
};

inline Graph *activeGraphForAnalysis(ApplicationWindow *app, QMdiArea &workspace, QAction *btnPointer,
                              bool requireCurves = true)
{
    QWidget *w = workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer")) {
        if (app && !ApplicationWindow::isNonInteractiveContext(app))
            app->showUxHint(ApplicationWindow::tr("Click or activate a 2D graph window first."));
        return nullptr;
    }

    MultiLayer *plot = static_cast<MultiLayer *>(w);
    if (plot->isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(app)) {
            QMessageBox::warning(
                    app, ApplicationWindow::tr("Warning"),
                    ApplicationWindow::tr("There are no plot layers available in this window!"));
        }
        if (btnPointer)
            btnPointer->setChecked(true);
        return nullptr;
    }

    Graph *g = static_cast<Graph *>(plot->activeGraph());
    if (!g)
        return nullptr;

    if (g->isPiePlot()) {
        if (!ApplicationWindow::isNonInteractiveContext(app)) {
            QMessageBox::warning(
                    app, ApplicationWindow::tr("Warning"),
                    ApplicationWindow::tr("This functionality is not available for pie plots!"));
        }
        if (btnPointer)
            btnPointer->setChecked(true);
        return nullptr;
    }

    if (requireCurves && !g->validCurvesDataSize()) {
        if (app && !ApplicationWindow::isNonInteractiveContext(app)) {
            app->showUxHint(
                    ApplicationWindow::tr("This graph has no plottable curve data yet."));
        }
        return nullptr;
    }

    return g;
}

} // namespace ApplicationWindowInternal

#endif // APPLICATION_WINDOW_INTERNAL_H
