/***************************************************************************
    File                 : PlotStyleLimits.h
    Project              : ToriGraph
    Description          : Value ranges and choices shared by the Graph Inspector and the
                           detailed dialogs (PlotDialog / AxesDialog), so that a value set in
                           one place is never clamped or shown differently in the other (C1).
 ***************************************************************************/
#ifndef PLOT_STYLE_LIMITS_H
#define PLOT_STYLE_LIMITS_H

#include <QList>

namespace PlotStyleLimits {

// Symbol size in pixels (QwtSymbol size).
constexpr int kSymbolSizeMin = 1;
constexpr int kSymbolSizeMax = 100;

// Symbol edge (outline) width in pixels. The inspector edits it in 0.5 px steps, the
// detailed dialog in whole pixels; both use the same limits.
constexpr double kSymbolEdgeWidthMin = 0.5;
constexpr double kSymbolEdgeWidthMax = 20.0;

// Curve line width in pixels.
constexpr int kLineWidthMin = 1;
constexpr int kLineWidthMax = 20;

// Upper limit for a typed-in number of minor ticks.
constexpr int kMinorTicksMax = 100;

// Preset numbers of minor ticks between two major ticks. On a log10 axis Qwt only
// distinguishes 0, 1-2 (ticks at 2 and 5), 4 (2, 4, 6, 8) and 8 or more (2, 3, ..., 9).
inline QList<int> minorTickChoices(bool log10Axis)
{
    if (log10Axis)
        return QList<int>() << 0 << 2 << 4 << 8;
    return QList<int>() << 0 << 1 << 4 << 9;
}

} // namespace PlotStyleLimits

#endif
