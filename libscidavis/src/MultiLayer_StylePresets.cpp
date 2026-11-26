/***************************************************************************
    File                 : MultiLayer_StylePresets.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Multi layer widget (style presets application, JSON persistence, manager dialog)

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/
#include <QVector>
#include <QWidgetList>
#include <QPrinter>
#include <QPrintDialog>
#include <QDateTime>
#include <QApplication>
#include <QMessageBox>
#include <QBitmap>
#include <QImageWriter>
#include <QPainter>
#include <QPicture>
#include <QClipboard>
#include <QMouseEvent>
#include <QAbstractEventDispatcher>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>
#include <limits>

#if QT_VERSION >= 0x040300
#include <QSvgGenerator>
#endif

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_text_label.h>

#include "MultiLayer.h"
#include "ApplicationWindow.h"
#ifdef Q_OS_WIN
#include "OleServer.h"
#include "EmfPaintDevice.h"
#endif
#include "Plot.h"
#include "IconLoader.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ImageMarker.h"
#include "SelectionMoveResizer.h"
#include "LayerDialog.h"
#include "PlotCurve.h"
#include "FunctionCurve.h"
#include "ColorButton.h"
#include "ScaleDraw.h"
#include "ScalePicker.h"
#include "Spectrogram.h"
#include "GraphInspectorDock.h"
#include "GraphMiniToolbar.h"
#include "CanvasInlineTextEditor.h"
#include "ModernRibbonAndHud.h"
#include "ModernUiTheme.h"
#include "I18nJapanese.h"
#include <cmath>
#include "ExtraPlotTypes.h"
#include "OriginGadgets.h"

#include "QwtErrorPlotCurve.h"
#include "QwtBarCurve.h"
#include "QwtHistogram.h"
#include "BoxCurve.h"
#include "QwtPieCurve.h"
#include "RecalcEngine.h"
#include "Table.h"
#include "globals.h"

#include <qwt_symbol.h>
#include <QFile>
#include <QFrame>
#include <QRegularExpression>
#include <QToolButton>
#include <QMenu>
#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QUndoStack>
#include <QTimer>
#include <QSignalBlocker>
#include <QPointer>
#include <QSettings>
#include <QStatusBar>
#include <QFileInfo>
#include <QUndoCommand>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTreeWidget>
#include <QHeaderView>
#include <QScrollBar>
#include <QScrollArea>
#include <QMdiArea>
#include <QSet>
#include <QSignalBlocker>
#include <QPainterPath>
#include <cmath>

#include <gsl/gsl_vector.h>

#include <iostream>
using namespace std;
#include "MultiLayer_Internal.h"
using namespace MultiLayerInternal;

namespace {
bool isPresetManagedValue(double v, const QVector<int> &managed)
{
    for (int m : managed) {
        if (std::fabs(v - double(m)) < 0.05)
            return true;
    }
    return false;
}

bool isPresetManagedValue(double v, const QVector<double> &managed)
{
    for (double m : managed) {
        if (std::fabs(v - m) < 0.05)
            return true;
    }
    return false;
}
} // namespace

void MultiLayer::applyStylePreset(StylePreset preset)
{
    QString id = QStringLiteral("isij_85mm");
    if (preset == StylePresentationSlide28pt)
        id = QStringLiteral("slide_28pt");

    GraphStylePresetSpec spec;
    if (!findStylePreset(id, &spec)) {
        const QVector<GraphStylePresetSpec> defs = defaultBuiltInStylePresets();
        const int idx = qBound(0, static_cast<int>(preset), defs.size() - 1);
        spec = defs.at(idx);
    }
    applyStylePresetSpec(spec);
}

bool MultiLayer::applyStylePresetById(const QString &idOrName)
{
    GraphStylePresetSpec spec;
    if (!findStylePreset(idOrName, &spec))
        return false;
    applyStylePresetSpec(spec);
    return true;
}

void MultiLayer::applyStylePresetSpec(const GraphStylePresetSpec &spec)
{
    const bool suppressUndo = property("torigraph_suppress_preset_undo").toBool();
    const QString beforeSnap = suppressUndo ? QString() : captureUndoSnapshot();
    clearCtrlFontGestureBaseline();
    d_user_zoomed = false;
    d_applied_style_preset_id = spec.id;
    // Remembered for graph.to_python and .sciprj persistence.
    setProperty("toriAppliedStylePreset", spec.id);

    const bool isIsijFormat = spec.formatIsijUnitSlash;
    const bool isSlidePreset = spec.largeSlideLayout;
    const QString fontFam = spec.fontFamily.trimmed().isEmpty() ? QStringLiteral("Arial")
                                                                : spec.fontFamily.trimmed();

    d_keep_aspect_ratio = spec.keepAspectRatio;
    d_target_aspect_ratio = (spec.pageHeightMm > 1e-6)
            ? (spec.pageWidthMm / spec.pageHeightMm)
            : 1.0;
    setPageBackgroundColor(spec.pageBackground.isValid() ? spec.pageBackground : QColor(Qt::white));

    double pageW = qBound(20.0, spec.pageWidthMm, 1000.0);
    double pageH = qBound(20.0, spec.pageHeightMm, 1000.0);
    const int targetDpi = qBound(72, spec.targetDpi, 2400);
    const int marginPx = qBound(0, spec.marginPx, 80);
    const Graph::FramePreset framePreset = static_cast<Graph::FramePreset>(
            qBound(0, spec.framePreset, 3));
    const int axisLw = qBound(1, spec.axisLineWidth, 10);
    const int minTickLen = qBound(0, spec.minorTickLen, 20);
    const int majTickLen = qBound(0, spec.majorTickLen, 30);

    QFont titleFnt(fontFam, qMax(5, qRound(spec.axisTitlePt)),
                   spec.boldTitle ? QFont::Bold : QFont::Normal, false);
    titleFnt.setPointSizeF(qBound(5.0, spec.axisTitlePt, 72.0));
    titleFnt.setBold(spec.boldTitle);

    QFont tickFnt(fontFam, qMax(5, qRound(spec.tickLabelPt)),
                  spec.boldTick ? QFont::Bold : QFont::Normal, false);
    tickFnt.setPointSizeF(qBound(5.0, spec.tickLabelPt, 72.0));
    tickFnt.setBold(spec.boldTick);

    QFont legFnt(fontFam, qMax(5, qRound(spec.legendPt)),
                 spec.boldLegend ? QFont::Bold : QFont::Normal, false);
    legFnt.setPointSizeF(qBound(5.0, spec.legendPt, 72.0));
    legFnt.setBold(spec.boldLegend);

    const int legFrame = qBound(0, spec.legendFrame, 2);
    const QColor legBg(255, 255, 255, (legFrame == Legend::None) ? 0 : 235);
    const double targetCurveWidth = (spec.curveWidthF > 0.0)
            ? qBound(0.5, spec.curveWidthF, 20.0)
            : double(qBound(1, spec.minCurveWidth, 20));
    const int minSymSize = qBound(0, spec.minSymbolSize, 100);
    // Values regarded as "not customised by the user": the application defaults used for new
    // curves plus every known preset's value.
    QVector<double> managedCurveWidths;
    QVector<int> managedSymbolSizes;
    {
        ApplicationWindow *mApp = findMultiLayerApp(this);
        managedCurveWidths << double(mApp ? mApp->defaultCurveLineWidth : 2) << 2.0;
        managedSymbolSizes << (mApp ? mApp->defaultSymbolSize : 9) << 9;
        for (const GraphStylePresetSpec &ps : listStylePresets()) {
            const double pw = (ps.curveWidthF > 0.0) ? ps.curveWidthF : double(ps.minCurveWidth);
            if (!isPresetManagedValue(pw, managedCurveWidths))
                managedCurveWidths << pw;
            if (!managedSymbolSizes.contains(ps.minSymbolSize))
                managedSymbolSizes << ps.minSymbolSize;
        }
    }
    const QString undoLabel = spec.name.trimmed().isEmpty()
            ? tr("Apply Style Preset")
            : tr("Apply Preset: %1").arg(spec.name.trimmed());

    titleFnt.setHintingPreference(QFont::PreferNoHinting);
    tickFnt.setHintingPreference(QFont::PreferNoHinting);
    legFnt.setHintingPreference(QFont::PreferNoHinting);

    // E7: multi-panel figures get either "preset size per panel" or an explicit journal
    // double-column width (175 mm). The old 0.85/0.90 shrink factors produced odd sizes such as
    // 153 x 85 mm.
    d_last_preset_warnings.clear();
    const int panelCols = qMax(1, cols);
    const int panelRows = qMax(1, rows);
    if (panelCols > 1 || panelRows > 1) {
        if (spec.id.compare(QLatin1String("journal_2col_175mm"), Qt::CaseInsensitive) == 0
            || pageW >= 150.0) {
            if (pageH < 150.0 && panelRows > 1)
                pageH = qBound(20.0, pageH * double(panelRows), 1000.0);
        } else if (multiPanelSizing() == PanelSizingTwoColumn175) {
            const double aspectHW = (pageW > 1e-6) ? (pageH / pageW) : 1.0;
            const double panelW = 175.0 / double(panelCols);
            pageW = 175.0;
            pageH = qBound(20.0, panelW * aspectHW * double(panelRows), 1000.0);
        } else {
            pageW = qBound(20.0, pageW * double(panelCols), 1000.0);
            pageH = qBound(20.0, pageH * double(panelRows), 1000.0);
        }
    }
    if (panelCols <= 1 && panelRows <= 1 && pageW < 110.0) {
        for (int k = 0; k < graphsList.count(); ++k) {
            const Graph *gk = qobject_cast<const Graph *>(graphsList.at(k));
            if (gk && !d_inset_specs.contains(const_cast<Graph *>(gk)) && !gk->linkedParentLayer()
                && gk->isLegendVisible() && gk->plotWidget()
                && gk->plotWidget()->property("_outsideRightMargin").toInt() > 0) {
                pageW = 115.0;
                break;
            }
        }
    }

    setMargins(marginPx, marginPx, marginPx, marginPx);
    const double desiredUpperZoom = (pageW <= 110.0 && pageH <= 110.0) ? 1.6 : 1.0;
    {
        const bool prevInternalZoom = d_internal_zoom_change;
        d_internal_zoom_change = true;
        setPageZoom(1.0);
        d_internal_zoom_change = prevInternalZoom;
    }
    d_zoom_applied_lcanvas = QSize();
    d_zoom_base_lcanvas = QSizeF();
    for (int i = 0; i < graphsList.count(); ++i) {
        if (Graph *gr = qobject_cast<Graph *>(graphsList.at(i))) {
            gr->setProperty(kZoomAppliedGeomProp, QVariant());
            gr->setProperty(kZoomBaseGeomProp, QVariant());
        }
    }
    setPageSizePhysical(pageW, pageH, UnitMm, targetDpi);
    const double panelW = pageW / double(qMax(1, panelCols));
    const double panelH = pageH / double(qMax(1, panelRows));
    const double specAspect =
            (spec.pageHeightMm > 1e-6) ? (spec.pageWidthMm / spec.pageHeightMm) : 1.0;
    const double cellAspect = (panelH > 1.0) ? (panelW / panelH) : 1.0;
    d_keep_aspect_ratio = spec.keepAspectRatio;
    const bool isSquarePagePreset = d_keep_aspect_ratio
            && ((panelH > 1.0 && qAbs(cellAspect - 1.0) <= 0.12)
                || (panelCols == 1 && panelRows == 1 && qAbs(specAspect - 1.0) <= 0.12));
    d_target_aspect_ratio = isSquarePagePreset ? 1.0 : cellAspect;

    static const QRegularExpression kBoldTagRx(QStringLiteral("</?b\\s*>"),
                                               QRegularExpression::CaseInsensitiveOption);

    QVector<bool> legendWasDefaultTopLeft(graphsList.count(), false);

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr || !gr->plotWidget())
            continue;

        const bool isInset = d_inset_specs.contains(gr);
        const bool isLinkedOverlay = (gr->linkedParentLayer() != nullptr);

        if (!isLinkedOverlay && !isInset) {
            if (Legend *leg = gr->legend()) {
                const QRect cRect = gr->plotWidget()->plotLayout()->canvasRect();
                const QRect lRect = leg->rect();
                if (cRect.width() > 30 && cRect.height() > 30) {
                    if (lRect.left() <= cRect.width() * 0.28
                        && lRect.top() <= cRect.height() * 0.28) {
                        legendWasDefaultTopLeft[i] = true;
                    }
                } else {
                    legendWasDefaultTopLeft[i] = true;
                }
            }
            if (gr->isPoleFigurePlot()) {
                gr->enableAxis(QwtPlot::xBottom, false);
                gr->enableAxis(QwtPlot::yLeft, false);
                gr->enableAxis(QwtPlot::xTop, false);
                if (!gr->hasColorScaleOnAxis(QwtPlot::yRight))
                    gr->enableAxis(QwtPlot::yRight, false);
                gr->drawCanvasFrame(false, 0);
                gr->drawAxesBackbones(false);
            } else if (!gr->isTernaryPlot() && !gr->isPolarPlot() && !gr->isPiePlot()) {
                gr->applyFramePreset(framePreset);
                for (int k = 0; k < graphsList.count(); ++k) {
                    Graph *ch = qobject_cast<Graph *>(graphsList.at(k));
                    if (!ch || ch->linkedParentLayer() != gr)
                        continue;
                    if (Plot *pPlot = gr->plotWidget()) {
                        if (ch->xLinkMode() != 0) {
                            pPlot->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
                            pPlot->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
                        }
                        if (ch->yLinkMode() != 0) {
                            pPlot->setMajorTicksType(QwtPlot::yRight, ScaleDraw::None);
                            pPlot->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
                        }
                    }
                }
            } else {
                for (int a = 0; a < QwtPlot::axisCnt; ++a)
                    gr->enableAxis(a, false);
                gr->drawAxesBackbones(false);
            }
            gr->setBackgroundColor(QColor(Qt::white));
            gr->setCanvasBackground(QColor(Qt::white));
        } else if (isInset) {
            gr->setBackgroundColor(QColor(255, 255, 255, 0));
            gr->setCanvasBackground(QColor(Qt::white));
            gr->setAutoFillBackground(false);
            if (gr->plotWidget()) {
                gr->plotWidget()->setAutoFillBackground(false);
                gr->plotWidget()->setMargin(4);
            }
        }

        if (isInset) {
            gr->setAxesLinewidth(isSlidePreset ? 2 : 1);
            gr->setTicksLength(isSlidePreset ? 3 : 2,
                               isSlidePreset ? 6 : 4);
        } else {
            gr->setAxesLinewidth(axisLw);
            gr->changeTicksLength(minTickLen, majTickLen);
        }

        Plot *plot = gr->plotWidget();
        QwtText pTitle = plot->title();
        QString cleanPlotTitle = pTitle.text();
        cleanPlotTitle.remove(kBoldTagRx);
        pTitle.setText(cleanPlotTitle);
        pTitle.setFont(titleFnt);
        plot->setTitle(pTitle);

        if (isIsijFormat && !isInset) {
            gr->setAxisUnitFormat(Column::UnitFormat::ISIJSlash);
            gr->refreshAutoAxisTitlesFromColumns();
        }

        auto formatPresetAxisTitle = [](const QString &rawIn, bool toISIJSlash) -> QString {
            QString s = rawIn;
            static const QString kNoWrapOpen = QStringLiteral("<span style=\"white-space:nowrap;\">");
            static const QString kNoWrapClose = QStringLiteral("</span>");
            while (s.startsWith(kNoWrapOpen, Qt::CaseInsensitive)
                   && s.endsWith(kNoWrapClose, Qt::CaseInsensitive)
                   && s.size() >= kNoWrapOpen.size() + kNoWrapClose.size()) {
                s = s.mid(kNoWrapOpen.size(),
                          s.size() - kNoWrapOpen.size() - kNoWrapClose.size());
            }
            if (s.trimmed().isEmpty())
                return s;

            if (s.contains(QLatin1Char('^')) || s.contains(QLatin1Char('_'))) {
                static const QRegularExpression kHtmlSplitRx(QStringLiteral("(<[^>]+>)"));
                static const QRegularExpression kBrSupRx(QStringLiteral("\\^\\{([^}]+)\\}"));
                static const QRegularExpression kBrSubRx(QStringLiteral("_\\{([^}]+)\\}"));
                static const QRegularExpression kPlSupRx(QStringLiteral(
                        "([A-Za-z0-9\\)\\]\\x{0370}-\\x{03FF}\\x{00B0}\\x{00C5}])\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"));
                static const QRegularExpression kLog10Rx(QStringLiteral("\\blog_10\\b"));
                static const QRegularExpression kSymSubRx(QStringLiteral(
                        "(?<![A-Za-z0-9\\x{0370}-\\x{03FF}])([A-Za-z\\x{0370}-\\x{03FF}])_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"));

                QString rebuilt;
                rebuilt.reserve(s.size() + 24);
                int subD = 0, supD = 0, pos = 0;
                auto xform = [&](QString ch) -> QString {
                    if (subD > 0 || supD > 0 || ch.isEmpty())
                        return ch;
                    ch.replace(kBrSupRx, QStringLiteral("<sup>\\1</sup>"));
                    ch.replace(kBrSubRx, QStringLiteral("<sub>\\1</sub>"));
                    ch.replace(kPlSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
                    ch.replace(kLog10Rx, QStringLiteral("log<sub>10</sub>"));
                    ch.replace(kSymSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
                    return ch;
                };
                auto it = kHtmlSplitRx.globalMatch(s);
                while (it.hasNext()) {
                    const QRegularExpressionMatch m = it.next();
                    if (m.capturedStart() > pos)
                        rebuilt += xform(s.mid(pos, m.capturedStart() - pos));
                    const QString tag = m.captured(1);
                    const QString tl = tag.toLower();
                    if (tl.startsWith(QLatin1String("<sub")) && !tl.endsWith(QLatin1String("/>")))
                        ++subD;
                    else if (tl.startsWith(QLatin1String("</sub")) && subD > 0)
                        --subD;
                    else if (tl.startsWith(QLatin1String("<sup"))
                             && !tl.endsWith(QLatin1String("/>")))
                        ++supD;
                    else if (tl.startsWith(QLatin1String("</sup")) && supD > 0)
                        --supD;
                    rebuilt += tag;
                    pos = m.capturedEnd();
                }
                if (pos < s.size())
                    rebuilt += xform(s.mid(pos));
                static const QRegularExpression kPostTagSubRx(
                        QStringLiteral(
                                "(</\\s*i\\s*>)_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"),
                        QRegularExpression::CaseInsensitiveOption);
                static const QRegularExpression kPostTagSupRx(
                        QStringLiteral(
                                "(</\\s*(?:i|sub)\\s*>)\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"),
                        QRegularExpression::CaseInsensitiveOption);
                rebuilt.replace(kPostTagSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
                rebuilt.replace(kPostTagSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
                s = rebuilt;
            }

            if (toISIJSlash) {
                const QString tr = s.trimmed();
                if (tr.endsWith(QLatin1Char(')')) && !tr.contains(QLatin1String(" / "))) {
                    int depth = 0;
                    int openIdx = -1;
                    for (int idx = tr.size() - 1; idx >= 0; --idx) {
                        const QChar c = tr.at(idx);
                        if (c == QLatin1Char(')')) {
                            ++depth;
                        } else if (c == QLatin1Char('(')) {
                            --depth;
                            if (depth == 0) {
                                openIdx = idx;
                                break;
                            }
                        }
                    }
                    if (openIdx >= 2 && tr.at(openIdx - 1).isSpace()) {
                        const QString base = tr.left(openIdx).trimmed();
                        const QString unit = tr.mid(openIdx + 1, tr.size() - openIdx - 2).trimmed();
                        QString plainUnit = unit;
                        plainUnit.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
                        const QString uLow = plainUnit.trimmed().toLower();
                        static const QRegularExpression kTemperCodeRx(
                                QStringLiteral("^(?:T\\d+|O|F|H\\d+|RRA)$"));
                        const bool keepParen = base.isEmpty() || unit.isEmpty()
                                || plainUnit == QLatin1String("-")
                                || plainUnit == QStringLiteral("\u2013")
                                || plainUnit == QStringLiteral("\u2014")
                                || plainUnit == QStringLiteral("\u2212")
                                || uLow == QLatin1String("none")
                                || uLow == QLatin1String("dimensionless")
                                || uLow == QLatin1String("a.u.") || uLow == QLatin1String("a.u")
                                || uLow == QLatin1String("au")
                                || uLow == QLatin1String("arb. units")
                                || uLow == QLatin1String("arb. unit")
                                || uLow == QLatin1String("arb.units")
                                || uLow == QLatin1String("arbitrary units")
                                || kTemperCodeRx.match(plainUnit.trimmed()).hasMatch();
                        if (!keepParen)
                            s = base + QStringLiteral(" / ") + unit;
                    }
                }
            }
            return s;
        };

        const int insetPt = isSlidePreset ? 14 : 8;
        const bool applyPresetTitleFmt = !isInset && (isIsijFormat || isSlidePreset);
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            QFont useTick = isInset ? QFont(fontFam, insetPt, QFont::Normal, false) : tickFnt;
            useTick.setHintingPreference(QFont::PreferNoHinting);
            QFont useTitle = isInset ? QFont(fontFam, insetPt, QFont::Normal, false) : titleFnt;
            useTitle.setHintingPreference(QFont::PreferNoHinting);
            gr->setAxisFont(a, useTick);
            gr->setAxisTitleFont(a, useTitle);
            if (QwtScaleWidget *sw = plot->axisWidget(a)) {
                const double pt = (useTick.pointSizeF() > 0.0) ? useTick.pointSizeF() : 12.0;
                sw->setSpacing(qMax(8, qRound(pt * 0.45)));
            }
            QString rawTitle = gr->axisTitle(a);
            QString nextAxisTitle = rawTitle;
            nextAxisTitle.remove(kBoldTagRx);
            if (applyPresetTitleFmt) {
                nextAxisTitle = formatPresetAxisTitle(nextAxisTitle, isIsijFormat);
            }
            if (nextAxisTitle != rawTitle) {
                const bool wasAuto = gr->isAxisTitleAuto(a);
                gr->setAxisTitleAuto(a, false);
                gr->setAxisTitle(a, nextAxisTitle);
                gr->setAxisTitleAuto(a, wasAuto);
                gr->setAxisTitleFont(a, useTitle);
            }
        }

        for (QwtPlotItem *it : plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                Spectrogram *sp = static_cast<Spectrogram *>(it);
                if (applyPresetTitleFmt) {
                    QString cbT = sp->colorBarTitle();
                    if (cbT.trimmed().isEmpty() && sp->hasColorScale())
                        cbT = gr->axisTitle(sp->colorScaleAxis());
                    if (!cbT.trimmed().isEmpty()) {
                        cbT.remove(kBoldTagRx);
                        const QString fmtCbT = formatPresetAxisTitle(cbT, isIsijFormat);
                        if (fmtCbT != sp->colorBarTitle())
                            sp->setColorBarTitle(fmtCbT);
                    }
                }
                sp->enforceColorBarArialFont();
                if (sp->hasColorScale()) {
                    QFont useCbTitle =
                            isInset ? QFont("Arial", insetPt, QFont::Normal, false) : titleFnt;
                    useCbTitle.setHintingPreference(QFont::PreferNoHinting);
                    gr->setAxisTitleFont(sp->colorScaleAxis(), useCbTitle);
                }
                if (gr->isPoleFigurePlot())
                    sp->syncColorScale();
            }
        }
        if (DataCurve *pcm = gr->pointColorMapCurve()) {
            if (applyPresetTitleFmt) {
                DataCurve::PointColorMap pm = pcm->pointColorMap();
                QString cbT = pm.colorBarTitle;
                if (cbT.trimmed().isEmpty())
                    cbT = gr->axisTitle(QwtPlot::yRight);
                if (!cbT.trimmed().isEmpty()) {
                    cbT.remove(kBoldTagRx);
                    const QString fmtCbT = formatPresetAxisTitle(cbT, isIsijFormat);
                    if (fmtCbT != pm.colorBarTitle) {
                        pm.colorBarTitle = fmtCbT;
                        pcm->setPointColorMap(pm);
                    }
                }
            }
            gr->syncPointColorBar();
            QFont useCbTitle = isInset ? QFont("Arial", insetPt, QFont::Normal, false) : titleFnt;
            useCbTitle.setHintingPreference(QFont::PreferNoHinting);
            gr->setAxisTitleFont(QwtPlot::yRight, useCbTitle);
        }

        static const QRegularExpression kPanelLabelMarkerRx(
                QStringLiteral("^\\s*\\(?[A-Za-z0-9]{1,2}\\)?\\.?\\s*$"));
        gr->setTextMarkerDefaults(legFrame, legFnt, QColor(Qt::black), legBg);
        const QVector<int> txtKeys = gr->textMarkerKeys();
        for (int k = 0; k < txtKeys.size(); ++k) {
            Legend *mrk = gr->textMarker(txtKeys[k]);
            if (!mrk)
                continue;
            const QString rawMrkText = mrk->text();
            const QString trimmedMrkText = rawMrkText.trimmed();
            const bool isCurveLegend = (mrk == gr->legend()
                                        || rawMrkText.contains(QLatin1String("\\c{"))
                                        || rawMrkText.contains(QLatin1String("\\p{")));
            const bool isPanelLabel = kPanelLabelMarkerRx.match(trimmedMrkText).hasMatch();
            const bool isFitCallout = trimmedMrkText.contains(QStringLiteral("R\u00B2"))
                    || trimmedMrkText.contains(QLatin1String("R^2"))
                    || trimmedMrkText.contains(QLatin1String("Chi^2"))
                    || (trimmedMrkText.contains(QLatin1Char('='))
                        && trimmedMrkText.contains(QStringLiteral("\u00B1")));
            const bool isSigLabel = !isCurveLegend && !isPanelLabel && !isFitCallout
                    && (trimmedMrkText == QLatin1String("*")
                        || trimmedMrkText == QLatin1String("**")
                        || trimmedMrkText == QLatin1String("***")
                        || trimmedMrkText == QLatin1String("****")
                        || trimmedMrkText.compare(QLatin1String("ns"), Qt::CaseInsensitive) == 0
                        || trimmedMrkText.startsWith(QLatin1String("p="), Qt::CaseInsensitive)
                        || trimmedMrkText.startsWith(QLatin1String("p "), Qt::CaseInsensitive)
                        || trimmedMrkText.startsWith(QLatin1String("<i>p</i>"),
                                                     Qt::CaseInsensitive));
            const QFont mf = mrk->font();
            const double curPt = mf.pointSizeF() > 0.0
                    ? mf.pointSizeF()
                    : (mf.pointSize() > 0 ? double(mf.pointSize()) : 0.0);
            const double maxAutoScalePt = isSlidePreset ? 14.0 : 12.0;
            const bool shouldApplyLegFnt = !isSigLabel
                    && (isCurveLegend || isPanelLabel || isFitCallout
                        || (curPt > 0.0 && curPt <= maxAutoScalePt + 1e-6));
            if (isSigLabel) {
                QFont sigFnt = legFnt;
                const double baseLegPt = legFnt.pointSizeF() > 0.0
                        ? legFnt.pointSizeF()
                        : (legFnt.pointSize() > 0 ? double(legFnt.pointSize()) : 9.0);
                sigFnt.setPointSizeF(
                        qBound(8.0, baseLegPt - (isSlidePreset ? 4.0 : 0.0), 16.0));
                sigFnt.setBold(false);
                sigFnt.setHintingPreference(QFont::PreferNoHinting);
                mrk->setFont(sigFnt);
            } else if (shouldApplyLegFnt) {
                mrk->setFont(legFnt);
            } else {
                QFont customFnt = mrk->font();
                customFnt.setFamily(fontFam);
                customFnt.setBold(false);
                customFnt.setHintingPreference(QFont::PreferNoHinting);
                mrk->setFont(customFnt);
            }
            if (isCurveLegend || mrk->textColor().lightness() <= 200)
                mrk->setTextColor(QColor(Qt::black));
            if (isCurveLegend) {
                mrk->setFrameStyle(legFrame);
                mrk->setBackgroundColor(legBg);
            }
        }

        // E2: the preset's curve width / symbol size are set values (so switching presets can
        // also make lines thinner again). Curves the user adjusted by hand keep their values: a
        // value counts as "managed" only if it equals the app default or some preset's value.
        for (int cIdx = 0; cIdx < gr->curves(); ++cIdx) {
            QwtPlotCurve *c = gr->curve(cIdx);
            if (!c)
                continue;
            const bool isFilledType = dynamic_cast<QwtBarCurve *>(c) != nullptr
                    || dynamic_cast<QwtHistogram *>(c) != nullptr
                    || dynamic_cast<BoxCurve *>(c) != nullptr
                    || dynamic_cast<QwtPieCurve *>(c) != nullptr
                    || dynamic_cast<QwtErrorPlotCurve *>(c) != nullptr
                    || c->rtti() != QwtPlotItem::Rtti_PlotCurve;
            QPen pen = c->pen();
            if (isFilledType) {
                // Bar/box outlines and error bars keep their own widths, except that very thin
                // outlines are still raised to 1 px on slides.
                if (pen.style() != Qt::NoPen && isSlidePreset && pen.widthF() < 1.0) {
                    pen.setWidth(1);
                    c->setPen(pen);
                }
            } else if (pen.style() != Qt::NoPen
                       && isPresetManagedValue(pen.widthF(), managedCurveWidths)) {
                pen.setWidthF(targetCurveWidth);
                c->setPen(pen);
            }
            if (!isFilledType && minSymSize > 0 && c->symbol().style() != QwtSymbol::NoSymbol) {
                QwtSymbol sym = c->symbol();
                if (sym.size().width() == sym.size().height()
                    && isPresetManagedValue(double(sym.size().width()), managedSymbolSizes)) {
                    sym.setSize(QSize(minSymSize, minSymSize));
                }
                if (isSlidePreset) {
                    QPen sp = sym.pen();
                    if (sp.widthF() < 2.0)
                        sp.setWidthF(2.0);
                    sym.setPen(sp);
                }
                c->setSymbol(sym);
            }
        }
        if ((isIsijFormat || isSlidePreset || isInset)
            && !isLinkedOverlay && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPoleFigurePlot() && !gr->isPiePlot()) {
            plot->updateAxes();
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (gr->hasColorScaleOnAxis(a) || gr->hasWeibullProbabilityAxis(a)
                    || gr->hasCustomTickLabels(a)) {
                    if (gr->hasWeibullProbabilityAxis(a) || gr->hasCustomTickLabels(a))
                        gr->updateSecondaryAxis(a);
                    continue;
                }
                if (const QwtTextScaleDraw *tsd =
                            dynamic_cast<const QwtTextScaleDraw *>(plot->axisScaleDraw(a))) {
                    const QMap<int, QString> catMap = tsd->labelsMap();
                    plot->setAxisMaxMinor(a, 0);
                    gr->plotWidget()->setMinorTicksType(a, ScaleDraw::None);
                    if (!catMap.isEmpty()) {
                        const double minKey = double(catMap.firstKey());
                        const double maxKey = double(catMap.lastKey());
                        const bool isInvCat = plot->axisScaleEngine(a)
                                && plot->axisScaleEngine(a)->testAttribute(
                                        QwtScaleEngine::Inverted);
                        double cLo = minKey - 0.5;
                        double cHi = maxKey + 0.5;
                        if (gr->isAxisUserLocked(a)) {
                            if (const QwtScaleDiv *curDiv = plot->axisScaleDiv(a)) {
                                const double curLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
                                const double curHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
                                if (std::isfinite(curLo) && std::isfinite(curHi) && curHi > curLo
                                    && curLo >= minKey - 1.5 && curLo <= minKey
                                    && curHi >= maxKey && curHi <= maxKey + 4.5) {
                                    cLo = qMin(cLo, curLo);
                                    cHi = qMax(cHi, curHi);
                                }
                            }
                        }
                        QwtValueList catMaj;
                        for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
                            catMaj += double(it.key());
                        QwtValueList emptyTicks;
                        QwtValueList catTicks[QwtScaleDiv::NTickTypes];
                        catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
                        catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
                        catTicks[QwtScaleDiv::MajorTick] = catMaj;
                        QwtScaleEngine *catEng = new QwtLinearScaleEngine();
                        QwtScaleDiv catDiv(cLo, cHi, catTicks);
                        if (isInvCat) {
                            catEng->setAttribute(QwtScaleEngine::Inverted);
                            catDiv.invert();
                        }
                        plot->setAxisMaxMajor(a, qMax(8, catMap.size() + 2));
                        plot->setAxisScale(a, isInvCat ? cHi : cLo, isInvCat ? cLo : cHi, 1.0);
                        plot->setAxisScaleEngine(a, catEng);
                        plot->setAxisScaleDiv(a, catDiv);
                        if (a == QwtPlot::xBottom && gr->labelsRotation(QwtPlot::xBottom) == 0
                            && catMap.size() >= 3) {
                            const QFontMetrics cfm(gr->axisFont(QwtPlot::xBottom));
                            int sumCatW = 0;
                            for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
                                sumCatW += cfm.horizontalAdvance(it.value()) + 8;
                            const int estW = (plot->plotLayout()
                                              && plot->plotLayout()->canvasRect().width() >= 80)
                                    ? plot->plotLayout()->canvasRect().width()
                                    : qMax(140,
                                           qRound(0.70 * qMax(160, canvas ? canvas->width() : 321)));
                            if (sumCatW > qRound(0.84 * double(estW)))
                                gr->setAxisLabelRotation(QwtPlot::xBottom, -35);
                        }
                        const int opp = (a == QwtPlot::xBottom) ? QwtPlot::xTop
                                : (a == QwtPlot::yLeft)         ? QwtPlot::yRight
                                                                : -1;
                        if (opp >= 0 && !gr->plotWidget()->hasVisibleItemOnAxis(opp)
                            && !gr->hasColorScaleOnAxis(opp)
                            && !gr->hasWeibullProbabilityAxis(opp)
                            && !gr->hasCustomTickLabels(opp)) {
                            gr->plotWidget()->setMinorTicksType(opp, ScaleDraw::None);
                            gr->plotWidget()->setMajorTicksType(opp, ScaleDraw::None);
                        }
                    }
                    continue;
                }
                const int insetMaj =
                        (a == QwtPlot::xBottom || a == QwtPlot::xTop) ? 2 : 3;
                if ((a == QwtPlot::xTop || a == QwtPlot::yRight)
                    && !gr->plotWidget()->hasVisibleItemOnAxis(a)) {
                    if (isInset) {
                        plot->setAxisMaxMajor(a, insetMaj);
                        plot->setAxisMaxMinor(a, 1);
                    }
                    continue;
                }
                const QwtScaleEngine *se = plot->axisScaleEngine(a);
                const QwtScaleDiv *sd = plot->axisScaleDiv(a);
                if (!se || !sd)
                    continue;
                const bool isLog = Plot::isLog10ScaleEngine(se);
                const bool isInv = se->testAttribute(QwtScaleEngine::Inverted);
                const int targetMinTicks = isInset ? 1 : (isLog ? 8 : 1);
                const int maxMinInt = isInset ? 2 : (isLog ? 9 : 2);
                const double lo = qMin(sd->lowerBound(), sd->upperBound());
                const double hi = qMax(sd->lowerBound(), sd->upperBound());
                const QwtValueList majTicks = sd->ticks(QwtScaleDiv::MajorTick);
                if (isInset)
                    plot->setAxisMaxMajor(a, insetMaj);
                plot->setAxisMaxMinor(a, targetMinTicks);

                if (gr->axisBreak(a).enabled) {
                    gr->setAxisBreak(a, gr->axisBreak(a));
                    continue;
                }

                if (!(hi > lo) || !std::isfinite(lo) || !std::isfinite(hi))
                    continue;

                bool crowdedHorizTicks = false;
                if (!isInset && !isLog && a == QwtPlot::xBottom && majTicks.size() > 5) {
                    const QFontMetrics tfm(gr->axisFont(a));
                    int sumLabelW = 0;
                    for (int ti = 0; ti < majTicks.size(); ++ti) {
                        const QString lbl = plot->axisScaleDraw(a)
                                ? plot->axisScaleDraw(a)->label(majTicks[ti]).text()
                                : QString::number(majTicks[ti], 'g', 6);
                        sumLabelW += tfm.horizontalAdvance(lbl) + 8;
                    }
                    const int estCanvasW =
                            (plot->plotLayout() && plot->plotLayout()->canvasRect().width() >= 80)
                            ? plot->plotLayout()->canvasRect().width()
                            : qMax(140, qRound(0.70 * qMax(160, canvas ? canvas->width() : 321)));
                    if (sumLabelW > qRound(0.88 * double(estCanvasW)))
                        crowdedHorizTicks = true;
                }

                int maj = plot->axisMaxMajor(a);
                if (isInset)
                    maj = insetMaj;
                else if (isSlidePreset || crowdedHorizTicks)
                    maj = qMin(maj, crowdedHorizTicks ? 4 : 5);
                double step = isInset ? 0.0 : gr->axisStep(a);
                if (!isInset && step <= 0.0 && !isLog && majTicks.size() >= 2) {
                    if (!((isSlidePreset && majTicks.size() > 6)
                          || crowdedHorizTicks))
                        step = std::fabs(majTicks[1] - majTicks[0]);
                }

                QwtScaleEngine *newEng = isLog
                        ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                        : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
                QwtScaleDiv newDiv =
                        newEng->divideScale(lo, hi, maj, maxMinInt, isLog ? 0.0 : step);
                if (!isLog && targetMinTicks == 1
                    && newDiv.ticks(QwtScaleDiv::MinorTick).isEmpty()
                    && newDiv.ticks(QwtScaleDiv::MediumTick).isEmpty()) {
                    const QwtValueList newMaj = newDiv.ticks(QwtScaleDiv::MajorTick);
                    if (newMaj.size() >= 2) {
                        const double s = std::fabs(newMaj[1] - newMaj[0]);
                        const double half = 0.5 * s;
                        const double dLo = qMin(newDiv.lowerBound(), newDiv.upperBound());
                        const double dHi = qMax(newDiv.lowerBound(), newDiv.upperBound());
                        const double eps = 1e-9 * s;
                        double fM = newMaj.first();
                        double lM = newMaj.last();
                        if (fM > lM)
                            qSwap(fM, lM);
                        QwtValueList mids;
                        if (fM - half >= dLo - eps)
                            mids += (fM - half);
                        for (int mi = 0; mi + 1 < newMaj.size(); ++mi) {
                            const double mv = 0.5 * (newMaj[mi] + newMaj[mi + 1]);
                            if (mv >= dLo - eps && mv <= dHi + eps)
                                mids += mv;
                        }
                        if (lM + half <= dHi + eps)
                            mids += (lM + half);
                        if (!mids.isEmpty())
                            newDiv.setTicks(QwtScaleDiv::MediumTick, mids);
                    }
                }
                if (isInv) {
                    newEng->setAttribute(QwtScaleEngine::Inverted);
                    newDiv.invert();
                }
                // Keep the major-tick cap the division above used (slide presets: at most 5) as
                // the axis' max-major count, so re-dividing the range (autoscale on the next
                // updatePlot, the pptx OLE export, saving/reopening, cloning) gives the same ticks.
                if (!isLog && maj != plot->axisMaxMajor(a))
                    plot->setAxisMaxMajor(a, maj);
                plot->setAxisScale(a, isInv ? hi : lo, isInv ? lo : hi, isLog ? 0.0 : step);
                plot->setAxisScaleEngine(a, newEng);
                plot->setAxisScaleDiv(a, newDiv);
            }
            gr->updateSecondaryAxis(QwtPlot::xTop);
            gr->updateSecondaryAxis(QwtPlot::yRight);
            gr->syncChildLinkedLayers();
        }

        plot->updateLayout();
        plot->replot();
    }

    if (isSquarePagePreset)
        applySquareCanvasFrame70Percent();
    else
        fitLayersToPage();

    {
        bool anyPostRotated = false;
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget() || d_inset_specs.contains(gr) || gr->linkedParentLayer()
                || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPoleFigurePlot()
                || gr->isPiePlot())
                continue;
            Plot *pl = gr->plotWidget();
            pl->updateLayout();
            if (const QwtTextScaleDraw *xCatSd = dynamic_cast<const QwtTextScaleDraw *>(
                        pl->axisScaleDraw(QwtPlot::xBottom))) {
                const QMap<int, QString> cMap = xCatSd->labelsMap();
                if (cMap.size() >= 2 && gr->labelsRotation(QwtPlot::xBottom) == 0) {
                    const QFontMetrics xfm(gr->axisFont(QwtPlot::xBottom));
                    const int minGapPx = qMax(10, xfm.height() / 2);
                    int sumW = 0;
                    int maxItemW = 0;
                    for (auto it = cMap.constBegin(); it != cMap.constEnd(); ++it) {
                        const int w = xfm.horizontalAdvance(it.value());
                        sumW += w + minGapPx;
                        maxItemW = qMax(maxItemW, w);
                    }
                    const int finalCw = qMax(60, pl->plotLayout()->canvasRect().width());
                    const int slotW = finalCw / qMax(1, cMap.size());
                    if (sumW > qRound(0.82 * double(finalCw)) || maxItemW + minGapPx > slotW) {
                        gr->setAxisLabelRotation(QwtPlot::xBottom, -35);
                        pl->updateLayout();
                        anyPostRotated = true;
                    }
                }
            }
        }
        if (anyPostRotated) {
            if (isSquarePagePreset)
                applySquareCanvasFrame70Percent();
            else
                fitLayersToPage();
        }
    }

    {
        // E2: axis titles keep the point size given by the preset. A title wider than its axis
        // is reported (status bar + lastStylePresetWarnings()) instead of being silently
        // wrapped onto two lines or shrunk.
        for (int i = 0; i < graphsList.count(); ++i) {
            Graph *gr = (Graph *)graphsList.at(i);
            if (!gr || !gr->plotWidget() || d_inset_specs.contains(gr) || gr->linkedParentLayer()
                || gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPoleFigurePlot()
                || gr->isPiePlot())
                continue;
            Plot *pl = gr->plotWidget();
            pl->updateLayout();
            const QRect cr = pl->plotLayout()->canvasRect();
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (!pl->axisEnabled(a))
                    continue;
                const int spanPx =
                        (a == QwtPlot::xBottom || a == QwtPlot::xTop) ? cr.width() : cr.height();
                if (spanPx < 50)
                    continue;
                const QwtText curQt = pl->axisTitle(a);
                if (curQt.text().trimmed().isEmpty())
                    continue;
                if (curQt.textSize().width() <= spanPx - 6)
                    continue;
                QString plain = curQt.text();
                plain.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                const QFont tf = gr->axisTitleFont(a);
                const double pt = (tf.pointSizeF() > 0.0) ? tf.pointSizeF() : double(tf.pointSize());
                d_last_preset_warnings << tr("Axis title \"%1\" is wider than the axis at %2 pt "
                                             "(size kept; shorten the title or enlarge the page).")
                                                  .arg(plain.trimmed())
                                                  .arg(pt, 0, 'g', 3);
            }
        }
        if (!d_last_preset_warnings.isEmpty()) {
            if (ApplicationWindow *wApp = findMultiLayerApp(this)) {
                if (wApp->statusBar())
                    wApp->statusBar()->showMessage(
                            d_last_preset_warnings.join(QStringLiteral("  ")), 12000);
            }
        }
    }

    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = (Graph *)graphsList.at(i);
        if (!gr || !gr->plotWidget())
            continue;
        const bool isInset = d_inset_specs.contains(gr);
        const bool isLinkedOverlay = (gr->linkedParentLayer() != nullptr);
        if (!isInset && !isLinkedOverlay && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPoleFigurePlot() && !gr->isPiePlot()) {
            Plot *pl = gr->plotWidget();
            pl->updateLayout();
            const QVector<int> lineKeys = gr->lineMarkerKeys();
            const QVector<int> txtKeys = gr->textMarkerKeys();
            if (!lineKeys.isEmpty() && !txtKeys.isEmpty()) {
                int maxSigBoxH = 0;
                for (int tid : txtKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (!tm || tm == gr->legend() || !tm->isVisible())
                        continue;
                    const QString st = tm->text().trimmed();
                    const bool isSig = (st == QLatin1String("*") || st == QLatin1String("**")
                                        || st == QLatin1String("***")
                                        || st == QLatin1String("****")
                                        || st.compare(QLatin1String("ns"), Qt::CaseInsensitive) == 0
                                        || st.startsWith(QLatin1String("p="), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("p "), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("<i>p</i>"),
                                                         Qt::CaseInsensitive));
                    if (isSig) {
                        QwtText qt(st);
                        qt.setFont(tm->font());
                        maxSigBoxH = qMax(maxSigBoxH, qt.textSize().height() + 4);
                    }
                }

                if (maxSigBoxH > 0 && pl->axisEnabled(QwtPlot::yLeft)
                    && !gr->axisBreak(QwtPlot::yLeft).enabled) {
                    const QwtScaleEngine *yEng = pl->axisScaleEngine(QwtPlot::yLeft);
                    const QwtScaleDiv *yDiv = pl->axisScaleDiv(QwtPlot::yLeft);
                    if (yEng && yDiv && !Plot::isLog10ScaleEngine(yEng)
                        && !yEng->testAttribute(QwtScaleEngine::Inverted)) {
                        double maxHorizBrkY = -1e300;
                        for (int lid : lineKeys) {
                            ArrowMarker *am = gr->arrow(lid);
                            if (!am || !am->isVisible() || am->yAxis() != QwtPlot::yLeft)
                                continue;
                            const QwtDoublePoint sC = am->startPointCoord();
                            const QwtDoublePoint eC = am->endPointCoord();
                            if (std::fabs(sC.y() - eC.y())
                                <= 1e-4 * qMax(1.0, std::fabs(sC.y()) + std::fabs(eC.y()))) {
                                maxHorizBrkY = qMax(maxHorizBrkY, qMax(sC.y(), eC.y()));
                            }
                        }
                        const QwtScaleMap yMap0 = pl->canvasMap(QwtPlot::yLeft);
                        const double chPx = qMax(40.0, std::fabs(yMap0.pDist()));
                        if (maxHorizBrkY > -1e290) {
                            const double topBrkPy = yMap0.xTransform(maxHorizBrkY);
                            const double reqTopPy = double(maxSigBoxH) + 8.0;
                            if (std::isfinite(topBrkPy) && topBrkPy < reqTopPy) {
                                const double yLo = qMin(yDiv->lowerBound(), yDiv->upperBound());
                                const double yHi = qMax(yDiv->lowerBound(), yDiv->upperBound());
                                const double reqFrac = qBound(0.10, reqTopPy / chPx, 0.38);
                                const double newYHi =
                                        qMax(yHi, (maxHorizBrkY - reqFrac * yLo) / (1.0 - reqFrac));
                                if (newYHi > yHi + 1e-9) {
                                    const QwtValueList mTicks =
                                            yDiv->ticks(QwtScaleDiv::MajorTick);
                                    const double step = (mTicks.size() >= 2)
                                            ? std::fabs(mTicks[1] - mTicks[0])
                                            : 0.0;
                                    gr->setScale(QwtPlot::yLeft, yLo, newYHi, step,
                                                 pl->axisMaxMajor(QwtPlot::yLeft),
                                                 pl->axisMaxMinor(QwtPlot::yLeft), 0, false);
                                    // setScale() already kept the markers in place; only the
                                    // secondary axis changes from here.
                                    const Graph::MarkerAxisState axesBefore =
                                            gr->markerAxisState();
                                    gr->updateSecondaryAxis(QwtPlot::yRight);
                                    gr->updateMarkersBoundingRect(axesBefore);
                                    pl->updateLayout();
                                }
                            }
                        }
                    }
                }

                for (int tid : txtKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (!tm || tm == gr->legend() || !tm->isVisible())
                        continue;
                    const QString st = tm->text().trimmed();
                    const bool isSig = (st == QLatin1String("*") || st == QLatin1String("**")
                                        || st == QLatin1String("***")
                                        || st == QLatin1String("****")
                                        || st.compare(QLatin1String("ns"), Qt::CaseInsensitive) == 0
                                        || st.startsWith(QLatin1String("p="), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("p "), Qt::CaseInsensitive)
                                        || st.startsWith(QLatin1String("<i>p</i>"),
                                                         Qt::CaseInsensitive));
                    if (!isSig)
                        continue;
                    const QwtScaleMap xMap = pl->canvasMap(tm->xAxis());
                    const QwtScaleMap yMap = pl->canvasMap(tm->yAxis());
                    if (std::fabs(xMap.pDist()) < 10.0 || std::fabs(yMap.pDist()) < 10.0)
                        continue;
                    ArrowMarker *bestBrk = nullptr;
                    double bestDyPx = 1e300;
                    const double tmPy = yMap.xTransform(tm->yValue());
                    for (int lid : lineKeys) {
                        ArrowMarker *am = gr->arrow(lid);
                        if (!am || !am->isVisible())
                            continue;
                        const QwtDoublePoint sC = am->startPointCoord();
                        const QwtDoublePoint eC = am->endPointCoord();
                        if (std::fabs(sC.y() - eC.y())
                            > 1e-4 * qMax(1.0, std::fabs(sC.y()) + std::fabs(eC.y())))
                            continue;
                        const double xLo = qMin(sC.x(), eC.x());
                        const double xHi = qMax(sC.x(), eC.x());
                        const double span = qMax(1e-9, xHi - xLo);
                        if (tm->xValue() < xLo - 0.35 * span || tm->xValue() > xHi + 0.35 * span)
                            continue;
                        const double brkPy = yMap.xTransform(sC.y());
                        const double dyPx = std::fabs(brkPy - tmPy);
                        if (dyPx < 48.0 && dyPx < bestDyPx) {
                            bestDyPx = dyPx;
                            bestBrk = am;
                        }
                    }
                    if (bestBrk) {
                        QwtText qt(st);
                        qt.setFont(tm->font());
                        const QSize tsz = qt.textSize();
                        const int boxW = tsz.width() + 8;
                        const int boxH = tsz.height() + 4;
                        const QwtDoublePoint sC = bestBrk->startPointCoord();
                        const QwtDoublePoint eC = bestBrk->endPointCoord();
                        const double midPx =
                                0.5 * (xMap.xTransform(sC.x()) + xMap.xTransform(eC.x()));
                        const double brkPy = yMap.xTransform(sC.y());
                        const double targetTopPy = qMax(2.0, brkPy - double(boxH) - 2.0);
                        tm->setOriginCoord(xMap.invTransform(midPx - 0.5 * double(boxW)),
                                           yMap.invTransform(targetTopPy));
                    }
                }
            }
            clampGraphCalloutMarkersInCanvas(gr);
        }
        if (!isInset && !isLinkedOverlay && gr->isPiePlot()) {
            Plot *pl = gr->plotWidget();
            const QwtPlotCurve *pieCv = gr->curve(0);
            double sumY = 0.0;
            double maxPieY = 1.0;
            if (pieCv) {
                for (int k = 0; k < pieCv->dataSize(); ++k) {
                    const double yk = pieCv->y(k);
                    if (std::isfinite(yk) && yk > 0.0) {
                        sumY += yk;
                        if (yk > maxPieY)
                            maxPieY = yk;
                    }
                }
            }
            gr->setScale(QwtPlot::xBottom, -2.0 * maxPieY, 3.0 * maxPieY, 0.0, 5, 0, 0, false);
            gr->setScale(QwtPlot::yLeft, -1.2 * maxPieY, 3.8 * maxPieY, 0.0, 5, 0, 0, false);
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                gr->enableAxis(a, false);
            gr->drawAxesBackbones(false);
            gr->drawCanvasFrame(false, 0);
            pl->updateLayout();

            Legend *pieLeg = gr->legend();
            const bool hasVisPieLeg = pieLeg && gr->hasLegend() && gr->isLegendVisible();
            const bool legendShowsPct = hasVisPieLeg
                    && pieLeg->text().contains(QLatin1String("\\p{"))
                    && pieLeg->text().contains(QLatin1Char('%'));
            if (legendShowsPct) {
                static const QRegularExpression kPiePctMarkerRx(
                        QStringLiteral("^\\s*\\d+(?:\\.\\d+)?%\\s*$"));
                const QVector<int> tKeys = gr->textMarkerKeys();
                for (int tid : tKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (tm && tm != pieLeg && kPiePctMarkerRx.match(tm->text()).hasMatch()) {
                        tm->setText(QString());
                        tm->setVisible(false);
                    }
                }
            }
            if (hasVisPieLeg) {
                const QVariant ancProp = pl->property("_toriLegendAnchor");
                const QRect cr0 = pl->plotLayout()->canvasRect();
                QwtText qtLeg(pieLeg->text());
                qtLeg.setFont(pieLeg->font());
                const int estW = qMax(pieLeg->rect().width(), qtLeg.textSize().width() + 24);
                const bool useTopCenter =
                        (ancProp.isValid() && ancProp.toInt() == int(Graph::LegendTopCenter))
                        || (cr0.width() >= 40 && estW > int(0.34 * double(cr0.width())));
                gr->placeLegendAnchor(useTopCenter ? Graph::LegendTopCenter
                                                   : Graph::LegendTopRight);
                pl->updateLayout();
            }

            const QRect cr = pl->plotLayout()->canvasRect();
            if (pieCv && pieCv->dataSize() > 0 && sumY > 0.0 && cr.width() >= 40
                && cr.height() >= 40 && !legendShowsPct) {
                bool hasAnyPieCallout = false;
                const QVector<int> tKeys = gr->textMarkerKeys();
                for (int tid : tKeys) {
                    Legend *tm = gr->textMarker(tid);
                    if (tm && tm != pieLeg && tm->isVisible()
                        && tm->text().trimmed().endsWith(QLatin1Char('%'))) {
                        hasAnyPieCallout = true;
                        break;
                    }
                }
                if (hasAnyPieCallout) {
                    double pieR = 0.285 * qMin(cr.width(), cr.height());
                    double pCx = double(cr.center().x());
                    double pCy = double(cr.center().y());
                    if (hasVisPieLeg && pieLeg) {
                        const QRect lr = pieLeg->rect();
                        const QVariant ancProp = pl->property("_toriLegendAnchor");
                        const bool topBand =
                                (ancProp.isValid()
                                 && ancProp.toInt() == int(Graph::LegendTopCenter))
                                || (double(lr.width()) > 0.34 * double(cr.width()));
                        if (topBand) {
                            const double legBottomFrac = qBound(
                                    0.08, double(qMax(4, lr.top()) + lr.height()) / double(cr.height()),
                                    0.52);
                            const double legBottom = legBottomFrac * double(cr.height());
                            const double pad = 0.028 * double(cr.height());
                            const double availH = qMax(0.20 * double(cr.height()),
                                                       double(cr.height()) - legBottom - 2.0 * pad);
                            pieR = qMax(10.0, qMin(0.255 * double(cr.width()), 0.34 * availH));
                            pCx = double(cr.center().x());
                            pCy = double(cr.top()) + legBottom + pad + 0.5 * availH;
                        } else {
                            pieR = 0.24 * qMin(cr.width(), cr.height());
                            pCx = double(cr.left()) + 0.36 * double(cr.width());
                            pCy = double(cr.top()) + 0.52 * double(cr.height());
                        }
                    }
                    const double lblR = 1.22 * pieR;
                    constexpr double kPi = 3.14159265358979323846;
                    double ang = 90.0;
                    int sliceOrd = 0;
                    QVector<QRect> placedBoxes;
                    if (hasVisPieLeg && pieLeg && pieLeg->rect().isValid())
                        placedBoxes.append(pieLeg->rect());
                    for (int tid : tKeys) {
                        Legend *tm = gr->textMarker(tid);
                        if (!tm || tm == pieLeg || !tm->isVisible())
                            continue;
                        if (!tm->text().trimmed().endsWith(QLatin1Char('%')))
                            continue;
                        while (sliceOrd < pieCv->dataSize()
                               && !(std::isfinite(pieCv->y(sliceOrd))
                                    && pieCv->y(sliceOrd) > 0.0)) {
                            ++sliceOrd;
                        }
                        if (sliceOrd >= pieCv->dataSize())
                            break;
                        const double pctVal = pieCv->y(sliceOrd) / sumY * 100.0;
                        const double degSpan = pctVal * 3.6;
                        const double aRad = (ang - 0.5 * degSpan) * kPi / 180.0;
                        QwtText qt(tm->text().trimmed());
                        qt.setFont(tm->font());
                        const QSize tsz = qt.textSize();
                        const int bw = tsz.width() + 8;
                        const int bh = tsz.height() + 4;
                        const int ox = qBound(2, qRound(pCx + lblR * std::cos(aRad) - 0.5 * bw),
                                              qMax(2, cr.width() - bw - 2));
                        int oy = qBound(2, qRound(pCy - lblR * std::sin(aRad) - 0.5 * bh),
                                        qMax(2, cr.height() - bh - 2));
                        QRect candBox(ox, oy, bw, bh);
                        bool collides = false;
                        for (const QRect &prev : placedBoxes) {
                            if (candBox.intersects(prev)) {
                                collides = true;
                                break;
                            }
                        }
                        if (collides) {
                            const int dirY = (std::sin(aRad) >= 0.0) ? -1 : 1;
                            for (int step = 1; step <= 4 && collides; ++step) {
                                const int tryY = qBound(2, oy + dirY * step * (bh - 2),
                                                        qMax(2, cr.height() - bh - 2));
                                const QRect tryBox(ox, tryY, bw, bh);
                                bool hitAny = false;
                                for (const QRect &prev : placedBoxes) {
                                    if (tryBox.intersects(prev)) {
                                        hitAny = true;
                                        break;
                                    }
                                }
                                if (!hitAny) {
                                    oy = tryY;
                                    candBox = tryBox;
                                    collides = false;
                                }
                            }
                        }
                        if (collides && pctVal < 6.0) {
                            tm->setText(QString());
                            tm->setVisible(false);
                        } else {
                            tm->setOrigin(QPoint(ox, oy));
                            placedBoxes.append(candBox);
                        }
                        ang -= degSpan;
                        ++sliceOrd;
                    }
                }
            }
        }
        if (!isInset && !isLinkedOverlay && !gr->isPiePlot() && gr->hasLegend()
            && gr->isLegendVisible()) {
            Plot *plLeg = gr->plotWidget();
            plLeg->updateLayout();
            const QRect crLeg = plLeg->plotLayout()->canvasRect();
            Legend *legPtr = gr->legend();
            if (legPtr && !gr->isTernaryPlot() && legPtr->numColumns() <= 1
                && plLeg->property("_outsideRightMargin").toInt() <= 0 && crLeg.width() >= 60) {
                const int maxAllowedW = crLeg.width() - 10;
                QFont lf = legPtr->font();
                double curPt = lf.pointSizeF();
                if (curPt <= 0.0 && lf.pointSize() > 0)
                    curPt = double(lf.pointSize());
                const double floorPt =
                        (isSlidePreset || curPt > 14.0) ? 16.0 : 7.0;
                while (legPtr->rect().width() > maxAllowedW && curPt > floorPt + 0.1) {
                    curPt = qMax(floorPt, curPt - ((curPt > 14.0) ? 1.0 : 0.5));
                    lf.setPointSizeF(curPt);
                    lf.setHintingPreference(QFont::PreferNoHinting);
                    legPtr->setFont(lf);
                }
            }
            const QVariant ancProp = gr->plotWidget()->property("_toriLegendAnchor");
            if (ancProp.isValid()) {
                gr->placeLegendAnchor(static_cast<Graph::LegendAnchor>(ancProp.toInt()));
            } else if (gr->isTernaryPlot() || gr->isPolarPlot() || gr->isPoleFigurePlot()) {
                gr->placeLegendAnchor(Graph::LegendTopRight);
            } else if (legendWasDefaultTopLeft[i] || isSlidePreset || !gr->lineMarkerKeys().isEmpty()) {
                gr->placeLegendAnchor(Graph::LegendAutoAvoidData);
            }
            if (legPtr && !gr->isTernaryPlot()
                && plLeg->property("_outsideRightMargin").toInt() <= 0 && crLeg.width() >= 60) {
                const QRect finalLr = legPtr->rect();
                if (finalLr.right() > crLeg.width() - 4) {
                    legPtr->setOrigin(QPoint(qMax(2, crLeg.width() - finalLr.width() - 4),
                                             qMax(2, finalLr.top())));
                }
            }

            if (gr->isPolarPlot() && gr->legend() && gr->plotWidget()->canvas()) {
                Plot *pl = gr->plotWidget();
                pl->updateLayout();
                const QwtScaleMap yMap = pl->canvasMap(QwtPlot::yLeft);
                const double cy = yMap.xTransform(0.0);
                const double rMax = qMax(1e-6, gr->polarRadiusMax());
                const double ryOuter = std::fabs(yMap.xTransform(rMax) - cy);
                const double lblH = QFontMetricsF(gr->axisFont(QwtPlot::xBottom)).height();
                const double topOf90Px = cy - (ryOuter + 5.5 + 0.95 * lblH);
                const double legBottomPx = double(gr->legend()->rect().bottom() + 4);
                if (ryOuter > 8.0 && legBottomPx > topOf90Px) {
                    const double curYLo = -1.44 * rMax;
                    const double chPx = double(qMax(60, pl->canvas()->height()));
                    const double reqTopFrac =
                            qBound(0.18, (legBottomPx + 6.5 + lblH) / chPx, 0.58);
                    const double newYHi =
                            qMax(1.64 * rMax, (rMax - reqTopFrac * curYLo) / (1.0 - reqTopFrac));
                    const double halfX = 0.5 * (newYHi - curYLo);
                    // Kept by updatePlot() and saved with the project (<ToriPolarView>).
                    gr->setPolarViewRange(-halfX, halfX, curYLo, newYHi);
                    gr->placeLegendAnchor(Graph::LegendTopRight);
                }
            }
        }
        gr->plotWidget()->updateLayout();
        gr->plotWidget()->replot();
    }
    syncChildOverlayAndInsetGeometries();
    double initialViewZoom = desiredUpperZoom;
    {
        ApplicationWindow *mApp = findMultiLayerApp(this);
        QMdiArea *mdi = mdiArea();
        const bool splitMode = mApp && mApp->modernRibbonBar()
                && mApp->modernRibbonBar()->workspaceLayoutMode()
                        == ModernRibbonBar::LayoutSplitTableGraph;
        const bool viewportReady = isVisible()
                || (mdi && mdi->viewport() && mdi->viewport()->isVisible())
                || splitMode;
        if (viewportReady)
            initialViewZoom = qBound(0.35, std::min(desiredUpperZoom, fitPageZoomFactor()), 1.6);
    }
    if (std::fabs(initialViewZoom - 1.0) > 1e-4) {
        const bool prevInternalZoom = d_internal_zoom_change;
        d_internal_zoom_change = true;
        setPageZoom(initialViewZoom);
        d_internal_zoom_change = prevInternalZoom;
    }
    d_user_zoomed = false;
    emit modifiedPlot();
    if (!suppressUndo && !beforeSnap.isEmpty())
        pushUndoSnapshot(undoLabel, beforeSnap);
}

// ============================================================================
// User-Customizable Graph Style Presets (JSON Persistence + Manager Dialog)
// ============================================================================

namespace {

void registerStylePresetTranslationsOnce()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(
                QString::fromUtf8(en), QString::fromUtf8(ja));
    };
    reg("Manage Style Presets...", "グラフプリセットの編集・追加...");
    reg("Manage Presets...", "プリセット管理...");
    reg("Presets...", "プリセット...");
    reg("Graph Style Preset Manager", "グラフスタイルプリセット管理");
    reg("Apply Preset: %1", "プリセットを適用: %1");
    reg("Built-in", "組み込み");
    reg("Custom", "ユーザー");
    reg("New", "新規追加");
    reg("From Current Graph", "現在のグラフから作成");
    reg("Duplicate", "複製");
    reg("Delete", "削除");
    reg("Reset Default", "初期値に戻す");
    reg("Import JSON...", "JSON 読込...");
    reg("Export JSON...", "JSON 書出...");
    reg("Preset Name:", "プリセット名:");
    reg("Page Width (mm):", "ページ幅 (mm):");
    reg("Page Height (mm):", "ページ高さ (mm):");
    reg("Lock Aspect Ratio", "縦横比を固定");
    reg("Target DPI:", "出力解像度 (DPI):");
    reg("Margin (px):", "余白 (px):");
    reg("Frame & Ticks:", "枠と目盛り:");
    reg("Box + Inward Ticks (ISIJ)", "四辺枠 + 内向き目盛り (ISIJ)");
    reg("Box + Plain Opposite Axes", "四辺枠 (上・右目盛りなし)");
    reg("Open L + Outward Ticks (Nature)", "L字枠 + 外向き目盛り (Nature)");
    reg("Zero-Crossed Axes", "原点交差軸 (0基準)");
    reg("Axis Line Width (px):", "軸線の太さ (px):");
    reg("Minor Tick Length (px):", "副目盛りの長さ (px):");
    reg("Major Tick Length (px):", "主目盛りの長さ (px):");
    reg("Font Family:", "フォント:");
    reg("Axis Title Size (pt):", "軸タイトルサイズ (pt):");
    reg("Tick Label Size (pt):", "目盛り数字サイズ (pt):");
    reg("Legend Size (pt):", "凡例フォントサイズ (pt):");
    reg("Bold", "太字");
    reg("Min Curve Width (px):", "最小曲線幅 (px):");
    reg("Min Symbol Size (px):", "最小シンボルサイズ (px):");
    reg("Legend Frame:", "凡例枠:");
    reg("None (Transparent)", "なし (透過)");
    reg("Line Box", "実線枠");
    reg("Shadow Box", "影付き枠");
    reg("Format axis units as ISIJ slash (Quantity, sym / unit)",
        "軸単位を ISIJ スラッシュ形式 (量, 記号 / 単位) に整形");
    reg("Use large presentation-slide layout rules",
        "スライド向け大フォント・余白レイアウトを使用");
    reg("Save Changes", "変更を保存");
    reg("Save && Apply to Graph", "保存してグラフに適用");
    reg("Close", "閉じる");
    reg("Saved presets to %1", "プリセットを %1 に保存しました");
    reg("Imported %1 preset(s).", "%1 件のプリセットを読み込みました。");
    reg("New Custom Preset", "カスタムプリセット");
    reg("Copy of %1", "%1 のコピー");
    reg("Cannot delete a built-in preset. Use 'Reset Default' to restore its original settings.",
        "組み込みプリセットは削除できません。元に戻すには「初期値に戻す」を使用してください。");
    reg("Custom Preset:", "プリセット:");
    reg("Apply Selected Preset", "選択したプリセットを適用");
    reg("Curve Width (px):", "曲線の太さ (px):");
    reg("Symbol Size (px):", "シンボルサイズ (px):");
    reg("Defaults for New Graphs", "新規グラフの既定");
    reg("Preset applied to new graphs:", "新規グラフに適用するプリセット:");
    reg("Colors for new curves:", "新しい曲線の色:");
    reg("Origin Classic (black, red, blue, ...)", "Origin クラシック (黒・赤・青…)");
    Graph::registerPaletteTranslations();
    reg("Okabe-Ito (Colorblind Safe)", "岡部・伊藤 (色覚バリアフリー)");
    reg("Matplotlib (tab10)", "Matplotlib 既定色 (tab10)");
    reg("Multi-panel page size:", "複数パネルのページ寸法:");
    reg("Preset size per panel (columns x width)", "パネルごとにプリセット寸法 (列数 × 幅)");
    reg("Journal double column (175 mm wide)", "論文 2 段幅 (幅 175 mm)");
    reg("Shared preset folder (read-only):", "共有プリセットフォルダ (読み取り専用):");
    reg("(none)", "(なし)");
    reg("Browse...", "参照...");
    reg("None (keep plain pixel page)", "なし (ピクセル単位のページのまま)");
    reg("Shared", "共有");
    reg("Apply to Graph", "グラフに適用");
    reg("This preset comes from the shared folder and is read-only. Duplicate it to make a personal copy.",
        "このプリセットは共有フォルダのもので読み取り専用です。複製すると自分用のコピーを作れます。");
    reg("%1 was written by a newer ToriGraph (preset format %2; this version reads format %3). Unknown settings are ignored.",
        "%1 は新しい版の ToriGraph で書かれています (プリセット形式 %2、この版が読めるのは形式 %3 まで)。分からない設定は無視しました。");
    reg("Axis title \"%1\" is wider than the axis at %2 pt (size kept; shorten the title or enlarge the page).",
        "軸タイトル「%1」は %2 pt では軸より長くなります (文字サイズは変えていません。タイトルを短くするか、ページを大きくしてください)。");
    reg("Lab Slide (18 pt on slide, 83.3 mm, paste at 140%)",
        "研究室スライド (スライド上で 18 pt、83.3 mm 角、PowerPoint で 140% に拡大)");
    reg("Journal 2-Column (175x85 mm, 600 DPI)", "論文 2 段幅 (175×85 mm, 600 DPI)");
    reg("ISIJ 1-Column (85x85 mm, 600 DPI)", "ISIJ 1段幅 (85×85 mm, 600 DPI)");
    reg("Presentation Slide (28 pt Arial)", "発表スライド (28 pt Arial)");
    reg("Large Slide (28 pt, 185 mm)", "大判スライド (28 pt, 185 mm)");
    reg("Lab Slide (18 pt, 83.3 mm)", "研究室スライド (18 pt, 83.3 mm)");
    reg("Journal 2-Column (175 mm)", "論文 2 段幅 (175 mm)");
    reg("More / Legacy Presets", "その他 / 旧プリセット");
    reg("Show Zoom Connectors", "ズーム接続線を表示");
    reg("Toggle Zoom Connectors", "ズーム接続線の表示切替");
    reg("Move Inset Layer", "インセットレイヤーを移動");
    reg("Failed to render EMF graphics.", "EMF グラフィックスの描画に失敗しました。");
    reg("Could not open file for writing: %1", "書き込み用にファイルを開けませんでした: %1");
    reg("Failed to write complete EMF file.", "EMF ファイルの書き込みに失敗しました。");
    reg("Image size is too large to allocate for export.",
        "画像サイズが大きすぎるためエクスポートできません。");
    reg("Failed to initialize image painter.", "画像ペインターの初期化に失敗しました。");
    reg("Could not copy image to clipboard.", "画像をクリップボードにコピーできませんでした。");
    reg("Failed to write image file: %1", "画像ファイルの書き込みに失敗しました: %1");
    reg("EPS/PostScript export is not supported in Qt5 builds; export as PDF or SVG instead.",
        "Qt5 ビルドでは EPS/PostScript エクスポートは未対応です。PDF または SVG で出力してください。");
    reg("Could not open vector output file for writing: %1",
        "ベクター出力ファイルを開けませんでした: %1");
    reg("Failed to write vector output file: %1",
        "ベクター出力ファイルの書き込みに失敗しました: %1");
    reg("Could not open SVG file for writing: %1", "SVG ファイルを開けませんでした: %1");
    reg("Failed to initialize SVG painter.", "SVG ペインターの初期化に失敗しました。");
    reg("Failed to write SVG file: %1", "SVG ファイルの書き込みに失敗しました: %1");
    reg("Failed to update SVG dimensions in %1", "%1 の SVG 寸法を更新できませんでした");
    reg("SVG export is not supported in this build.",
        "このビルドでは SVG エクスポートに対応していません。");
    reg("PowerPoint Paste Scale (%):", "PowerPoint 貼付倍率 (%):");
    reg("OK", "OK");
    reg("Cancel", "キャンセル");
}

QJsonObject stylePresetToJson(const MultiLayer::GraphStylePresetSpec &s)
{
    QJsonObject o;
    o[QStringLiteral("id")] = s.id;
    o[QStringLiteral("name")] = s.name;
    o[QStringLiteral("built_in")] = s.builtIn;
    o[QStringLiteral("page_width_mm")] = s.pageWidthMm;
    o[QStringLiteral("page_height_mm")] = s.pageHeightMm;
    o[QStringLiteral("keep_aspect_ratio")] = s.keepAspectRatio;
    o[QStringLiteral("target_dpi")] = s.targetDpi;
    o[QStringLiteral("margin_px")] = s.marginPx;
    o[QStringLiteral("frame_preset")] = s.framePreset;
    o[QStringLiteral("axis_line_width")] = s.axisLineWidth;
    o[QStringLiteral("minor_tick_len")] = s.minorTickLen;
    o[QStringLiteral("major_tick_len")] = s.majorTickLen;
    o[QStringLiteral("font_family")] = s.fontFamily;
    o[QStringLiteral("axis_title_pt")] = s.axisTitlePt;
    o[QStringLiteral("tick_label_pt")] = s.tickLabelPt;
    o[QStringLiteral("legend_pt")] = s.legendPt;
    o[QStringLiteral("bold_title")] = s.boldTitle;
    o[QStringLiteral("bold_tick")] = s.boldTick;
    o[QStringLiteral("bold_legend")] = s.boldLegend;
    const double effCw = (s.curveWidthF > 0.0) ? s.curveWidthF : double(s.minCurveWidth);
    o[QStringLiteral("curve_width")] = effCw;
    o[QStringLiteral("symbol_size")] = s.minSymbolSize;
    // legacy keys (read by older ToriGraph builds)
    o[QStringLiteral("min_curve_width")] = qBound(1, qRound(effCw), 20);
    o[QStringLiteral("min_symbol_size")] = s.minSymbolSize;
    o[QStringLiteral("legend_frame")] = s.legendFrame;
    o[QStringLiteral("page_background")] =
            (s.pageBackground.isValid() ? s.pageBackground : QColor(Qt::white)).name();
    o[QStringLiteral("format_isij_unit_slash")] = s.formatIsijUnitSlash;
    o[QStringLiteral("large_slide_layout")] = s.largeSlideLayout;
    if (s.slidePastePercent > 0.0)
        o[QStringLiteral("slide_paste_percent")] = s.slidePastePercent;
    return o;
}

MultiLayer::GraphStylePresetSpec stylePresetFromJson(const QJsonObject &o,
                                                     const MultiLayer::GraphStylePresetSpec &fallback)
{
    MultiLayer::GraphStylePresetSpec s = fallback;
    if (o.contains(QStringLiteral("id")))
        s.id = o.value(QStringLiteral("id")).toString(s.id).trimmed();
    if (o.contains(QStringLiteral("name")))
        s.name = o.value(QStringLiteral("name")).toString(s.name).trimmed();
    if (o.contains(QStringLiteral("page_width_mm")))
        s.pageWidthMm = qBound(20.0, o.value(QStringLiteral("page_width_mm")).toDouble(s.pageWidthMm), 1000.0);
    if (o.contains(QStringLiteral("page_height_mm")))
        s.pageHeightMm = qBound(20.0, o.value(QStringLiteral("page_height_mm")).toDouble(s.pageHeightMm), 1000.0);
    if (o.contains(QStringLiteral("keep_aspect_ratio")))
        s.keepAspectRatio = o.value(QStringLiteral("keep_aspect_ratio")).toBool(s.keepAspectRatio);
    if (o.contains(QStringLiteral("target_dpi")))
        s.targetDpi = qBound(72, o.value(QStringLiteral("target_dpi")).toInt(s.targetDpi), 2400);
    if (o.contains(QStringLiteral("margin_px")))
        s.marginPx = qBound(0, o.value(QStringLiteral("margin_px")).toInt(s.marginPx), 80);
    if (o.contains(QStringLiteral("frame_preset")))
        s.framePreset = qBound(0, o.value(QStringLiteral("frame_preset")).toInt(s.framePreset), 3);
    if (o.contains(QStringLiteral("axis_line_width")))
        s.axisLineWidth = qBound(1, o.value(QStringLiteral("axis_line_width")).toInt(s.axisLineWidth), 10);
    if (o.contains(QStringLiteral("minor_tick_len")))
        s.minorTickLen = qBound(0, o.value(QStringLiteral("minor_tick_len")).toInt(s.minorTickLen), 20);
    if (o.contains(QStringLiteral("major_tick_len")))
        s.majorTickLen = qBound(0, o.value(QStringLiteral("major_tick_len")).toInt(s.majorTickLen), 30);
    if (o.contains(QStringLiteral("font_family"))) {
        const QString ff = o.value(QStringLiteral("font_family")).toString().trimmed();
        if (!ff.isEmpty())
            s.fontFamily = ff;
    }
    if (o.contains(QStringLiteral("axis_title_pt")))
        s.axisTitlePt = qBound(5.0, o.value(QStringLiteral("axis_title_pt")).toDouble(s.axisTitlePt), 72.0);
    if (o.contains(QStringLiteral("tick_label_pt")))
        s.tickLabelPt = qBound(5.0, o.value(QStringLiteral("tick_label_pt")).toDouble(s.tickLabelPt), 72.0);
    if (o.contains(QStringLiteral("legend_pt")))
        s.legendPt = qBound(5.0, o.value(QStringLiteral("legend_pt")).toDouble(s.legendPt), 72.0);
    if (o.contains(QStringLiteral("bold_title")))
        s.boldTitle = o.value(QStringLiteral("bold_title")).toBool(s.boldTitle);
    if (o.contains(QStringLiteral("bold_tick")))
        s.boldTick = o.value(QStringLiteral("bold_tick")).toBool(s.boldTick);
    if (o.contains(QStringLiteral("bold_legend")))
        s.boldLegend = o.value(QStringLiteral("bold_legend")).toBool(s.boldLegend);
    if (o.contains(QStringLiteral("curve_width"))) {
        const double defCw = (s.curveWidthF > 0.0) ? s.curveWidthF : double(s.minCurveWidth);
        const double cw = qBound(0.5, o.value(QStringLiteral("curve_width")).toDouble(defCw), 20.0);
        s.curveWidthF = cw;
        s.minCurveWidth = qBound(1, qRound(cw), 20);
    } else if (o.contains(QStringLiteral("min_curve_width"))) {
        s.minCurveWidth = qBound(1, o.value(QStringLiteral("min_curve_width")).toInt(s.minCurveWidth), 20);
        s.curveWidthF = double(s.minCurveWidth);
    }
    if (o.contains(QStringLiteral("symbol_size")))
        s.minSymbolSize = qBound(0, o.value(QStringLiteral("symbol_size")).toInt(s.minSymbolSize), 100);
    else if (o.contains(QStringLiteral("min_symbol_size")))
        s.minSymbolSize = qBound(0, o.value(QStringLiteral("min_symbol_size")).toInt(s.minSymbolSize), 100);
    if (o.contains(QStringLiteral("legend_frame")))
        s.legendFrame = qBound(0, o.value(QStringLiteral("legend_frame")).toInt(s.legendFrame), 2);
    if (o.contains(QStringLiteral("page_background"))) {
        QColor c(o.value(QStringLiteral("page_background")).toString());
        if (c.isValid())
            s.pageBackground = c;
    }
    if (o.contains(QStringLiteral("format_isij_unit_slash")))
        s.formatIsijUnitSlash = o.value(QStringLiteral("format_isij_unit_slash")).toBool(s.formatIsijUnitSlash);
    if (o.contains(QStringLiteral("large_slide_layout")))
        s.largeSlideLayout = o.value(QStringLiteral("large_slide_layout")).toBool(s.largeSlideLayout);
    if (o.contains(QStringLiteral("slide_paste_percent")))
        s.slidePastePercent = qBound(10.0, o.value(QStringLiteral("slide_paste_percent")).toDouble(s.slidePastePercent), 400.0);
    return s;
}

bool writeStylePresetsToJsonFile(const QString &path,
                                 const QVector<MultiLayer::GraphStylePresetSpec> &entries,
                                 QString *errOut)
{
    QJsonArray arr;
    for (const auto &s : entries)
        arr.append(stylePresetToJson(s));
    QJsonObject root;
    root[QStringLiteral("version")] = MultiLayer::stylePresetFormatVersion();
    root[QStringLiteral("presets")] = arr;
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut)
            *errOut = QObject::tr("Could not write file: %1").arg(path);
        return false;
    }
    const qint64 written = f.write(payload);
    const bool flushed = f.flush();
    const bool ok = (written == payload.size()) && flushed && (f.error() == QFile::NoError);
    if (!ok && errOut)
        *errOut = QObject::tr("Could not write file: %1").arg(path);
    f.close();
    return ok;
}

bool writeUserStylePresetsArray(const QVector<MultiLayer::GraphStylePresetSpec> &userEntries,
                                QString *errOut)
{
    const QString path = MultiLayer::userStylePresetsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    return writeStylePresetsToJsonFile(path, userEntries, errOut);
}

//! Reads the "version" field of a preset document. Files without one are version 1.
int presetDocumentVersion(const QJsonDocument &doc)
{
    if (!doc.isObject())
        return 1;
    const QJsonValue v = doc.object().value(QStringLiteral("version"));
    return v.isDouble() ? qMax(1, v.toInt(1)) : 1;
}

bool parseStylePresetsFromJsonFile(const QString &path, bool readOnly, bool skipEmptyId,
                                   QVector<MultiLayer::GraphStylePresetSpec> *out,
                                   QString *errOut, QString *warnOut = nullptr)
{
    if (!out)
        return false;
    out->clear();
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        if (errOut)
            *errOut = QObject::tr("Could not open file: %1").arg(path);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QJsonArray arr;
    if (doc.isObject() && doc.object().contains(QStringLiteral("presets")))
        arr = doc.object().value(QStringLiteral("presets")).toArray();
    else if (doc.isArray())
        arr = doc.array();
    else if (doc.isObject())
        arr.append(doc.object());
    else {
        if (errOut)
            *errOut = QObject::tr("Invalid JSON preset file.");
        return false;
    }
    const int ver = presetDocumentVersion(doc);
    if (ver > MultiLayer::stylePresetFormatVersion() && warnOut) {
        *warnOut = QObject::tr("%1 was written by a newer ToriGraph (preset format %2; this "
                               "version reads format %3). Unknown settings are ignored.")
                           .arg(QFileInfo(path).fileName())
                           .arg(ver)
                           .arg(MultiLayer::stylePresetFormatVersion());
    }
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        MultiLayer::GraphStylePresetSpec base;
        base.builtIn = false;
        MultiLayer::GraphStylePresetSpec parsed = stylePresetFromJson(v.toObject(), base);
        if (skipEmptyId && parsed.id.isEmpty())
            continue;
        parsed.readOnly = readOnly;
        parsed.sourcePath = readOnly ? path : QString();
        out->append(parsed);
    }
    return true;
}

QVector<MultiLayer::GraphStylePresetSpec> readSharedPresetEntriesFromDir(const QString &folder)
{
    QVector<MultiLayer::GraphStylePresetSpec> out;
    if (folder.trimmed().isEmpty())
        return out;
    QDir dir(folder);
    if (!dir.exists())
        return out;
    const QStringList files =
            dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Name);
    for (const QString &fn : files) {
        QVector<MultiLayer::GraphStylePresetSpec> filePresets;
        if (parseStylePresetsFromJsonFile(dir.filePath(fn), true, true, &filePresets, nullptr))
            out += filePresets;
    }
    return out;
}

QVector<MultiLayer::GraphStylePresetSpec> readSharedPresetEntries()
{
    return readSharedPresetEntriesFromDir(MultiLayer::sharedStylePresetsFolder());
}

void appendMergedSharedPresets(QVector<MultiLayer::GraphStylePresetSpec> &result,
                               const QVector<MultiLayer::GraphStylePresetSpec> &sharedEntries)
{
    for (const MultiLayer::GraphStylePresetSpec &se : sharedEntries) {
        bool clash = false;
        for (const MultiLayer::GraphStylePresetSpec &r : result) {
            if (r.id.compare(se.id, Qt::CaseInsensitive) == 0) {
                clash = true;
                break;
            }
        }
        if (clash)
            continue;
        MultiLayer::GraphStylePresetSpec shared = se;
        shared.builtIn = false;
        shared.readOnly = true;
        if (shared.name.trimmed().isEmpty())
            shared.name = shared.id;
        result.append(shared);
    }
}

QVector<MultiLayer::GraphStylePresetSpec> readRawUserPresetEntries()
{
    QVector<MultiLayer::GraphStylePresetSpec> out;
    parseStylePresetsFromJsonFile(MultiLayer::userStylePresetsFilePath(), false, true, &out,
                                  nullptr);
    return out;
}

QString canonicalBuiltInAlias(const QString &raw)
{
    const QString k = raw.trimmed().toLower();
    if (k == QLatin1String("isij") || k == QLatin1String("isij_85mm")
        || k == QLatin1String("isij_journal") || k == QLatin1String("isij85"))
        return QStringLiteral("isij_85mm");
    if (k == QLatin1String("slide") || k == QLatin1String("slide_28pt")
        || k == QLatin1String("large_slide")
        || k == QLatin1String("presentation") || k == QLatin1String("presentation_slide"))
        return QStringLiteral("slide_28pt");
    if (k == QLatin1String("lab_slide") || k == QLatin1String("lab_slide_83mm")
        || k == QLatin1String("slide_140"))
        return QStringLiteral("lab_slide_83mm");
    if (k == QLatin1String("2col") || k == QLatin1String("two_column")
        || k == QLatin1String("journal_2col_175mm") || k == QLatin1String("175mm"))
        return QStringLiteral("journal_2col_175mm");
    return raw.trimmed();
}

void normalizePresetSpecForPersonalSave(MultiLayer::GraphStylePresetSpec &spec,
                                        const QVector<MultiLayer::GraphStylePresetSpec> &builtIns)
{
    spec.id = canonicalBuiltInAlias(spec.id);
    if (spec.id.isEmpty()) {
        spec.id = QStringLiteral("custom_")
                + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    }
    if (spec.name.trimmed().isEmpty())
        spec.name = spec.id;
    spec.readOnly = false;
    spec.sourcePath.clear();
    spec.builtIn = false;
    for (const MultiLayer::GraphStylePresetSpec &d : builtIns) {
        if (d.id.compare(spec.id, Qt::CaseInsensitive) == 0) {
            spec.id = d.id;
            spec.builtIn = true;
            break;
        }
    }
}

} // namespace

QString MultiLayer::newGraphStylePresetId()
{
    QSettings &s = ApplicationWindow::getSettings();
    const QVariant v = s.value(QStringLiteral("/StylePresets/NewGraphPreset"));
    if (!v.isValid())
        return QStringLiteral("isij_85mm");
    const QString id = v.toString().trimmed();
    if (id.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
        return QString();
    return id;
}

void MultiLayer::setNewGraphStylePresetId(const QString &id)
{
    QSettings &s = ApplicationWindow::getSettings();
    const QString v = id.trimmed().isEmpty() ? QStringLiteral("none") : id.trimmed();
    s.setValue(QStringLiteral("/StylePresets/NewGraphPreset"), v);
}

QString MultiLayer::sharedStylePresetsFolder()
{
    QSettings &s = ApplicationWindow::getSettings();
    return s.value(QStringLiteral("/StylePresets/SharedFolder"), QString()).toString().trimmed();
}

void MultiLayer::setSharedStylePresetsFolder(const QString &folder)
{
    QSettings &s = ApplicationWindow::getSettings();
    s.setValue(QStringLiteral("/StylePresets/SharedFolder"), folder.trimmed());
}

MultiLayer::MultiPanelSizing MultiLayer::multiPanelSizing()
{
    QSettings &s = ApplicationWindow::getSettings();
    const int v = s.value(QStringLiteral("/StylePresets/MultiPanelSizing"),
                          int(PanelSizingPerPanel))
                          .toInt();
    return (v == int(PanelSizingTwoColumn175)) ? PanelSizingTwoColumn175 : PanelSizingPerPanel;
}

void MultiLayer::setMultiPanelSizing(MultiPanelSizing mode)
{
    QSettings &s = ApplicationWindow::getSettings();
    s.setValue(QStringLiteral("/StylePresets/MultiPanelSizing"), int(mode));
}

void MultiLayer::requestNewGraphStylePreset(Graph *g)
{
    if (!g)
        return;
    MultiLayer *ml = g->multiLayerParent();
    if (!ml || ml->property("_toriNewGraphPresetDone").toBool()
        || ml->property("_toriNewGraphPresetPending").toBool())
        return;
    if (newGraphStylePresetId().isEmpty())
        return;
    // The preset is a convenience for graphs made by hand. Graphs made by scripts, the Python
    // SDK (RPC), headless/batch runs or OLE automation keep the plain defaults, so their output
    // does not depend on a GUI setting; SDK users pick a preset with apply_style_preset().
    if (ApplicationWindow::isNonInteractiveContext(ml))
        return;
    ml->setProperty("_toriNewGraphPresetPending", true);
    // Applied after the creating function has finished (legend, layout and window geometry are
    // in place by then), exactly as if the user had chosen the preset.
    QPointer<MultiLayer> guard(ml);
    QTimer::singleShot(0, ml, [guard]() {
        if (guard)
            guard->applyPendingNewGraphStylePreset();
    });
}

bool MultiLayer::applyPendingNewGraphStylePreset()
{
    if (!property("_toriNewGraphPresetPending").toBool())
        return false;
    setProperty("_toriNewGraphPresetPending", false);
    setProperty("_toriNewGraphPresetDone", true);
    const QString id = newGraphStylePresetId();
    if (id.isEmpty())
        return false;
    bool anyCartesian = false;
    for (int i = 0; i < graphsList.count(); ++i) {
        Graph *gr = qobject_cast<Graph *>(graphsList.at(i));
        if (gr && !gr->isPiePlot() && !gr->isTernaryPlot() && !gr->isPolarPlot()
            && !gr->isPoleFigurePlot())
            anyCartesian = true;
    }
    if (!anyCartesian)
        return false;
    GraphStylePresetSpec spec;
    if (!findStylePreset(id, &spec))
        return false;
    const int undoBefore = d_undo_stack ? d_undo_stack->count() : -1;
    // A window size/position remembered from the last session (restored just before this
    // runs) wins over the preset's automatic window fit; the preset only sets the page.
    const bool keepWindowGeometry =
            (property("_tg_has_saved_window_geo").toBool()
             || ApplicationWindow::getSettings()
                        .value(QStringLiteral("/2DPlots/LastGraphWindow/HasGeometry"), false)
                        .toBool())
            && !isMaximized();
    const QRect geoBefore = geometry();
    const bool prevUserZoomed = d_user_zoomed;
    applyStylePresetSpec(spec);
    if (prevUserZoomed)
        d_user_zoomed = true;
    if (keepWindowGeometry && geoBefore.width() >= 100 && geoBefore.height() >= 100
        && geometry() != geoBefore)
        setGeometry(geoBefore);
    // Part of creating the graph, not an edit the user should undo step by step.
    if (d_undo_stack && undoBefore == 0)
        d_undo_stack->clear();
    return true;
}

QString MultiLayer::userStylePresetsFilePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.torigraph");
    return QDir(base).filePath(QStringLiteral("graph_style_presets.json"));
}

QVector<MultiLayer::GraphStylePresetSpec> MultiLayer::defaultBuiltInStylePresets()
{
    QVector<GraphStylePresetSpec> defs;
    defs.reserve(4);

    GraphStylePresetSpec isij;
    isij.id = QStringLiteral("isij_85mm");
    isij.name = QStringLiteral("ISIJ 1-Column (85x85 mm, 600 DPI)");
    isij.builtIn = true;
    isij.pageWidthMm = 85.0;
    isij.pageHeightMm = 85.0;
    isij.keepAspectRatio = true;
    isij.targetDpi = 600;
    isij.marginPx = 6;
    isij.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    isij.axisLineWidth = 2;
    isij.minorTickLen = 4;
    isij.majorTickLen = 8;
    isij.fontFamily = QStringLiteral("Arial");
    isij.axisTitlePt = 11.0;
    isij.tickLabelPt = 10.0;
    isij.legendPt = 9.0;
    isij.minCurveWidth = 2;
    isij.minSymbolSize = 7;
    isij.legendFrame = Legend::None;
    isij.pageBackground = Qt::white;
    isij.formatIsijUnitSlash = true;
    isij.largeSlideLayout = false;
    defs.append(isij);

    GraphStylePresetSpec slide;
    slide.id = QStringLiteral("slide_28pt");
    slide.name = QStringLiteral("Large Slide (28 pt, 185 mm)");
    slide.builtIn = true;
    slide.pageWidthMm = 185.0;
    slide.pageHeightMm = 185.0;
    slide.keepAspectRatio = true;
    slide.targetDpi = 300;
    slide.marginPx = 10;
    slide.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    slide.axisLineWidth = 3;
    slide.minorTickLen = 6;
    slide.majorTickLen = 12;
    slide.fontFamily = QStringLiteral("Arial");
    slide.axisTitlePt = 28.0;
    slide.tickLabelPt = 28.0;
    slide.legendPt = 24.0;
    slide.minCurveWidth = 3;
    slide.minSymbolSize = 12;
    slide.legendFrame = Legend::None;
    slide.pageBackground = Qt::white;
    slide.formatIsijUnitSlash = false;
    slide.largeSlideLayout = true;
    defs.append(slide);

    // E3: lab slide preset. The lab pastes slide graphs as an 83.3 mm square page (the size of
    // an 8.2 in Origin page copied at 40 %) and enlarges it to 140 % in PowerPoint (11.7 cm
    // square). ToriGraph pastes at the physical page size, so the text is sized for how it should
    // read on the slide after that enlargement: axis titles and tick labels 18 pt / 1.40 = 12.9 pt
    // (18 pt on the slide), legend 11.5 pt (16 pt on the slide; same 12.9 / 11.2 ratio over the
    // former 10 pt). The former 11.2 pt (= Origin 28 pt x 0.40) only read as 15.7 pt on a slide.
    GraphStylePresetSpec lab;
    lab.id = QStringLiteral("lab_slide_83mm");
    lab.name = QStringLiteral("Lab Slide (18 pt on slide, 83.3 mm, paste at 140%)");
    lab.builtIn = true;
    lab.pageWidthMm = 83.3;
    lab.pageHeightMm = 83.3;
    lab.keepAspectRatio = true;
    lab.targetDpi = 600;
    lab.marginPx = 6;
    lab.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    lab.axisLineWidth = 2;
    lab.minorTickLen = 4;
    lab.majorTickLen = 8;
    lab.fontFamily = QStringLiteral("Arial");
    lab.axisTitlePt = 12.9;
    lab.tickLabelPt = 12.9;
    lab.legendPt = 11.5;
    lab.minCurveWidth = 2;
    lab.minSymbolSize = 7;
    lab.legendFrame = Legend::None;
    lab.pageBackground = Qt::white;
    lab.formatIsijUnitSlash = false;
    lab.largeSlideLayout = false;
    lab.slidePastePercent = 140.0; // E8: "Send to PowerPoint" pastes it at this size
    defs.append(lab);

    // E3: journal double-column width (175 mm).
    GraphStylePresetSpec twoCol;
    twoCol.id = QStringLiteral("journal_2col_175mm");
    twoCol.name = QStringLiteral("Journal 2-Column (175x85 mm, 600 DPI)");
    twoCol.builtIn = true;
    twoCol.pageWidthMm = 175.0;
    twoCol.pageHeightMm = 85.0;
    twoCol.keepAspectRatio = true;
    twoCol.targetDpi = 600;
    twoCol.marginPx = 6;
    twoCol.framePreset = static_cast<int>(Graph::FrameBoxInwardTicks);
    twoCol.axisLineWidth = 2;
    twoCol.minorTickLen = 4;
    twoCol.majorTickLen = 8;
    twoCol.fontFamily = QStringLiteral("Arial");
    twoCol.axisTitlePt = 11.0;
    twoCol.tickLabelPt = 10.0;
    twoCol.legendPt = 9.0;
    twoCol.minCurveWidth = 2;
    twoCol.minSymbolSize = 7;
    twoCol.legendFrame = Legend::None;
    twoCol.pageBackground = Qt::white;
    twoCol.formatIsijUnitSlash = true;
    twoCol.largeSlideLayout = false;
    defs.append(twoCol);

    return defs;
}

QVector<MultiLayer::GraphStylePresetSpec> MultiLayer::listStylePresets()
{
    registerStylePresetTranslationsOnce();
    QVector<GraphStylePresetSpec> result = defaultBuiltInStylePresets();
    const QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();

    for (const GraphStylePresetSpec &ue : userEntries) {
        bool merged = false;
        for (int i = 0; i < result.size(); ++i) {
            if (result[i].id.compare(ue.id, Qt::CaseInsensitive) == 0) {
                GraphStylePresetSpec ov = ue;
                ov.id = result[i].id;
                ov.builtIn = true;
                if (ov.name.trimmed().isEmpty())
                    ov.name = result[i].name;
                if (ov.slidePastePercent <= 0.0)
                    ov.slidePastePercent = result[i].slidePastePercent;
                result[i] = ov;
                merged = true;
                break;
            }
        }
        if (!merged) {
            GraphStylePresetSpec custom = ue;
            custom.builtIn = false;
            if (custom.name.trimmed().isEmpty())
                custom.name = custom.id;
            result.append(custom);
        }
    }
    // E4: presets from the shared (lab) folder are merged read-only. Built-in and personal
    // presets win on an id clash so a shared file can never silently change them.
    appendMergedSharedPresets(result, readSharedPresetEntries());
    return result;
}

bool MultiLayer::findStylePreset(const QString &idOrName, GraphStylePresetSpec *outSpec)
{
    const QString key = canonicalBuiltInAlias(idOrName);
    if (key.isEmpty())
        return false;
    const QVector<GraphStylePresetSpec> all = listStylePresets();
    for (const GraphStylePresetSpec &s : all) {
        if (s.id.compare(key, Qt::CaseInsensitive) == 0) {
            if (outSpec)
                *outSpec = s;
            return true;
        }
    }
    for (const GraphStylePresetSpec &s : all) {
        if (s.name.compare(idOrName.trimmed(), Qt::CaseInsensitive) == 0
            || (s.builtIn
                && QObject::tr(s.name.toUtf8().constData())
                           .compare(idOrName.trimmed(), Qt::CaseInsensitive)
                        == 0)) {
            if (outSpec)
                *outSpec = s;
            return true;
        }
    }
    return false;
}

bool MultiLayer::saveStylePreset(const GraphStylePresetSpec &inSpec, QString *errOut)
{
    GraphStylePresetSpec spec = inSpec;
    normalizePresetSpecForPersonalSave(spec, defaultBuiltInStylePresets());

    QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();
    bool replaced = false;
    for (int i = 0; i < userEntries.size(); ++i) {
        if (userEntries[i].id.compare(spec.id, Qt::CaseInsensitive) == 0) {
            userEntries[i] = spec;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        userEntries.append(spec);
    return writeUserStylePresetsArray(userEntries, errOut);
}

bool MultiLayer::deleteStylePreset(const QString &id, QString *errOut)
{
    const QString key = canonicalBuiltInAlias(id);
    for (const GraphStylePresetSpec &d : defaultBuiltInStylePresets()) {
        if (d.id.compare(key, Qt::CaseInsensitive) == 0) {
            if (errOut)
                *errOut = QObject::tr(
                        "Cannot delete a built-in preset. Use 'Reset Default' to restore its original settings.");
            return false;
        }
    }
    QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();
    bool inUserFile = false;
    for (const GraphStylePresetSpec &u : userEntries) {
        if (u.id.compare(key, Qt::CaseInsensitive) == 0)
            inUserFile = true;
    }
    if (!inUserFile) {
        for (const GraphStylePresetSpec &se : readSharedPresetEntries()) {
            if (se.id.compare(key, Qt::CaseInsensitive) == 0) {
                if (errOut)
                    *errOut = QObject::tr("This preset comes from the shared folder and is "
                                          "read-only. Duplicate it to make a personal copy.");
                return false;
            }
        }
    }
    int removed = 0;
    for (int i = userEntries.size() - 1; i >= 0; --i) {
        if (userEntries[i].id.compare(key, Qt::CaseInsensitive) == 0) {
            userEntries.removeAt(i);
            ++removed;
        }
    }
    if (removed == 0)
        return false;
    return writeUserStylePresetsArray(userEntries, errOut);
}

bool MultiLayer::resetBuiltInStylePreset(const QString &id, QString *errOut)
{
    const QString key = canonicalBuiltInAlias(id);
    QVector<GraphStylePresetSpec> userEntries = readRawUserPresetEntries();
    for (int i = userEntries.size() - 1; i >= 0; --i) {
        if (userEntries[i].id.compare(key, Qt::CaseInsensitive) == 0)
            userEntries.removeAt(i);
    }
    return writeUserStylePresetsArray(userEntries, errOut);
}

bool MultiLayer::exportStylePresetsToJson(const QString &filePath, QString *errOut)
{
    return writeStylePresetsToJsonFile(filePath, listStylePresets(), errOut);
}

int MultiLayer::importStylePresetsFromJson(const QString &filePath, QString *errOut)
{
    QVector<GraphStylePresetSpec> parsedSpecs;
    QString warnMsg;
    if (!parseStylePresetsFromJsonFile(filePath, false, false, &parsedSpecs, errOut, &warnMsg))
        return -1;
    if (errOut && !warnMsg.isEmpty())
        *errOut = warnMsg;

    int count = 0;
    for (const GraphStylePresetSpec &parsed : parsedSpecs) {
        QString saveErr;
        if (saveStylePreset(parsed, &saveErr))
            ++count;
        else if (errOut && !saveErr.isEmpty())
            *errOut = saveErr;
    }
    return count;
}

MultiLayer::GraphStylePresetSpec MultiLayer::captureStylePresetSpec(const QString &name) const
{
    GraphStylePresetSpec s;
    s.id = QStringLiteral("custom_")
            + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    s.name = name.trimmed().isEmpty() ? QObject::tr("New Custom Preset") : name.trimmed();
    s.builtIn = false;

    double pageW = d_page_width_val;
    double pageH = d_page_height_val;
    if (d_page_unit == UnitPx && canvas && canvas->width() > 0 && canvas->height() > 0
        && d_page_zoom > 1e-6) {
        pageW = double(canvas->width()) / d_page_zoom;
        pageH = double(canvas->height()) / d_page_zoom;
    }

    constexpr double screenPpi = 96.0;
    auto toMm = [&](double val) -> double {
        switch (d_page_unit) {
        case UnitMm:
            return val;
        case UnitCm:
            return val * 10.0;
        case UnitInch:
            return val * 25.4;
        case UnitPx:
        default:
            return val * 25.4 / screenPpi;
        }
    };
    s.pageWidthMm = qBound(20.0, toMm(pageW), 1000.0);
    s.pageHeightMm = qBound(20.0, toMm(pageH), 1000.0);
    s.keepAspectRatio = d_keep_aspect_ratio;
    s.targetDpi = qBound(72, d_page_dpi, 2400);
    s.marginPx = qBound(0, left_margin, 80);
    s.pageBackground = d_page_bg_color.isValid() ? d_page_bg_color : QColor(Qt::white);

    if (Graph *g = active_graph) {
        const QFont tf = g->axisTitleFont(QwtPlot::xBottom);
        const QFont lf = g->axisFont(QwtPlot::xBottom);
        if (!tf.family().isEmpty())
            s.fontFamily = tf.family();
        s.axisTitlePt = (tf.pointSizeF() > 0.0) ? tf.pointSizeF() : double(qMax(6, tf.pointSize()));
        s.tickLabelPt = (lf.pointSizeF() > 0.0) ? lf.pointSizeF() : double(qMax(6, lf.pointSize()));
        s.boldTitle = tf.bold();
        s.boldTick = lf.bold();
        if (Legend *leg = g->legend()) {
            const QFont legF = leg->font();
            s.legendPt = (legF.pointSizeF() > 0.0) ? legF.pointSizeF()
                                                   : double(qMax(6, legF.pointSize()));
            s.boldLegend = legF.bold();
            s.legendFrame = qBound(0, leg->frameStyle(), 2);
        } else {
            s.legendPt = qMax(6.0, s.tickLabelPt - 1.0);
        }
        if (Plot *pl = g->plotWidget())
            s.axisLineWidth = qBound(1, pl->axesLinewidth(), 10);
        s.minorTickLen = qBound(0, g->minorTickLength(), 20);
        s.majorTickLen = qBound(0, g->majorTickLength(), 30);
        s.largeSlideLayout = (s.axisTitlePt >= 20.0 || s.tickLabelPt >= 20.0);
        s.formatIsijUnitSlash = (g->axisUnitFormat() == Column::UnitFormat::ISIJSlash);
        const bool topOn = g->plotWidget() && g->plotWidget()->axisEnabled(QwtPlot::xTop);
        const bool rightOn = g->plotWidget() && g->plotWidget()->axisEnabled(QwtPlot::yRight);
        s.framePreset = (topOn && rightOn) ? static_cast<int>(Graph::FrameBoxInwardTicks)
                                           : static_cast<int>(Graph::FrameOpenL);
        for (int cIdx = 0; cIdx < g->curves(); ++cIdx) {
            QwtPlotCurve *c = g->curve(cIdx);
            if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
                continue;
            const double cw = (c->pen().widthF() > 0.0) ? c->pen().widthF()
                                                        : double(qMax(1, c->pen().width()));
            if (c->pen().style() != Qt::NoPen) {
                s.curveWidthF = qBound(0.5, cw, 20.0);
                s.minCurveWidth = qBound(1, qRound(s.curveWidthF), 20);
            }
            if (c->symbol().style() != QwtSymbol::NoSymbol)
                s.minSymbolSize = qBound(0, c->symbol().size().width(), 100);
            break;
        }
    }
    return s;
}

void MultiLayer::showStylePresetManagerDialog(ApplicationWindow *app, MultiLayer *targetGraph)
{
    registerStylePresetTranslationsOnce();
    QPointer<ApplicationWindow> appGuard(app);
    QPointer<MultiLayer> targetGraphGuard(targetGraph);
    if (appGuard && ApplicationWindow::isNonInteractiveContext(appGuard.data()))
        return;

    if (!targetGraphGuard && appGuard)
        targetGraphGuard = qobject_cast<MultiLayer *>(appGuard->d_workspace.activeSubWindow());

    const QString origNewGraphPresetId = newGraphStylePresetId();
    const QString origCurvePalette = Graph::defaultCurvePaletteName();
    const MultiPanelSizing origPanelSizing = multiPanelSizing();
    const QString origSharedFolder = sharedStylePresetsFolder();
    QString pendingNewGraphPresetId = origNewGraphPresetId;
    QString stagedSharedFolder = origSharedFolder;

    QDialog dlg(appGuard.data());
    dlg.setWindowTitle(QObject::tr("Graph Style Preset Manager"));
    dlg.setMinimumSize(680, 410);
    dlg.resize(750, 480);

    QVBoxLayout *rootLay = new QVBoxLayout(&dlg);
    rootLay->setContentsMargins(8, 8, 8, 8);
    rootLay->setSpacing(6);
    QHBoxLayout *mainSplit = new QHBoxLayout();

    // Left column: Preset list & CRUD buttons
    QVBoxLayout *leftCol = new QVBoxLayout();
    QListWidget *lw = new QListWidget(&dlg);
    lw->setMinimumWidth(235);
    leftCol->addWidget(lw, 1);

    QHBoxLayout *addRow = new QHBoxLayout();
    QPushButton *btnNew = new QPushButton(QObject::tr("New"), &dlg);
    QPushButton *btnFromGraph = new QPushButton(QObject::tr("From Current Graph"), &dlg);
    btnFromGraph->setEnabled(targetGraphGuard != nullptr);
    addRow->addWidget(btnNew);
    addRow->addWidget(btnFromGraph);
    leftCol->addLayout(addRow);

    QHBoxLayout *actRow = new QHBoxLayout();
    QPushButton *btnDup = new QPushButton(QObject::tr("Duplicate"), &dlg);
    QPushButton *btnDel = new QPushButton(QObject::tr("Delete"), &dlg);
    QPushButton *btnReset = new QPushButton(QObject::tr("Reset Default"), &dlg);
    actRow->addWidget(btnDup);
    actRow->addWidget(btnDel);
    actRow->addWidget(btnReset);
    leftCol->addLayout(actRow);

    QHBoxLayout *ioRow = new QHBoxLayout();
    QPushButton *btnImport = new QPushButton(QObject::tr("Import JSON..."), &dlg);
    QPushButton *btnExport = new QPushButton(QObject::tr("Export JSON..."), &dlg);
    ioRow->addWidget(btnImport);
    ioRow->addWidget(btnExport);
    leftCol->addLayout(ioRow);

    mainSplit->addLayout(leftCol, 0);

    // Right column: Scrollable parameter editor + global defaults so 911x512 screens fit cleanly
    QScrollArea *rightScroll = new QScrollArea(&dlg);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    QWidget *rightScrollContent = new QWidget(rightScroll);
    QVBoxLayout *rightColLay = new QVBoxLayout(rightScrollContent);
    rightColLay->setContentsMargins(4, 0, 4, 0);
    rightColLay->setSpacing(8);

    QLabel *lblReadOnlyHint = new QLabel(
            QObject::tr("This preset comes from the shared folder and is read-only. "
                        "Duplicate it to make a personal copy."),
            rightScrollContent);
    lblReadOnlyHint->setWordWrap(true);
    lblReadOnlyHint->setStyleSheet(
            QStringLiteral("QLabel { color: #64748B; font-style: italic; padding: 2px 6px; }"));
    lblReadOnlyHint->hide();
    rightColLay->addWidget(lblReadOnlyHint);

    QWidget *editorPane = new QWidget(rightScrollContent);
    QFormLayout *form = new QFormLayout(editorPane);
    form->setContentsMargins(6, 2, 2, 2);

    QLineEdit *leName = new QLineEdit(editorPane);
    form->addRow(QObject::tr("Preset Name:"), leName);

    QHBoxLayout *sizeRow = new QHBoxLayout();
    QDoubleSpinBox *sbW = new QDoubleSpinBox(editorPane);
    sbW->setRange(20.0, 1000.0);
    sbW->setDecimals(1);
    QDoubleSpinBox *sbH = new QDoubleSpinBox(editorPane);
    sbH->setRange(20.0, 1000.0);
    sbH->setDecimals(1);
    QCheckBox *chkAspect = new QCheckBox(QObject::tr("Lock Aspect Ratio"), editorPane);
    sizeRow->addWidget(sbW);
    sizeRow->addWidget(new QLabel(QStringLiteral("\u00D7"), editorPane));
    sizeRow->addWidget(sbH);
    sizeRow->addWidget(chkAspect);
    form->addRow(QObject::tr("Page Width (mm):"), sizeRow);

    QHBoxLayout *dpiRow = new QHBoxLayout();
    QSpinBox *sbDpi = new QSpinBox(editorPane);
    sbDpi->setRange(72, 2400);
    sbDpi->setSingleStep(50);
    QSpinBox *sbMargin = new QSpinBox(editorPane);
    sbMargin->setRange(0, 80);
    dpiRow->addWidget(sbDpi);
    dpiRow->addWidget(new QLabel(QObject::tr("Margin (px):"), editorPane));
    dpiRow->addWidget(sbMargin);
    form->addRow(QObject::tr("Target DPI:"), dpiRow);

    QComboBox *cbFrame = new QComboBox(editorPane);
    cbFrame->addItem(QObject::tr("Box + Inward Ticks (ISIJ)"),
                     static_cast<int>(Graph::FrameBoxInwardTicks));
    cbFrame->addItem(QObject::tr("Box + Plain Opposite Axes"),
                     static_cast<int>(Graph::FrameBoxPlainOpposite));
    cbFrame->addItem(QObject::tr("Open L + Outward Ticks (Nature)"),
                     static_cast<int>(Graph::FrameOpenL));
    cbFrame->addItem(QObject::tr("Zero-Crossed Axes"),
                     static_cast<int>(Graph::FrameZeroCrossed));
    form->addRow(QObject::tr("Frame && Ticks:"), cbFrame);

    QHBoxLayout *tickRow = new QHBoxLayout();
    QSpinBox *sbAxisLw = new QSpinBox(editorPane);
    sbAxisLw->setRange(1, 10);
    QSpinBox *sbMinTick = new QSpinBox(editorPane);
    sbMinTick->setRange(0, 20);
    QSpinBox *sbMajTick = new QSpinBox(editorPane);
    sbMajTick->setRange(0, 30);
    tickRow->addWidget(sbAxisLw);
    tickRow->addWidget(new QLabel(QObject::tr("Minor Tick Length (px):"), editorPane));
    tickRow->addWidget(sbMinTick);
    tickRow->addWidget(new QLabel(QObject::tr("Major Tick Length (px):"), editorPane));
    tickRow->addWidget(sbMajTick);
    form->addRow(QObject::tr("Axis Line Width (px):"), tickRow);

    QFontComboBox *cbFont = new QFontComboBox(editorPane);
    form->addRow(QObject::tr("Font Family:"), cbFont);

    QHBoxLayout *fTitleRow = new QHBoxLayout();
    QDoubleSpinBox *sbTitlePt = new QDoubleSpinBox(editorPane);
    sbTitlePt->setRange(5.0, 72.0);
    sbTitlePt->setDecimals(1);
    QCheckBox *chkBoldTitle = new QCheckBox(QObject::tr("Bold"), editorPane);
    fTitleRow->addWidget(sbTitlePt);
    fTitleRow->addWidget(chkBoldTitle);
    form->addRow(QObject::tr("Axis Title Size (pt):"), fTitleRow);

    QHBoxLayout *fTickRow = new QHBoxLayout();
    QDoubleSpinBox *sbTickPt = new QDoubleSpinBox(editorPane);
    sbTickPt->setRange(5.0, 72.0);
    sbTickPt->setDecimals(1);
    QCheckBox *chkBoldTick = new QCheckBox(QObject::tr("Bold"), editorPane);
    fTickRow->addWidget(sbTickPt);
    fTickRow->addWidget(chkBoldTick);
    form->addRow(QObject::tr("Tick Label Size (pt):"), fTickRow);

    QHBoxLayout *fLegRow = new QHBoxLayout();
    QDoubleSpinBox *sbLegPt = new QDoubleSpinBox(editorPane);
    sbLegPt->setRange(5.0, 72.0);
    sbLegPt->setDecimals(1);
    QCheckBox *chkBoldLeg = new QCheckBox(QObject::tr("Bold"), editorPane);
    fLegRow->addWidget(sbLegPt);
    fLegRow->addWidget(chkBoldLeg);
    form->addRow(QObject::tr("Legend Size (pt):"), fLegRow);

    QHBoxLayout *curveRow = new QHBoxLayout();
    QDoubleSpinBox *sbCurveW = new QDoubleSpinBox(editorPane);
    sbCurveW->setRange(0.5, 20.0);
    sbCurveW->setDecimals(1);
    sbCurveW->setSingleStep(0.5);
    QSpinBox *sbSymSz = new QSpinBox(editorPane);
    sbSymSz->setRange(0, 100);
    curveRow->addWidget(sbCurveW);
    curveRow->addWidget(new QLabel(QObject::tr("Symbol Size (px):"), editorPane));
    curveRow->addWidget(sbSymSz);
    form->addRow(QObject::tr("Curve Width (px):"), curveRow);

    QComboBox *cbLegFrame = new QComboBox(editorPane);
    cbLegFrame->addItem(QObject::tr("None (Transparent)"), static_cast<int>(Legend::None));
    cbLegFrame->addItem(QObject::tr("Line Box"), static_cast<int>(Legend::Line));
    cbLegFrame->addItem(QObject::tr("Shadow Box"), static_cast<int>(Legend::Shadow));
    form->addRow(QObject::tr("Legend Frame:"), cbLegFrame);

    QDoubleSpinBox *sbPastePct = new QDoubleSpinBox(editorPane);
    sbPastePct->setRange(10.0, 400.0);
    sbPastePct->setDecimals(0);
    sbPastePct->setSingleStep(10.0);
    sbPastePct->setSuffix(QStringLiteral(" %"));
    form->addRow(QObject::tr("PowerPoint Paste Scale (%):"), sbPastePct);

    QCheckBox *chkIsij = new QCheckBox(
            QObject::tr("Format axis units as ISIJ slash (Quantity, sym / unit)"), editorPane);
    QCheckBox *chkSlide = new QCheckBox(
            QObject::tr("Use large presentation-slide layout rules"), editorPane);
    form->addRow(QString(), chkIsij);
    form->addRow(QString(), chkSlide);

    rightColLay->addWidget(editorPane);

    // E1/E4/E5/E7: application-wide defaults that belong with the presets (saved on confirm only).
    QGroupBox *defaultsBox = new QGroupBox(QObject::tr("Defaults for New Graphs"), rightScrollContent);
    QFormLayout *defForm = new QFormLayout(defaultsBox);
    QComboBox *cbNewGraphPreset = new QComboBox(defaultsBox);
    QComboBox *cbCurvePalette = new QComboBox(defaultsBox);
    Graph::registerPaletteTranslations();
    cbCurvePalette->addItem(QObject::tr("ToriGraph Standard (black, vermilion, blue, ...)"),
                            Graph::toriGraphStandardPaletteName());
    cbCurvePalette->addItem(QObject::tr("Origin Classic (black, red, blue, ...)"),
                            Graph::originClassicPaletteName());
    cbCurvePalette->addItem(QObject::tr("Okabe-Ito (Colorblind Safe)"),
                            Graph::okabeItoPaletteName());
    cbCurvePalette->addItem(QObject::tr("Matplotlib (tab10)"), Graph::tab10PaletteName());
    {
        // A palette chosen elsewhere (lab defaults ini, Python) is shown as it is instead of
        // silently displaying the first entry.
        int pIdx = cbCurvePalette->findData(origCurvePalette);
        if (pIdx < 0 && !origCurvePalette.isEmpty()) {
            cbCurvePalette->addItem(QObject::tr(origCurvePalette.toUtf8().constData()),
                                    origCurvePalette);
            pIdx = cbCurvePalette->count() - 1;
        }
        cbCurvePalette->setCurrentIndex(pIdx >= 0 ? pIdx : 0);
    }
    QComboBox *cbPanelSizing = new QComboBox(defaultsBox);
    cbPanelSizing->addItem(QObject::tr("Preset size per panel (columns x width)"),
                           int(PanelSizingPerPanel));
    cbPanelSizing->addItem(QObject::tr("Journal double column (175 mm wide)"),
                           int(PanelSizingTwoColumn175));
    cbPanelSizing->setCurrentIndex(origPanelSizing == PanelSizingTwoColumn175 ? 1 : 0);
    QHBoxLayout *sharedRow = new QHBoxLayout();
    QLineEdit *leShared = new QLineEdit(origSharedFolder, defaultsBox);
    leShared->setPlaceholderText(QObject::tr("(none)"));
    QPushButton *btnSharedBrowse = new QPushButton(QObject::tr("Browse..."), defaultsBox);
    sharedRow->addWidget(leShared, 1);
    sharedRow->addWidget(btnSharedBrowse);
    defForm->addRow(QObject::tr("Preset applied to new graphs:"), cbNewGraphPreset);
    defForm->addRow(QObject::tr("Colors for new curves:"), cbCurvePalette);
    defForm->addRow(QObject::tr("Multi-panel page size:"), cbPanelSizing);
    defForm->addRow(QObject::tr("Shared preset folder (read-only):"), sharedRow);
    rightColLay->addWidget(defaultsBox);
    rightColLay->addStretch(1);

    rightScroll->setWidget(rightScrollContent);
    mainSplit->addWidget(rightScroll, 1);
    rootLay->addLayout(mainSplit, 1);

    // Bottom action buttons (no default/autoDefault so Enter inPreset Name never applies & closes)
    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *btnSave = new QPushButton(QObject::tr("Save Changes"), &dlg);
    QPushButton *btnApply = new QPushButton(QObject::tr("Save && Apply to Graph"), &dlg);
    btnApply->setEnabled(targetGraphGuard != nullptr);
    if (targetGraphGuard) {
        QObject::connect(targetGraphGuard.data(), &QObject::destroyed, &dlg,
                         [btnFromGraph, btnApply]() {
                             btnFromGraph->setEnabled(false);
                             btnApply->setEnabled(false);
                         });
    }
    QPushButton *btnOk = new QPushButton(QObject::tr("OK"), &dlg);
    QPushButton *btnCancel = new QPushButton(QObject::tr("Cancel"), &dlg);
    for (QPushButton *b : { btnNew, btnFromGraph, btnDup, btnDel, btnReset, btnImport, btnExport,
                            btnSharedBrowse, btnSave, btnApply, btnOk, btnCancel }) {
        b->setAutoDefault(false);
        b->setDefault(false);
    }
    btnRow->addStretch(1);
    btnRow->addWidget(btnSave);
    btnRow->addWidget(btnApply);
    btnRow->addWidget(btnOk);
    btnRow->addWidget(btnCancel);
    rootLay->addLayout(btnRow);

    QVector<GraphStylePresetSpec> presets = listStylePresets();
    int currentEditRow = -1;
    bool loadingUi = false;

    auto displayPresetName = [](const GraphStylePresetSpec &s) -> QString {
        return s.builtIn ? QObject::tr(s.name.toUtf8().constData()) : s.name;
    };

    auto presetListLabel = [&](const GraphStylePresetSpec &s) -> QString {
        const QString tag = s.builtIn ? QObject::tr("Built-in")
                : (s.readOnly ? QObject::tr("Shared") : QObject::tr("Custom"));
        return QStringLiteral("[%1]  %2").arg(tag, displayPresetName(s));
    };

    auto readFormIntoSpec = [&](GraphStylePresetSpec &s) {
        const QString enteredName = leName->text().trimmed();
        if (enteredName.isEmpty())
            s.name = s.id;
        else if (!(s.builtIn && enteredName == displayPresetName(s)))
            s.name = enteredName;
        s.pageWidthMm = sbW->value();
        s.pageHeightMm = sbH->value();
        s.keepAspectRatio = chkAspect->isChecked();
        s.targetDpi = sbDpi->value();
        s.marginPx = sbMargin->value();
        s.framePreset = cbFrame->currentData().toInt();
        s.axisLineWidth = sbAxisLw->value();
        s.minorTickLen = sbMinTick->value();
        s.majorTickLen = sbMajTick->value();
        s.fontFamily = cbFont->currentFont().family();
        s.axisTitlePt = sbTitlePt->value();
        s.tickLabelPt = sbTickPt->value();
        s.legendPt = sbLegPt->value();
        s.boldTitle = chkBoldTitle->isChecked();
        s.boldTick = chkBoldTick->isChecked();
        s.boldLegend = chkBoldLeg->isChecked();
        s.curveWidthF = sbCurveW->value();
        s.minCurveWidth = qBound(1, qRound(s.curveWidthF), 20);
        s.minSymbolSize = sbSymSz->value();
        s.legendFrame = cbLegFrame->currentData().toInt();
        const double pct = sbPastePct->value();
        s.slidePastePercent =
                (std::fabs(pct - 100.0) < 1e-6 && s.slidePastePercent <= 0.0) ? 0.0 : pct;
        s.formatIsijUnitSlash = chkIsij->isChecked();
        s.largeSlideLayout = chkSlide->isChecked();
    };

    auto flushCurrentRowToModel = [&]() {
        if (loadingUi || currentEditRow < 0 || currentEditRow >= presets.size())
            return;
        if (presets[currentEditRow].readOnly)
            return;
        readFormIntoSpec(presets[currentEditRow]);
        if (QListWidgetItem *item = lw->item(currentEditRow))
            item->setText(presetListLabel(presets[currentEditRow]));
        const int comboIdx = cbNewGraphPreset->findData(presets[currentEditRow].id);
        if (comboIdx >= 0) {
            cbNewGraphPreset->setItemText(comboIdx,
                                          displayPresetName(presets[currentEditRow]));
        }
    };

    auto populateForm = [&](int row) {
        loadingUi = true;
        if (row < 0 || row >= presets.size()) {
            lblReadOnlyHint->hide();
            editorPane->setEnabled(false);
            btnDup->setEnabled(false);
            btnDel->setEnabled(false);
            btnReset->setEnabled(false);
            btnApply->setEnabled(false);
            loadingUi = false;
            return;
        }
        const GraphStylePresetSpec &s = presets[row];
        leName->setText(displayPresetName(s));
        sbW->setValue(s.pageWidthMm);
        sbH->setValue(s.pageHeightMm);
        chkAspect->setChecked(s.keepAspectRatio);
        sbDpi->setValue(s.targetDpi);
        sbMargin->setValue(s.marginPx);
        const int fIdx = cbFrame->findData(s.framePreset);
        cbFrame->setCurrentIndex(fIdx >= 0 ? fIdx : 0);
        sbAxisLw->setValue(s.axisLineWidth);
        sbMinTick->setValue(s.minorTickLen);
        sbMajTick->setValue(s.majorTickLen);
        cbFont->setCurrentFont(QFont(s.fontFamily));
        sbTitlePt->setValue(s.axisTitlePt);
        sbTickPt->setValue(s.tickLabelPt);
        sbLegPt->setValue(s.legendPt);
        chkBoldTitle->setChecked(s.boldTitle);
        chkBoldTick->setChecked(s.boldTick);
        chkBoldLeg->setChecked(s.boldLegend);
        sbCurveW->setValue((s.curveWidthF > 0.0) ? s.curveWidthF : double(s.minCurveWidth));
        sbSymSz->setValue(s.minSymbolSize);
        const int lIdx = cbLegFrame->findData(s.legendFrame);
        cbLegFrame->setCurrentIndex(lIdx >= 0 ? lIdx : 0);
        sbPastePct->setValue((s.slidePastePercent > 0.0) ? s.slidePastePercent : 100.0);
        chkIsij->setChecked(s.formatIsijUnitSlash);
        chkSlide->setChecked(s.largeSlideLayout);
        lblReadOnlyHint->setVisible(s.readOnly);
        editorPane->setEnabled(!s.readOnly);
        btnDup->setEnabled(true);
        btnDel->setEnabled(!s.builtIn && !s.readOnly);
        btnReset->setEnabled(s.builtIn);
        btnSave->setEnabled(true);
        btnApply->setEnabled(targetGraphGuard != nullptr);
        btnApply->setText(s.readOnly ? QObject::tr("Apply to Graph")
                                     : QObject::tr("Save && Apply to Graph"));
        loadingUi = false;
    };

    auto refillNewGraphPresetCombo = [&]() {
        QSignalBlocker blk(cbNewGraphPreset);
        cbNewGraphPreset->clear();
        cbNewGraphPreset->addItem(QObject::tr("None (keep plain pixel page)"), QString());
        for (const GraphStylePresetSpec &s : presets)
            cbNewGraphPreset->addItem(displayPresetName(s), s.id);
        const int idx = cbNewGraphPreset->findData(pendingNewGraphPresetId);
        cbNewGraphPreset->setCurrentIndex(idx >= 0 ? idx : 0);
    };

    auto refreshListUi = [&](const QString &selectId) {
        loadingUi = true;
        currentEditRow = -1;
        lw->clear();
        int targetRow = 0;
        for (int i = 0; i < presets.size(); ++i) {
            const GraphStylePresetSpec &s = presets[i];
            QListWidgetItem *it = new QListWidgetItem(presetListLabel(s), lw);
            it->setData(Qt::UserRole, s.id);
            if (!selectId.isEmpty() && s.id.compare(selectId, Qt::CaseInsensitive) == 0)
                targetRow = i;
        }
        if (lw->count() > 0) {
            targetRow = qBound(0, targetRow, lw->count() - 1);
            lw->setCurrentRow(targetRow);
            currentEditRow = targetRow;
        }
        populateForm(currentEditRow);
        refillNewGraphPresetCombo();
    };
    refreshListUi(QString());

    auto insertPersonalPresetInModel = [&](const GraphStylePresetSpec &spec) {
        int insertPos = presets.size();
        for (int i = 0; i < presets.size(); ++i) {
            if (presets[i].readOnly) {
                insertPos = i;
                break;
            }
        }
        presets.insert(insertPos, spec);
    };

    auto reloadSharedIntoStaged = [&](const QString &newFolder) {
        flushCurrentRowToModel();
        const QString curId = (currentEditRow >= 0 && currentEditRow < presets.size())
                ? presets[currentEditRow].id
                : QString();
        stagedSharedFolder = newFolder.trimmed();
        for (int i = presets.size() - 1; i >= 0; --i) {
            if (presets[i].readOnly)
                presets.removeAt(i);
        }
        appendMergedSharedPresets(presets, readSharedPresetEntriesFromDir(stagedSharedFolder));
        refreshListUi(curId);
    };

    QObject::connect(cbNewGraphPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [&](int) {
                         pendingNewGraphPresetId = cbNewGraphPreset->currentData().toString();
                     });
    QObject::connect(leShared, &QLineEdit::editingFinished, &dlg, [&]() {
        const QString next = leShared->text().trimmed();
        if (next == stagedSharedFolder)
            return;
        reloadSharedIntoStaged(next);
    });
    QObject::connect(btnSharedBrowse, &QPushButton::clicked, &dlg, [&]() {
        const QString dir = QFileDialog::getExistingDirectory(
                &dlg, QObject::tr("Shared preset folder (read-only):"),
                leShared->text().trimmed().isEmpty() ? QDir::homePath() : leShared->text());
        if (dir.isEmpty())
            return;
        leShared->setText(QDir::toNativeSeparators(dir));
        reloadSharedIntoStaged(leShared->text());
    });

    QObject::connect(lw, &QListWidget::currentRowChanged, &dlg, [&](int row) {
        if (loadingUi)
            return;
        if (currentEditRow >= 0 && currentEditRow < presets.size() && currentEditRow != row)
            flushCurrentRowToModel();
        currentEditRow = row;
        populateForm(row);
    });

    auto commitAllChanges = [&]() -> bool {
        if (leShared->text().trimmed() != stagedSharedFolder)
            reloadSharedIntoStaged(leShared->text());
        else
            flushCurrentRowToModel();

        const QVector<GraphStylePresetSpec> builtInDefs = defaultBuiltInStylePresets();
        QVector<GraphStylePresetSpec> userEntriesToWrite;
        for (const GraphStylePresetSpec &s : presets) {
            if (s.readOnly)
                continue;
            if (s.builtIn) {
                bool matchesDefault = false;
                for (const GraphStylePresetSpec &def : builtInDefs) {
                    if (def.id.compare(s.id, Qt::CaseInsensitive) == 0) {
                        matchesDefault = (stylePresetToJson(s) == stylePresetToJson(def));
                        break;
                    }
                }
                if (matchesDefault)
                    continue;
            }
            userEntriesToWrite.append(s);
        }
        QString err;
        if (!writeUserStylePresetsArray(userEntriesToWrite, &err)) {
            QMessageBox::warning(&dlg, dlg.windowTitle(), err);
            return false;
        }
        setNewGraphStylePresetId(cbNewGraphPreset->currentData().toString());
        Graph::setDefaultCurvePaletteName(cbCurvePalette->currentData().toString());
        if (appGuard && appGuard->graphInspectorDock())
            appGuard->graphInspectorDock()->syncSeriesSchemeToDefaultPalette();
        setMultiPanelSizing(static_cast<MultiPanelSizing>(cbPanelSizing->currentData().toInt()));
        setSharedStylePresetsFolder(stagedSharedFolder);
        return true;
    };

    QObject::connect(btnNew, &QPushButton::clicked, &dlg, [&]() {
        flushCurrentRowToModel();
        GraphStylePresetSpec s;
        s.id = QStringLiteral("custom_")
                + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        s.name = QObject::tr("New Custom Preset");
        s.builtIn = false;
        insertPersonalPresetInModel(s);
        refreshListUi(s.id);
        leName->setFocus();
        leName->selectAll();
    });

    QObject::connect(btnFromGraph, &QPushButton::clicked, &dlg, [&]() {
        if (!targetGraphGuard) {
            btnFromGraph->setEnabled(false);
            btnApply->setEnabled(false);
            return;
        }
        flushCurrentRowToModel();
        GraphStylePresetSpec s =
                targetGraphGuard->captureStylePresetSpec(QObject::tr("New Custom Preset"));
        insertPersonalPresetInModel(s);
        refreshListUi(s.id);
        leName->setFocus();
        leName->selectAll();
    });

    QObject::connect(btnDup, &QPushButton::clicked, &dlg, [&]() {
        const int row = lw->currentRow();
        if (row < 0 || row >= presets.size())
            return;
        flushCurrentRowToModel();
        GraphStylePresetSpec s = presets[row];
        s.id = QStringLiteral("custom_")
                + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        s.builtIn = false;
        s.readOnly = false;
        s.sourcePath.clear();
        s.name = QObject::tr("Copy of %1").arg(displayPresetName(presets[row]));
        insertPersonalPresetInModel(s);
        refreshListUi(s.id);
    });

    QObject::connect(btnDel, &QPushButton::clicked, &dlg, [&]() {
        const int row = lw->currentRow();
        if (row < 0 || row >= presets.size())
            return;
        flushCurrentRowToModel();
        const GraphStylePresetSpec &target = presets[row];
        if (target.builtIn) {
            QMessageBox::information(
                    &dlg, dlg.windowTitle(),
                    QObject::tr("Cannot delete a built-in preset. Use 'Reset Default' to "
                                "restore its original settings."));
            return;
        }
        if (target.readOnly) {
            QMessageBox::information(
                    &dlg, dlg.windowTitle(),
                    QObject::tr("This preset comes from the shared folder and is read-only. "
                                "Duplicate it to make a personal copy."));
            return;
        }
        if (pendingNewGraphPresetId.compare(target.id, Qt::CaseInsensitive) == 0)
            pendingNewGraphPresetId.clear();
        presets.removeAt(row);
        const QString nextId =
                (!presets.isEmpty()) ? presets[qMin(row, presets.size() - 1)].id : QString();
        refreshListUi(nextId);
    });

    QObject::connect(btnReset, &QPushButton::clicked, &dlg, [&]() {
        const int row = lw->currentRow();
        if (row < 0 || row >= presets.size() || !presets[row].builtIn)
            return;
        const QString id = presets[row].id;
        for (const GraphStylePresetSpec &def : defaultBuiltInStylePresets()) {
            if (def.id.compare(id, Qt::CaseInsensitive) == 0) {
                presets[row] = def;
                refreshListUi(id);
                break;
            }
        }
    });

    QObject::connect(btnExport, &QPushButton::clicked, &dlg, [&]() {
        flushCurrentRowToModel();
        const QString path = QFileDialog::getSaveFileName(
                &dlg, QObject::tr("Export JSON..."),
                QDir::homePath() + QStringLiteral("/torigraph_presets.json"),
                QStringLiteral("JSON (*.json)"));
        if (path.isEmpty())
            return;
        QString err;
        if (!writeStylePresetsToJsonFile(path, presets, &err)) {
            QMessageBox::warning(&dlg, dlg.windowTitle(), err);
        } else {
            QMessageBox::information(&dlg, dlg.windowTitle(),
                                     QObject::tr("Saved presets to %1").arg(path));
        }
    });

    QObject::connect(btnImport, &QPushButton::clicked, &dlg, [&]() {
        flushCurrentRowToModel();
        const QString path = QFileDialog::getOpenFileName(
                &dlg, QObject::tr("Import JSON..."), QDir::homePath(),
                QStringLiteral("JSON (*.json)"));
        if (path.isEmpty())
            return;
        QVector<GraphStylePresetSpec> parsedSpecs;
        QString err;
        QString warnMsg;
        if (!parseStylePresetsFromJsonFile(path, false, false, &parsedSpecs, &err, &warnMsg)) {
            QMessageBox::warning(&dlg, dlg.windowTitle(), err);
            return;
        }
        const QVector<GraphStylePresetSpec> builtInDefs = defaultBuiltInStylePresets();
        int count = 0;
        QString lastId;
        for (GraphStylePresetSpec spec : parsedSpecs) {
            normalizePresetSpecForPersonalSave(spec, builtInDefs);
            for (int i = presets.size() - 1; i >= 0; --i) {
                if (presets[i].readOnly
                    && presets[i].id.compare(spec.id, Qt::CaseInsensitive) == 0) {
                    presets.removeAt(i);
                }
            }
            bool replaced = false;
            for (int i = 0; i < presets.size(); ++i) {
                if (!presets[i].readOnly
                    && presets[i].id.compare(spec.id, Qt::CaseInsensitive) == 0) {
                    presets[i] = spec;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
                insertPersonalPresetInModel(spec);
            lastId = spec.id;
            ++count;
        }
        refreshListUi(lastId);
        QString msg = QObject::tr("Imported %1 preset(s).").arg(count);
        if (!warnMsg.isEmpty())
            msg += QStringLiteral("\n\n") + warnMsg;
        QMessageBox::information(&dlg, dlg.windowTitle(), msg);
    });

    QObject::connect(btnSave, &QPushButton::clicked, &dlg, [&]() { commitAllChanges(); });
    QObject::connect(btnApply, &QPushButton::clicked, &dlg, [&]() {
        if (!targetGraphGuard) {
            btnFromGraph->setEnabled(false);
            btnApply->setEnabled(false);
            return;
        }
        if (!commitAllChanges())
            return;
        const int row = lw->currentRow();
        if (targetGraphGuard && row >= 0 && row < presets.size()) {
            targetGraphGuard->applyStylePresetSpec(presets[row]);
            if (appGuard) {
                appGuard->modifiedProject();
                if (GraphInspectorDock *insp = appGuard->graphInspectorDock())
                    insp->refreshAll();
            }
        }
        dlg.accept();
    });
    QObject::connect(btnOk, &QPushButton::clicked, &dlg, [&]() {
        if (!commitAllChanges())
            return;
        dlg.accept();
    });
    QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.exec();
    if (appGuard) {
        if (GraphInspectorDock *insp = appGuard->graphInspectorDock())
            insp->refreshAll();
    }
}
