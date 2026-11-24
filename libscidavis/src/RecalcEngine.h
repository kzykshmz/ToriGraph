/***************************************************************************
    File                 : RecalcEngine.h
    Project              : ToriGraph (SciDAVis OriginPro 2021 Parity)
    Description          : OriginPro Dynamic Recalculation Engine ("Green Lock"),
                           Batch Repeat for All Curves, and Dialog Theme Manager
 ***************************************************************************/
#ifndef RECALC_ENGINE_H
#define RECALC_ENGINE_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class ApplicationWindow;
class Graph;
class Table;
class QWidget;

class RecalcManager : public QObject
{
    Q_OBJECT
public:
    enum RecalcMode { Auto = 0, Manual = 1, Disabled = 2 };
    enum OpType {
        OpLinearFit = 0,
        OpPolyFit = 1,
        OpNonLinearBuiltIn = 2,
        OpMultiPeakFit = 3,
        OpSmooth = 4,
        OpNormalize = 5,
        OpDifferentiate = 6,
        OpRobustFit = 7,
        OpPiecewiseFit = 8,
        OpIIRFilter = 9,
        OpSavGolDeriv = 10,
        OpWaveletDenoise = 11
    };

    //! Per-window manager. app == nullptr (or no argument) returns the legacy default instance.
    //! A manager created for a window is released automatically when that window is destroyed.
    static RecalcManager *instance(ApplicationWindow *app = nullptr);
    //! Lookup existing per-window (or default) manager without creating one.
    static RecalcManager *existing(ApplicationWindow *app = nullptr);

    void beginDeferAutoRecalc();
    void endDeferAutoRecalc();

    class ScopedDeferAutoRecalc
    {
    public:
        explicit ScopedDeferAutoRecalc(RecalcManager *mgr = nullptr) : d_mgr(mgr)
        {
            if (d_mgr)
                d_mgr->beginDeferAutoRecalc();
        }
        ~ScopedDeferAutoRecalc()
        {
            if (d_mgr)
                d_mgr->endDeferAutoRecalc();
        }
        ScopedDeferAutoRecalc(const ScopedDeferAutoRecalc &) = delete;
        ScopedDeferAutoRecalc &operator=(const ScopedDeferAutoRecalc &) = delete;
        ScopedDeferAutoRecalc(ScopedDeferAutoRecalc &&other) noexcept : d_mgr(other.d_mgr)
        {
            other.d_mgr.clear();
        }
        ScopedDeferAutoRecalc &operator=(ScopedDeferAutoRecalc &&other) noexcept
        {
            if (this != &other) {
                if (d_mgr)
                    d_mgr->endDeferAutoRecalc();
                d_mgr = other.d_mgr;
                other.d_mgr.clear();
            }
            return *this;
        }

    private:
        QPointer<RecalcManager> d_mgr;
    };

    void setRecalcMode(RecalcMode mode);
    RecalcMode recalcMode() const { return d_mode; }
    bool isRecalculating() const { return d_recalculating; }

    int registerLink(ApplicationWindow *app, Graph *g, Table *srcTable,
                     const QString &srcCurveTitle, const QString &outCurveTitle,
                     OpType op, const QVariantMap &params = QVariantMap(),
                     Table *resultTable = nullptr);
    void removeLinksForGraph(Graph *g);
    int activeLinkCount(Graph *g = nullptr) const;
    bool hasDirtyLinks(Graph *g = nullptr) const;
    int recalculateAll(Graph *filterGraph = nullptr);
    void updateLockBadge(Graph *g);

    //! Error text of the last failed recalculation (empty if the links are healthy).
    //! g == nullptr looks at every link; otherwise only the links of that graph.
    QString lastError(Graph *g = nullptr) const;
    QString lastErrorForLink(int linkId) const;

    //! Serialize the links of one graph (single-line XML fragment, empty if it has none).
    QString saveLinksXmlForGraph(const Graph *g) const;
    //! Recreate links from saveLinksXmlForGraph() output. Does not recalculate.
    void restoreLinkXmlForGraph(ApplicationWindow *app, Graph *g, const QString &xml);
    //! Reference-only updates after a table / column rename (links are not made dirty).
    //! notifyColumnRenamed takes short column names (without the "<table>_" prefix).
    void notifyTableRenamed(const QString &oldName, const QString &newName);
    void notifyColumnRenamed(const QString &tblName, const QString &oldCol,
                             const QString &newCol);

    static void repeatForAllCurves(ApplicationWindow *app, Graph *g, OpType op,
                                   const QVariantMap &params = QVariantMap());

signals:
    //! Emitted when a link could not be recalculated. The previous output is left untouched.
    void linkFailed(Graph *g, const QString &message);

private slots:
    void onTableDataModified(Table *t, const QString &colName);
    void onGraphDestroyed(QObject *obj);

private:
    explicit RecalcManager(ApplicationWindow *app = nullptr);

    struct RecalcLink
    {
        int id = 0;
        QPointer<ApplicationWindow> app;
        QPointer<Graph> graph;
        QPointer<Table> srcTable;
        QString srcCurveTitle;
        QString outCurveTitle;
        QString desiredOutTitle;
        QStringList extraCurveTitles;
        OpType op = OpLinearFit;
        QVariantMap params;
        QPointer<Table> resultTable;
        bool dirty = false;
        QString lastError;
    };

    int findLinkIndexById(int id) const;
    bool executeLinkById(int id);
    bool executeLink(RecalcLink &link);
    void reportLinkFailure(const RecalcLink &link, const QString &message);
    void connectLinkSources(Graph *g, Table *srcTable);
    void pruneDeadLinks();

    QPointer<ApplicationWindow> d_app;
    RecalcMode d_mode = Auto;
    bool d_recalculating = false;
    int d_defer_auto_depth = 0;
    bool d_has_deferred_dirty = false;
    int d_next_id = 1;
    QVector<RecalcLink> d_links;
};

class DialogThemeManager
{
public:
    static void saveTheme(const QString &toolId, const QString &themeName,
                          const QVariantMap &settings);
    static QVariantMap loadTheme(const QString &toolId, const QString &themeName);
    static QStringList themeNames(const QString &toolId = QString());
    static void deleteTheme(const QString &toolId, const QString &themeName);
    static void showManageThemesDialog(QWidget *parent, const QString &toolId = QString());
};

#endif // RECALC_ENGINE_H
