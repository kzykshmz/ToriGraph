/***************************************************************************
    File                 : GraphMiniToolbar.h
    Project              : ToriGraph / SciDAVis
    Description          : OriginPro-style contextual floating mini-toolbar
 ***************************************************************************/
#ifndef GRAPHMINITOOLBAR_H
#define GRAPHMINITOOLBAR_H

#include <QFrame>
#include <QPointer>
#include <QPoint>

class QHBoxLayout;
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

    void popupForCanvas(Graph *g, const QPoint &globalPos);
    void popupForAxis(Graph *g, int qwtAxisId, const QPoint &globalPos);
    void popupForCurve(Graph *g, int curveIndex, const QPoint &globalPos);
    void popupForLegend(Graph *g, Legend *legend, const QPoint &globalPos);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    // Curve actions
    void onCurvePickColor();
    void onCurveLineWidthDec();
    void onCurveLineWidthInc();
    void onCurveSymbolSizeDec();
    void onCurveSymbolSizeInc();
    void onCurveStyleLine();
    void onCurveStyleScatter();
    void onCurveStyleLineSymbol();
    void onCurveToggleErrorBars();
    void onCurveClearMask();

    // Axis actions
    void onAxisToggleLog();
    void onAxisToggleTicksInOut();
    void onAxisToggleOppositeLine();
    void onAxisAddMeanLine();
    void onAxisFontDec();
    void onAxisFontInc();

    // Canvas actions
    void onCanvasToggleBoxFrame();
    void onCanvasToggleMajorGrid();
    void onCanvasRescaleAll();
    void onCanvasAspectSquare();
    void onCanvasFitPageToLayers();

    // Legend actions
    void onLegendToggleFrame();
    void onLegendToggleTransparent();
    void onLegendFontDec();
    void onLegendFontInc();

private:
    void clearButtons();
    QToolButton *addActionBtn(const QString &text, const QString &tooltip, const char *slotName,
                              bool checkable = false, bool checked = false);
    void addSeparatorLine();
    void showNearGlobalPos(const QPoint &globalPos);
    bool isLegendValid() const;
    void applyCurveLineSymbolMode(int mode);
    void adjustAxisFont(int deltaPt);
    void adjustLegendFont(int deltaPt);

    static bool s_enabled;

    QHBoxLayout *d_layout;
    TargetType d_target;
    QPointer<Graph> d_graph;
    int d_axis_id;
    int d_curve_index;
    Legend *d_legend;
    bool d_filter_installed;
    bool d_in_modal_dialog;
};

#endif // GRAPHMINITOOLBAR_H
