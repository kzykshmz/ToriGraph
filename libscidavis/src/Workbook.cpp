/***************************************************************************
    File                 : Workbook.cpp
    Project              : ToriGraph
    Description          : Workbooks: several worksheets shown as sheet tabs of one
                           window. See Workbook.h for the design.
 ***************************************************************************/

#include "Workbook.h"

#include "ApplicationWindow.h"
#include "Folder.h"
#include "I18nJapanese.h"
#include "MyWidget.h"
#include "Table.h"

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLayout>
#include <QLineEdit>
#include <QMdiArea>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

const char *const kManagerObjectName = "torigraph_workbook_manager";
const char *const kBarObjectName = "torigraph_workbook_sheet_bar";
const char *const kTabBarObjectName = "torigraph_workbook_tabs";

QString wbTr(const char *s)
{
    return QCoreApplication::translate("WorkbookManager", s);
}

void setErr(QString *err, const QString &msg)
{
    if (err)
        *err = msg;
}

bool nonInteractive(ApplicationWindow *app)
{
    return ApplicationWindow::isNonInteractiveContext(app);
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / lookup
// ---------------------------------------------------------------------------
WorkbookManager::WorkbookManager(ApplicationWindow *app) : QObject(app), d_app(app)
{
    setObjectName(QString::fromLatin1(kManagerObjectName));
    registerTranslations();
    if (app)
        app->installEventFilter(this); // LanguageChange -> retranslate the action
}

WorkbookManager *WorkbookManager::existing(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    return app->findChild<WorkbookManager *>(QString::fromLatin1(kManagerObjectName),
                                             Qt::FindDirectChildrenOnly);
}

WorkbookManager *WorkbookManager::instance(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (WorkbookManager *m = existing(app))
        return m;
    return new WorkbookManager(app);
}

void WorkbookManager::shutdown()
{
    d_shutdown = true;
    for (Book &b : d_books)
        for (Table *t : b.sheets)
            if (t) {
                t->removeEventFilter(this);
                disconnect(t, nullptr, this, nullptr);
            }
    if (d_app)
        d_app->removeEventFilter(this);
    d_books.clear();
    d_user_hidden.clear();
    d_last_geometry.clear();
    d_last_maximized.clear();
}

QString WorkbookManager::sanitizeName(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (const QChar ch : raw.trimmed()) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-'))
            out += ch;
        else
            out += QLatin1Char('_');
    }
    static const QRegularExpression kRuns(QStringLiteral("_+"));
    out.replace(kRuns, QStringLiteral("_"));
    while (out.startsWith(QLatin1Char('_')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('_')))
        out.chop(1);
    return out;
}

QString WorkbookManager::sheetLabel(const QString &book, const QString &tableName)
{
    if (!book.isEmpty() && tableName.size() > book.size() + 1 && tableName.startsWith(book)) {
        const QChar sep = tableName.at(book.size());
        if (sep == QLatin1Char('_') || sep == QLatin1Char('-'))
            return tableName.mid(book.size() + 1);
    }
    return tableName;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
WorkbookManager::Book *WorkbookManager::find(const QString &name)
{
    for (Book &b : d_books)
        if (b.name == name)
            return &b;
    return nullptr;
}

const WorkbookManager::Book *WorkbookManager::find(const QString &name) const
{
    for (const Book &b : d_books)
        if (b.name == name)
            return &b;
    return nullptr;
}

WorkbookManager::Book *WorkbookManager::bookFor(const MyWidget *w)
{
    if (!w)
        return nullptr;
    for (Book &b : d_books)
        for (Table *t : b.sheets)
            if (t == w)
                return &b;
    return nullptr;
}

const WorkbookManager::Book *WorkbookManager::bookFor(const MyWidget *w) const
{
    if (!w)
        return nullptr;
    for (const Book &b : d_books)
        for (Table *t : b.sheets)
            if (t == w)
                return &b;
    return nullptr;
}

QStringList WorkbookManager::bookNames() const
{
    QStringList out;
    for (const Book &b : d_books)
        out << b.name;
    return out;
}

void WorkbookManager::eraseBook(const QString &name)
{
    for (int i = d_books.size() - 1; i >= 0; --i)
        if (d_books.at(i).name == name)
            d_books.removeAt(i);
}

bool WorkbookManager::hasBook(const QString &book) const
{
    return find(book) != nullptr;
}

QString WorkbookManager::bookOf(const MyWidget *w) const
{
    const Book *b = bookFor(w);
    return b ? b->name : QString();
}

QList<Table *> WorkbookManager::sheets(const QString &book) const
{
    const Book *b = find(book);
    return b ? b->sheets : QList<Table *>();
}

Table *WorkbookManager::currentSheet(const QString &book) const
{
    const Book *b = find(book);
    return b ? b->current : nullptr;
}

QString WorkbookManager::uniqueBookName(const QString &base) const
{
    QString b = sanitizeName(base);
    if (b.isEmpty())
        b = QStringLiteral("Book");
    if (!find(b))
        return b;
    for (int i = 2;; ++i) {
        const QString cand = b + QString::number(i);
        if (!find(cand))
            return cand;
    }
}

Folder *WorkbookManager::bookFolder(const Book &b) const
{
    if (b.current && b.current->folder())
        return b.current->folder();
    for (Table *t : b.sheets)
        if (t && t->folder())
            return t->folder();
    return nullptr;
}

bool WorkbookManager::isShown(Table *t) const
{
    return t && d_app && !t->isHidden() && !d_app->hidden(t);
}

QString WorkbookManager::nextSheetLabel(const Book &b) const
{
    QSet<QString> used;
    for (Table *t : b.sheets)
        if (t)
            used.insert(sheetLabel(b.prefix, t->name()).toLower());
    for (int n = 1;; ++n) {
        const QString lbl = QStringLiteral("Sheet%1").arg(n);
        if (!used.contains(lbl.toLower()) && !d_app->alreadyUsedName(b.prefix + QLatin1Char('_') + lbl))
            return lbl;
    }
}

QJsonObject WorkbookManager::describe(const QString &book) const
{
    QJsonObject o;
    const Book *b = find(book);
    if (!b)
        return o;
    o[QStringLiteral("name")] = b->name;
    o[QStringLiteral("prefix")] = b->prefix;
    o[QStringLiteral("current")] = b->current ? b->current->name() : QString();
    QJsonArray arr;
    for (Table *t : b->sheets) {
        if (!t)
            continue;
        QJsonObject s;
        s[QStringLiteral("table")] = t->name();
        s[QStringLiteral("label")] = sheetLabel(b->prefix, t->name());
        arr.append(s);
    }
    o[QStringLiteral("sheets")] = arr;
    Folder *f = bookFolder(*b);
    o[QStringLiteral("folder")] = f ? f->path() : QString();
    return o;
}

// ---------------------------------------------------------------------------
// Book membership
// ---------------------------------------------------------------------------
void WorkbookManager::watch(Table *t)
{
    if (!t)
        return;
    t->installEventFilter(this);
    connect(t, &MyWidget::closedWindow, this, [this](MyWidget *w) {
        if (!d_shutdown)
            removeSheetInternal(w, false);
    });
    connect(t, &QObject::destroyed, this, [this](QObject *o) {
        if (!d_shutdown)
            removeSheetInternal(o, true);
    });
    connect(t, &QObject::objectNameChanged, this, [this, t]() {
        if (d_shutdown)
            return;
        if (const Book *b = bookFor(t))
            scheduleRefresh(b->name);
    });
    d_user_hidden[t] = d_app ? d_app->hidden(t) : false;
    d_last_geometry[t] = t->geometry();
    d_last_maximized[t] = t->isMaximized();
}

void WorkbookManager::unwatch(Table *t)
{
    if (!t)
        return;
    t->removeEventFilter(this);
    disconnect(t, nullptr, this, nullptr);
    d_user_hidden.remove(t);
    d_last_geometry.remove(t);
    d_last_maximized.remove(t);
    removeBar(t);
}

void WorkbookManager::prune(Book &b)
{
    Folder *f = bookFolder(b);
    for (int i = b.sheets.size() - 1; i >= 0; --i) {
        Table *t = b.sheets.at(i);
        if (!t || !t->folder() || t->folder() != f) {
            b.sheets.removeAt(i);
            if (t)
                unwatch(t);
            if (b.current == t)
                b.current = nullptr;
        }
    }
    if (!b.current && !b.sheets.isEmpty())
        b.current = b.sheets.first();
}

void WorkbookManager::removeSheetInternal(QObject *sheet, bool destroyed)
{
    for (int bi = 0; bi < d_books.size(); ++bi) {
        Book &b = d_books[bi];
        int idx = -1;
        for (int i = 0; i < b.sheets.size(); ++i)
            if (static_cast<QObject *>(b.sheets.at(i)) == sheet) {
                idx = i;
                break;
            }
        if (idx < 0)
            continue;
        const bool wasCurrent = static_cast<QObject *>(b.current) == sheet;
        const bool wasShown = !d_user_hidden.value(sheet, true);
        const QRect geom = d_last_geometry.value(sheet);
        const bool wasMax = d_last_maximized.value(sheet, false);
        b.sheets.removeAt(idx);
        if (!destroyed)
            unwatch(static_cast<Table *>(sheet));
        d_user_hidden.remove(sheet);
        d_last_geometry.remove(sheet);
        d_last_maximized.remove(sheet);
        const QString name = b.name;
        if (b.sheets.isEmpty()) {
            d_books.removeAt(bi);
        } else if (wasCurrent) {
            b.current = b.sheets.at(std::min(idx, int(b.sheets.size()) - 1));
            if (wasShown)
                scheduleShow(name, geom, wasMax);
        }
        scheduleRefresh(name);
        return;
    }
}

void WorkbookManager::scheduleShow(const QString &book, const QRect &geometry, bool maximized)
{
    QTimer::singleShot(0, this, [this, book, geometry, maximized]() {
        if (d_shutdown)
            return;
        Book *b = find(book);
        if (!b || !b->current)
            return;
        Table *t = b->current;
        if (t->folder() != d_app->current_folder || isShown(t))
            return;
        showInPlaceOf(t, geometry, maximized);
        refreshBars(book);
    });
}

void WorkbookManager::showInPlaceOf(Table *t, const QRect &geometry, bool maximized)
{
    if (!t || !d_app)
        return;
    const bool prev = d_switching;
    d_switching = true;
    // activate first (it restores a maximized window to normal), then move it into place;
    // both happen before the next repaint
    d_app->activateSubWindow(t);
    if (maximized && !d_app->isTabbedWorkspaceView())
        t->setMaximized();
    else if (geometry.isValid() && !d_app->isTabbedWorkspaceView())
        t->setGeometry(geometry);
    d_user_hidden[t] = false;
    d_switching = prev;
}

QString WorkbookManager::createBook(const QString &name, int sheetCount, QString *err)
{
    if (!d_app) {
        setErr(err, QStringLiteral("no application window"));
        return QString();
    }
    sheetCount = std::max(1, std::min(sheetCount, 255));
    const QString bookName = uniqueBookName(name.isEmpty() ? QStringLiteral("Book") : name);
    Book nb;
    nb.name = bookName;
    nb.prefix = bookName;
    d_books.append(nb);
    for (int i = 0; i < sheetCount; ++i) {
        if (!addSheet(bookName, QString(), -1, i == 0, err)) {
            if (Book *b = find(bookName))
                if (b->sheets.isEmpty())
                    eraseBook(bookName);
            return QString();
        }
    }
    d_app->modifiedProject();
    return bookName;
}

QString WorkbookManager::createBookFromTables(const QString &name, const QList<Table *> &tables,
                                              Table *current, QString *err)
{
    if (tables.isEmpty()) {
        setErr(err, wbTr("No worksheet was given."));
        return QString();
    }
    Folder *f = tables.first() ? tables.first()->folder() : nullptr;
    for (Table *t : tables) {
        if (!t || !t->folder()) {
            setErr(err, wbTr("No worksheet was given."));
            return QString();
        }
        if (t->folder() != f) {
            setErr(err, wbTr("All sheets of a workbook must be in the same folder."));
            return QString();
        }
        if (bookFor(t)) {
            setErr(err, wbTr("The worksheet %1 already belongs to the workbook %2.")
                                .arg(t->name(), bookFor(t)->name));
            return QString();
        }
    }
    Book nb;
    nb.name = uniqueBookName(name.isEmpty() ? tables.first()->name() : name);
    nb.prefix = sanitizeName(name.isEmpty() ? tables.first()->name() : name);
    if (nb.prefix.isEmpty())
        nb.prefix = nb.name;
    for (Table *t : tables)
        if (!nb.sheets.contains(t))
            nb.sheets.append(t);
    nb.current = (current && nb.sheets.contains(current)) ? current : nb.sheets.first();
    // the current sheet takes the place of the first visible one
    Table *firstShown = nullptr;
    for (Table *t : nb.sheets)
        if (isShown(t)) {
            firstShown = t;
            break;
        }
    d_books.append(nb);
    for (Table *t : nb.sheets)
        watch(t);
    d_switching = true;
    if (firstShown && firstShown != nb.current && nb.current->folder() == d_app->current_folder) {
        const QRect g = firstShown->geometry();
        const bool max = firstShown->isMaximized();
        showInPlaceOf(nb.current, g, max);
    }
    for (Table *t : nb.sheets)
        if (t != nb.current && !d_app->hidden(t)) {
            d_app->hideWindow(t);
            d_user_hidden[t] = true;
        }
    d_switching = false;
    refreshBars(nb.name);
    refreshExplorer();
    d_app->modifiedProject();
    return nb.name;
}

Table *WorkbookManager::addSheet(const QString &book, const QString &label, int index,
                                 bool activate, QString *err)
{
    Book *b = find(book);
    if (!b) {
        setErr(err, wbTr("There is no workbook named %1.").arg(book));
        return nullptr;
    }
    QString lbl = sanitizeName(label);
    if (lbl.isEmpty())
        lbl = nextSheetLabel(*b);
    QString tname = b->prefix + QLatin1Char('_') + lbl;
    for (int n = 2; d_app->alreadyUsedName(tname); ++n)
        tname = b->prefix + QLatin1Char('_') + lbl + QString::number(n);

    Folder *target = bookFolder(*b);
    Table *t = d_app->newTable(30, 2, tname, QString());
    if (!t) {
        setErr(err, wbTr("The worksheet could not be created."));
        return nullptr;
    }
    // newTable() puts the window into the current folder; keep the book in one folder
    if (target && t->folder() && t->folder() != target) {
        Folder *src = t->folder();
        src->removeWindow(t);
        target->addWindow(t);
        const QList<QTreeWidgetItem *> items =
                d_app->lv.findItems(t->name(), Qt::MatchExactly | Qt::MatchCaseSensitive);
        for (QTreeWidgetItem *it : items)
            if (it->type() == WindowListItem::WindowType
                && static_cast<WindowListItem *>(it)->window() == t)
                delete it;
        if (target != d_app->current_folder)
            t->hide();
    }
    b = find(book); // newTable() may have run nested event handling
    if (!b)
        return t;
    if (index < 0 || index > b->sheets.size())
        index = b->sheets.size();
    b->sheets.insert(index, t);
    watch(t);
    if (!b->current) {
        b->current = t; // first sheet of a new book: shown where newTable() put it
        d_user_hidden[t] = d_app->hidden(t);
    } else if (activate) {
        setCurrentSheet(t, true);
    } else {
        d_switching = true;
        d_app->hideWindow(t);
        d_user_hidden[t] = true;
        d_switching = false;
    }
    refreshBars(book);
    refreshExplorer();
    d_app->modifiedProject();
    return t;
}

bool WorkbookManager::attachTable(const QString &book, Table *t, int index, QString *err)
{
    Book *b = find(book);
    if (!b) {
        setErr(err, wbTr("There is no workbook named %1.").arg(book));
        return false;
    }
    if (!t || !t->folder()) {
        setErr(err, wbTr("No worksheet was given."));
        return false;
    }
    if (const Book *other = bookFor(t)) {
        setErr(err, wbTr("The worksheet %1 already belongs to the workbook %2.")
                            .arg(t->name(), other->name));
        return false;
    }
    if (bookFolder(*b) && t->folder() != bookFolder(*b)) {
        setErr(err, wbTr("All sheets of a workbook must be in the same folder."));
        return false;
    }
    if (index < 0 || index > b->sheets.size())
        index = b->sheets.size();
    b->sheets.insert(index, t);
    watch(t);
    if (!b->current) {
        b->current = t;
    } else {
        // the attached sheet becomes the visible one (it is what the user is looking at)
        setCurrentSheet(t, isShown(t) || isShown(b->current));
    }
    refreshBars(book);
    refreshExplorer();
    d_app->modifiedProject();
    return true;
}

bool WorkbookManager::detachSheet(Table *t, bool show, QString *err)
{
    Book *b = bookFor(t);
    if (!b) {
        setErr(err, wbTr("The worksheet is not in a workbook."));
        return false;
    }
    const QString name = b->name;
    const int idx = b->sheets.indexOf(t);
    const bool wasCurrent = b->current == t;
    const bool wasShown = isShown(t);
    const QRect g = t->geometry();
    const bool wasMax = t->isMaximized();
    b->sheets.removeAt(idx);
    unwatch(t);
    if (b->sheets.isEmpty()) {
        eraseBook(name);
        b = nullptr;
    } else if (wasCurrent) {
        b->current = b->sheets.at(std::min(idx, int(b->sheets.size()) - 1));
        if (wasShown && b->current->folder() == d_app->current_folder)
            showInPlaceOf(b->current, g, wasMax);
    }
    if (show && t->folder() == d_app->current_folder && !isShown(t)) {
        // the sheet comes out as its own window, slightly offset from the book
        QRect ng = g.isValid() ? g.translated(30, 30) : g;
        showInPlaceOf(t, ng, false);
    } else if (!show && !wasCurrent && d_app->hidden(t)) {
        // it was hidden only because another sheet was on top (e.g. dragged to another
        // folder in the explorer): as a stand-alone table it is a normal window again
        d_app->updateWindowLists(t);
        t->setStatus(MyWidget::Normal);
    }
    if (b)
        refreshBars(name);
    refreshExplorer();
    d_app->modifiedProject();
    return true;
}

bool WorkbookManager::dissolveBook(const QString &book, QString *err)
{
    Book *b = find(book);
    if (!b) {
        setErr(err, wbTr("There is no workbook named %1.").arg(book));
        return false;
    }
    const QList<Table *> list = b->sheets;
    Table *cur = b->current;
    const bool shown = isShown(cur);
    QRect g = cur ? cur->geometry() : QRect();
    for (Table *t : list)
        unwatch(t);
    eraseBook(book);
    if (shown) {
        int k = 1;
        for (Table *t : list) {
            if (t == cur || t->folder() != d_app->current_folder)
                continue;
            showInPlaceOf(t, g.isValid() ? g.translated(30 * k, 30 * k) : g, false);
            ++k;
        }
        if (cur)
            d_app->activateSubWindow(cur);
    }
    refreshExplorer();
    d_app->modifiedProject();
    return true;
}

bool WorkbookManager::deleteSheet(Table *t, bool confirm, QString *err)
{
    if (!t) {
        setErr(err, wbTr("No worksheet was given."));
        return false;
    }
    if (confirm && !nonInteractive(d_app)) {
        const auto ans = QMessageBox::question(
                d_app, wbTr("Delete Sheet"),
                wbTr("Delete the sheet %1 and its data?\nThis cannot be undone.").arg(t->name()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return false;
    }
    t->askOnCloseEvent(false);
    t->close();
    return true;
}

bool WorkbookManager::renameSheet(Table *t, const QString &label, QString *err)
{
    Book *b = bookFor(t);
    if (!b) {
        setErr(err, wbTr("The worksheet is not in a workbook."));
        return false;
    }
    const QString lbl = sanitizeName(label);
    if (lbl.isEmpty()) {
        setErr(err, wbTr("Please enter a sheet name (letters, digits, _ or -)."));
        return false;
    }
    const bool prefixed = sheetLabel(b->prefix, t->name()) != t->name();
    const QString newName = prefixed ? b->prefix + QLatin1Char('_') + lbl : lbl;
    if (newName == t->name())
        return true;
    if (!d_app->renameWindow(t, newName)) {
        setErr(err, wbTr("The sheet could not be renamed to %1 (the name is invalid or already used).")
                            .arg(newName));
        return false;
    }
    refreshBars(b->name);
    return true;
}

bool WorkbookManager::renameBook(const QString &book, const QString &newName, QString *err)
{
    Book *b = find(book);
    if (!b) {
        setErr(err, wbTr("There is no workbook named %1.").arg(book));
        return false;
    }
    const QString nn = sanitizeName(newName);
    if (nn.isEmpty()) {
        setErr(err, wbTr("Please enter a workbook name (letters, digits, _ or -)."));
        return false;
    }
    if (nn == b->name)
        return true;
    if (find(nn)) {
        setErr(err, wbTr("A workbook named %1 already exists.").arg(nn));
        return false;
    }
    // The sheets keep their table names (and so every graph / formula reference); the old
    // name stays the prefix that is left out of the tab labels.
    b->name = nn;
    refreshBars(nn);
    refreshExplorer();
    d_app->modifiedProject();
    return true;
}

bool WorkbookManager::moveSheet(const QString &book, int from, int to, QString *err)
{
    Book *b = find(book);
    if (!b) {
        setErr(err, wbTr("There is no workbook named %1.").arg(book));
        return false;
    }
    if (from < 0 || from >= b->sheets.size()) {
        setErr(err, wbTr("Sheet index out of range."));
        return false;
    }
    to = std::max(0, std::min(to, int(b->sheets.size()) - 1));
    if (from != to) {
        b->sheets.move(from, to);
        refreshBars(book);
        d_app->modifiedProject();
    }
    return true;
}

bool WorkbookManager::setCurrentSheet(Table *t, bool activate, QString *err)
{
    Book *b = bookFor(t);
    if (!b) {
        setErr(err, wbTr("The worksheet is not in a workbook."));
        return false;
    }
    Table *old = b->current;
    b->current = t;
    const QString name = b->name;
    if (activate && d_app) {
        const bool prev = d_switching;
        d_switching = true;
        if (t->folder() == d_app->current_folder) {
            QRect g;
            bool max = false;
            if (old && old != t && isShown(old)) {
                g = old->geometry();
                max = old->isMaximized();
            } else if (old && old != t) {
                g = d_last_geometry.value(old);
                max = d_last_maximized.value(old, false);
            }
            if (!isShown(t) || g.isValid() || max)
                showInPlaceOf(t, g, max);
            else
                d_app->activateSubWindow(t);
            for (Table *s : b->sheets)
                if (s != t && !d_app->hidden(s)) {
                    d_app->hideWindow(s);
                    d_user_hidden[s] = true;
                }
            if (Folder *f = t->folder())
                f->setActiveWindow(t);
            d_app->d_workspace.setActiveSubWindow(t);
        } else {
            // book in another folder: swap the hidden flags, it shows when the folder opens
            for (Table *s : b->sheets)
                if (s != t && !d_app->hidden(s)) {
                    d_app->hideWindow(s);
                    d_user_hidden[s] = true;
                }
            d_app->updateWindowLists(t);
            t->setStatus(MyWidget::Normal);
            d_user_hidden[t] = false;
            if (Folder *f = t->folder())
                f->setActiveWindow(t);
        }
        d_switching = prev;
    }
    refreshBars(name);
    if (old != t)
        d_app->modifiedProject();
    return true;
}

// ---------------------------------------------------------------------------
// Visibility tracking
// ---------------------------------------------------------------------------
bool WorkbookManager::eventFilter(QObject *watched, QEvent *event)
{
    if (d_shutdown)
        return QObject::eventFilter(watched, event);
    if (watched == d_app) {
        if (event->type() == QEvent::LanguageChange)
            retranslateUi();
        return QObject::eventFilter(watched, event);
    }
    Table *t = qobject_cast<Table *>(watched);
    if (!t)
        return QObject::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::ShowToParent:
        d_user_hidden[t] = false;
        if (!d_switching) {
            QPointer<Table> safe(t);
            QTimer::singleShot(0, this, [this, safe]() {
                if (safe && !d_shutdown)
                    checkShown(safe);
            });
        }
        break;
    case QEvent::HideToParent:
        if (d_app && d_app->hidden(t))
            d_user_hidden[t] = true;
        break;
    case QEvent::Move:
    case QEvent::Resize:
        if (!t->isHidden()) {
            d_last_maximized[t] = t->isMaximized();
            if (!t->isMaximized())
                d_last_geometry[t] = t->geometry();
        }
        break;
    case QEvent::Close:
        // remember where the book was before the window goes away
        if (!t->isHidden()) {
            d_last_maximized[t] = t->isMaximized();
            if (!t->isMaximized())
                d_last_geometry[t] = t->geometry();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void WorkbookManager::checkShown(Table *t)
{
    Book *b = bookFor(t);
    if (!b || !isShown(t))
        return;
    if (b->current != t) {
        // shown from outside (project explorer, Window menu, Find ...): it takes the book's place
        setCurrentSheet(t, true);
        return;
    }
    bool hidOne = false;
    d_switching = true;
    for (Table *s : b->sheets)
        if (s != t && isShown(s)) {
            d_app->hideWindow(s);
            d_user_hidden[s] = true;
            hidOne = true;
        }
    d_switching = false;
    if (hidOne)
        refreshBars(b->name);
}

// ---------------------------------------------------------------------------
// Sheet tab bar
// ---------------------------------------------------------------------------
void WorkbookManager::removeBar(Table *t)
{
    if (!t)
        return;
    if (QWidget *bar = t->findChild<QWidget *>(QString::fromLatin1(kBarObjectName))) {
        bar->hide();
        bar->setParent(nullptr);
        bar->deleteLater();
    }
}

void WorkbookManager::scheduleRefresh(const QString &book)
{
    QTimer::singleShot(0, this, [this, book]() {
        if (d_shutdown)
            return;
        refreshBars(book);
        refreshExplorer();
    });
}

void WorkbookManager::refreshBars(const QString &book)
{
    Book *b = find(book);
    if (!b)
        return;
    prune(*b);
    if (b->sheets.isEmpty()) {
        eraseBook(book);
        return;
    }
    const Book copy = *b;
    for (Table *t : copy.sheets)
        refreshBar(t, copy);
}

void WorkbookManager::refreshBar(Table *t, const Book &b)
{
    if (!t || !t->widget())
        return;
    QWidget *bar = t->findChild<QWidget *>(QString::fromLatin1(kBarObjectName));
    QTabBar *tabs = nullptr;
    if (!bar) {
        auto *root = qobject_cast<QBoxLayout *>(t->widget()->layout());
        if (!root)
            return;
        bar = new QWidget(t->widget());
        bar->setObjectName(QString::fromLatin1(kBarObjectName));
        auto *h = new QHBoxLayout(bar);
        h->setContentsMargins(2, 0, 2, 2);
        h->setSpacing(2);
        tabs = new QTabBar(bar);
        tabs->setObjectName(QString::fromLatin1(kTabBarObjectName));
        tabs->setShape(QTabBar::RoundedSouth);
        tabs->setMovable(true);
        tabs->setExpanding(false);
        tabs->setDocumentMode(true);
        tabs->setUsesScrollButtons(true);
        tabs->setElideMode(Qt::ElideRight);
        tabs->setContextMenuPolicy(Qt::CustomContextMenu);
        auto *plus = new QToolButton(bar);
        plus->setObjectName(QStringLiteral("torigraph_workbook_add_sheet"));
        plus->setText(QStringLiteral("+"));
        plus->setAutoRaise(true);
        plus->setToolTip(wbTr("Add a sheet to this workbook"));
        h->addWidget(tabs, 0);
        h->addWidget(plus, 0);
        h->addStretch(1);
        root->addWidget(bar);

        QPointer<Table> self(t);
        auto sheetAt = [tabs](int i) -> QString { return tabs->tabData(i).toString(); };
        connect(tabs, &QTabBar::currentChanged, this, [this, self, sheetAt](int i) {
            if (!self || i < 0 || d_shutdown)
                return;
            const QString target = sheetAt(i);
            if (target == self->name())
                return;
            // switch after the click has been handled (the window holding this bar gets hidden)
            QTimer::singleShot(0, this, [this, self, target]() {
                if (!self || d_shutdown)
                    return;
                const Book *bk = bookFor(self);
                if (!bk)
                    return;
                for (Table *s : bk->sheets)
                    if (s && s->name() == target) {
                        setCurrentSheet(s, true);
                        return;
                    }
            });
        });
        connect(tabs, &QTabBar::tabMoved, this, [this, self, tabs](int, int) {
            if (!self || d_shutdown)
                return;
            Book *bk = bookFor(self);
            if (!bk)
                return;
            QList<Table *> order;
            for (int i = 0; i < tabs->count(); ++i)
                for (Table *s : bk->sheets)
                    if (s && s->name() == tabs->tabData(i).toString() && !order.contains(s))
                        order << s;
            for (Table *s : bk->sheets)
                if (!order.contains(s))
                    order << s;
            bk->sheets = order;
            d_app->modifiedProject();
            // the other sheets' bars follow; this one is still being dragged
            const Book copy = *bk;
            for (Table *s : copy.sheets)
                if (s != self)
                    refreshBar(s, copy);
        });
        connect(tabs, &QTabBar::tabBarDoubleClicked, this, [this, self, sheetAt](int i) {
            if (!self || i < 0 || d_shutdown)
                return;
            const Book *bk = bookFor(self);
            if (!bk)
                return;
            for (Table *s : bk->sheets)
                if (s && s->name() == sheetAt(i)) {
                    showRenameSheetDialog(s);
                    return;
                }
        });
        connect(tabs, &QWidget::customContextMenuRequested, this,
                [this, self, tabs, sheetAt](const QPoint &pos) {
                    if (!self || d_shutdown || nonInteractive(d_app))
                        return;
                    const Book *bk = bookFor(self);
                    if (!bk)
                        return;
                    Table *target = self;
                    const int i = tabs->tabAt(pos);
                    if (i >= 0)
                        for (Table *s : bk->sheets)
                            if (s && s->name() == sheetAt(i))
                                target = s;
                    QMenu menu(tabs);
                    buildSheetMenu(&menu, target, true);
                    menu.exec(tabs->mapToGlobal(pos));
                });
        connect(plus, &QToolButton::clicked, this, [this, self]() {
            if (!self || d_shutdown)
                return;
            const Book *bk = bookFor(self);
            if (!bk)
                return;
            const QString name = bk->name;
            QTimer::singleShot(0, this, [this, name]() {
                if (!d_shutdown && find(name))
                    addSheet(name, QString(), -1, true);
            });
        });
    } else {
        tabs = bar->findChild<QTabBar *>(QString::fromLatin1(kTabBarObjectName));
        if (!tabs)
            return;
    }

    const QSignalBlocker blocker(tabs);
    while (tabs->count() > b.sheets.size())
        tabs->removeTab(tabs->count() - 1);
    int cur = 0;
    for (int i = 0; i < b.sheets.size(); ++i) {
        Table *s = b.sheets.at(i);
        const QString label = sheetLabel(b.prefix, s->name());
        if (i >= tabs->count())
            tabs->addTab(label);
        else
            tabs->setTabText(i, label);
        tabs->setTabData(i, s->name());
        tabs->setTabToolTip(i,
                            wbTr("Workbook %1, worksheet %2 (graphs and formulas refer to "
                                 "this name)")
                                    .arg(b.name, s->name()));
        if (s == t)
            cur = i;
    }
    tabs->setCurrentIndex(cur);
    bar->setToolTip(wbTr("Workbook %1").arg(b.name));
    bar->show();
}

// ---------------------------------------------------------------------------
// Menus and dialogs
// ---------------------------------------------------------------------------
void WorkbookManager::showRenameSheetDialog(Table *t)
{
    Book *b = bookFor(t);
    if (!b || nonInteractive(d_app))
        return;
    bool ok = false;
    const QString cur = sheetLabel(b->prefix, t->name());
    const QString text = QInputDialog::getText(
            d_app, wbTr("Rename Sheet"),
            wbTr("Sheet name (letters, digits, _ or -).\nGraphs that use this sheet follow the new "
                 "name; column formulas that name it (col(\"%1/...\")) are not changed.")
                    .arg(t->name()),
            QLineEdit::Normal, cur, &ok);
    if (!ok || text.trimmed().isEmpty() || text.trimmed() == cur)
        return;
    QString err;
    if (!renameSheet(t, text, &err) && !err.isEmpty())
        QMessageBox::warning(d_app, wbTr("Rename Sheet"), err);
}

void WorkbookManager::showRenameBookDialog(const QString &book)
{
    if (!find(book) || nonInteractive(d_app))
        return;
    bool ok = false;
    const QString text = QInputDialog::getText(
            d_app, wbTr("Rename Workbook"),
            wbTr("Workbook name (letters, digits, _ or -).\nThe sheets keep their names, so graphs "
                 "and formulas are not affected."),
            QLineEdit::Normal, book, &ok);
    if (!ok || text.trimmed().isEmpty() || text.trimmed() == book)
        return;
    QString err;
    if (!renameBook(book, text, &err) && !err.isEmpty())
        QMessageBox::warning(d_app, wbTr("Rename Workbook"), err);
}

void WorkbookManager::buildSheetMenu(QMenu *menu, Table *t, bool fromTab)
{
    if (!menu || !t)
        return;
    QPointer<Table> sp(t);
    Book *b = bookFor(t);
    auto later = [this](std::function<void()> fn) {
        QTimer::singleShot(0, this, [this, fn]() {
            if (!d_shutdown)
                fn();
        });
    };
    if (b) {
        const QString book = b->name;
        menu->addAction(wbTr("&Add Sheet"), this, [this, book, later]() {
            later([this, book]() {
                if (find(book))
                    addSheet(book, QString(), -1, true);
            });
        });
        menu->addAction(wbTr("&Rename Sheet..."), this, [this, sp]() {
            if (sp)
                showRenameSheetDialog(sp);
        });
        menu->addAction(wbTr("&Delete Sheet..."), this, [this, sp, later]() {
            later([this, sp]() {
                if (sp)
                    deleteSheet(sp, true);
            });
        });
        const int idx = b->sheets.indexOf(t);
        QAction *left = menu->addAction(wbTr("Move Sheet &Left"), this, [this, sp, book]() {
            const Book *bk = find(book);
            if (sp && bk)
                moveSheet(book, bk->sheets.indexOf(sp), bk->sheets.indexOf(sp) - 1);
        });
        left->setEnabled(idx > 0);
        QAction *right = menu->addAction(wbTr("Move Sheet Ri&ght"), this, [this, sp, book]() {
            const Book *bk = find(book);
            if (sp && bk)
                moveSheet(book, bk->sheets.indexOf(sp), bk->sheets.indexOf(sp) + 1);
        });
        right->setEnabled(idx >= 0 && idx < b->sheets.size() - 1);
        menu->addSeparator();
        menu->addAction(wbTr("&Move Sheet Out of Workbook"), this, [this, sp, later]() {
            later([this, sp]() {
                if (sp)
                    detachSheet(sp, true);
            });
        });
        // stand-alone worksheets of the same folder can be pulled in
        QMenu *add = menu->addMenu(wbTr("Add &Existing Worksheet"));
        Folder *f = bookFolder(*b);
        int n = 0;
        if (f) {
            for (MyWidget *w : f->windowsList()) {
                Table *cand = qobject_cast<Table *>(w);
                if (!cand || bookFor(cand))
                    continue;
                QPointer<Table> cp(cand);
                add->addAction(cand->name(), this, [this, cp, book, later]() {
                    later([this, cp, book]() {
                        if (cp && find(book))
                            attachTable(book, cp);
                    });
                });
                ++n;
            }
        }
        add->setEnabled(n > 0);
        menu->addSeparator();
        menu->addAction(wbTr("Rena&me Workbook..."), this,
                        [this, book]() { showRenameBookDialog(book); });
        menu->addAction(wbTr("&Ungroup Workbook (Separate Worksheets)"), this,
                        [this, book, later]() {
                            later([this, book]() {
                                if (find(book))
                                    dissolveBook(book);
                            });
                        });
    } else {
        menu->addAction(wbTr("&Make Workbook from This Worksheet"), this, [this, sp, later]() {
            later([this, sp]() {
                if (sp && !bookFor(sp))
                    createBookFromTables(sp->name(), QList<Table *>() << sp, sp);
            });
        });
        QMenu *into = menu->addMenu(wbTr("Move &into Workbook"));
        int n = 0;
        for (const Book &bk : d_books) {
            if (bookFolder(bk) != t->folder())
                continue;
            const QString book = bk.name;
            into->addAction(book, this, [this, sp, book, later]() {
                later([this, sp, book]() {
                    if (sp && find(book))
                        attachTable(book, sp);
                });
            });
            ++n;
        }
        into->setEnabled(n > 0);
    }
    Q_UNUSED(fromTab)
}

void WorkbookManager::populateWindowMenu(QMenu *menu, MyWidget *w)
{
    Table *t = qobject_cast<Table *>(w);
    if (!menu || !t)
        return;
    menu->addSeparator();
    QMenu *sub = menu->addMenu(wbTr("&Workbook"));
    sub->setObjectName(QStringLiteral("torigraph_workbook_window_menu"));
    buildSheetMenu(sub, t, false);
}

QAction *WorkbookManager::newWorkbookAction()
{
    if (!d_new_action) {
        d_new_action = new QAction(QIcon(QPixmap(":/table.xpm")), QString(), d_app);
        d_new_action->setObjectName(QStringLiteral("actionNewWorkbook"));
        connect(d_new_action, &QAction::triggered, this, [this]() {
            QString err;
            const QString name = createBook(QString(), 1, &err);
            if (name.isEmpty() && !err.isEmpty() && !nonInteractive(d_app))
                QMessageBox::warning(d_app, wbTr("New Workbook"), err);
        });
        retranslateUi();
    }
    return d_new_action;
}

void WorkbookManager::retranslateUi()
{
    if (d_new_action) {
        d_new_action->setText(wbTr("New &Workbook"));
        d_new_action->setToolTip(
                wbTr("New workbook: several worksheets as sheet tabs of one window"));
        d_new_action->setStatusTip(d_new_action->toolTip());
    }
}

void WorkbookManager::decorateListItem(QTreeWidgetItem *it, MyWidget *w) const
{
    if (!it || !qobject_cast<Table *>(w))
        return;
    const QString book = bookOf(w);
    if (book.isEmpty())
        it->setText(1, QCoreApplication::translate("ApplicationWindow", "Table"));
    else
        it->setText(1, wbTr("Sheet of %1").arg(book));
}

void WorkbookManager::refreshExplorer()
{
    if (!d_app || d_shutdown)
        return;
    for (int i = 0; i < d_app->lv.topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = d_app->lv.topLevelItem(i);
        if (it && it->type() == WindowListItem::WindowType)
            decorateListItem(it, static_cast<WindowListItem *>(it)->window());
    }
}

// ---------------------------------------------------------------------------
// Project file
// ---------------------------------------------------------------------------
QString WorkbookManager::saveFolderBooks(Folder *folder)
{
    QString out;
    if (!folder)
        return out;
    for (int bi = d_books.size() - 1; bi >= 0; --bi) {
        prune(d_books[bi]);
        if (d_books[bi].sheets.isEmpty())
            d_books.removeAt(bi);
    }
    for (const Book &b : d_books) {
        if (bookFolder(b) != folder)
            continue;
        QString line = QStringLiteral("<workbook>\tname=") + b.name;
        line += QStringLiteral("\tprefix=") + b.prefix;
        line += QStringLiteral("\tcurrent=") + (b.current ? b.current->name() : QString());
        for (Table *t : b.sheets)
            line += QStringLiteral("\tsheet=") + t->name();
        out += line + QLatin1Char('\n');
    }
    return out;
}

bool WorkbookManager::restoreFromLine(const QString &line, Folder *folder,
                                      const QStringList &renamedTables)
{
    if (!d_app || !line.startsWith(QLatin1String("<workbook>")))
        return false;
    const QStringList fields = line.split(QLatin1Char('\t'));
    QString name, current, prefix;
    QStringList sheetNames;
    for (int i = 1; i < fields.size(); ++i) {
        const QString &f = fields.at(i);
        const int eq = f.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = f.left(eq);
        const QString val = f.mid(eq + 1).trimmed();
        if (key == QLatin1String("name"))
            name = val;
        else if (key == QLatin1String("current"))
            current = val;
        else if (key == QLatin1String("prefix"))
            prefix = val;
        else if (key == QLatin1String("sheet") && !val.isEmpty())
            sheetNames << val;
        // unknown keys are ignored (room for later additions)
    }
    auto mapped = [&renamedTables](const QString &n) {
        for (int k = 0; k + 1 < renamedTables.size(); k += 2)
            if (renamedTables.at(k) == n)
                return renamedTables.at(k + 1);
        return n;
    };
    auto findTable = [this, folder](const QString &n) -> Table * {
        if (folder)
            for (MyWidget *w : folder->windowsList())
                if (Table *t = qobject_cast<Table *>(w))
                    if (t->name() == n)
                        return t;
        return nullptr;
    };
    Book nb;
    nb.name = uniqueBookName(name.isEmpty() ? QStringLiteral("Book") : name);
    nb.prefix = sanitizeName(prefix.isEmpty() ? name : prefix);
    if (nb.prefix.isEmpty())
        nb.prefix = nb.name;
    for (const QString &sn : sheetNames) {
        Table *t = findTable(mapped(sn));
        if (t && !bookFor(t) && !nb.sheets.contains(t))
            nb.sheets << t;
    }
    if (nb.sheets.isEmpty())
        return false;
    nb.current = findTable(mapped(current));
    if (!nb.current || !nb.sheets.contains(nb.current))
        nb.current = nb.sheets.first();
    // only one sheet is visible: the current one, unless the whole book was hidden and
    // another sheet was saved visible
    if (d_app->hidden(nb.current)) {
        for (Table *t : nb.sheets)
            if (!d_app->hidden(t)) {
                nb.current = t;
                break;
            }
    }
    d_books.append(nb);
    d_switching = true;
    for (Table *t : nb.sheets) {
        watch(t);
        if (t != nb.current && !d_app->hidden(t)) {
            d_app->hideWindow(t);
            d_user_hidden[t] = true;
        }
    }
    d_switching = false;
    refreshBars(nb.name);
    return true;
}

// ---------------------------------------------------------------------------
// Python JSON-RPC
// ---------------------------------------------------------------------------
bool WorkbookManager::handleRpc(const QString &method, const QJsonObject &params, QJsonObject &out,
                                QString *err)
{
    auto str = [&params](const char *k) {
        return params.value(QString::fromLatin1(k)).toString().trimmed();
    };
    auto tableByName = [this](const QString &n) -> Table * {
        for (MyWidget *w : d_app->windowsList())
            if (Table *t = qobject_cast<Table *>(w))
                if (t->name() == n)
                    return t;
        return nullptr;
    };
    auto needTable = [&](const char *key, Table **out) -> bool {
        const QString n = str(key);
        *out = tableByName(n);
        if (!*out) {
            setErr(err, QStringLiteral("Worksheet '%1' not found.").arg(n));
            return false;
        }
        return true;
    };
    auto finish = [&](const QString &book) {
        out = describe(book);
        out[QStringLiteral("ok")] = true;
        return true;
    };

    if (method == QLatin1String("workbook.list")) {
        QJsonArray arr;
        for (const QString &n : bookNames())
            arr.append(describe(n));
        out[QStringLiteral("ok")] = true;
        out[QStringLiteral("workbooks")] = arr;
        return true;
    }
    if (method == QLatin1String("workbook.create")) {
        const QString name = str("name");
        QList<Table *> tables;
        for (const QJsonValue &v : params.value(QStringLiteral("tables")).toArray()) {
            Table *t = tableByName(v.toString());
            if (!t) {
                setErr(err, QStringLiteral("Worksheet '%1' not found.").arg(v.toString()));
                return false;
            }
            tables << t;
        }
        QString book;
        if (!tables.isEmpty()) {
            book = createBookFromTables(name, tables, nullptr, err);
        } else {
            const int n = params.value(QStringLiteral("sheets")).toInt(1);
            book = createBook(name, n, err);
        }
        if (book.isEmpty())
            return false;
        return finish(book);
    }
    if (method == QLatin1String("workbook.add_sheet")) {
        const QString book = str("book");
        const int index = params.value(QStringLiteral("index")).toInt(-1);
        const bool activate = params.value(QStringLiteral("activate")).toBool(true);
        Table *t = addSheet(book, str("name"), index, activate, err);
        if (!t)
            return false;
        finish(book);
        out[QStringLiteral("table")] = t->name();
        return true;
    }
    if (method == QLatin1String("workbook.attach_sheet")) {
        Table *t = nullptr;
        if (!needTable("table", &t))
            return false;
        const QString book = str("book");
        if (!attachTable(book, t, params.value(QStringLiteral("index")).toInt(-1), err))
            return false;
        return finish(book);
    }
    if (method == QLatin1String("workbook.detach_sheet")) {
        Table *t = nullptr;
        if (!needTable("table", &t))
            return false;
        const QString book = bookOf(t);
        if (!detachSheet(t, true, err))
            return false;
        finish(book);
        out[QStringLiteral("table")] = t->name();
        return true;
    }
    if (method == QLatin1String("workbook.set_current")) {
        Table *t = nullptr;
        if (!needTable("table", &t))
            return false;
        if (!setCurrentSheet(t, true, err))
            return false;
        return finish(bookOf(t));
    }
    if (method == QLatin1String("workbook.move_sheet")) {
        Table *t = nullptr;
        if (!needTable("table", &t))
            return false;
        const QString book = bookOf(t);
        if (book.isEmpty()) {
            setErr(err, wbTr("The worksheet is not in a workbook."));
            return false;
        }
        if (!moveSheet(book, sheets(book).indexOf(t), params.value(QStringLiteral("index")).toInt(),
                       err))
            return false;
        return finish(book);
    }
    if (method == QLatin1String("workbook.rename_sheet")) {
        Table *t = nullptr;
        if (!needTable("table", &t))
            return false;
        if (!renameSheet(t, str("name"), err))
            return false;
        finish(bookOf(t));
        out[QStringLiteral("table")] = t->name();
        return true;
    }
    if (method == QLatin1String("workbook.rename")) {
        const QString newName = sanitizeName(str("new_name"));
        if (!renameBook(str("book"), newName, err))
            return false;
        return finish(newName);
    }
    if (method == QLatin1String("workbook.ungroup")) {
        if (!dissolveBook(str("book"), err))
            return false;
        out[QStringLiteral("ok")] = true;
        return true;
    }
    setErr(err, QStringLiteral("Unknown workbook method '%1'.").arg(method));
    return false;
}

// ---------------------------------------------------------------------------
// Translations
// ---------------------------------------------------------------------------
void WorkbookManager::registerTranslations()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("New &Workbook", "新しいブック(&W)");
    reg("New Workbook", "新しいブック");
    reg("New workbook: several worksheets as sheet tabs of one window",
        "新しいブック: 1 つのウィンドウに複数のワークシートをシートのタブとしてまとめます");
    reg("&Workbook", "ブック(&W)");
    reg("&Add Sheet", "シートを追加(&A)");
    reg("&Rename Sheet...", "シート名の変更(&R)...");
    reg("&Delete Sheet...", "シートを削除(&D)...");
    reg("Move Sheet &Left", "シートを左へ移動(&L)");
    reg("Move Sheet Ri&ght", "シートを右へ移動(&G)");
    reg("&Move Sheet Out of Workbook", "シートをブックから出す(&M)");
    reg("Add &Existing Worksheet", "既存のワークシートを加える(&E)");
    reg("Rena&me Workbook...", "ブック名の変更(&M)...");
    reg("&Ungroup Workbook (Separate Worksheets)", "ブックを解く (別々のワークシートにする)(&U)");
    reg("&Make Workbook from This Worksheet", "このワークシートからブックを作る(&M)");
    reg("Move &into Workbook", "ブックに入れる(&I)");
    reg("Add a sheet to this workbook", "このブックにシートを追加");
    reg("Workbook %1", "ブック %1");
    reg("Workbook %1, worksheet %2 (graphs and formulas refer to this name)",
        "ブック %1 のワークシート %2 (グラフと列の数式はこの名前で参照します)");
    reg("Sheet of %1", "%1 のシート");
    reg("Delete Sheet", "シートを削除");
    reg("Delete the sheet %1 and its data?\nThis cannot be undone.",
        "シート %1 とそのデータを削除しますか?\n元に戻せません。");
    reg("Rename Sheet", "シート名の変更");
    reg("Sheet name (letters, digits, _ or -).\nGraphs that use this sheet follow the new name; "
        "column formulas that name it (col(\"%1/...\")) are not changed.",
        "シート名 (文字・数字・_・-)。\nこのシートを使うグラフは新しい名前に追従します。"
        "このシートを名前で参照する列の数式 (col(\"%1/...\")) は変わりません。");
    reg("Rename Workbook", "ブック名の変更");
    reg("Workbook name (letters, digits, _ or -).\nThe sheets keep their names, so graphs and "
        "formulas are not affected.",
        "ブック名 (文字・数字・_・-)。\nシートの名前は変わらないので、グラフと列の数式には"
        "影響しません。");
    reg("No worksheet was given.", "ワークシートが指定されていません。");
    reg("All sheets of a workbook must be in the same folder.",
        "ブックのシートはすべて同じフォルダーに置く必要があります。");
    reg("The worksheet %1 already belongs to the workbook %2.",
        "ワークシート %1 はすでにブック %2 に入っています。");
    reg("There is no workbook named %1.", "ブック %1 はありません。");
    reg("The worksheet could not be created.", "ワークシートを作成できませんでした。");
    reg("The worksheet is not in a workbook.", "このワークシートはブックに入っていません。");
    reg("Please enter a sheet name (letters, digits, _ or -).",
        "シート名を入力してください (文字・数字・_・-)。");
    reg("The sheet could not be renamed to %1 (the name is invalid or already used).",
        "シート名を %1 に変更できませんでした (使えない名前か、すでに使われています)。");
    reg("Please enter a workbook name (letters, digits, _ or -).",
        "ブック名を入力してください (文字・数字・_・-)。");
    reg("A workbook named %1 already exists.", "ブック %1 はすでにあります。");
    reg("Sheet index out of range.", "シートの番号が範囲外です。");
}
