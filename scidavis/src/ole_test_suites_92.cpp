#include "ole_test_common.h"
#include "Folder.h"
#include "PythonAutomationServer.h"
#include "RenameWindowDialog.h"

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

    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    if (g92Failures != 0) {
        std::cerr << "[FAIL] Suite 92: " << g92Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 92: table / column renames reach graphs, graph undo snapshots, "
                 "explorer and formulas, also on Undo / Redo.\n";
    return 0;
}
