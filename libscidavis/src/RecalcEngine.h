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

    static RecalcManager *instance(ApplicationWindow *app = nullptr);

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

    static void repeatForAllCurves(ApplicationWindow *app, Graph *g, OpType op,
                                   const QVariantMap &params = QVariantMap());

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
    };

    int findLinkIndexById(int id) const;
    bool executeLinkById(int id);
    bool executeLink(RecalcLink &link);
    void pruneDeadLinks();

    QPointer<ApplicationWindow> d_app;
    RecalcMode d_mode = Auto;
    bool d_recalculating = false;
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
