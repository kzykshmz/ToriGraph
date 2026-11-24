/***************************************************************************
    File                 : GraphInspectorDock_Internal.h
    Project              : ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : Shared internal helpers and undo merge IDs for
                           GraphInspectorDock split translation units
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
#ifndef GRAPH_INSPECTOR_DOCK_INTERNAL_H
#define GRAPH_INSPECTOR_DOCK_INTERNAL_H

#include "Graph.h"
#include "MultiLayer.h"

#include <QColor>
#include <QString>
#include <QVector>

class ApplicationWindow;
class MyWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QwtErrorPlotCurve;
class Spectrogram;

namespace GraphInspectorInternal {

inline constexpr int kMergePageDims = 3101;
inline constexpr int kMergeAxisScale = 3201;
inline constexpr int kMergeAxisBreak = 3202;
inline constexpr int kMergeAxisTitle = 3203;
inline constexpr int kMergeAxisTitlePt = 3204;
inline constexpr int kMergeAxisTickPt = 3205;
inline constexpr int kMergeTickPrec = 3206;
inline constexpr int kMergeCurveWidth = 3301;
inline constexpr int kMergeSymSize = 3302;
inline constexpr int kMergeCurveOpacity = 3303;
inline constexpr int kMergeSymEdgeWidth = 3304;
inline constexpr int kMergeErrWidth = 3305;
inline constexpr int kMergeErrCap = 3306;
inline constexpr int kMergeSpecCbWidth = 3307;
inline constexpr int kMergeSpecLevels = 3308;
inline constexpr int kMergeSymSkip = 3309;
inline constexpr int kMergeSpecCbTitle = 3310;
inline constexpr int kMergeErrBandOpacity = 3311;
inline constexpr int kMergePcmCbTitle = 3312;
inline constexpr int kMergeLegendHeader = 3401;
inline constexpr int kMergeLegendCols = 3402;
inline constexpr int kMergeLegendFontPt = 3403;
inline constexpr int kMergeArrowWidth = 3404;
inline constexpr int kMergeArrowHead = 3405;
inline constexpr int kMergeImageGeom = 3406;
inline constexpr int kMergePlotTitle = 3407;
inline constexpr int kMergePlotTitlePt = 3408;
inline constexpr int kMergeInsetRoi = 3409;

//! Strip scientific HTML formatting tags (<i>, <b>, <u>, <sub>, <sup>, <span>, <font>, <br>)
//! into clean readable Unicode text for tree items, combos, labels, and tooltips (A3-2).
QString stripHtmlToCleanUnicode(const QString &raw);

void setLineEditPendingCue(QLineEdit *ed, bool pending);
void wirePendingEditCue(QLineEdit *ed);
bool isUsableInspectorTable(ApplicationWindow *app, MyWidget *w);
void syncLinkedChildrenOf(MultiLayer *ml, Graph *parentG);
Spectrogram *effectiveSelectedSpectrogram(Graph *g, int *outIdx = nullptr);
QVector<QwtErrorPlotCurve *> effectiveSelectedErrorBars(Graph *g);
QString friendlyCurveDisplayName(Graph *g, int curveIdx);
int resolveMasterCurveIndex(Graph *g, int curveIdx);
int effectiveSelectedCurveIndex(Graph *g, bool fallbackToFirstDataCurve = true);
double pageUnitToMmFactor(MultiLayer::PageUnit unit);
bool isLegendEntryLine(const QString &line);
void updateColorSwatchButton(QPushButton *btn, const QColor &c);

// Fills the minor-tick combo with the presets for a linear or log10 axis (shared with
// AxesDialog through PlotStyleLimits) and selects `current`, adding it when it is not a preset.
void fillMinorTickCombo(QComboBox *cb, bool log10Axis, int current);

// Value of the (editable) minor-tick combo: the selected preset, or the leading number of a
// typed-in text. Returns `fallback` when nothing usable is entered.
int minorTickComboValue(const QComboBox *cb, int fallback);

} // namespace GraphInspectorInternal

#endif // GRAPH_INSPECTOR_DOCK_INTERNAL_H
