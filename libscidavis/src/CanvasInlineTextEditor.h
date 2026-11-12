/***************************************************************************
    File                 : CanvasInlineTextEditor.h
    Project              : ToriGraph
    Description          : Frameless inline rich-text popup editor for titles,
                           legend items, and text annotations on 2D graphs
 ***************************************************************************/
#ifndef CANVAS_INLINE_TEXT_EDITOR_H
#define CANVAS_INLINE_TEXT_EDITOR_H

#include <QFrame>
#include <QPointer>
#include <QRect>
#include <QString>

class Graph;
class MultiLayer;
class QLineEdit;
class QPlainTextEdit;

class CanvasInlineTextEditor : public QFrame
{
    Q_OBJECT

public:
    enum class TargetKind {
        AxisTitle,
        PlotTitle,
        LegendItem,
        TextAnnotation
    };

    static CanvasInlineTextEditor *openForAxisTitle(Graph *graph, int qwtAxisId,
                                                    const QRect &anchorGlobalRect);
    static CanvasInlineTextEditor *openForPlotTitle(Graph *graph, const QRect &anchorGlobalRect);
    static CanvasInlineTextEditor *openForLegendOrText(Graph *graph, long markerKey,
                                                       int legendLineIndex,
                                                       const QRect &anchorGlobalRect);

    static void cancelActiveForMultiLayer(MultiLayer *ml);
    static void cancelActiveForGraph(Graph *g);

    static QString normalizeOriginRichText(const QString &input);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    explicit CanvasInlineTextEditor(Graph *graph, TargetKind kind, QWidget *parent = nullptr);

    void buildUi(bool multiLine, const QString &initialText);
    void popupNearRect(const QRect &anchorGlobalRect);

    QString currentEditorText() const;
    void wrapSelection(const QString &openTag, const QString &closeTag);
    void insertSnippet(const QString &snippet);

    void commitIfNeeded();
    void cancelAndClose();
    void openLegacyDetailsDialog();

    static QPointer<CanvasInlineTextEditor> s_active;

    QPointer<Graph> d_graph;
    TargetKind d_kind;
    int d_qwt_axis = -1;
    long d_marker_key = -1;
    int d_legend_line_index = -1;

    QString d_initial_clean_text;
    QString d_preserved_prefix;
    QString d_original_placeholder_tail;
    bool d_had_placeholder = false;

    QLineEdit *d_line_edit = nullptr;
    QPlainTextEdit *d_text_edit = nullptr;

    bool d_finished = false;
};

#endif // CANVAS_INLINE_TEXT_EDITOR_H
