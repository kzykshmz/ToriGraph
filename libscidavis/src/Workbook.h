/***************************************************************************
    File                 : Workbook.h
    Project              : ToriGraph
    Description          : Workbooks (Origin-like "one book, several sheets").
                           Every sheet stays an ordinary, independent Table window
                           (same name, same column references, same <table> element in
                           the project file). A workbook is only a thin layer on top:
                           an ordered list of tables shown one at a time in the same
                           place, with a sheet tab bar at the bottom of the visible one.
                           The project file gets one extra line per workbook,
                             <workbook>\tname=Book1\tprefix=Book1\tcurrent=Book1_Sheet2\tsheet=...\tsheet=...
                           which older versions ignore (they show the sheets as separate
                           tables, the inactive ones hidden).
 ***************************************************************************/

#ifndef TORIGRAPH_WORKBOOK_H
#define TORIGRAPH_WORKBOOK_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSet>
#include <QString>
#include <QStringList>

class ApplicationWindow;
class Folder;
class MyWidget;
class QAction;
class QMenu;
class QTreeWidgetItem;
class Table;

class WorkbookManager : public QObject
{
    Q_OBJECT

public:
    //! The manager of \a app (created on first use as a child object of \a app).
    static WorkbookManager *instance(ApplicationWindow *app);
    //! The manager of \a app if it was created already, otherwise nullptr.
    static WorkbookManager *existing(ApplicationWindow *app);
    //! Registers the Japanese translations of this module (idempotent).
    static void registerTranslations();
    //! Name suitable for a workbook / the prefix of its sheet tables (letters, digits, _ and -).
    static QString sanitizeName(const QString &raw);
    //! Text shown on the sheet tab: the table name without a leading "<book>_" / "<book>-".
    static QString sheetLabel(const QString &book, const QString &tableName);

    // ---- queries
    QStringList bookNames() const;
    bool hasBook(const QString &book) const;
    //! Workbook holding \a w, or an empty string for a stand-alone window.
    QString bookOf(const MyWidget *w) const;
    //! Tab label of \a t within its workbook, or an empty string if stand-alone.
    QString sheetDisplayLabel(const Table *t) const;
    QList<Table *> sheets(const QString &book) const;
    Table *currentSheet(const QString &book) const;
    QString uniqueBookName(const QString &base) const;
    //! JSON description {name, current, sheets:[{table, label}], folder} (empty if unknown).
    QJsonObject describe(const QString &book) const;

    // ---- operations (return false / empty with a reason in \a err)
    //! New workbook with \a sheetCount new empty sheets in the current folder; returns its name.
    QString createBook(const QString &name = QString(), int sheetCount = 1, QString *err = nullptr);
    //! Groups existing stand-alone tables of one folder into a new workbook.
    QString createBookFromTables(const QString &name, const QList<Table *> &tables,
                                 Table *current = nullptr, QString *err = nullptr);
    //! Appends (or inserts at \a index) a new empty sheet; shows it when \a activate.
    Table *addSheet(const QString &book, const QString &label = QString(), int index = -1,
                    bool activate = true, QString *err = nullptr);
    //! Moves a stand-alone table of the same folder into \a book.
    bool attachTable(const QString &book, Table *t, int index = -1, QString *err = nullptr);
    //! Takes \a t out of its workbook; it becomes a stand-alone table (shown when \a show;
    //! show = false is for moving it to another folder, where it is then a normal window).
    bool detachSheet(Table *t, bool show = true, QString *err = nullptr);
    //! Takes every sheet out; all of them become visible stand-alone tables.
    bool dissolveBook(const QString &book, QString *err = nullptr);
    //! Deletes the sheet (the table). Asks first when \a confirm and interactive.
    bool deleteSheet(Table *t, bool confirm, QString *err = nullptr);
    //! Renames the sheet: \a label replaces the part after "<prefix>_" (or the whole name).
    bool renameSheet(Table *t, const QString &label, QString *err = nullptr);
    //! Renames the workbook only; the sheets keep their table names (references unchanged).
    bool renameBook(const QString &book, const QString &newName, QString *err = nullptr);
    bool moveSheet(const QString &book, int from, int to, QString *err = nullptr);
    //! Makes \a t the visible sheet of its workbook (in place of the previous one).
    bool setCurrentSheet(Table *t, bool activate = true, QString *err = nullptr);

    // ---- project file
    //! "<workbook>..." lines for the workbooks living in \a folder (may be empty).
    QString saveFolderBooks(Folder *folder);
    //! "<workbook>..." lines for workbooks with 2+ sheets included in \a windows (OLE mini-projects).
    QString saveBooksForWindows(const QList<MyWidget *> &windows);
    //! Restores one "<workbook>" line read while loading / appending \a folder.
    bool restoreFromLine(const QString &line, Folder *folder, const QStringList &renamedTables);

    // ---- Undo / Redo synchronization hooks used by ApplicationWindow
    void onTableClosed(Table *t, bool preserveForRedo);
    void onTableReadded(Table *t);

    // ---- UI hooks used by ApplicationWindow
    void decorateListItem(QTreeWidgetItem *it, MyWidget *w) const;
    void refreshExplorer();
    //! Adds the "Workbook" submenu for the worksheet \a w to a window context menu.
    void populateWindowMenu(QMenu *menu, MyWidget *w);
    //! File > New > New Workbook.
    QAction *newWorkbookAction();
    //! Re-reads the translated texts (called from translateActionsStrings()).
    void retranslateUi();
    //! Called first thing in ~ApplicationWindow: stop reacting to the windows.
    void shutdown();

    // ---- Python JSON-RPC ("workbook.*")
    bool handleRpc(const QString &method, const QJsonObject &params, QJsonObject &out,
                   QString *err);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit WorkbookManager(ApplicationWindow *app);

    struct Book
    {
        QString name;
        QString prefix; //!< "<prefix>_" is left out of the tab labels
        QList<Table *> sheets; //!< raw pointers; removed in the destroyed() handler
        Table *current = nullptr;
    };

    Book *find(const QString &name);
    const Book *find(const QString &name) const;
    Book *bookFor(const MyWidget *w);
    const Book *bookFor(const MyWidget *w) const;
    Folder *bookFolder(const Book &b) const;
    void eraseBook(const QString &name);
    void prune(Book &b);
    void watch(Table *t);
    void unwatch(Table *t);
    void removeSheetInternal(QObject *sheet, bool destroyed);
    void scheduleShow(const QString &book, const QRect &geometry, bool maximized);
    void checkShown(Table *t);
    void refreshBars(const QString &book);
    void scheduleRefresh(const QString &book);
    void armPendingRefreshTimer();
    void refreshBar(Table *t, const Book &b);
    static void removeBar(Table *t);
    void buildSheetMenu(QMenu *menu, Table *t, bool fromTab);
    void showRenameSheetDialog(Table *t);
    void showRenameBookDialog(const QString &book);
    QString nextSheetLabel(const Book &b) const;
    bool isShown(Table *t) const;
    void showInPlaceOf(Table *t, const QRect &geometry, bool maximized);

    struct ArchivedSheet
    {
        QString bookName;
        QString bookPrefix;
        int index = -1;
        bool wasCurrent = false;
    };

    ApplicationWindow *d_app = nullptr;
    QList<Book> d_books;
    QHash<QObject *, ArchivedSheet> d_archived_sheets;
    QHash<QObject *, bool> d_user_hidden;
    QHash<QObject *, QRect> d_last_geometry;
    QHash<QObject *, bool> d_last_maximized;
    QPointer<QAction> d_new_action;
    QSet<QString> d_pending_refresh_books;
    QSet<QString> d_batch_used_names_lower;
    mutable int d_batch_next_sheet_num = 1;
    bool d_create_book_cache_active = false;
    bool d_refresh_timer_armed = false;
    bool d_batch_updating = false;
    bool d_switching = false;
    bool d_shutdown = false;
};

#endif // TORIGRAPH_WORKBOOK_H
