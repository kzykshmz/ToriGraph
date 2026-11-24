/***************************************************************************
    File                 : CanvasInlineTextEditor.cpp
    Project              : ToriGraph
    Description          : Frameless inline rich-text popup editor for titles,
                           legend items, and text annotations on 2D graphs
 ***************************************************************************/
#include "CanvasInlineTextEditor.h"

#include "Graph.h"
#include "GraphMiniToolbar.h"
#include "Legend.h"
#include "MultiLayer.h"
#include "Plot.h"

#include <QCloseEvent>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScreen>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <qwt_plot.h>
#include <qwt_text.h>

QPointer<CanvasInlineTextEditor> CanvasInlineTextEditor::s_active = nullptr;

namespace {
static QStringList splitLegendLinesPreservingEmpty(const QString &raw)
{
    QString norm = raw;
    norm.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    norm.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (norm.isEmpty())
        return QStringList{ QString() };
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return norm.split('\n', Qt::KeepEmptyParts);
#else
    return norm.split('\n', QString::KeepEmptyParts);
#endif
}

static void splitLegendRowToken(Graph *graph, const QString &rawLine, QString &outPrefix,
                                QString &outCleanText, bool &outHadPlaceholder,
                                QString &outPlaceholderTail)
{
    outPrefix.clear();
    outCleanText = rawLine;
    outHadPlaceholder = false;
    outPlaceholderTail.clear();

    static const QRegularExpression symTokenRe(
            QStringLiteral("^(\\s*(?:\\\\c\\{(\\d+)\\}|\\\\l\\((\\d+)\\)|\\\\p\\{(\\d+)\\})\\s*)"));
    static const QRegularExpression purePlaceholderRe(QStringLiteral("^%\\((\\d+)\\)$"));
    static const QRegularExpression leadingPlaceholderRe(QStringLiteral("^(\\s*%\\((\\d+)\\)\\s*)"));

    const QRegularExpressionMatch symMatch = symTokenRe.match(rawLine);
    if (symMatch.hasMatch()) {
        outPrefix = symMatch.captured(1);
        const QString remainder = rawLine.mid(outPrefix.length());
        const QRegularExpressionMatch phMatch = purePlaceholderRe.match(remainder.trimmed());
        if (phMatch.hasMatch()) {
            const int oneBased = phMatch.captured(1).toInt();
            if (graph && oneBased >= 1 && oneBased <= graph->curves()) {
                if (const QwtPlotCurve *cv = graph->curve(oneBased - 1)) {
                    outCleanText = cv->title().text();
                    outHadPlaceholder = true;
                    outPlaceholderTail = remainder;
                    return;
                }
            }
        }
        outCleanText = remainder;
        return;
    }

    const QRegularExpressionMatch purePh = purePlaceholderRe.match(rawLine.trimmed());
    if (purePh.hasMatch()) {
        const int oneBased = purePh.captured(1).toInt();
        if (graph && oneBased >= 1 && oneBased <= graph->curves()) {
            if (const QwtPlotCurve *cv = graph->curve(oneBased - 1)) {
                outCleanText = cv->title().text();
                outHadPlaceholder = true;
                outPlaceholderTail = rawLine;
                return;
            }
        }
    }

    const QRegularExpressionMatch leadPh = leadingPlaceholderRe.match(rawLine);
    if (leadPh.hasMatch() && leadPh.capturedLength(1) < rawLine.length()) {
        outPrefix = leadPh.captured(1);
        outCleanText = rawLine.mid(outPrefix.length());
    }
}
} // namespace

QString CanvasInlineTextEditor::normalizeOriginRichText(const QString &input)
{
    QString out;
    out.reserve(input.size() + 16);

    const int n = input.length();
    int i = 0;
    while (i < n) {
        if (input[i] == QLatin1Char('\\') && i + 2 < n && input[i + 2] == QLatin1Char('(')) {
            const QChar code = input[i + 1];
            QString tag;
            if (code == QLatin1Char('i'))
                tag = QStringLiteral("i");
            else if (code == QLatin1Char('b'))
                tag = QStringLiteral("b");
            else if (code == QLatin1Char('-'))
                tag = QStringLiteral("sub");
            else if (code == QLatin1Char('+'))
                tag = QStringLiteral("sup");

            if (!tag.isEmpty()) {
                int depth = 1;
                int j = i + 3;
                while (j < n && depth > 0) {
                    if (input[j] == QLatin1Char('('))
                        ++depth;
                    else if (input[j] == QLatin1Char(')'))
                        --depth;
                    if (depth > 0)
                        ++j;
                }
                if (depth == 0 && j < n) {
                    const QString inner = input.mid(i + 3, j - (i + 3));
                    out += QLatin1Char('<') + tag + QLatin1Char('>')
                            + normalizeOriginRichText(inner) + QStringLiteral("</") + tag
                            + QLatin1Char('>');
                    i = j + 1;
                    continue;
                }
            }
        }
        out.append(input[i]);
        ++i;
    }
    return out;
}

CanvasInlineTextEditor::CanvasInlineTextEditor(Graph *graph, TargetKind kind, QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
      d_graph(graph),
      d_kind(kind)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setObjectName(QStringLiteral("CanvasInlineTextEditor"));
    setStyleSheet(QStringLiteral(
            "#CanvasInlineTextEditor {"
            "  background: #FFFFFF;"
            "  border: 1.5px solid #2563EB;"
            "  border-radius: 5px;"
            "}"
            "QToolButton {"
            "  font-family: 'Arial';"
            "  font-size: 9pt;"
            "  color: #1F2937;"
            "  background: #F3F4F6;"
            "  border: 1px solid #D1D5DB;"
            "  border-radius: 3px;"
            "  padding: 1px 5px;"
            "  min-height: 20px;"
            "}"
            "QToolButton:hover {"
            "  background: #E5E7EB;"
            "  border-color: #9CA3AF;"
            "}"
            "QLineEdit, QPlainTextEdit {"
            "  font-family: 'Arial';"
            "  font-size: 10pt;"
            "  color: #111827;"
            "  background: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 3px;"
            "  padding: 2px 5px;"
            "  selection-background-color: #2563EB;"
            "  selection-color: #FFFFFF;"
            "}"));
}

CanvasInlineTextEditor *CanvasInlineTextEditor::openForAxisTitle(Graph *graph, int qwtAxisId,
                                                                 const QRect &anchorGlobalRect)
{
    if (!graph || qwtAxisId < 0 || qwtAxisId >= QwtPlot::axisCnt)
        return nullptr;
    if (s_active)
        s_active->close();
    if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
        bar->hide();

    CanvasInlineTextEditor *ed =
            new CanvasInlineTextEditor(graph, TargetKind::AxisTitle, graph->window());
    ed->d_qwt_axis = qwtAxisId;
    ed->d_initial_clean_text = graph->axisTitle(qwtAxisId);
    ed->buildUi(false, ed->d_initial_clean_text);
    s_active = ed;
    ed->popupNearRect(anchorGlobalRect);
    return ed;
}

CanvasInlineTextEditor *CanvasInlineTextEditor::openForPlotTitle(Graph *graph,
                                                                 const QRect &anchorGlobalRect)
{
    if (!graph || !graph->plotWidget())
        return nullptr;
    if (s_active)
        s_active->close();
    if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
        bar->hide();

    CanvasInlineTextEditor *ed =
            new CanvasInlineTextEditor(graph, TargetKind::PlotTitle, graph->window());
    ed->d_initial_clean_text = graph->plotWidget()->title().text();
    ed->buildUi(false, ed->d_initial_clean_text);
    s_active = ed;
    ed->popupNearRect(anchorGlobalRect);
    return ed;
}

CanvasInlineTextEditor *CanvasInlineTextEditor::openForLegendOrText(Graph *graph, long markerKey,
                                                                    int legendLineIndex,
                                                                    const QRect &anchorGlobalRect)
{
    if (!graph || !graph->plotWidget())
        return nullptr;
    Legend *leg = dynamic_cast<Legend *>(graph->plotWidget()->marker(markerKey));
    if (!leg)
        return nullptr;
    if (s_active)
        s_active->close();
    if (GraphMiniToolbar *bar = GraphMiniToolbar::instance())
        bar->hide();

    const bool isMainLegend = (graph->legend() == leg);
    const TargetKind kind = isMainLegend ? TargetKind::LegendItem : TargetKind::TextAnnotation;
    CanvasInlineTextEditor *ed = new CanvasInlineTextEditor(graph, kind, graph->window());
    ed->d_marker_key = markerKey;

    if (isMainLegend) {
        const QStringList lines = splitLegendLinesPreservingEmpty(leg->text());
        const int idx = (legendLineIndex >= 0 && legendLineIndex < lines.size()) ? legendLineIndex
                                                                                 : 0;
        ed->d_legend_line_index = idx;
        const QString rawLine = (idx < lines.size()) ? lines[idx] : QString();
        splitLegendRowToken(graph, rawLine, ed->d_preserved_prefix, ed->d_initial_clean_text,
                            ed->d_had_placeholder, ed->d_original_placeholder_tail);
        ed->buildUi(false, ed->d_initial_clean_text);
    } else {
        ed->d_legend_line_index = -1;
        ed->d_initial_clean_text = leg->text();
        const bool multiLine = true;
        ed->buildUi(multiLine, ed->d_initial_clean_text);
    }

    s_active = ed;
    ed->popupNearRect(anchorGlobalRect);
    return ed;
}

void CanvasInlineTextEditor::buildUi(bool multiLine, const QString &initialText)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 5, 6, 6);
    root->setSpacing(4);

    QHBoxLayout *strip = new QHBoxLayout();
    strip->setContentsMargins(0, 0, 0, 0);
    strip->setSpacing(3);

    auto makeBtn = [this](const QString &label, const QString &tip, bool bold = false,
                          bool italic = false) -> QToolButton * {
        QToolButton *b = new QToolButton(this);
        b->setText(label);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);
        QFont f(QStringLiteral("Arial"), 9);
        f.setBold(bold);
        f.setItalic(italic);
        b->setFont(f);
        return b;
    };

    QToolButton *btnItalic = makeBtn(QStringLiteral("I"), tr("Italic (Ctrl+I)"), false, true);
    connect(btnItalic, &QToolButton::clicked, this,
            [this]() { wrapSelection(QStringLiteral("<i>"), QStringLiteral("</i>")); });
    strip->addWidget(btnItalic);

    QToolButton *btnBold = makeBtn(QStringLiteral("B"), tr("Bold (Ctrl+B)"), true, false);
    connect(btnBold, &QToolButton::clicked, this,
            [this]() { wrapSelection(QStringLiteral("<b>"), QStringLiteral("</b>")); });
    strip->addWidget(btnBold);

    QToolButton *btnSub = makeBtn(QStringLiteral("X\u2082"), tr("Subscript"));
    connect(btnSub, &QToolButton::clicked, this,
            [this]() { wrapSelection(QStringLiteral("<sub>"), QStringLiteral("</sub>")); });
    strip->addWidget(btnSub);

    QToolButton *btnSup = makeBtn(QStringLiteral("X\u00B2"), tr("Superscript"));
    connect(btnSup, &QToolButton::clicked, this,
            [this]() { wrapSelection(QStringLiteral("<sup>"), QStringLiteral("</sup>")); });
    strip->addWidget(btnSup);

    strip->addSpacing(3);

    const QStringList greekSymbols = { QStringLiteral("\u03B5"), QStringLiteral("\u03C3"),
                                       QStringLiteral("\u03BC"), QStringLiteral("\u0394"),
                                       QStringLiteral("\u03B8"), QStringLiteral("\u00B0"),
                                       QStringLiteral("\u00B1") };
    for (const QString &sym : greekSymbols) {
        QToolButton *gb = makeBtn(sym, tr("Insert %1").arg(sym));
        connect(gb, &QToolButton::clicked, this, [this, sym]() { insertSnippet(sym); });
        strip->addWidget(gb);
    }

    strip->addSpacing(3);

    QToolButton *btnUnits =
            makeBtn(tr("Unit") + QStringLiteral(" \u25BE"), tr("Insert Common Unit"));
    btnUnits->setPopupMode(QToolButton::InstantPopup);
    QMenu *unitMenu = new QMenu(btnUnits);
    unitMenu->setFont(QFont(QStringLiteral("Arial"), 9));
    const QList<QPair<QString, QString>> unitItems = {
        { QStringLiteral("MPa"), QStringLiteral("MPa") },
        { QStringLiteral("GPa"), QStringLiteral("GPa") },
        { QStringLiteral("wt.%"), QStringLiteral("wt.%") },
        { QStringLiteral("at.%"), QStringLiteral("at.%") },
        { QStringLiteral("mass ppm"), QStringLiteral("mass ppm") },
        { QStringLiteral("K"), QStringLiteral("K") },
        { QStringLiteral("\u00B0C"), QStringLiteral("\u00B0C") },
        { QStringLiteral("s\u207B\u00B9"), QStringLiteral("s<sup>-1</sup>") },
        { QStringLiteral("mm\u207B\u00B3"), QStringLiteral("mm<sup>-3</sup>") },
        { QStringLiteral("\u03BCm"), QStringLiteral("\u03BCm") },
        { QStringLiteral("nm"), QStringLiteral("nm") }
    };
    for (const auto &pair : unitItems) {
        QAction *act = unitMenu->addAction(pair.first);
        const QString snippet = pair.second;
        connect(act, &QAction::triggered, this, [this, snippet]() { insertSnippet(snippet); });
    }
    btnUnits->setMenu(unitMenu);
    strip->addWidget(btnUnits);

    strip->addStretch(1);

    QToolButton *btnDetails = makeBtn(tr("Details..."), tr("Open Full Properties Dialog"));
    connect(btnDetails, &QToolButton::clicked, this,
            &CanvasInlineTextEditor::openLegacyDetailsDialog);
    strip->addWidget(btnDetails);

    root->addLayout(strip);

    if (multiLine) {
        d_text_edit = new QPlainTextEdit(this);
        d_text_edit->setFont(QFont(QStringLiteral("Arial"), 10));
        d_text_edit->setPlainText(initialText);
        d_text_edit->setFixedHeight(64);
        d_text_edit->setMinimumWidth(340);
        d_text_edit->selectAll();
        d_text_edit->installEventFilter(this);
        connect(d_text_edit, &QPlainTextEdit::textChanged, this,
                &CanvasInlineTextEditor::updateRichPreview);
        root->addWidget(d_text_edit);
    } else {
        d_line_edit = new QLineEdit(this);
        d_line_edit->setFont(QFont(QStringLiteral("Arial"), 10));
        d_line_edit->setText(initialText);
        d_line_edit->setMinimumWidth(340);
        d_line_edit->selectAll();
        d_line_edit->installEventFilter(this);
        connect(d_line_edit, &QLineEdit::textChanged, this,
                &CanvasInlineTextEditor::updateRichPreview);
        connect(d_line_edit, &QLineEdit::returnPressed, this, [this]() {
            commitIfNeeded();
            close();
        });
        root->addWidget(d_line_edit);
    }

    d_preview_label = new QLabel(this);
    d_preview_label->setTextFormat(Qt::RichText);
    d_preview_label->setWordWrap(true);
    d_preview_label->setMinimumWidth(340);
    d_preview_label->setStyleSheet(QStringLiteral(
            "QLabel { font-family: 'Arial'; font-size: 9pt; color: #374151;"
            " background: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 3px;"
            " padding: 3px 6px; }"));
    d_preview_label->hide();
    root->addWidget(d_preview_label);
    updateRichPreview();
}

void CanvasInlineTextEditor::popupNearRect(const QRect &anchorGlobalRect)
{
    adjustSize();
    const QSize sz = sizeHint().expandedTo(QSize(356, height()));
    resize(sz);

    QPoint pt = anchorGlobalRect.topLeft() - QPoint(0, sz.height() + 4);
    QScreen *scr = QGuiApplication::screenAt(anchorGlobalRect.center());
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    if (scr) {
        const QRect avail = scr->availableGeometry();
        if (pt.y() < avail.top() + 4)
            pt.setY(anchorGlobalRect.bottom() + 4);
        if (pt.y() + sz.height() > avail.bottom() - 4)
            pt.setY(qMax(avail.top() + 4, avail.bottom() - sz.height() - 4));
        if (pt.x() + sz.width() > avail.right() - 4)
            pt.setX(qMax(avail.left() + 4, avail.right() - sz.width() - 4));
        if (pt.x() < avail.left() + 4)
            pt.setX(avail.left() + 4);
    }

    move(pt);
    show();
    raise();
    activateWindow();
    if (d_line_edit)
        d_line_edit->setFocus(Qt::PopupFocusReason);
    else if (d_text_edit)
        d_text_edit->setFocus(Qt::PopupFocusReason);
}

static bool hasRichTextMarkupInline(const QString &raw)
{
    static const QRegularExpression rxHtml(
            QStringLiteral("</?(?:i|b|u|sub|sup)>"),
            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rxOrigin(QStringLiteral("\\\\[ib\\-\\+]\\("));
    return raw.contains(rxHtml) || raw.contains(rxOrigin);
}

void CanvasInlineTextEditor::updateRichPreview()
{
    if (!d_preview_label)
        return;
    const QString raw = currentEditorText();
    if (raw.trimmed().isEmpty() || !hasRichTextMarkupInline(raw)) {
        if (!d_preview_label->isHidden()) {
            d_preview_label->hide();
            adjustSize();
        }
        return;
    }
    QString esc = normalizeOriginRichText(raw).toHtmlEscaped();
    static const QRegularExpression rxAllowed(
            QStringLiteral("&lt;(/?(?:i|b|u|sub|sup))&gt;"),
            QRegularExpression::CaseInsensitiveOption);
    esc.replace(rxAllowed, QStringLiteral("<\\1>"));
    esc.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    d_preview_label->setText(esc);
    if (d_preview_label->isHidden()) {
        d_preview_label->show();
        adjustSize();
    }
}

QString CanvasInlineTextEditor::currentEditorText() const
{
    if (d_line_edit)
        return d_line_edit->text();
    if (d_text_edit)
        return d_text_edit->toPlainText();
    return QString();
}

void CanvasInlineTextEditor::wrapSelection(const QString &openTag, const QString &closeTag)
{
    if (d_line_edit) {
        const QString text = d_line_edit->text();
        if (d_line_edit->hasSelectedText()) {
            const int start = d_line_edit->selectionStart();
            const QString sel = d_line_edit->selectedText();
            const QString repl = openTag + sel + closeTag;
            d_line_edit->setText(text.left(start) + repl + text.mid(start + sel.length()));
            d_line_edit->setSelection(start + openTag.length(), sel.length());
        } else {
            const int pos = d_line_edit->cursorPosition();
            d_line_edit->setText(text.left(pos) + openTag + closeTag + text.mid(pos));
            d_line_edit->setCursorPosition(pos + openTag.length());
        }
        d_line_edit->setFocus();
    } else if (d_text_edit) {
        QTextCursor cur = d_text_edit->textCursor();
        if (cur.hasSelection()) {
            const QString sel = cur.selectedText().replace(QChar(0x2029), QLatin1Char('\n'));
            const int start = cur.selectionStart();
            cur.insertText(openTag + sel + closeTag);
            cur.setPosition(start + openTag.length());
            cur.setPosition(start + openTag.length() + sel.length(), QTextCursor::KeepAnchor);
            d_text_edit->setTextCursor(cur);
        } else {
            const int pos = cur.position();
            cur.insertText(openTag + closeTag);
            cur.setPosition(pos + openTag.length());
            d_text_edit->setTextCursor(cur);
        }
        d_text_edit->setFocus();
    }
}

void CanvasInlineTextEditor::insertSnippet(const QString &snippet)
{
    if (d_line_edit) {
        d_line_edit->insert(snippet);
        d_line_edit->setFocus();
    } else if (d_text_edit) {
        d_text_edit->insertPlainText(snippet);
        d_text_edit->setFocus();
    }
}

bool CanvasInlineTextEditor::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == d_line_edit || watched == d_text_edit) && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            cancelAndClose();
            return true;
        }
        if (watched == d_text_edit
            && (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && !(ke->modifiers() & Qt::ShiftModifier)) {
            commitIfNeeded();
            close();
            return true;
        }
        if (ke->modifiers() == Qt::ControlModifier) {
            if (ke->key() == Qt::Key_I) {
                wrapSelection(QStringLiteral("<i>"), QStringLiteral("</i>"));
                return true;
            }
            if (ke->key() == Qt::Key_B) {
                wrapSelection(QStringLiteral("<b>"), QStringLiteral("</b>"));
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void CanvasInlineTextEditor::closeEvent(QCloseEvent *event)
{
    if (!d_finished)
        commitIfNeeded();
    if (s_active == this)
        s_active = nullptr;
    QFrame::closeEvent(event);
}

void CanvasInlineTextEditor::cancelActiveForMultiLayer(MultiLayer *ml)
{
    if (!ml || !s_active)
        return;
    Graph *g = s_active->d_graph.data();
    if (!g || g->multiLayerParent() == ml)
        s_active->cancelAndClose();
}

void CanvasInlineTextEditor::cancelActiveForGraph(Graph *g)
{
    if (!g || !s_active)
        return;
    if (s_active->d_graph.data() == g)
        s_active->cancelAndClose();
}

void CanvasInlineTextEditor::cancelAndClose()
{
    d_finished = true;
    close();
}

void CanvasInlineTextEditor::commitIfNeeded()
{
    if (d_finished)
        return;
    d_finished = true;

    Graph *g = d_graph.data();
    if (!g || !g->plotWidget())
        return;

    MultiLayer *ml = g->multiLayerParent();
    if (!ml)
        return;

    const QString rawEdited = currentEditorText();
    const QString normalized = normalizeOriginRichText(rawEdited);
    if (normalized == d_initial_clean_text && rawEdited == d_initial_clean_text)
        return;

    const QString beforeSnap = ml->captureUndoSnapshot();

    switch (d_kind) {
    case TargetKind::AxisTitle: {
        if (d_qwt_axis >= 0 && d_qwt_axis < QwtPlot::axisCnt) {
            QFont af = g->axisTitleFont(d_qwt_axis);
            if (af.family().trimmed().isEmpty()) {
                af.setFamily(QStringLiteral("Arial"));
                g->setAxisTitleFont(d_qwt_axis, af);
            }
            g->setAxisTitleAuto(d_qwt_axis, false);
            g->setAxisTitle(d_qwt_axis, normalized);
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Edit Axis Title"), beforeSnap);
        }
        break;
    }
    case TargetKind::PlotTitle: {
        QwtText t = g->plotWidget()->title();
        QFont tf = t.font();
        if (tf.family().trimmed().isEmpty())
            tf = QFont(QStringLiteral("Arial"), 14, QFont::Bold, false);
        t.setFont(tf);
        t.setText(normalized);
        g->plotWidget()->setTitle(t);
        g->replot();
        emit g->modifiedGraph();
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Edit Plot Title"), beforeSnap);
        break;
    }
    case TargetKind::LegendItem: {
        Legend *leg = dynamic_cast<Legend *>(g->plotWidget()->marker(d_marker_key));
        if (!leg)
            break;
        QFont lf = leg->font();
        if (lf.family().trimmed().isEmpty()) {
            lf.setFamily(QStringLiteral("Arial"));
            leg->setFont(lf);
        }
        QStringList lines = splitLegendLinesPreservingEmpty(leg->text());
        if (d_legend_line_index >= 0 && d_legend_line_index < lines.size()) {
            if (d_had_placeholder && normalized == d_initial_clean_text)
                lines[d_legend_line_index] = d_preserved_prefix + d_original_placeholder_tail;
            else
                lines[d_legend_line_index] = d_preserved_prefix + normalized;
            leg->setText(lines.join(QStringLiteral("\n")));
            g->replot();
            emit g->modifiedGraph();
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Edit Legend Text"), beforeSnap);
        }
        break;
    }
    case TargetKind::TextAnnotation: {
        Legend *leg = dynamic_cast<Legend *>(g->plotWidget()->marker(d_marker_key));
        if (!leg)
            break;
        QFont lf = leg->font();
        if (lf.family().trimmed().isEmpty()) {
            lf.setFamily(QStringLiteral("Arial"));
            leg->setFont(lf);
        }
        leg->setText(normalized);
        g->replot();
        emit g->modifiedGraph();
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Edit Text Annotation"), beforeSnap);
        break;
    }
    }
}

void CanvasInlineTextEditor::openLegacyDetailsDialog()
{
    commitIfNeeded();
    const QPointer<Graph> graphGuard = d_graph;
    const TargetKind kind = d_kind;
    const int qwtAxis = d_qwt_axis;
    const long markerKey = d_marker_key;
    close();

    QTimer::singleShot(0, [graphGuard, kind, qwtAxis, markerKey]() {
        Graph *g = graphGuard.data();
        if (!g)
            return;
        switch (kind) {
        case TargetKind::PlotTitle:
            emit g->viewTitleDialog();
            break;
        case TargetKind::AxisTitle:
            if (qwtAxis == QwtPlot::xBottom)
                emit g->xAxisTitleDblClicked();
            else if (qwtAxis == QwtPlot::yLeft)
                emit g->yAxisTitleDblClicked();
            else if (qwtAxis == QwtPlot::yRight)
                emit g->rightAxisTitleDblClicked();
            else if (qwtAxis == QwtPlot::xTop)
                emit g->topAxisTitleDblClicked();
            break;
        case TargetKind::LegendItem:
        case TargetKind::TextAnnotation:
            if (markerKey >= 0)
                g->setSelectedMarker(markerKey, false);
            emit g->viewTextDialog();
            break;
        }
    });
}
