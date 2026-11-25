#include "ole_test_common.h"
#include "Folder.h"
#include "PythonAutomationServer.h"
#include "RenameWindowDialog.h"
#include "ImportASCIIDialog.h"

// =============================================================================
// Suite 92: table / column renames reach graphs, graph undo snapshots, the project explorer and
// the formulas of every table, also on Undo / Redo of the rename.
//   92a sheet rename -> project Undo -> Redo: curve title, graph undo snapshot and explorer item
//       follow each step (Table::handleAspectDescriptionChange, not only renameWindow)
//   92b renaming table S92P leaves the curve / snapshot of table S92P_Sheet1 alone (the snapshot
//       remap matches `oldName_<column of the renamed table>`, not the bare prefix), so a graph
//       Undo afterwards keeps that curve; the curve of S92P itself is remapped
//   92c column rename: tablecol("other","A") in the own table is kept, col("Self/A") /
//       tablecol("Self","A") in other tables and the own table are rewritten, and one project
//       Undo / Redo restores / reapplies them together with the name
//   92d table rename through the explorer (F2), RenameWindowDialog and RPC app.rename_window
//       rewrites col("Old/x") / tablecol("Old","x") of other tables, once, in the rename's undo
//       step
//   92e re-import / overwrite import of [A,B] with a [B,A] header: the columns go through
//       temporary names, but the formulas of the own table (col(A), col("A"), column("A")) and
//       of other tables (col("T/A"), tablecol("T","A"), column("T/A")) keep "A" and evaluate to
//       the new column A
//   92f opening / appending an XML project does not propagate the per-table load renames
//       (temporary name -> saved name -> unique name): no propagation counted, and on append
//       the existing same-named table's graph, graph undo snapshot and formula references stay
//   92g column names with \, / and ": references are rewritten in each function's own syntax
//       (legacy col/tablecol literal, column/cell path escaped) and evaluate to the column;
//       a legacy reference cannot hold " and is left alone
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g92Failures = 0;

#define CHECK92(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g92Failures;                                                                      \
        }                                                                                       \
    } while (0)

QTreeWidgetItem *suite92ExplorerItem(ApplicationWindow *app, MyWidget *w)
{
    for (QTreeWidgetItemIterator it(&app->lv); *it; ++it) {
        QTreeWidgetItem *item = *it;
        if (item->type() == WindowListItem::WindowType
            && static_cast<WindowListItem *>(item)->window() == w)
            return item;
    }
    return nullptr;
}

QString suite92ExplorerText(ApplicationWindow *app, MyWidget *w)
{
    QTreeWidgetItem *item = suite92ExplorerItem(app, w);
    return item ? item->text(0) : QStringLiteral("<no explorer item>");
}

QStringList suite92CurveTitles(MultiLayer *ml)
{
    QStringList out;
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (!g)
        return out;
    for (int i = 0; i < g->curves(); ++i) {
        if (QwtPlotCurve *c = g->curve(i))
            out << c->title().text();
    }
    return out;
}

QString suite92Formula(Table *t, int col)
{
    Column *c = t ? t->column(col) : nullptr;
    return c ? c->formula(0) : QStringLiteral("<no column>");
}

void suite92SetFormula(Table *t, int col, const QString &f)
{
    if (Column *c = t->column(col))
        c->setFormula(Interval<int>(0, t->numRows() - 1), f);
}

QUndoStack *suite92ProjectStack(ApplicationWindow *app)
{
    return app->project() ? app->project()->undoStack() : nullptr;
}

void suite92CloseWindow(MyWidget *w)
{
    if (!w)
        return;
    w->askOnCloseEvent(false);
    w->close();
}

// Recalculates column `col` (stale values are overwritten first) and compares it with `want`.
bool suite92Values(Table *t, int col, const QVector<double> &want, QString *got)
{
    got->clear();
    Column *c = t ? t->column(col) : nullptr;
    if (!c)
        return false;
    for (int r = 0; r < want.size(); ++r)
        c->setValueAt(r, -98765.0);
    t->recalculate(col, false);
    bool ok = true;
    for (int r = 0; r < want.size(); ++r) {
        const double v = t->cell(r, col);
        *got += QString::number(v) + QLatin1Char(' ');
        if (!(std::fabs(v - want.at(r)) < 1e-9))
            ok = false;
    }
    return ok;
}

bool suite92WriteFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(bytes) == bytes.size();
}

struct Suite92App
{
    ApplicationWindow *win = nullptr;
    Suite92App() : win(new ApplicationWindow())
    {
        win->setHeadlessMode(true);
        win->applyUserSettings();
        win->confirmCloseTable = false;
        win->confirmCloseMatrix = false;
        win->confirmClosePlot2D = false;
        win->confirmClosePlot3D = false;
        win->confirmCloseFolder = false;
        win->confirmCloseNotes = false;
    }
    ~Suite92App()
    {
        win->savedProject();
        win->setOleEmbeddedMode(true);
        delete win;
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    Suite92App(const Suite92App &) = delete;
    Suite92App &operator=(const Suite92App &) = delete;
    ApplicationWindow *operator->() const { return win; }
};

Table *suite92FindTable(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList()) {
        Table *t = qobject_cast<Table *>(w);
        if (t && t->name() == name)
            return t;
    }
    return nullptr;
}

std::string q92(const QString &s)
{
    return s.toStdString();
}

std::string q92(const QStringList &s)
{
    return s.join(QStringLiteral(" | ")).toStdString();
}

} // namespace

int runSuite92RenamePropagation(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 92: table / column rename propagation to graphs, graph "
                 "undo snapshots, explorer and formulas (with Undo / Redo)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 92");
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    g92Failures = 0;
    QUndoStack *us = suite92ProjectStack(mw);
    CHECK_COND(us != nullptr, "92: project undo stack");

    // ------------------------------------------------ 92a: sheet rename, Undo, Redo
    {
        Table *t = mw->newTable(QStringLiteral("S92Src"), 4, 2);
        CHECK_COND(t != nullptr && t->name() == QLatin1String("S92Src"), "92a: table");
        t->setColName(0, QStringLiteral("X"));
        t->setColName(1, QStringLiteral("Y"));
        for (int r = 0; r < 4; ++r) {
            t->column(0)->setValueAt(r, r + 1.0);
            t->column(1)->setValueAt(r, 10.0 * (r + 1));
        }
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << QStringLiteral("S92Src_Y"),
                                            Graph::LineSymbols);
        CHECK_COND(ml && ml->activeGraph() && ml->activeGraph()->curves() == 1, "92a: plot");
        const QString snapBefore = ml->saveToString(QString());
        ml->activeGraph()->setAxisTitle(QwtPlot::xBottom, QStringLiteral("S92 edited title"));
        ml->pushUndoSnapshot(QStringLiteral("Edit X Title"), snapBefore);
        CHECK_COND(ml->undoStack() && ml->undoStack()->canUndo(), "92a: graph undo stack");

        CHECK92(mw->renameWindow(t, QStringLiteral("S92Ren")), "92a: renameWindow");
        CHECK92(t->name() == QLatin1String("S92Ren"), "92a: renamed to " << q92(t->name()));
        CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92Ren_Y"),
                "92a: curve after rename " << q92(suite92CurveTitles(ml)));
        CHECK92(suite92ExplorerText(mw, t) == QLatin1String("S92Ren"),
                "92a: explorer after rename " << q92(suite92ExplorerText(mw, t)));
        CHECK92(us->canUndo() && us->undoText().contains(QLatin1String("S92Ren")),
                "92a: project undo text after rename '" << q92(us->undoText()) << "'");

        // project Undo of the rename
        us->undo();
        CHECK92(t->name() == QLatin1String("S92Src"), "92a: undo name " << q92(t->name()));
        CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92Src_Y"),
                "92a: curve after undo " << q92(suite92CurveTitles(ml)));
        CHECK92(suite92ExplorerText(mw, t) == QLatin1String("S92Src"),
                "92a: explorer after undo " << q92(suite92ExplorerText(mw, t)));
        // the graph undo snapshot follows the table name back
        ml->undoStack()->undo();
        CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92Src_Y"),
                "92a: graph undo after rename undo " << q92(suite92CurveTitles(ml)));
        ml->undoStack()->redo();
        CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92Src_Y"),
                "92a: graph redo after rename undo " << q92(suite92CurveTitles(ml)));

        // project Redo of the rename
        us->redo();
        CHECK92(t->name() == QLatin1String("S92Ren"), "92a: redo name " << q92(t->name()));
        CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92Ren_Y"),
                "92a: curve after redo " << q92(suite92CurveTitles(ml)));
        CHECK92(suite92ExplorerText(mw, t) == QLatin1String("S92Ren"),
                "92a: explorer after redo " << q92(suite92ExplorerText(mw, t)));
        ml->undoStack()->undo();
        CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92Ren_Y"),
                "92a: graph undo after rename redo " << q92(suite92CurveTitles(ml)));
        ml->undoStack()->redo();

        suite92CloseWindow(ml);
        suite92CloseWindow(t);
    }

    // ------------------------------------------------ 92b: S92P vs S92P_Sheet1
    {
        Table *tp = mw->newTable(QStringLiteral("S92P"), 4, 2);
        Table *tq = mw->newTable(QStringLiteral("S92P_Sheet1"), 4, 2);
        CHECK_COND(tp && tq && tq->name() == QLatin1String("S92P_Sheet1"),
                   "92b: tables " << (tq ? q92(tq->name()) : std::string("null")));
        tp->setColName(0, QStringLiteral("X"));
        tp->setColName(1, QStringLiteral("Y"));
        tq->setColName(0, QStringLiteral("A"));
        tq->setColName(1, QStringLiteral("B"));
        for (int r = 0; r < 4; ++r) {
            tp->column(0)->setValueAt(r, r + 1.0);
            tp->column(1)->setValueAt(r, 2.0 * (r + 1));
            tq->column(0)->setValueAt(r, r + 1.0);
            tq->column(1)->setValueAt(r, 5.0 * (r + 1));
        }
        MultiLayer *ml = mw->multilayerPlot(tq, QStringList() << QStringLiteral("S92P_Sheet1_B"),
                                            Graph::LineSymbols);
        CHECK_COND(ml && ml->activeGraph() && ml->activeGraph()->curves() == 1, "92b: plot");
        CHECK92(ml->activeGraph()->insertCurve(tp, QStringLiteral("S92P_Y"), Graph::LineSymbols),
                "92b: second curve from S92P");
        const QStringList before = suite92CurveTitles(ml);
        CHECK92(before == QStringList() << QStringLiteral("S92P_Sheet1_B") << QStringLiteral("S92P_Y"),
                "92b: curves before " << q92(before));
        const QString snapBefore = ml->saveToString(QString());
        ml->activeGraph()->setAxisTitle(QwtPlot::xBottom, QStringLiteral("S92b edited"));
        ml->pushUndoSnapshot(QStringLiteral("Edit X Title"), snapBefore);

        CHECK92(mw->renameWindow(tp, QStringLiteral("S92Q")), "92b: rename S92P");
        const QStringList expected = QStringList() << QStringLiteral("S92P_Sheet1_B")
                                                   << QStringLiteral("S92Q_Y");
        CHECK92(suite92CurveTitles(ml) == expected,
                "92b: live curves after rename " << q92(suite92CurveTitles(ml)));
        ml->undoStack()->undo();
        CHECK92(suite92CurveTitles(ml) == expected,
                "92b: graph undo after renaming S92P kept the S92P_Sheet1 curve and remapped "
                "S92P's: got " << q92(suite92CurveTitles(ml)));
        ml->undoStack()->redo();
        CHECK92(suite92CurveTitles(ml) == expected,
                "92b: graph redo " << q92(suite92CurveTitles(ml)));

        suite92CloseWindow(ml);
        suite92CloseWindow(tp);
        suite92CloseWindow(tq);
    }

    // ------------------------------------------------ 92c: column rename and formulas
    {
        Table *ts = mw->newTable(QStringLiteral("S92C"), 3, 3);
        Table *to = mw->newTable(QStringLiteral("S92D"), 3, 2);
        CHECK_COND(ts && to, "92c: tables");
        ts->setAutoRecalcFormulas(false);
        to->setAutoRecalcFormulas(false);
        ts->setColName(0, QStringLiteral("A"));
        ts->setColName(1, QStringLiteral("B"));
        ts->setColName(2, QStringLiteral("C"));
        to->setColName(0, QStringLiteral("A"));
        to->setColName(1, QStringLiteral("F"));
        for (int r = 0; r < 3; ++r) {
            ts->column(0)->setValueAt(r, r + 1.0);
            to->column(0)->setValueAt(r, 10.0 * (r + 1));
        }
        const QString fB = QStringLiteral("tablecol(\"S92D\",\"A\")*2");
        const QString fC = QStringLiteral(
                "col(\"A\")+col(\"S92C/A\")+tablecol(\"S92C\",\"A\")+cell(\"A\",1)+col(A)");
        const QString fF = QStringLiteral("col(\"S92C/A\")+tablecol(\"S92C\",\"A\")+col(\"A\")");
        suite92SetFormula(ts, 1, fB);
        suite92SetFormula(ts, 2, fC);
        suite92SetFormula(to, 1, fF);

        ts->setColName(0, QStringLiteral("AA"));
        const QString fCNew = QStringLiteral(
                "col(\"AA\")+col(\"S92C/AA\")+tablecol(\"S92C\",\"AA\")+cell(\"AA\",1)+col(AA)");
        const QString fFNew = QStringLiteral("col(\"S92C/AA\")+tablecol(\"S92C\",\"AA\")+col(\"A\")");
        CHECK92(suite92Formula(ts, 1) == fB,
                "92c: own tablecol(\"S92D\",\"A\") changed to " << q92(suite92Formula(ts, 1)));
        CHECK92(suite92Formula(ts, 2) == fCNew,
                "92c: own references " << q92(suite92Formula(ts, 2)));
        CHECK92(suite92Formula(to, 1) == fFNew,
                "92c: other table references " << q92(suite92Formula(to, 1)));

        us->undo();
        CHECK92(ts->column(0)->name() == QLatin1String("A"), "92c: undo column name " << q92(ts->column(0)->name()));
        CHECK92(suite92Formula(ts, 2) == fC, "92c: undo own formula " << q92(suite92Formula(ts, 2)));
        CHECK92(suite92Formula(to, 1) == fF,
                "92c: undo other table formula " << q92(suite92Formula(to, 1)));
        CHECK92(suite92Formula(ts, 1) == fB, "92c: undo kept " << q92(suite92Formula(ts, 1)));

        us->redo();
        CHECK92(ts->column(0)->name() == QLatin1String("AA"), "92c: redo column name " << q92(ts->column(0)->name()));
        CHECK92(suite92Formula(ts, 2) == fCNew, "92c: redo own formula " << q92(suite92Formula(ts, 2)));
        CHECK92(suite92Formula(to, 1) == fFNew,
                "92c: redo other table formula " << q92(suite92Formula(to, 1)));

        // renaming the other table's column A must not touch S92C's own col("A")-style text
        to->setColName(0, QStringLiteral("A2"));
        CHECK92(suite92Formula(ts, 1) == QLatin1String("tablecol(\"S92D\",\"A2\")*2"),
                "92c: S92D column rename reaches S92C's tablecol: " << q92(suite92Formula(ts, 1)));
        CHECK92(suite92Formula(ts, 2) == fCNew,
                "92c: S92D column rename left S92C's own refs: " << q92(suite92Formula(ts, 2)));
        CHECK92(suite92Formula(to, 1) == QLatin1String("col(\"S92C/AA\")+tablecol(\"S92C\",\"AA\")+col(\"A2\")"),
                "92c: S92D own col(\"A\") follows: " << q92(suite92Formula(to, 1)));

        suite92CloseWindow(ts);
        suite92CloseWindow(to);
    }

    // ------------------------------------------------ 92d: table rename paths and formulas
    {
        Table *tr = mw->newTable(QStringLiteral("S92R"), 3, 1);
        Table *tf = mw->newTable(QStringLiteral("S92F"), 3, 1);
        CHECK_COND(tr && tf, "92d: tables");
        tr->setAutoRecalcFormulas(false);
        tf->setAutoRecalcFormulas(false);
        tr->setColName(0, QStringLiteral("x"));
        tf->setColName(0, QStringLiteral("f"));
        auto formulaFor = [](const QString &tbl) {
            return QStringLiteral("col(\"%1/x\")*2+tablecol(\"%1\",\"x\")").arg(tbl);
        };
        suite92SetFormula(tf, 0, formulaFor(QStringLiteral("S92R")));

        // explorer F2 (in-place edit of the explorer item)
        QTreeWidgetItem *item = suite92ExplorerItem(mw, tr);
        CHECK92(item != nullptr, "92d: explorer item of S92R");
        if (item)
            mw->renameWindow(item, 0, QStringLiteral("S92R2"));
        CHECK92(tr->name() == QLatin1String("S92R2"), "92d: F2 rename " << q92(tr->name()));
        CHECK92(suite92Formula(tf, 0) == formulaFor(QStringLiteral("S92R2")),
                "92d: F2 rename formula " << q92(suite92Formula(tf, 0)));
        // one undo step restores the name and the formula
        us->undo();
        CHECK92(tr->name() == QLatin1String("S92R") && suite92Formula(tf, 0) == formulaFor(QStringLiteral("S92R")),
                "92d: F2 undo name " << q92(tr->name()) << " formula " << q92(suite92Formula(tf, 0)));
        CHECK92(suite92ExplorerText(mw, tr) == QLatin1String("S92R"),
                "92d: F2 undo explorer " << q92(suite92ExplorerText(mw, tr)));
        us->redo();
        CHECK92(tr->name() == QLatin1String("S92R2") && suite92Formula(tf, 0) == formulaFor(QStringLiteral("S92R2")),
                "92d: F2 redo name " << q92(tr->name()) << " formula " << q92(suite92Formula(tf, 0)));

        // Rename Window dialog
        {
            QPointer<RenameWindowDialog> dlg(new RenameWindowDialog(mw));
            dlg->setWidget(tr);
            QLineEdit *nameEdit = dlg->findChild<QLineEdit *>();
            CHECK92(nameEdit != nullptr, "92d: dialog name edit");
            if (nameEdit) {
                nameEdit->setText(QStringLiteral("S92R3"));
                dlg->accept();
            }
            if (dlg)
                delete dlg.data();
        }
        CHECK92(tr->name() == QLatin1String("S92R3"), "92d: dialog rename " << q92(tr->name()));
        CHECK92(suite92Formula(tf, 0) == formulaFor(QStringLiteral("S92R3")),
                "92d: dialog rename formula " << q92(suite92Formula(tf, 0)));
        CHECK92(suite92ExplorerText(mw, tr) == QLatin1String("S92R3"),
                "92d: dialog rename explorer " << q92(suite92ExplorerText(mw, tr)));

        // RPC app.rename_window
        PythonAutomationServer *srv = mw->pythonAutomationServer();
        CHECK92(srv != nullptr, "92d: RPC server");
        if (srv) {
            QJsonObject req;
            req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            req[QStringLiteral("id")] = 9201;
            req[QStringLiteral("method")] = QStringLiteral("app.rename_window");
            req[QStringLiteral("params")] =
                    QJsonObject{ { "name", "S92R3" }, { "new_name", "S92R4" } };
            const QJsonObject resp = srv->handleRpcRequest(req);
            QCoreApplication::processEvents();
            CHECK92(!resp.contains(QStringLiteral("error")), "92d: RPC rename error");
        }
        CHECK92(tr->name() == QLatin1String("S92R4"), "92d: RPC rename " << q92(tr->name()));
        CHECK92(suite92Formula(tf, 0) == formulaFor(QStringLiteral("S92R4")),
                "92d: RPC rename formula " << q92(suite92Formula(tf, 0)));
        us->undo();
        CHECK92(tr->name() == QLatin1String("S92R3") && suite92Formula(tf, 0) == formulaFor(QStringLiteral("S92R3")),
                "92d: RPC undo name " << q92(tr->name()) << " formula "
                                      << q92(suite92Formula(tf, 0)));
        us->redo();
        CHECK92(tr->name() == QLatin1String("S92R4") && suite92Formula(tf, 0) == formulaFor(QStringLiteral("S92R4")),
                "92d: RPC redo name " << q92(tr->name()) << " formula "
                                      << q92(suite92Formula(tf, 0)));

        suite92CloseWindow(tr);
        suite92CloseWindow(tf);
    }

    // ------------------------------------------------ 92e: re-import / overwrite with [B,A]
    {
        QTemporaryDir tmp92;
        CHECK_COND(tmp92.isValid(), "92e: temp dir");
        const QString csv = tmp92.filePath(QStringLiteral("s92e.csv"));
        CHECK_COND(suite92WriteFile(csv, "A,B\n1,10\n2,20\n3,30\n"), "92e: write [A,B]");
        QSet<MyWidget *> before;
        for (MyWidget *w : mw->windowsList())
            before.insert(w);
        mw->importASCII(QStringList() << csv, ImportASCIIDialog::NewTables, QStringLiteral(","),
                        0, true, true, false, true, QLocale::c());
        Table *ti = nullptr;
        for (MyWidget *w : mw->windowsList()) {
            if (!before.contains(w) && qobject_cast<Table *>(w))
                ti = qobject_cast<Table *>(w);
        }
        CHECK_COND(ti && ti->numCols() == 2 && ti->numRows() == 3, "92e: imported table");
        CHECK_COND(mw->renameWindow(ti, QStringLiteral("S92I")), "92e: name the table S92I");
        Table *tj = mw->newTable(QStringLiteral("S92J"), 3, 3);
        CHECK_COND(tj, "92e: table S92J");
        ti->setAutoRecalcFormulas(false);
        tj->setAutoRecalcFormulas(false);
        ti->setNumCols(4);
        ti->setColName(2, QStringLiteral("D"));
        ti->setColName(3, QStringLiteral("E"));
        const QString fD = QStringLiteral("col(A)*10");
        const QString fE = QStringLiteral("col(\"A\")+column(\"A\")");
        const QString fJ0 = QStringLiteral("col(\"S92I/A\")*2");
        const QString fJ1 = QStringLiteral("tablecol(\"S92I\",\"A\")*3");
        const QString fJ2 = QStringLiteral("column(\"S92I/A\")*4");
        suite92SetFormula(ti, 2, fD);
        suite92SetFormula(ti, 3, fE);
        suite92SetFormula(tj, 0, fJ0);
        suite92SetFormula(tj, 1, fJ1);
        suite92SetFormula(tj, 2, fJ2);

        // A = {a0, a1, a2}: every formula of both tables evaluates against column "A"
        auto checkAll = [&](const char *step, const QVector<double> &a) {
            CHECK92(suite92Formula(ti, 2) == fD && suite92Formula(ti, 3) == fE,
                    "92e " << step << ": own formulas " << q92(suite92Formula(ti, 2)) << " / "
                           << q92(suite92Formula(ti, 3)));
            CHECK92(suite92Formula(tj, 0) == fJ0 && suite92Formula(tj, 1) == fJ1
                            && suite92Formula(tj, 2) == fJ2,
                    "92e " << step << ": other table formulas " << q92(suite92Formula(tj, 0))
                           << " / " << q92(suite92Formula(tj, 1)) << " / "
                           << q92(suite92Formula(tj, 2)));
            const int aIdx = ti->colIndex(QStringLiteral("A"));
            CHECK92(aIdx >= 0 && aIdx < 2, "92e " << step << ": column A at " << aIdx);
            QVector<double> got;
            for (int r = 0; r < 3 && aIdx >= 0; ++r)
                got << ti->cell(r, aIdx);
            CHECK92(got == a, "92e " << step << ": values of A");
            auto scaled = [&](double k) {
                QVector<double> v;
                for (double x : a)
                    v << k * x;
                return v;
            };
            QString vals;
            CHECK92(suite92Values(ti, 2, scaled(10), &vals),
                    "92e " << step << ": col(A)*10 -> " << q92(vals));
            CHECK92(suite92Values(ti, 3, scaled(2), &vals),
                    "92e " << step << ": col(\"A\")+column(\"A\") -> " << q92(vals));
            CHECK92(suite92Values(tj, 1, scaled(3), &vals),
                    "92e " << step << ": tablecol(\"S92I\",\"A\")*3 -> " << q92(vals));
            CHECK92(suite92Values(tj, 2, scaled(4), &vals),
                    "92e " << step << ": column(\"S92I/A\")*4 -> " << q92(vals));
            // col("T/A") is evaluated as a column of the formula's own table (legacy col()
            // escapes the '/'), so only its text is checked above.
        };
        checkAll("before", QVector<double>() << 1 << 2 << 3);

        CHECK_COND(suite92WriteFile(csv, "B,A\n100,4\n200,5\n300,6\n"), "92e: write [B,A]");
        CHECK92(ti->reimportFromSource(), "92e: reimportFromSource");
        QCoreApplication::processEvents();
        CHECK92(ti->column(0)->name() == QLatin1String("B")
                        && ti->column(1)->name() == QLatin1String("A"),
                "92e: re-import names " << q92(ti->colNames()));
        checkAll("re-import", QVector<double>() << 4 << 5 << 6);

        CHECK_COND(suite92WriteFile(csv, "A,B\n7,70\n8,80\n9,90\n"), "92e: write [A,B] again");
        mw->d_workspace.setActiveSubWindow(ti);
        mw->importASCII(QStringList() << csv, ImportASCIIDialog::Overwrite, QStringLiteral(","),
                        0, true, true, false, true, QLocale::c());
        CHECK92(ti->column(0)->name() == QLatin1String("A")
                        && ti->column(1)->name() == QLatin1String("B"),
                "92e: overwrite names " << q92(ti->colNames()));
        checkAll("overwrite", QVector<double>() << 7 << 8 << 9);

        suite92CloseWindow(ti);
        suite92CloseWindow(tj);
    }

    // ------------------------------------------------ 92f: XML project open / append
    {
        QTemporaryDir tmp92;
        CHECK_COND(tmp92.isValid(), "92f: temp dir");
        const QString prj = tmp92.filePath(QStringLiteral("s92f.sciprj"));
        const QString fY = QStringLiteral("col(\"S92X/y\")*2+tablecol(\"S92X\",\"y\")");
        {
            Suite92App src;
            Table *tx = src->newTable(QStringLiteral("S92X"), 3, 2);
            Table *ty = src->newTable(QStringLiteral("S92Y"), 3, 1);
            CHECK_COND(tx && ty, "92f: source tables");
            tx->setColName(0, QStringLiteral("x"));
            tx->setColName(1, QStringLiteral("y"));
            ty->setAutoRecalcFormulas(false);
            suite92SetFormula(ty, 0, fY);
            CHECK_COND(src->saveFolder(src->projectFolder(), prj), "92f: save project");
        }

        // open: the tables arrive under their saved names without one propagation per table
        {
            Suite92App rd;
            const quint64 n0 = Table::tableRenamePropagationCount();
            CHECK92(rd->loadProject(prj), "92f: loadProject");
            CHECK92(Table::tableRenamePropagationCount() == n0,
                    "92f: open propagated " << (Table::tableRenamePropagationCount() - n0)
                                            << " table rename(s)");
            Table *ty = suite92FindTable(rd.win, QStringLiteral("S92Y"));
            CHECK92(ty && suite92Formula(ty, 0) == fY,
                    "92f: opened formula " << (ty ? q92(suite92Formula(ty, 0)) : std::string("<none>")));
            CHECK92(suite92FindTable(rd.win, QStringLiteral("S92X")) != nullptr, "92f: opened S92X");
        }

        // append into a session that already has a table S92X (with a graph and a reference):
        // the appended S92X is renamed to a unique name, which must not re-point the existing
        // graph, its undo snapshot or the existing formula to the appended table.
        {
            Suite92App ap;
            Table *ex = ap->newTable(QStringLiteral("S92X"), 3, 2);
            Table *ez = ap->newTable(QStringLiteral("S92Z"), 3, 1);
            CHECK_COND(ex && ez, "92f: existing tables");
            ex->setColName(0, QStringLiteral("x"));
            ex->setColName(1, QStringLiteral("y"));
            for (int r = 0; r < 3; ++r) {
                ex->column(0)->setValueAt(r, r + 1.0);
                ex->column(1)->setValueAt(r, 2.0 * (r + 1));
            }
            ez->setAutoRecalcFormulas(false);
            const QString fZ = QStringLiteral("tablecol(\"S92X\",\"y\")+col(\"S92X/y\")");
            suite92SetFormula(ez, 0, fZ);
            MultiLayer *ml = ap->multilayerPlot(ex, QStringList() << QStringLiteral("S92X_y"),
                                                Graph::LineSymbols);
            CHECK_COND(ml && ml->activeGraph() && ml->activeGraph()->curves() == 1, "92f: plot");
            const QString snapBefore = ml->saveToString(QString());
            ml->activeGraph()->setAxisTitle(QwtPlot::xBottom, QStringLiteral("S92f edited"));
            ml->pushUndoSnapshot(QStringLiteral("Edit X Title"), snapBefore);

            const quint64 n0 = Table::tableRenamePropagationCount();
            ap->setProperty("torigraph_last_append_ok", QVariant());
            ap->appendProject(prj);
            CHECK92(ap->property("torigraph_last_append_ok").toBool(), "92f: appendProject");
            CHECK92(Table::tableRenamePropagationCount() == n0,
                    "92f: append propagated " << (Table::tableRenamePropagationCount() - n0)
                                              << " table rename(s)");
            int nS92X = 0;
            QStringList tableNames;
            for (MyWidget *w : ap->windowsList()) {
                if (Table *t = qobject_cast<Table *>(w)) {
                    tableNames << t->name();
                    if (t->name() == QLatin1String("S92X"))
                        ++nS92X;
                }
            }
            CHECK92(nS92X == 1 && ex->name() == QLatin1String("S92X") && tableNames.size() == 4,
                    "92f: tables after append " << q92(tableNames));
            CHECK92(suite92Formula(ez, 0) == fZ,
                    "92f: existing formula after append " << q92(suite92Formula(ez, 0)));
            CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92X_y"),
                    "92f: existing curve after append " << q92(suite92CurveTitles(ml)));
            ml->undoStack()->undo();
            CHECK92(suite92CurveTitles(ml) == QStringList() << QStringLiteral("S92X_y"),
                    "92f: existing graph undo after append " << q92(suite92CurveTitles(ml)));
        }
    }

    // ------------------------------------------------ 92g: names with \, / and "
    {
        Table *ts = mw->newTable(QStringLiteral("S92S"), 3, 6);
        Table *tu = mw->newTable(QStringLiteral("S92U"), 3, 3);
        CHECK_COND(ts && tu, "92g: tables");
        ts->setAutoRecalcFormulas(false);
        tu->setAutoRecalcFormulas(false);
        ts->setColName(0, QStringLiteral("A"));
        for (int c = 1; c < 6; ++c)
            ts->setColName(c, QStringLiteral("C%1").arg(c));
        for (int r = 0; r < 3; ++r)
            ts->column(0)->setValueAt(r, r + 1.0);
        suite92SetFormula(ts, 2, QStringLiteral("col(\"A\")*2"));
        suite92SetFormula(ts, 3, QStringLiteral("col(A)*3"));
        suite92SetFormula(ts, 4, QStringLiteral("column(\"A\")*4"));
        suite92SetFormula(ts, 5, QStringLiteral("cell(\"A\", i)*5"));
        suite92SetFormula(tu, 0, QStringLiteral("tablecol(\"S92S\",\"A\")*6"));
        suite92SetFormula(tu, 1, QStringLiteral("column(\"S92S/A\")*7"));
        suite92SetFormula(tu, 2, QStringLiteral("col(\"S92S/A\")*8"));

        struct Step
        {
            QString name;
            QStringList formulas; // ts 2..5, tu 0..2
            QVector<bool> evaluate; // same order
        };
        const QVector<Step> steps = {
            { QStringLiteral("a\\b"),
              { QStringLiteral("col(\"a\\b\")*2"), QStringLiteral("col(\"a\\b\")*3"),
                QStringLiteral("column(\"a\\\\b\")*4"), QStringLiteral("cell(\"a\\\\b\", i)*5"),
                QStringLiteral("tablecol(\"S92S\",\"a\\b\")*6"),
                QStringLiteral("column(\"S92S/a\\\\b\")*7"), QStringLiteral("col(\"S92S/a\\b\")*8") },
              { true, true, true, true, true, true, false } },
            { QStringLiteral("a/b"),
              { QStringLiteral("col(\"a/b\")*2"), QStringLiteral("col(\"a/b\")*3"),
                QStringLiteral("column(\"a\\/b\")*4"), QStringLiteral("cell(\"a\\/b\", i)*5"),
                QStringLiteral("tablecol(\"S92S\",\"a/b\")*6"),
                QStringLiteral("column(\"S92S/a\\/b\")*7"), QStringLiteral("col(\"S92S/a/b\")*8") },
              { true, true, true, true, true, true, false } },
            // legacy col / tablecol cannot hold ": left as they were (they no longer resolve)
            { QStringLiteral("x\"y"),
              { QStringLiteral("col(\"a/b\")*2"), QStringLiteral("col(\"a/b\")*3"),
                QStringLiteral("column(\"x\\\"y\")*4"), QStringLiteral("cell(\"x\\\"y\", i)*5"),
                QStringLiteral("tablecol(\"S92S\",\"a/b\")*6"),
                QStringLiteral("column(\"S92S/x\\\"y\")*7"), QStringLiteral("col(\"S92S/a/b\")*8") },
              { false, false, true, true, false, true, false } },
        };
        const double factors[] = { 2, 3, 4, 5, 6, 7, 8 };
        for (const Step &st : steps) {
            ts->setColName(0, st.name);
            CHECK92(ts->column(0)->name() == st.name,
                    "92g: column renamed to " << q92(ts->column(0)->name()));
            for (int k = 0; k < 7; ++k) {
                Table *t = k < 4 ? ts : tu;
                const int col = k < 4 ? k + 2 : k - 4;
                CHECK92(suite92Formula(t, col) == st.formulas.at(k),
                        "92g [" << q92(st.name) << "] formula " << k << ": "
                                << q92(suite92Formula(t, col)) << " (want "
                                << q92(st.formulas.at(k)) << ")");
                if (!st.evaluate.at(k))
                    continue;
                QString vals;
                QVector<double> want;
                for (int r = 0; r < 3; ++r)
                    want << factors[k] * (r + 1);
                CHECK92(suite92Values(t, col, want, &vals),
                        "92g [" << q92(st.name) << "] " << q92(st.formulas.at(k)) << " -> "
                                << q92(vals));
            }
        }

        suite92CloseWindow(ts);
        suite92CloseWindow(tu);
    }

    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    if (g92Failures != 0) {
        std::cerr << "[FAIL] Suite 92: " << g92Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 92: table / column renames reach graphs, graph undo snapshots, "
                 "explorer and formulas, also on Undo / Redo.\n";
    return 0;
}
