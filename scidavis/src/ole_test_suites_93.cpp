#include "ole_test_common.h"

// =============================================================================
// Suite 93: worksheet core - sorting tables with formula columns, and rows beyond a column's
// stored data being empty (invalid) instead of a phantom 0.
//   93a sort by A (descending) with C = col("A")*2 right of A: C is permuted once, so the row
//       relation C = 2A holds ([8,6,4,2], not the doubly permuted [6,2,4,8]); one Undo restores
//       A/B/C, Redo re-applies the sort with the same values
//   93b sort by the formula column C itself (ascending): A=[1,2,3,4], C=[2,4,6,8]
//   93c two keys as two stable sorts (secondary K2, then primary K1) with F = 10*K1+K2 as the
//       last column: rows stay consistent, and Undo / Undo restore the original order
//   93d sorting the columns separately (no leading column) and sorting only A: the formula
//       column follows A once; a formula column left of the key keeps working
//   93e a column shorter than the table: rows after its data are invalid (isInvalid(row),
//       isInvalid(interval), text()), statistics count only the data, col("y")*2 is empty there
//       and its curve has 3 points; the same after saving and reopening the project (with a
//       recalculation of the formula and the curve of the reopened graph)
//   93f rows added by setNumRows(), by a longer neighbouring column and by setValueAt() past
//       the end are empty, negative rows are invalid
//   93g formula references past a source column's data: tablecol("Short","V"),
//       column("Short/V"), cell("A",11) and cell("A",0) are empty, cell("A",3) is the value
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g93Failures = 0;

#define CHECK93(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g93Failures;                                                                      \
        }                                                                                       \
    } while (0)

struct Suite93App
{
    ApplicationWindow *win = nullptr;
    Suite93App() : win(new ApplicationWindow())
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
    ~Suite93App()
    {
        win->savedProject();
        win->setOleEmbeddedMode(true);
        delete win;
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    Suite93App(const Suite93App &) = delete;
    Suite93App &operator=(const Suite93App &) = delete;
    ApplicationWindow *operator->() const { return win; }
};

void suite93CloseWindow(MyWidget *w)
{
    if (!w)
        return;
    w->askOnCloseEvent(false);
    w->close();
}

Table *suite93FindTable(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList()) {
        Table *t = qobject_cast<Table *>(w);
        if (t && t->name() == name)
            return t;
    }
    return nullptr;
}

MultiLayer *suite93FirstGraph(ApplicationWindow *app)
{
    for (MyWidget *w : app->windowsList()) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(w))
            return ml;
    }
    return nullptr;
}

int suite93CurvePoints(MultiLayer *ml)
{
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (!g || g->curves() < 1 || !g->curve(0))
        return -1;
    return g->curve(0)->dataSize();
}

void suite93Fill(Table *t, int col, const QVector<double> &v)
{
    Column *c = t->column(col);
    for (int r = 0; r < v.size(); ++r)
        c->setValueAt(r, v.at(r));
}

// Values of rows 0..rows-1, "_" for invalid rows.
QString suite93Dump(Column *c, int rows)
{
    QStringList out;
    for (int r = 0; r < rows; ++r)
        out << (!c ? QStringLiteral("?")
                   : c->isInvalid(r) ? QStringLiteral("_") : QString::number(c->valueAt(r)));
    return out.join(QLatin1Char(','));
}

std::string s93(const QString &s)
{
    return s.toStdString();
}

QString suite93Expect(const QVector<double> &v, int rows)
{
    QStringList out;
    for (int r = 0; r < rows; ++r)
        out << (r < v.size() ? QString::number(v.at(r)) : QStringLiteral("_"));
    return out.join(QLatin1Char(','));
}

// Number of valid (non-empty) rows among the table rows, like table.column_stats counts them.
int suite93ValidCount(Table *t, int col)
{
    Column *c = t ? t->column(col) : nullptr;
    if (!c)
        return -1;
    int n = 0;
    for (int r = 0; r < t->numRows(); ++r)
        if (!c->isInvalid(r))
            ++n;
    return n;
}

QList<Column *> suite93AllColumns(Table *t)
{
    QList<Column *> cols;
    for (int c = 0; c < t->numCols(); ++c)
        cols << t->column(c);
    return cols;
}

} // namespace

int runSuite93WorksheetSortAndBlankRows(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 93: worksheet sort with formula columns and empty rows "
                 "beyond a column's data...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 93");
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    g93Failures = 0;

    // ------------------------------------------------ 93a: sort by A, C = 2A, Undo / Redo
    {
        Table *t = mw->newTable(QStringLiteral("S93Sort"), 4, 3);
        CHECK_COND(t && t->d_future_table, "93a: table");
        t->setColName(0, QStringLiteral("A"));
        t->setColName(1, QStringLiteral("B"));
        t->setColName(2, QStringLiteral("C"));
        suite93Fill(t, 0, { 1, 4, 2, 3 });
        suite93Fill(t, 1, { 10, 40, 20, 30 });
        t->setCommand(2, QStringLiteral("col(\"A\")*2"));
        CHECK93(suite93Dump(t->column(2), 4) == QLatin1String("2,8,4,6"),
                "93a: formula C before sort " << s93(suite93Dump(t->column(2), 4)));
        QUndoStack *us = t->d_future_table->undoStack();
        CHECK_COND(us != nullptr, "93a: undo stack");
        const int idx0 = us->index();

        t->d_future_table->sortColumns(t->column(0), suite93AllColumns(t), false);
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("4,3,2,1"),
                "93a: A after sort " << s93(suite93Dump(t->column(0), 4)));
        CHECK93(suite93Dump(t->column(1), 4) == QLatin1String("40,30,20,10"),
                "93a: B after sort " << s93(suite93Dump(t->column(1), 4)));
        CHECK93(suite93Dump(t->column(2), 4) == QLatin1String("8,6,4,2"),
                "93a: C after sort (double permutation?) " << s93(suite93Dump(t->column(2), 4)));
        CHECK93(t->column(2)->formula(0) == QLatin1String("col(\"A\")*2"),
                "93a: formula kept " << s93(t->column(2)->formula(0)));
        CHECK93(us->index() == idx0 + 1,
                "93a: sort is one undo step, index " << idx0 << " -> " << us->index());

        us->undo();
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("1,4,2,3"),
                "93a: A after undo " << s93(suite93Dump(t->column(0), 4)));
        CHECK93(suite93Dump(t->column(1), 4) == QLatin1String("10,40,20,30"),
                "93a: B after undo " << s93(suite93Dump(t->column(1), 4)));
        CHECK93(suite93Dump(t->column(2), 4) == QLatin1String("2,8,4,6"),
                "93a: C after undo " << s93(suite93Dump(t->column(2), 4)));
        us->redo();
        QApplication::processEvents();
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("4,3,2,1"),
                "93a: A after redo " << s93(suite93Dump(t->column(0), 4)));
        CHECK93(suite93Dump(t->column(2), 4) == QLatin1String("8,6,4,2"),
                "93a: C after redo " << s93(suite93Dump(t->column(2), 4)));
        CHECK93(us->canRedo() == false, "93a: redo stack consumed, nothing pushed by redo");
        suite93CloseWindow(t);
    }

    // ------------------------------------------------ 93b: sort by the formula column
    {
        Table *t = mw->newTable(QStringLiteral("S93SortC"), 4, 2);
        CHECK_COND(t && t->d_future_table, "93b: table");
        t->setColName(0, QStringLiteral("A"));
        t->setColName(1, QStringLiteral("C"));
        suite93Fill(t, 0, { 1, 4, 2, 3 });
        t->setCommand(1, QStringLiteral("col(\"A\")*2"));
        t->d_future_table->sortColumns(t->column(1), suite93AllColumns(t), true);
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("1,2,3,4"),
                "93b: A after sort by C " << s93(suite93Dump(t->column(0), 4)));
        CHECK93(suite93Dump(t->column(1), 4) == QLatin1String("2,4,6,8"),
                "93b: C after sort by C " << s93(suite93Dump(t->column(1), 4)));
        suite93CloseWindow(t);
    }

    // ------------------------------------------------ 93c: two keys (two stable sorts)
    {
        Table *t = mw->newTable(QStringLiteral("S93Keys"), 4, 3);
        CHECK_COND(t && t->d_future_table, "93c: table");
        t->setColName(0, QStringLiteral("K1"));
        t->setColName(1, QStringLiteral("K2"));
        t->setColName(2, QStringLiteral("F"));
        suite93Fill(t, 0, { 2, 1, 2, 1 });
        suite93Fill(t, 1, { 4, 3, 1, 2 });
        t->setCommand(2, QStringLiteral("col(\"K1\")*10+col(\"K2\")"));
        CHECK93(suite93Dump(t->column(2), 4) == QLatin1String("24,13,21,12"),
                "93c: F before sort " << s93(suite93Dump(t->column(2), 4)));
        QUndoStack *us = t->d_future_table->undoStack();
        t->d_future_table->sortColumns(t->column(1), suite93AllColumns(t), true);
        t->d_future_table->sortColumns(t->column(0), suite93AllColumns(t), true);
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("1,1,2,2"),
                "93c: K1 " << s93(suite93Dump(t->column(0), 4)));
        CHECK93(suite93Dump(t->column(1), 4) == QLatin1String("2,3,1,4"),
                "93c: K2 " << s93(suite93Dump(t->column(1), 4)));
        CHECK93(suite93Dump(t->column(2), 4) == QLatin1String("12,13,21,24"),
                "93c: F " << s93(suite93Dump(t->column(2), 4)));
        us->undo();
        CHECK93(suite93Dump(t->column(2), 4) == QLatin1String("21,12,13,24"),
                "93c: F after one undo " << s93(suite93Dump(t->column(2), 4)));
        us->undo();
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("2,1,2,1")
                        && suite93Dump(t->column(2), 4) == QLatin1String("24,13,21,12"),
                "93c: after two undos K1 " << s93(suite93Dump(t->column(0), 4)) << " F "
                                          << s93(suite93Dump(t->column(2), 4)));
        suite93CloseWindow(t);
    }

    // ------------------------------------------------ 93d: separate sort / only A / C left of A
    {
        Table *t = mw->newTable(QStringLiteral("S93Sep"), 4, 2);
        CHECK_COND(t && t->d_future_table, "93d: table");
        t->setColName(0, QStringLiteral("A"));
        t->setColName(1, QStringLiteral("C"));
        suite93Fill(t, 0, { 1, 4, 2, 3 });
        t->setCommand(1, QStringLiteral("col(\"A\")*2"));
        t->d_future_table->sortColumns(nullptr, suite93AllColumns(t), false);
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("4,3,2,1")
                        && suite93Dump(t->column(1), 4) == QLatin1String("8,6,4,2"),
                "93d: separately A " << s93(suite93Dump(t->column(0), 4)) << " C "
                                     << s93(suite93Dump(t->column(1), 4)));

        // only the key column: the dependent formula column follows once
        t->d_future_table->sortColumns(t->column(0), QList<Column *>() << t->column(0), true);
        CHECK93(suite93Dump(t->column(0), 4) == QLatin1String("1,2,3,4")
                        && suite93Dump(t->column(1), 4) == QLatin1String("2,4,6,8"),
                "93d: only A, A " << s93(suite93Dump(t->column(0), 4)) << " C "
                                  << s93(suite93Dump(t->column(1), 4)));
        suite93CloseWindow(t);

        Table *l = mw->newTable(QStringLiteral("S93Left"), 4, 2);
        CHECK_COND(l && l->d_future_table, "93d: left table");
        l->setColName(0, QStringLiteral("C"));
        l->setColName(1, QStringLiteral("A"));
        suite93Fill(l, 1, { 1, 4, 2, 3 });
        l->setCommand(0, QStringLiteral("col(\"A\")*2"));
        l->d_future_table->sortColumns(l->column(1), suite93AllColumns(l), false);
        CHECK93(suite93Dump(l->column(1), 4) == QLatin1String("4,3,2,1")
                        && suite93Dump(l->column(0), 4) == QLatin1String("8,6,4,2"),
                "93d: C left of A, A " << s93(suite93Dump(l->column(1), 4)) << " C "
                                       << s93(suite93Dump(l->column(0), 4)));
        suite93CloseWindow(l);
    }

    // ------------------------------------------------ 93e: column shorter than the table
    {
        QTemporaryDir tmp93;
        CHECK_COND(tmp93.isValid(), "93e: temp dir");
        const QString prj = tmp93.filePath(QStringLiteral("s93e.sciprj"));
        {
            Suite93App src;
            Table *t = src->newTable(QStringLiteral("Z93"), 10, 3);
            CHECK_COND(t && t->d_future_table, "93e: table");
            t->setColName(0, QStringLiteral("x"));
            t->setColName(1, QStringLiteral("y"));
            t->setColName(2, QStringLiteral("f"));
            t->setColPlotDesignation(0, SciDAVis::X);
            t->setColPlotDesignation(2, SciDAVis::Y);
            suite93Fill(t, 0, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
            suite93Fill(t, 1, { 5, 6, 7 });
            Column *y = t->column(1);
            // what the RPC bulk writer does after a short write: mark the rest invalid (this
            // validity lies past the data and is not saved)
            y->setInvalid(Interval<int>(3, 9), true);
            CHECK93(y->rowCount() == 3, "93e: y data length " << y->rowCount());
            CHECK93(suite93Dump(y, 10) == QLatin1String("5,6,7,_,_,_,_,_,_,_"),
                    "93e: y " << s93(suite93Dump(y, 10)));
            CHECK93(y->isInvalid(Interval<int>(3, 9)) && y->isInvalid(Interval<int>(5, 20))
                            && !y->isInvalid(Interval<int>(0, 9))
                            && !y->isInvalid(Interval<int>(2, 4)),
                    "93e: interval validity");
            CHECK93(t->text(5, 1).isEmpty(), "93e: text of row 6 '" << s93(t->text(5, 1)) << "'");
            CHECK93(suite93ValidCount(t, 1) == 3, "93e: y count " << suite93ValidCount(t, 1));
            CHECK93(suite93ValidCount(t, 2) == 0,
                    "93e: untouched column count " << suite93ValidCount(t, 2));

            t->setCommand(2, QStringLiteral("col(\"y\")*2"));
            CHECK93(suite93Dump(t->column(2), 10) == QLatin1String("10,12,14,_,_,_,_,_,_,_"),
                    "93e: f " << s93(suite93Dump(t->column(2), 10)));
            MultiLayer *ml = src->multilayerPlot(t, QStringList() << QStringLiteral("Z93_f"),
                                                 Graph::LineSymbols);
            CHECK93(suite93CurvePoints(ml) == 3, "93e: curve points " << suite93CurvePoints(ml));
            CHECK_COND(src->saveFolder(src->projectFolder(), prj), "93e: save project");
        }
        {
            Suite93App rd;
            CHECK93(rd->loadProject(prj), "93e: loadProject");
            Table *t = suite93FindTable(rd.win, QStringLiteral("Z93"));
            CHECK_COND(t != nullptr, "93e: reopened table");
            CHECK93(t->numRows() == 10, "93e: reopened rows " << t->numRows());
            CHECK93(suite93Dump(t->column(1), 10) == QLatin1String("5,6,7,_,_,_,_,_,_,_"),
                    "93e: reopened y " << s93(suite93Dump(t->column(1), 10)));
            CHECK93(suite93ValidCount(t, 1) == 3,
                    "93e: reopened y count " << suite93ValidCount(t, 1));
            CHECK93(suite93Dump(t->column(2), 10) == QLatin1String("10,12,14,_,_,_,_,_,_,_"),
                    "93e: reopened f " << s93(suite93Dump(t->column(2), 10)));
            CHECK93(t->recalculate(2, false), "93e: recalculate f");
            CHECK93(suite93Dump(t->column(2), 10) == QLatin1String("10,12,14,_,_,_,_,_,_,_"),
                    "93e: recalculated f " << s93(suite93Dump(t->column(2), 10)));
            MultiLayer *ml = suite93FirstGraph(rd.win);
            CHECK93(ml != nullptr, "93e: reopened graph");
            rd->updateCurves(t, QStringLiteral("Z93_f"));
            CHECK93(suite93CurvePoints(ml) == 3,
                    "93e: reopened curve points " << suite93CurvePoints(ml));
        }
    }

    // ------------------------------------------------ 93f: grown tables / columns
    {
        Table *p = mw->newTable(QStringLiteral("P93"), 3, 2);
        CHECK_COND(p != nullptr, "93f: table P93");
        suite93Fill(p, 0, { 1, 2, 3 });
        suite93Fill(p, 1, { 10, 20, 30 });
        p->setNumRows(6);
        CHECK93(p->numRows() == 6, "93f: rows after resize " << p->numRows());
        CHECK93(suite93Dump(p->column(0), 6) == QLatin1String("1,2,3,_,_,_"),
                "93f: A after resize " << s93(suite93Dump(p->column(0), 6)));
        CHECK93(suite93ValidCount(p, 0) == 3, "93f: A count " << suite93ValidCount(p, 0));
        CHECK93(p->column(0)->isInvalid(-1), "93f: negative row is invalid");
        suite93CloseWindow(p);

        Table *q = mw->newTable(QStringLiteral("Q93"), 3, 2);
        CHECK_COND(q != nullptr, "93f: table Q93");
        suite93Fill(q, 0, { 1, 2, 3 });
        q->setNumRows(6);
        suite93Fill(q, 1, { 1, 2, 3, 4, 5, 6 });
        CHECK93(suite93Dump(q->column(0), 6) == QLatin1String("1,2,3,_,_,_"),
                "93f: Q col0 after a longer col1 " << s93(suite93Dump(q->column(0), 6)));
        CHECK93(suite93ValidCount(q, 0) == 3, "93f: Q col0 count " << suite93ValidCount(q, 0));
        suite93CloseWindow(q);

        Table *r = mw->newTable(QStringLiteral("R93"), 3, 2);
        CHECK_COND(r != nullptr, "93f: table R93");
        suite93Fill(r, 0, { 1, 2, 3 });
        suite93Fill(r, 1, { 1, 2, 3 });
        r->setNumRows(8);
        r->column(1)->setValueAt(7, 99.0);
        CHECK93(suite93Dump(r->column(1), 8) == QLatin1String("1,2,3,_,_,_,_,99"),
                "93f: R col1 " << s93(suite93Dump(r->column(1), 8)));
        CHECK93(suite93Dump(r->column(0), 8) == QLatin1String("1,2,3,_,_,_,_,_"),
                "93f: R col0 " << s93(suite93Dump(r->column(0), 8)));
        suite93CloseWindow(r);
    }

    // ------------------------------------------------ 93g: formula references past the data
    {
        Table *s = mw->newTable(QStringLiteral("Short"), 4, 1);
        Table *t = mw->newTable(QStringLiteral("F93"), 10, 3);
        CHECK_COND(s && t, "93g: tables");
        s->setColName(0, QStringLiteral("V"));
        suite93Fill(s, 0, { 1, 2, 3, 4 });
        t->setColName(0, QStringLiteral("A"));
        t->setColName(1, QStringLiteral("D"));
        t->setColName(2, QStringLiteral("E"));
        const QVector<double> a = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        suite93Fill(t, 0, a);

        t->setCommand(1, QStringLiteral("tablecol(\"Short\", \"V\")*10"));
        CHECK93(suite93Dump(t->column(1), 10) == suite93Expect({ 10, 20, 30, 40 }, 10),
                "93g: tablecol short " << s93(suite93Dump(t->column(1), 10)));
        t->setCommand(1, QStringLiteral("column(\"Short/V\")*10"));
        CHECK93(suite93Dump(t->column(1), 10) == suite93Expect({ 10, 20, 30, 40 }, 10),
                "93g: column(path) short " << s93(suite93Dump(t->column(1), 10)));
        t->setCommand(1, QStringLiteral("col(\"Short/V\")*10"));
        CHECK93(suite93Dump(t->column(1), 10) == suite93Expect({ 10, 20, 30, 40 }, 10),
                "93g: col(path) short " << s93(suite93Dump(t->column(1), 10)));

        t->setCommand(2, QStringLiteral("cell(\"A\", 11)"));
        CHECK93(suite93Dump(t->column(2), 10) == suite93Expect({}, 10),
                "93g: cell(A,11) " << s93(suite93Dump(t->column(2), 10)));
        t->setCommand(2, QStringLiteral("cell(\"A\", 0)"));
        CHECK93(suite93Dump(t->column(2), 10) == suite93Expect({}, 10),
                "93g: cell(A,0) " << s93(suite93Dump(t->column(2), 10)));
        t->setCommand(2, QStringLiteral("cell(\"A\", 3)"));
        CHECK93(suite93Dump(t->column(2), 10)
                        == suite93Expect({ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }, 10),
                "93g: cell(A,3) " << s93(suite93Dump(t->column(2), 10)));
        suite93CloseWindow(t);
        suite93CloseWindow(s);
    }

    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    if (g93Failures != 0) {
        std::cerr << "[FAIL] Suite 93: " << g93Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 93: sorting keeps formula columns consistent (also on Undo / "
                 "Redo) and rows beyond a column's data stay empty in readers, statistics, "
                 "formulas, curves and after reopening.\n";
    return 0;
}
