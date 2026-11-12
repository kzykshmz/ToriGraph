/***************************************************************************
    File                 : GraphMiniToolbar.h
    Project              : ToriGraph / SciDAVis
    Description          : OriginPro-style slimmed zero-flash floating mini-toolbar
 ***************************************************************************/
#ifndef GRAPHMINITOOLBAR_H
#define GRAPHMINITOOLBAR_H

#include <QFrame>
#include <QMap>
#include <QPoint>
#include <QPointer>

class QHBoxLayout;
class QTimer;
class QToolButton;
class Graph;
class Legend;

class GraphMiniToolbar : public QFrame
{
    Q_OBJECT

public:
    enum TargetType {
        TargetCanvas = 0,
        TargetAxis = 1,
        TargetCurve = 2,
        TargetLegend = 3
    };

    explicit GraphMiniToolbar(QWidget *parent = nullptr);
    ~GraphMiniToolbar() override;

    static GraphMiniToolbar *instance();
    static void setMiniToolbarEnabled(bool enabled);
    static bool isMiniToolbarEnabled();

    TargetType currentTarget() const { return d_target; }
    Graph *currentGraph() const;
    int currentCurveIndex() const { return d_curve_index; }

    void popupForCanvas(Graph *g, const QPoint &globalPos);
    void popupForAxis(Graph *g, int qwtAxisId, const QPoint &globalPos);
    void popupForCurve(Graph *g, int curveIndex, const QPoint &globalPos);
    void popupForLegend(Graph *g, Legend *legend, const QPoint &globalPos);

    void cancelScheduledAndHide();
    void retranslateUi();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onPopupTimerTimeout();

    // Curve actions
    void onCurvePickColor();
    void onCurveLineWidthDec();
    void onCurveLineWidthInc();
    void onCurveSymbolSizeDec();
    void onCurveSymbolSizeInc();
    void onCurveStyleLine();
    void onCurveStyleScatter();
    void onCurveStyleLineSymbol();
    void onCurveToggleHollowSymbol();
    void onCurveToggleRightY();
    void onCurveToggleErrorBars();
    void onCurveClearMask();

    // Axis actions
    void onAxisAutoRange();
    void onAxisToggleLog();
    void onAxisToggleTicksInOut();
    void onAxisToggleOppositeLine();
    void onAxisAddMeanLine();
    void onAxisFontDec();
    void onAxisFontInc();

    // Canvas actions (kept for slot compatibility; popupForCanvas is a no-op)
    void onCanvasToggleBoxFrame();
    void onCanvasToggleMajorGrid();
    void onCanvasRescaleAll();
    void onCanvasAspectSquare();
    void onCanvasFitPageToLayers();

    // Legend actions
    void onLegendAutoPlace();
    void onLegendToggleFrame();
    void onLegendToggleTransparent();
    void onLegendFontDec();
    void onLegendFontInc();
    void onLegendReconstruct();
    void onLegendCycleColumns();

private:
    struct SavedSymbolState {
        int style = 0;
        int size = 7;
        int penWidth = 1;
        bool hollowWhite = false;
        bool noFill = false;
    };

    void applyThemeStyle();
    void clearButtons();
    void rebuildCurrentButtons();
    QToolButton *addActionBtn(const QString &text, const QString &tooltip, const char *slotName,
                              bool checkable = false, bool checked = false,
                              const QString &semanticIconId = QString());
    void addSeparatorLine();
    void ensureEventFilterInstalled();
    void schedulePopup(const QPoint &globalPos);
    void showNearGlobalPos(const QPoint &globalPos);
    bool isLegendValid() const;
    void applyCurveLineSymbolMode(int mode);
    void adjustAxisFont(int deltaPt);
    void adjustLegendFont(int deltaPt);
    QString beginGraphUndo() const;
    void commitGraphUndo(const QString &label, const QString &beforeSnapshot, int mergeId = -1);

    static bool s_enabled;

    QHBoxLayout *d_layout;
    QTimer *d_popup_timer;
    TargetType d_target;
    QPointer<Graph> d_graph;
    int d_axis_id;
    int d_curve_index;
    Legend *d_legend;
    QPoint d_scheduled_global_pos;
    QPoint d_press_origin_global;
    bool d_popup_pending;
    bool d_filter_installed;
    bool d_in_modal_dialog;
    QMap<long, SavedSymbolState> d_saved_symbol_states;
};

#endif // GRAPHMINITOOLBAR_H
