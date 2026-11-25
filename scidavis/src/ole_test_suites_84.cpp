#include "ole_test_common.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

// =============================================================================
// Suite 84: project serialization fidelity. Every check saves a project (or template) and
//           reads it back through the real loaders:
//   84a  windows of two same-named sibling folders come back into their own folder
//        (openProject and appendProject), and addFolder() never repeats a sibling name
//   84b  fractional grid pen widths survive save/load (and the Undo snapshot string);
//        integer widths still read as before
//   84c  matrix coordinates, 3D axis ranges and histogram binning keep full precision
//   84d  empty <note>/<matrix>/<TableStatistics>/<spectrogram> blocks do not crash the
//        loader; unrecoverable ones are reported as a truncated project
//   84e  table template comments with tab/line break do not cut off later columns
//   84f  "<folder>" / "</folder>" lines inside a note body or the results log do not move
//        windows into another folder or create folders (open, append, .gz)
// =============================================================================

namespace {

struct NonInteractiveGuard84
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard84()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard84()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard84
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard84(ApplicationWindow *w = nullptr) : win(w)
    {
        if (win) {
            win->setHeadlessMode(true);
            win->applyUserSettings();
            win->confirmCloseTable = false;
            win->confirmCloseMatrix = false;
            win->confirmClosePlot2D = false;
            win->confirmClosePlot3D = false;
            win->confirmCloseFolder = false;
            win->confirmCloseNotes = false;
        }
    }
    ~AppWinGuard84() { reset(); }
    AppWinGuard84(const AppWinGuard84 &) = delete;
    AppWinGuard84 &operator=(const AppWinGuard84 &) = delete;
    void reset()
    {
        if (win) {
            win->savedProject();
            win->setOleEmbeddedMode(true);
            delete win;
            win = nullptr;
        }
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

MyWidget *findWindow84(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            return w;
    return nullptr;
}

QStringList windowNames84(Folder *f)
{
    QStringList names;
    if (f)
        for (MyWidget *w : f->windowsList())
            names << w->name();
    return names;
}

// The first folder (depth first) that has two direct children called `dupName`.
Folder *findDupParent84(Folder *root, const QString &dupName)
{
    if (!root)
        return nullptr;
    int n = 0;
    for (Folder *c : root->folders())
        if (c->name() == dupName)
            ++n;
    if (n >= 2)
        return root;
    for (Folder *c : root->folders())
        if (Folder *hit = findDupParent84(c, dupName))
            return hit;
    return nullptr;
}

QString readText84(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

bool writeText84(const QString &path, const QString &text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QByteArray b = text.toUtf8();
    return f.write(b) == b.size();
}

bool sameDouble84(double a, double b)
{
    return a == b || std::fabs(a - b) <= 1e-12 * std::max(std::fabs(a), std::fabs(b));
}

// Opens `path` in a fresh headless window. Returns the loadProject() result; `loadedOut`
// receives whether the call returned at all (a crash would end the test run instead).
bool loadInFreshApp84(const QString &path, const std::function<void(ApplicationWindow *)> &inspect = {})
{
    AppWinGuard84 rd(new ApplicationWindow());
    const bool ok = rd->loadProject(path);
    if (inspect)
        inspect(rd);
    return ok;
}

} // namespace

int runSuite84ProjectSerializationFidelity(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 84: Project Serialization Fidelity...\n";
    NonInteractiveGuard84 niGuard;

    QTemporaryDir tmp84;
    CHECK_COND(tmp84.isValid(), "84: temp dir creation failed");
    const QString prj = tmp84.filePath(QStringLiteral("fidelity84.sciprj"));

    // Values that the old 6-significant-digit writers rounded.
    const double mxs = 166.2375, mxe = 200.98765432109876, mys = -3.14159265358979,
                 mye = 2.718281828459045;
    const double hBin = 0.123456789012, hBegin = 0.987654321098, hEnd = 5.43210987654321;
    const double sx0 = 0.123456789012345, sx1 = 1.23456789012345, sy0 = -0.98765432109876,
                 sy1 = 0.87654321098765;

    double src3dXs = 0, src3dXe = 0, src3dYs = 0, src3dYe = 0;

    // ---------------------------------------------------------------------------------------
    // Build the source project: two sibling folders both called "Dup84", each with a table
    // and a graph; the second one also gets a 3D surface. The graph in the first folder has
    // a fractional grid and a histogram with long binning values; a matrix with long
    // coordinates sits in the project folder.
    // ---------------------------------------------------------------------------------------
    {
        AppWinGuard84 src(new ApplicationWindow());
        Folder *root = src->projectFolder();
        CHECK_COND(root != nullptr, "84: source project folder missing");

        src->changeFolder(root, true);
        src->addFolder();
        Folder *dupA = root->folders().value(root->folders().size() - 1);
        src->changeFolder(root, true);
        src->addFolder();
        Folder *dupB = root->folders().value(root->folders().size() - 1);
        CHECK_COND(dupA && dupB && dupA != dupB, "84a: addFolder did not create two folders");
        CHECK_COND(dupA->name() != dupB->name(), "84a: addFolder gave two siblings the same name");
        // Same-named siblings still occur (older files, drag & drop between folders); force it.
        dupA->setName(QStringLiteral("Dup84"));
        dupB->setName(QStringLiteral("Dup84"));

        // --- folder A: table + histogram graph with fractional grid
        CHECK_COND(src->changeFolder(dupA, true) || src->current_folder == dupA,
                   "84a: could not enter first Dup84");
        Table *tA = src->newTable(QStringLiteral("T84A"), 8, 2);
        CHECK_COND(tA != nullptr, "84a: table A creation failed");
        for (int r = 0; r < 8; ++r) {
            tA->setCell(r, 0, r);
            tA->setCell(r, 1, 0.5 + (r % 4));
        }
        MultiLayer *gA = src->multilayerPlot(tA, QStringList() << tA->colName(1), Graph::Histogram);
        CHECK_COND(gA && gA->activeGraph(), "84a: histogram graph A creation failed");
        gA->setName(QStringLiteral("G84A"));
        CHECK_COND(gA->folder() == dupA, "84a: graph A is not in the first Dup84 folder");
        Graph *g = gA->activeGraph();
        QwtHistogram *h = dynamic_cast<QwtHistogram *>(g->curve(0));
        CHECK_COND(h != nullptr, "84c: histogram curve missing");
        h->setBinning(false, hBin, hBegin, hEnd);
        h->loadData();
        CHECK_COND(g->grid() != nullptr, "84b: graph has no grid");
        g->grid()->setMajPenX(QPen(QColor(Qt::black), 0.8));
        g->grid()->setMinPenY(QPen(QColor(Qt::gray), 0.3));
        g->grid()->setMajPenY(QPen(QColor(Qt::black), 2.0));

        // 84b: the string used for both the project file and Undo snapshots
        {
            const QStringList f = g->grid()->saveToString().trimmed().split('\t');
            CHECK_COND(f.size() == 21, "84b: unexpected grid field count " << f.size());
            CHECK_COND(f[7] == QLatin1String("0.8"),
                       "84b: major X grid width saved as '" << f[7].toStdString() << "'");
            CHECK_COND(f[16] == QLatin1String("0.3"),
                       "84b: minor Y grid width saved as '" << f[16].toStdString() << "'");
            CHECK_COND(f[13] == QLatin1String("2"),
                       "84b: integer grid width no longer written as an integer: '"
                               << f[13].toStdString() << "'");
        }

        // --- folder B: table + line graph + 3D surface
        CHECK_COND(src->changeFolder(dupB, true) || src->current_folder == dupB,
                   "84a: could not enter second Dup84");
        Table *tB = src->newTable(QStringLiteral("T84B"), 4, 2);
        CHECK_COND(tB != nullptr, "84a: table B creation failed");
        for (int r = 0; r < 4; ++r) {
            tB->setCell(r, 0, r);
            tB->setCell(r, 1, r * r);
        }
        MultiLayer *gB = src->multilayerPlot(tB, QStringList() << tB->colName(1), Graph::LineSymbols);
        CHECK_COND(gB != nullptr, "84a: graph B creation failed");
        gB->setName(QStringLiteral("G84B"));
        CHECK_COND(gB->folder() == dupB, "84a: graph B is not in the second Dup84 folder");
        Graph3D *sB = src->newPlot3D(QStringLiteral("S84B"), QStringLiteral("x*y"), sx0, sx1, sy0,
                                     sy1, -1.0, 1.0);
        CHECK_COND(sB != nullptr, "84a: 3D plot creation failed");
        CHECK_COND(sB->folder() == dupB, "84a: 3D plot is not in the second Dup84 folder");
        src3dXs = sB->xStart();
        src3dXe = sB->xStop();
        src3dYs = sB->yStart();
        src3dYe = sB->yStop();
        {
            const QString s3 = sB->saveToString(QString());
            CHECK_COND(s3.contains(QString::number(src3dXs, 'g', QLocale::FloatingPointShortest))
                               && s3.contains(QString::number(src3dYe, 'g', QLocale::FloatingPointShortest)),
                       "84c: 3D axis range not saved with full precision");
        }

        // --- project folder: matrix with long coordinates
        src->changeFolder(root, true);
        Matrix *m = src->newMatrix(QStringLiteral("M84"), 3, 3);
        CHECK_COND(m != nullptr, "84c: matrix creation failed");
        m->setCoordinates(mxs, mxe, mys, mye);

        CHECK_COND(src->saveFolder(src->projectFolder(), prj), "84: saving the project failed");

        // 84a (addFolder): after a rename, the next new folder must not repeat a sibling name.
        // The old count-based naming produced a second "New Folder (2)" here.
        src->changeFolder(root, true);
        src->addFolder();
        Folder *n1 = root->folders().last();
        src->changeFolder(root, true);
        src->addFolder();
        Folder *n2 = root->folders().last();
        n1->setName(QStringLiteral("Renamed84"));
        src->changeFolder(root, true);
        src->addFolder();
        Q_UNUSED(n2);
        QStringList siblings = root->subfolders();
        siblings.removeAll(QStringLiteral("Dup84"));
        CHECK_COND(siblings.removeDuplicates() == 0,
                   "84a: addFolder repeated a sibling name: " << siblings.join(',').toStdString());
    }

    const QString saved = readText84(prj);
    {
        const int at = saved.indexOf(QStringLiteral("<coordinates"));
        CHECK_COND(at >= 0, "84c: matrix coordinates element missing from the project");
        const QString coords = saved.mid(at, saved.indexOf(QLatin1Char('>'), at) - at + 1);
        CHECK_COND(coords.contains(QStringLiteral("x_start=\"166.2375\""))
                           && coords.contains(QStringLiteral("x_end=\"%1\"")
                                                      .arg(QString::number(mxe, 'g', QLocale::FloatingPointShortest))),
                   "84c: matrix coordinates saved as " << coords.toStdString());
    }

    // ---------------------------------------------------------------------------------------
    // 84a-c: reopen and check folder membership and values
    // ---------------------------------------------------------------------------------------
    auto checkDupFolders = [&](ApplicationWindow *app, const char *tag) -> int {
        Folder *parent = findDupParent84(app->projectFolder(), QStringLiteral("Dup84"));
        CHECK_COND(parent != nullptr, tag << ": the two Dup84 folders did not come back");
        QList<Folder *> dups;
        for (Folder *c : parent->folders())
            if (c->name() == QLatin1String("Dup84"))
                dups << c;
        CHECK_COND(dups.size() == 2, tag << ": expected 2 Dup84 folders, got " << dups.size());
        const QStringList a = windowNames84(dups[0]);
        const QStringList b = windowNames84(dups[1]);
        CHECK_COND(a.contains(QStringLiteral("T84A")) && a.contains(QStringLiteral("G84A"))
                           && !a.contains(QStringLiteral("G84B")) && !a.contains(QStringLiteral("S84B")),
                   tag << ": first Dup84 holds " << a.join(',').toStdString());
        CHECK_COND(b.contains(QStringLiteral("T84B")) && b.contains(QStringLiteral("G84B"))
                           && b.contains(QStringLiteral("S84B")),
                   tag << ": second Dup84 holds " << b.join(',').toStdString());
        return 0;
    };

    {
        AppWinGuard84 rd(new ApplicationWindow());
        CHECK_COND(rd->loadProject(prj), "84: reopening the project failed");
        if (checkDupFolders(rd, "84a-open") != 0)
            return 1;

        // 84b: grid widths
        MultiLayer *gA = qobject_cast<MultiLayer *>(findWindow84(rd, QStringLiteral("G84A")));
        CHECK_COND(gA && gA->activeGraph() && gA->activeGraph()->grid(), "84b: G84A missing after reload");
        Grid *grid = gA->activeGraph()->grid();
        CHECK_COND(std::fabs(grid->majPenX().widthF() - 0.8) < 1e-9,
                   "84b: major X grid width reloaded as " << grid->majPenX().widthF());
        CHECK_COND(std::fabs(grid->minPenY().widthF() - 0.3) < 1e-9,
                   "84b: minor Y grid width reloaded as " << grid->minPenY().widthF());
        CHECK_COND(std::fabs(grid->majPenY().widthF() - 2.0) < 1e-9,
                   "84b: integer grid width reloaded as " << grid->majPenY().widthF());

        // 84c: histogram binning
        QwtHistogram *h = dynamic_cast<QwtHistogram *>(gA->activeGraph()->curve(0));
        CHECK_COND(h != nullptr, "84c: histogram missing after reload");
        CHECK_COND(sameDouble84(h->binSize(), hBin) && sameDouble84(h->begin(), hBegin)
                           && sameDouble84(h->end(), hEnd),
                   "84c: histogram binning reloaded as " << h->binSize() << "/" << h->begin()
                                                         << "/" << h->end());

        // 84c: matrix coordinates (exact round trip)
        Matrix *m = rd->matrix(QStringLiteral("M84"));
        CHECK_COND(m != nullptr, "84c: M84 missing after reload");
        CHECK_COND(m->xStart() == mxs && m->xEnd() == mxe && m->yStart() == mys && m->yEnd() == mye,
                   "84c: matrix coordinates reloaded as " << m->xStart() << " " << m->xEnd() << " "
                                                          << m->yStart() << " " << m->yEnd());

        // 84c: 3D axis ranges
        Graph3D *s = qobject_cast<Graph3D *>(findWindow84(rd, QStringLiteral("S84B")));
        CHECK_COND(s != nullptr, "84c: S84B missing after reload");
        CHECK_COND(sameDouble84(s->xStart(), src3dXs) && sameDouble84(s->xStop(), src3dXe)
                           && sameDouble84(s->yStart(), src3dYs) && sameDouble84(s->yStop(), src3dYe),
                   "84c: 3D axis range reloaded as " << s->xStart() << ".." << s->xStop() << ", "
                                                     << s->yStart() << ".." << s->yStop());
    }

    // 84a: the same through File > Append Project
    {
        AppWinGuard84 ap(new ApplicationWindow());
        ap->setProperty("torigraph_last_append_ok", QVariant());
        ap->appendProject(prj);
        CHECK_COND(ap->property("torigraph_last_append_ok").toBool(), "84a-append: appendProject failed");
        if (checkDupFolders(ap, "84a-append") != 0)
            return 1;
    }

    // 84b: an old integer-width grid line still loads
    {
        AppWinGuard84 lg(new ApplicationWindow());
        MultiLayer *ml = lg->newGraph(QStringLiteral("GLegacy84"));
        CHECK_COND(ml && ml->activeGraph() && ml->activeGraph()->grid(), "84b: legacy graph setup failed");
        Grid *grid = ml->activeGraph()->grid();
        QStringList f = grid->saveToString().trimmed().split('\t');
        CHECK_COND(f.size() == 21, "84b: unexpected grid field count");
        f[7] = QStringLiteral("3");
        f[10] = QStringLiteral("1");
        grid->load(f);
        CHECK_COND(grid->majPenX().widthF() == 3.0 && grid->minPenX().widthF() == 1.0,
                   "84b: integer grid widths read as " << grid->majPenX().widthF() << "/"
                                                       << grid->minPenX().widthF());
    }

    // ---------------------------------------------------------------------------------------
    // 84d: hand-made projects with empty blocks
    // ---------------------------------------------------------------------------------------
    {
        const QString basePrj = tmp84.filePath(QStringLiteral("base84.sciprj"));
        QString spectroText;
        {
            AppWinGuard84 b(new ApplicationWindow());
            Table *t = b->newTable(QStringLiteral("Base84"), 3, 2);
            CHECK_COND(t != nullptr, "84d: base table creation failed");
            Note *n = b->newNote(QStringLiteral("Note84"));
            CHECK_COND(n != nullptr, "84d: base note creation failed");
            CHECK_COND(b->saveFolder(b->projectFolder(), basePrj), "84d: saving base project failed");

            Matrix *sm = b->newMatrix(QStringLiteral("MS84"), 4, 4);
            CHECK_COND(sm != nullptr, "84d: spectrogram matrix creation failed");
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    sm->setCell(r, c, r * 4 + c);
            MultiLayer *sp = b->plotSpectrogram(sm, Graph::ColorMap);
            CHECK_COND(sp != nullptr, "84d: spectrogram plot creation failed");
            const QString spPrj = tmp84.filePath(QStringLiteral("spectro84.sciprj"));
            CHECK_COND(b->saveFolder(b->projectFolder(), spPrj), "84d: saving spectrogram project failed");
            spectroText = readText84(spPrj);
        }
        const QString base = readText84(basePrj);
        CHECK_COND(base.contains(QStringLiteral("<note>")), "84d: base project has no note");

        // Empty note body: the note itself is kept, just without text.
        {
            QString txt = base;
            const QRegularExpression body(QStringLiteral("AutoExec\\t[01]\\n<content>\\n[\\s\\S]*?</content>\\n"));
            CHECK_COND(txt.contains(body), "84d: note body not found in base project");
            txt.replace(body, QString());
            const QString p = tmp84.filePath(QStringLiteral("empty_note84.sciprj"));
            CHECK_COND(writeText84(p, txt), "84d: writing empty-note project failed");
            bool noteFound = false;
            const bool ok = loadInFreshApp84(p, [&](ApplicationWindow *app) {
                noteFound = findWindow84(app, QStringLiteral("Note84")) != nullptr;
            });
            CHECK_COND(ok, "84d: project with an empty note body did not load");
            CHECK_COND(noteFound, "84d: empty note was dropped");
        }

        struct Case84
        {
            const char *tag;
            QString block;
        };
        const QList<Case84> cases = {
            { "empty <matrix>", QStringLiteral("<matrix>\n</matrix>\n") },
            { "empty <TableStatistics>", QStringLiteral("<TableStatistics>\n</TableStatistics>\n") },
            { "header-only <TableStatistics>",
              QStringLiteral("<TableStatistics>\nTS84\tBase84\tcol\t01.01.2026\n</TableStatistics>\n") },
        };
        int idx = 0;
        for (const Case84 &c : cases) {
            QString txt = base;
            txt += c.block;
            const QString p = tmp84.filePath(QStringLiteral("empty_block84_%1.sciprj").arg(idx++));
            CHECK_COND(writeText84(p, txt), "84d: writing project failed for " << c.tag);
            const bool ok = loadInFreshApp84(p);
            CHECK_COND(!ok, "84d: project with " << c.tag << " was not reported as damaged");
            std::cout << "[INFO] Suite 84d: " << c.tag << " rejected without crash\n";
        }

        // Empty <spectrogram> inside a graph
        {
            CHECK_COND(spectroText.contains(QStringLiteral("<spectrogram>\n")),
                       "84d: spectrogram block not found in saved project");
            QString txt = spectroText;
            txt.replace(QRegularExpression(QStringLiteral("<spectrogram>\\n[\\s\\S]*?</spectrogram>\\n")),
                        QStringLiteral("<spectrogram>\n</spectrogram>\n"));
            const QString p = tmp84.filePath(QStringLiteral("empty_spectro84.sciprj"));
            CHECK_COND(writeText84(p, txt), "84d: writing empty-spectrogram project failed");
            const bool ok = loadInFreshApp84(p);
            CHECK_COND(!ok, "84d: project with an empty <spectrogram> was not reported as damaged");
            std::cout << "[INFO] Suite 84d: empty <spectrogram> rejected without crash\n";
        }
    }

    // ---------------------------------------------------------------------------------------
    // 84e: table template with a comment containing a line break and a tab
    // ---------------------------------------------------------------------------------------
    {
        AppWinGuard84 tw(new ApplicationWindow());
        Table *t = tw->newTable(QStringLiteral("Tpl84"), 3, 3);
        CHECK_COND(t != nullptr, "84e: template table creation failed");
        t->column(0)->setComment(QStringLiteral("line one\nline two\tafter tab"));
        t->column(1)->setComment(QStringLiteral("second"));
        t->column(2)->setComment(QStringLiteral("third"));
        const QString tpl = SciDAVis::schemaVersion() + " template file\n"
                + t->saveAsTemplate(tw->windowGeometryInfo(t));

        // Same parsing steps as ApplicationWindow::openTemplate for a table template.
        QString copy = tpl;
        QTextStream ts(&copy, QIODevice::ReadOnly);
        ts.readLine(); // version line
        QString templateType;
        int rows = 0, cols = 0;
        ts >> templateType;
        ts >> rows;
        ts >> cols;
        ts.skipWhiteSpace();
        ts.readLine(); // geometry
        QStringList lst;
        while (!ts.atEnd())
            lst << ts.readLine();
        CHECK_COND(templateType == QLatin1String("<table>") && rows == 3 && cols == 3,
                   "84e: unexpected template header");
        Table *t2 = tw->newTable(QStringLiteral("Tpl84b"), rows, cols);
        CHECK_COND(t2 != nullptr, "84e: target table creation failed");
        t2->restore(lst);
        CHECK_COND(t2->column(0)->comment() == QLatin1String("line one line two after tab"),
                   "84e: first comment reloaded as '" << t2->column(0)->comment().toStdString() << "'");
        CHECK_COND(t2->column(1)->comment() == QLatin1String("second")
                           && t2->column(2)->comment() == QLatin1String("third"),
                   "84e: later comments lost: '" << t2->column(1)->comment().toStdString() << "', '"
                                                 << t2->column(2)->comment().toStdString() << "'");
    }

    // ---------------------------------------------------------------------------------------
    // 84f: folder marker lines inside a note body or the results log are text, not folders.
    //      The two loader passes skip different blocks; matching folders by <folder> line
    //      count shifted every later window by one when such a line appeared in a note
    //      (second pass) or in the log (first pass).
    // ---------------------------------------------------------------------------------------
    {
        const QString prjF = tmp84.filePath(QStringLiteral("markers84f.sciprj"));
        const QString noteText = QStringLiteral(
                "日本語のメモ（UTF-8 の多バイト文字）\n"
                "<folder>\tDup84f\tfake\tfake\n"
                "</folder>\n"
                "<folder>\tOther84f\tfake\tfake\n"
                "end of note");
        {
            AppWinGuard84 src(new ApplicationWindow());
            Folder *root = src->projectFolder();
            CHECK_COND(root != nullptr, "84f: source project folder missing");
            QList<Folder *> made;
            for (int i = 0; i < 3; ++i) {
                src->changeFolder(root, true);
                src->addFolder();
                made << root->folders().value(root->folders().size() - 1);
            }
            CHECK_COND(made[0] && made[1] && made[2], "84f: addFolder failed");
            made[0]->setName(QStringLiteral("Dup84f"));
            made[1]->setName(QStringLiteral("Dup84f"));
            made[2]->setName(QStringLiteral("Other84f"));

            // first Dup84f: the note comes first, so its body precedes the graph in the file
            src->changeFolder(made[0], true);
            Note *n = src->newNote(QStringLiteral("N84fA"));
            CHECK_COND(n != nullptr, "84f: note creation failed");
            n->textWidget()->setPlainText(noteText);
            Table *tA = src->newTable(QStringLiteral("T84fA"), 3, 2);
            CHECK_COND(tA != nullptr, "84f: table A creation failed");
            for (int r = 0; r < 3; ++r) {
                tA->setCell(r, 0, r);
                tA->setCell(r, 1, r + 1);
            }
            MultiLayer *gA = src->multilayerPlot(tA, QStringList() << tA->colName(1),
                                                 Graph::LineSymbols);
            CHECK_COND(gA != nullptr, "84f: graph A creation failed");
            gA->setName(QStringLiteral("G84fA"));

            src->changeFolder(made[1], true);
            Table *tB = src->newTable(QStringLiteral("T84fB"), 3, 2);
            CHECK_COND(tB != nullptr, "84f: table B creation failed");
            for (int r = 0; r < 3; ++r) {
                tB->setCell(r, 0, r);
                tB->setCell(r, 1, 2 * r);
            }
            MultiLayer *gB = src->multilayerPlot(tB, QStringList() << tB->colName(1),
                                                 Graph::LineSymbols);
            CHECK_COND(gB != nullptr, "84f: graph B creation failed");
            gB->setName(QStringLiteral("G84fB"));

            src->changeFolder(made[2], true);
            MultiLayer *gO = src->newGraph(QStringLiteral("G84fO"));
            CHECK_COND(gO != nullptr, "84f: graph O creation failed");
            gO->setName(QStringLiteral("G84fO"));

            src->changeFolder(root, true);
            src->logInfo = QStringLiteral("fit log line\n<folder>\tGhost84f\tfake\tfake\n"
                                          "</folder>\nlast log line\n");
            CHECK_COND(src->saveFolder(src->projectFolder(), prjF), "84f: saving failed");
        }
        const QString savedF = readText84(prjF);
        CHECK_COND(savedF.contains(QStringLiteral("\n<folder>\tDup84f\tfake\tfake\n"))
                           && savedF.contains(QStringLiteral("\n<folder>\tGhost84f\tfake\tfake\n")),
                   "84f: the fake folder lines were not written verbatim (test premise)");

        std::function<bool(Folder *, const QString &)> hasFolderNamed =
                [&](Folder *f, const QString &name) -> bool {
            if (!f)
                return false;
            for (Folder *c : f->folders())
                if (c->name() == name || hasFolderNamed(c, name))
                    return true;
            return false;
        };
        auto checkMarkers = [&](ApplicationWindow *app, const char *tag) -> int {
            Folder *parent = findDupParent84(app->projectFolder(), QStringLiteral("Dup84f"));
            CHECK_COND(parent != nullptr, tag << ": the two Dup84f folders did not come back");
            QList<Folder *> dups;
            Folder *other = nullptr;
            for (Folder *c : parent->folders()) {
                if (c->name() == QLatin1String("Dup84f"))
                    dups << c;
                else if (c->name() == QLatin1String("Other84f"))
                    other = c;
            }
            CHECK_COND(dups.size() == 2 && other != nullptr && parent->folders().size() == 3,
                       tag << ": folders " << parent->subfolders().join(',').toStdString());
            const QStringList a = windowNames84(dups[0]);
            const QStringList b = windowNames84(dups[1]);
            const QStringList o = windowNames84(other);
            CHECK_COND(a.contains(QStringLiteral("N84fA")) && a.contains(QStringLiteral("T84fA"))
                               && a.contains(QStringLiteral("G84fA")) && a.size() == 3,
                       tag << ": first Dup84f holds " << a.join(',').toStdString());
            CHECK_COND(b.contains(QStringLiteral("T84fB")) && b.contains(QStringLiteral("G84fB"))
                               && b.size() == 2,
                       tag << ": second Dup84f holds " << b.join(',').toStdString());
            CHECK_COND(o == QStringList() << QStringLiteral("G84fO"),
                       tag << ": Other84f holds " << o.join(',').toStdString());
            CHECK_COND(!hasFolderNamed(app->projectFolder(), QStringLiteral("Ghost84f")),
                       tag << ": a folder was created from a results-log line");
            Note *n = qobject_cast<Note *>(findWindow84(app, QStringLiteral("N84fA")));
            CHECK_COND(n && n->text().trimmed() == noteText,
                       tag << ": note text changed: '"
                           << (n ? n->text().toStdString() : std::string("(missing)")) << "'");
            return 0;
        };

        {
            AppWinGuard84 rd(new ApplicationWindow());
            CHECK_COND(rd->loadProject(prjF), "84f: reopening failed");
            if (checkMarkers(rd, "84f-open") != 0)
                return 1;
        }
        {
            AppWinGuard84 ap(new ApplicationWindow());
            ap->setProperty("torigraph_last_append_ok", QVariant());
            ap->appendProject(prjF);
            CHECK_COND(ap->property("torigraph_last_append_ok").toBool(),
                       "84f-append: appendProject failed");
            if (checkMarkers(ap, "84f-append") != 0)
                return 1;
        }
        // compressed project: the same position matching through the decompressed stream
        {
            AppWinGuard84 gz(new ApplicationWindow());
            CHECK_COND(gz->loadProject(prjF), "84f-gz: reopening for the .gz copy failed");
            const QString prjGz = tmp84.filePath(QStringLiteral("markers84f.sciprj.gz"));
            CHECK_COND(gz->writeProjectFile(gz->projectFolder(), prjGz),
                       "84f-gz: saving compressed failed");
            AppWinGuard84 rd(new ApplicationWindow());
            CHECK_COND(rd->loadProject(prjGz), "84f-gz: reopening the compressed project failed");
            if (checkMarkers(rd, "84f-gz") != 0)
                return 1;
        }
    }

    std::cout << "[PASS] Suite 84 passed.\n";
    return 0;
}
