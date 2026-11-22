/***************************************************************************
    File                 : PlotStyleLimits.h
    Project              : ToriGraph
    Description          : Value ranges and choices shared by the Graph Inspector and the
                           detailed dialogs (PlotDialog / AxesDialog), so that a value set in
                           one place is never clamped or shown differently in the other (C1).
 ***************************************************************************/
#ifndef PLOT_STYLE_LIMITS_H
#define PLOT_STYLE_LIMITS_H

#include <QDoubleSpinBox>
#include <QFont>
#include <QList>
#include <QLocale>
#include <QString>
#include <QtGlobal>
#include <cmath>

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

//! Effective point size of a font as floating-point pt (never loses fractional sizes like 12.9 pt).
inline double effectiveFontPointSizeF(const QFont &f, double fallback = 12.0)
{
    return f.pointSizeF() > 0.0 ? f.pointSizeF()
                                : (f.pointSize() > 0 ? double(f.pointSize()) : fallback);
}

//! Preserve a fractional point size (e.g. 12.9 pt / 11.5 pt) across QFontDialog::getFont(),
//! which only returns integer point sizes, when the user kept the rounded size unchanged.
inline QFont preserveFractionalPtAfterFontDialog(const QFont &before, const QFont &after)
{
    QFont res = after;
    const double beforePt = effectiveFontPointSizeF(before);
    const int roundedBefore = qRound(beforePt);
    const bool wasFractional = std::fabs(beforePt - double(roundedBefore)) > 1e-4;
    const int afterIntPt =
            after.pointSize() > 0 ? after.pointSize() : qRound(effectiveFontPointSizeF(after, 0.0));
    if (wasFractional && roundedBefore == afterIntPt)
        res.setPointSizeF(beforePt);
    else if (after.pointSizeF() > 0.0)
        res.setPointSizeF(after.pointSizeF());
    else if (after.pointSize() > 0)
        res.setPointSizeF(double(after.pointSize()));
    return res;
}

//! Font-size spin box supporting 0.1-pt precision (e.g. 12.9 pt, 11.5 pt) while displaying
//! whole point sizes cleanly without a trailing ".0" (e.g. "14 pt").
class FractionalPtSpinBox : public QDoubleSpinBox
{
public:
    explicit FractionalPtSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent)
    {
        setRange(4.0, 144.0);
        setDecimals(1);
        setSingleStep(0.5);
        setSuffix(QStringLiteral(" pt"));
        setMinimumWidth(92);
    }

protected:
    QString textFromValue(double value) const override
    {
        const double factor = std::pow(10.0, decimals());
        const double rounded = std::round(value * factor) / factor;
        if (std::fabs(rounded - std::round(rounded)) < 1e-6)
            return QString::number(static_cast<int>(std::lround(rounded)));
        return QString::number(rounded, 'f', decimals());
    }

    double valueFromText(const QString &text) const override
    {
        QString t = text;
        if (!prefix().isEmpty() && t.startsWith(prefix()))
            t.remove(0, prefix().size());
        if (!suffix().isEmpty() && t.endsWith(suffix()))
            t.chop(suffix().size());
        t = t.trimmed();
        bool ok = false;
        double v = QLocale().toDouble(t, &ok);
        if (!ok)
            v = QLocale::c().toDouble(t, &ok);
        if (!ok)
            v = t.toDouble(&ok);
        return ok ? v : this->value();
    }
};

} // namespace PlotStyleLimits

#endif
