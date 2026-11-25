#include "OleServer.h"
#include "MultiLayer.h"
/***************************************************************************
    File                 : Graph_Axes.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph axes, titles, tick formatting, scale engines, axis breaks, and linked layers

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

#include <QVarLengthArray>

#include "Graph.h"
#include "ExtraPlotTypes.h"
#include "Grid.h"
#include "CanvasPicker.h"
#include "CanvasInlineTextEditor.h"
#include "QwtErrorPlotCurve.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ScalePicker.h"
#include "TitlePicker.h"
#include "QwtPieCurve.h"
#include "ImageMarker.h"
#include "QwtBarCurve.h"
#include "BoxCurve.h"
#include "QwtHistogram.h"
#include "VectorCurve.h"
#include "ScaleDraw.h"
#include "ColorButton.h"
#include "PatternBox.h"
#include "SymbolBox.h"
#include "FunctionCurve.h"
#include "Spectrogram.h"
#include "SelectionMoveResizer.h"
#include "DataPickerTool.h"
#include "RangeSelectorTool.h"
#include "PlotCurve.h"
#include "ApplicationWindow.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "core/Project.h"
#include "core/column/Column.h"
#include "MultiLayer.h"
#include "EmfPaintDevice.h"
#include "MyParser.h"
#include <algorithm>
#include <limits>
#include <mutex>

#include <QApplication>
#include <QFontMetricsF>
#include <QBitmap>
#include <QClipboard>
#include <QCursor>
#include <QFile>
#include <QImage>
#include <QMessageBox>
#include <QPainterPath>
#include <QPixmap>
#include <QPainter>
#include <QMenu>
#include <QTextStream>
#include <QLocale>
#include <QPrintDialog>
#include <QImageWriter>
#include <QFileInfo>
#include <QRegExp>
#include <QRegularExpression>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QMainWindow>
#include <QStatusBar>
#include <QTextDocument>
#include <QUndoCommand>
#include <QUndoStack>
#include <utility>

#if QT_VERSION >= 0x040300
#include <QSvgGenerator>
#endif

#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include <qwt_color_map.h>

#include <cmath>
#include <stdexcept>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
using namespace std;

#include "Graph_Internal.h"
using namespace GraphInternal;

namespace {

static QString formatScientificMarkupTokens(const QString &input)
{
    const QString clean = unwrapNoWrapAxisTitle(input);
    if (clean.trimmed().isEmpty()
        || (!clean.contains(QLatin1Char('^')) && !clean.contains(QLatin1Char('_')))) {
        return clean;
    }

    static const QRegularExpression kHtmlTagSplitRx(QStringLiteral("(<[^>]+>)"));
    static const QRegularExpression kBracedSupRx(QStringLiteral("\\^\\{([^}]+)\\}"));
    static const QRegularExpression kBracedSubRx(QStringLiteral("_\\{([^}]+)\\}"));
    static const QRegularExpression kPlainSupRx(QStringLiteral(
            "([A-Za-z0-9\\)\\]\\x{0370}-\\x{03FF}\\x{00B0}\\x{00C5}])\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"));
    static const QRegularExpression kLog10SubRx(QStringLiteral("\\blog_10\\b"));
    static const QRegularExpression kSingleSymSubRx(QStringLiteral(
            "(?<![A-Za-z0-9\\x{0370}-\\x{03FF}])([A-Za-z\\x{0370}-\\x{03FF}])_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"));

    QString out;
    out.reserve(clean.size() + 24);
    int subDepth = 0;
    int supDepth = 0;
    int pos = 0;
    auto it = kHtmlTagSplitRx.globalMatch(clean);
    auto transformChunk = [&](QString chunk) -> QString {
        if (subDepth > 0 || supDepth > 0 || chunk.isEmpty())
            return chunk;
        chunk.replace(kBracedSupRx, QStringLiteral("<sup>\\1</sup>"));
        chunk.replace(kBracedSubRx, QStringLiteral("<sub>\\1</sub>"));
        chunk.replace(kPlainSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
        chunk.replace(kLog10SubRx, QStringLiteral("log<sub>10</sub>"));
        chunk.replace(kSingleSymSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
        return chunk;
    };

    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedStart() > pos)
            out += transformChunk(clean.mid(pos, m.capturedStart() - pos));
        const QString tag = m.captured(1);
        const QString tagLower = tag.toLower();
        if (tagLower.startsWith(QLatin1String("<sub")) && !tagLower.endsWith(QLatin1String("/>")))
            ++subDepth;
        else if (tagLower.startsWith(QLatin1String("</sub")) && subDepth > 0)
            --subDepth;
        else if (tagLower.startsWith(QLatin1String("<sup"))
                 && !tagLower.endsWith(QLatin1String("/>")))
            ++supDepth;
        else if (tagLower.startsWith(QLatin1String("</sup")) && supDepth > 0)
            --supDepth;
        out += tag;
        pos = m.capturedEnd();
    }
    if (pos < clean.size())
        out += transformChunk(clean.mid(pos));
    static const QRegularExpression kPostTagSubRx(
            QStringLiteral("(</\\s*i\\s*>)_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"),
            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kPostTagSupRx(
            QStringLiteral("(</\\s*(?:i|sub)\\s*>)\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"),
            QRegularExpression::CaseInsensitiveOption);
    out.replace(kPostTagSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
    out.replace(kPostTagSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
    return out;
}

static QString wrapRichAxisTitleNoWrap(const QString &s)
{
    const QString clean = unwrapNoWrapAxisTitle(s);
    if (clean.trimmed().isEmpty())
        return clean;
    if (clean.contains(QLatin1Char('\n'))
        || clean.contains(QLatin1String("<br"), Qt::CaseInsensitive))
        return clean;
    static const QRegularExpression kRichTagRx(
            QStringLiteral("<\\s*(?:i|sub|sup|b|u|span|font)\\b"),
            QRegularExpression::CaseInsensitiveOption);
    if (clean.contains(kRichTagRx)) {
        return QStringLiteral("<span style=\"white-space:nowrap;\">") + clean
                + QStringLiteral("</span>");
    }
    return clean;
}

class PiecewiseBreakScaleTransformation : public QwtScaleTransformation
{
public:
    explicit PiecewiseBreakScaleTransformation(const Graph::AxisBreakSpec &spec)
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_spec(spec)
    {
    }

    PiecewiseBreakScaleTransformation(const PiecewiseBreakScaleTransformation &other)
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_spec(other.d_spec)
    {
    }

    QwtScaleTransformation *copy() const override
    {
        return new PiecewiseBreakScaleTransformation(*this);
    }

    double xForm(double s, double s1, double s2, double p1, double p2) const override
    {
        if (!std::isfinite(s))
            return std::numeric_limits<double>::quiet_NaN();
        const double lo = qMin(s1, s2);
        const double hi = qMax(s1, s2);
        if (hi - lo <= 1e-300)
            return p1;

        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        double u = 0.0;
        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15)) {
            u = (s - lo) / (hi - lo);
        } else if (s <= bf) {
            if (d_spec.log10Before && lo > 0.0 && bf > lo) {
                const double sc = qMax(s, 1e-300);
                u = pos * (std::log10(sc) - std::log10(lo)) / (std::log10(bf) - std::log10(lo));
            } else {
                u = pos * (s - lo) / (bf - lo);
            }
        } else if (s >= bt) {
            if (d_spec.log10After && bt > 0.0 && hi > bt) {
                const double sc = qMax(s, 1e-300);
                u = pos
                        + (1.0 - pos) * (std::log10(sc) - std::log10(bt))
                                / (std::log10(hi) - std::log10(bt));
            } else {
                u = pos + (1.0 - pos) * (s - bt) / (hi - bt);
            }
        } else {
            u = pos;
        }

        const double t = (s1 <= s2) ? u : (1.0 - u);
        return p1 + t * (p2 - p1);
    }

    double invXForm(double p, double p1, double p2, double s1, double s2) const override
    {
        if (!std::isfinite(p))
            return std::numeric_limits<double>::quiet_NaN();
        if (qAbs(p2 - p1) <= 1e-15)
            return s1;

        const double lo = qMin(s1, s2);
        const double hi = qMax(s1, s2);
        const double t = (p - p1) / (p2 - p1);
        const double u = (s1 <= s2) ? t : (1.0 - t);

        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15))
            return lo + u * (hi - lo);

        if (u <= pos) {
            const double f = (pos > 1e-12) ? (u / pos) : 0.0;
            if (d_spec.log10Before && lo > 0.0 && bf > lo)
                return std::pow(10.0, std::log10(lo) + f * (std::log10(bf) - std::log10(lo)));
            return lo + f * (bf - lo);
        }

        const double f = (1.0 - pos > 1e-12) ? ((u - pos) / (1.0 - pos)) : 0.0;
        if (d_spec.log10After && bt > 0.0 && hi > bt)
            return std::pow(10.0, std::log10(bt) + f * (std::log10(hi) - std::log10(bt)));
        return bt + f * (hi - bt);
    }

private:
    Graph::AxisBreakSpec d_spec;
};

class FormulaLinkedScaleTransformation : public QwtScaleTransformation
{
public:
    // xp/uc: monotone lookup table parent x -> child u. parentLo/parentHi: the parent axis range
    // the child axis really spans. They differ from the table ends when the table had to start
    // at a safe x (e.g. 1e7/x with a parent range starting at 0); positions are then still
    // measured against the real parent range.
    FormulaLinkedScaleTransformation(const QVector<double> &xp, const QVector<double> &uc,
                                     double parentLo = std::numeric_limits<double>::quiet_NaN(),
                                     double parentHi = std::numeric_limits<double>::quiet_NaN())
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_xp(xp), d_uc(uc)
    {
        d_parentLo = std::isfinite(parentLo) ? parentLo : (xp.isEmpty() ? 0.0 : xp.first());
        d_parentHi = std::isfinite(parentHi) ? parentHi : (xp.isEmpty() ? 0.0 : xp.last());
    }

    FormulaLinkedScaleTransformation(const FormulaLinkedScaleTransformation &other)
        : QwtScaleTransformation(QwtScaleTransformation::Other),
          d_xp(other.d_xp),
          d_uc(other.d_uc),
          d_parentLo(other.d_parentLo),
          d_parentHi(other.d_parentHi)
    {
    }

    //! Parent x for a child scale bound s. The child scale bounds are the table ends; the table
    //! start may be a substitute (safe) x, so map those ends onto the real parent range.
    double parentXAtScaleBound(double s) const
    {
        const double xp = invertUToParentX(s);
        if (d_xp.size() < 2)
            return xp;
        const double tol = 1e-9 * qMax(std::fabs(d_xp.last() - d_xp.first()), 1e-300);
        if (std::fabs(xp - d_xp.first()) <= tol)
            return d_parentLo;
        if (std::fabs(xp - d_xp.last()) <= tol)
            return d_parentHi;
        return xp;
    }

    QwtScaleTransformation *copy() const override
    {
        return new FormulaLinkedScaleTransformation(*this);
    }

    double invertUToParentX(double u) const
    {
        const int n = qMin(d_xp.size(), d_uc.size());
        if (n < 2)
            return u;

        const bool inc = (d_uc.last() >= d_uc.first());
        if (inc) {
            if (u <= d_uc.first()) {
                const double du = d_uc[1] - d_uc[0];
                const double dx = d_xp[1] - d_xp[0];
                return (qAbs(du) > 1e-300) ? (d_xp[0] + (u - d_uc[0]) * (dx / du)) : d_xp[0];
            }
            if (u >= d_uc.last()) {
                const double du = d_uc[n - 1] - d_uc[n - 2];
                const double dx = d_xp[n - 1] - d_xp[n - 2];
                return (qAbs(du) > 1e-300)
                        ? (d_xp[n - 1] + (u - d_uc[n - 1]) * (dx / du))
                        : d_xp[n - 1];
            }
            int lo = 0;
            int hi = n - 1;
            while (hi - lo > 1) {
                const int mid = (lo + hi) / 2;
                if (d_uc[mid] <= u)
                    lo = mid;
                else
                    hi = mid;
            }
            const double du = d_uc[hi] - d_uc[lo];
            const double frac = (qAbs(du) > 1e-300) ? ((u - d_uc[lo]) / du) : 0.0;
            return d_xp[lo] + frac * (d_xp[hi] - d_xp[lo]);
        } else {
            if (u >= d_uc.first()) {
                const double du = d_uc[1] - d_uc[0];
                const double dx = d_xp[1] - d_xp[0];
                return (qAbs(du) > 1e-300) ? (d_xp[0] + (u - d_uc[0]) * (dx / du)) : d_xp[0];
            }
            if (u <= d_uc.last()) {
                const double du = d_uc[n - 1] - d_uc[n - 2];
                const double dx = d_xp[n - 1] - d_xp[n - 2];
                return (qAbs(du) > 1e-300)
                        ? (d_xp[n - 1] + (u - d_uc[n - 1]) * (dx / du))
                        : d_xp[n - 1];
            }
            int lo = 0;
            int hi = n - 1;
            while (hi - lo > 1) {
                const int mid = (lo + hi) / 2;
                if (d_uc[mid] >= u)
                    lo = mid;
                else
                    hi = mid;
            }
            const double du = d_uc[hi] - d_uc[lo];
            const double frac = (qAbs(du) > 1e-300) ? ((u - d_uc[lo]) / du) : 0.0;
            return d_xp[lo] + frac * (d_xp[hi] - d_xp[lo]);
        }
    }

    double evalFromParentX(double xp) const
    {
        const int n = qMin(d_xp.size(), d_uc.size());
        if (n < 2)
            return xp;
        if (xp <= d_xp.first()) {
            const double dx = d_xp[1] - d_xp[0];
            const double du = d_uc[1] - d_uc[0];
            return (qAbs(dx) > 1e-300) ? (d_uc[0] + (xp - d_xp[0]) * (du / dx)) : d_uc[0];
        }
        if (xp >= d_xp.last()) {
            const double dx = d_xp[n - 1] - d_xp[n - 2];
            const double du = d_uc[n - 1] - d_uc[n - 2];
            return (qAbs(dx) > 1e-300)
                    ? (d_uc[n - 1] + (xp - d_xp[n - 1]) * (du / dx))
                    : d_uc[n - 1];
        }
        int lo = 0;
        int hi = n - 1;
        while (hi - lo > 1) {
            const int mid = (lo + hi) / 2;
            if (d_xp[mid] <= xp)
                lo = mid;
            else
                hi = mid;
        }
        const double dx = d_xp[hi] - d_xp[lo];
        const double frac = (qAbs(dx) > 1e-300) ? ((xp - d_xp[lo]) / dx) : 0.0;
        return d_uc[lo] + frac * (d_uc[hi] - d_uc[lo]);
    }

    double xForm(double u, double s1, double s2, double p1Px, double p2Px) const override
    {
        if (!std::isfinite(u))
            return std::numeric_limits<double>::quiet_NaN();
        const double xpAtS1 = parentXAtScaleBound(s1);
        const double xpAtS2 = parentXAtScaleBound(s2);
        const double denom = xpAtS2 - xpAtS1;
        if (qAbs(denom) <= 1e-300)
            return p1Px;
        const double xp = invertUToParentX(u);
        const double t = (xp - xpAtS1) / denom;
        return p1Px + t * (p2Px - p1Px);
    }

    double invXForm(double px, double p1Px, double p2Px, double s1, double s2) const override
    {
        if (!std::isfinite(px))
            return std::numeric_limits<double>::quiet_NaN();
        if (qAbs(p2Px - p1Px) <= 1e-15)
            return s1;
        const double xpAtS1 = parentXAtScaleBound(s1);
        const double xpAtS2 = parentXAtScaleBound(s2);
        const double t = (px - p1Px) / (p2Px - p1Px);
        const double xp = xpAtS1 + t * (xpAtS2 - xpAtS1);
        return evalFromParentX(xp);
    }

private:
    QVector<double> d_xp;
    QVector<double> d_uc;
    double d_parentLo = 0.0;
    double d_parentHi = 0.0;
};

class FormulaLinkedScaleEngine : public QwtLinearScaleEngine
{
public:
    FormulaLinkedScaleEngine(const QVector<double> &xp, const QVector<double> &uc,
                             double parentLo = std::numeric_limits<double>::quiet_NaN(),
                             double parentHi = std::numeric_limits<double>::quiet_NaN())
        : d_xp(xp), d_uc(uc), d_parentLo(parentLo), d_parentHi(parentHi)
    {
    }

    QwtScaleTransformation *transformation() const override
    {
        return new FormulaLinkedScaleTransformation(d_xp, d_uc, d_parentLo, d_parentHi);
    }

    QwtScaleDiv divideScale(double x1, double x2, int maxMajSteps, int maxMinSteps,
                            double stepSize = 0.0) const override
    {
        Q_UNUSED(maxMinSteps);
        const double lo = qMin(x1, x2);
        const double hi = qMax(x1, x2);
        const double span = hi - lo;
        const int maxMaj = qBound(2, maxMajSteps, 6);

        if (span > 1e-30 && stepSize <= 0.0 && d_xp.size() >= 2 && d_uc.size() >= 2) {
            const FormulaLinkedScaleTransformation tf(d_xp, d_uc, d_parentLo, d_parentHi);
            const double xp0 = tf.parentXAtScaleBound(lo);
            const double xp1 = tf.parentXAtScaleBound(hi);
            const double xpSpan = xp1 - xp0;
            if (std::isfinite(xpSpan) && std::fabs(xpSpan) > 1e-30) {
                double minDx = 1e300;
                double maxDx = 0.0;
                constexpr int kProbes = 16;
                for (int m = 0; m < kProbes; ++m) {
                    const double ua = lo + span * (double(m) / double(kProbes));
                    const double ub = lo + span * (double(m + 1) / double(kProbes));
                    const double dx = std::fabs(tf.invertUToParentX(ub) - tf.invertUToParentX(ua));
                    if (dx > 1e-30) {
                        minDx = qMin(minDx, dx);
                        maxDx = qMax(maxDx, dx);
                    }
                }

                if (minDx < 1e290 && maxDx > 1.5 * minDx) {
                    auto normT = [&](double u) -> double {
                        return (tf.invertUToParentX(u) - xp0) / xpSpan;
                    };

                    struct RoundCand {
                        double u;
                        double t;
                        int rank;
                    };
                    QVector<RoundCand> cands;
                    const double eps = 1e-11 * span;

                    auto addCand = [&](double val, int baseRank) {
                        if (!std::isfinite(val) || val < lo - eps || val > hi + eps)
                            return;
                        const double u = qBound(lo, val, hi);
                        const double t = normT(u);
                        if (!std::isfinite(t))
                            return;
                        const bool isInterior = (t >= 0.03 && t <= 0.97);
                        const bool isExactCorner = (baseRank <= 2)
                                && (t <= 1e-3 || t >= 1.0 - 1e-3) && (t >= -1e-4 && t <= 1.0 + 1e-4);
                        if (!isInterior && !isExactCorner)
                            return;
                        const int effRank = isInterior ? baseRank : (baseRank + 2);
                        for (int k = 0; k < cands.size(); ++k) {
                            if (std::fabs(cands[k].u - u) <= 1e-9 * qMax(1.0, std::fabs(u))) {
                                if (effRank < cands[k].rank)
                                    cands[k].rank = effRank;
                                return;
                            }
                        }
                        cands.append({ u, qBound(0.0, t, 1.0), effRank });
                    };

                    if (lo <= 0.0 && hi >= 0.0)
                        addCand(0.0, 0);

                    struct MantRank {
                        double m;
                        int r;
                    };
                    static const MantRank kMantissas[] = {
                        { 1.0, 0 },  { 5.0, 1 },  { 2.0, 2 },  { 3.0, 4 },  { 2.5, 5 },
                        { 4.0, 6 },  { 8.0, 7 },  { 6.0, 8 },  { 7.0, 9 },  { 1.5, 10 },
                        { 9.0, 11 }, { 1.2, 13 }, { 1.4, 14 }, { 1.6, 14 }, { 1.8, 14 },
                        { 3.5, 15 }, { 4.5, 15 }, { 5.5, 15 }, { 6.5, 15 }, { 7.5, 15 },
                        { 8.5, 15 }
                    };

                    auto populateSign = [&](double sgn, double absLo, double absHi) {
                        if (!(absHi > 0.0))
                            return;
                        const double safeLo = qMax(1e-24, absLo > 0.0 ? absLo : 1e-6 * absHi);
                        const int kMin = int(std::floor(std::log10(safeLo))) - 1;
                        const int kMax = int(std::ceil(std::log10(absHi))) + 1;
                        for (int k = qMax(-24, kMin); k <= qMin(24, kMax); ++k) {
                            const double p10 = std::pow(10.0, double(k));
                            for (const MantRank &mr : kMantissas)
                                addCand(sgn * mr.m * p10, mr.r);
                        }
                    };

                    if (hi > 0.0)
                        populateSign(1.0, qMax(0.0, lo), hi);
                    if (lo < 0.0)
                        populateSign(-1.0, qMax(0.0, -hi), -lo);

                    std::sort(cands.begin(), cands.end(),
                              [](const RoundCand &a, const RoundCand &b) {
                                  if (a.rank != b.rank)
                                      return a.rank < b.rank;
                                  return std::fabs(a.t - 0.5) < std::fabs(b.t - 0.5);
                              });

                    const int targetMaj = qBound(4, maxMajSteps, 6);
                    auto selectWithGap = [&](double minGap) -> QVector<RoundCand> {
                        QVector<RoundCand> picked;
                        for (const RoundCand &c : cands) {
                            if (picked.size() >= targetMaj)
                                break;
                            bool tooClose = false;
                            for (const RoundCand &p : picked) {
                                if (std::fabs(c.t - p.t) < minGap) {
                                    tooClose = true;
                                    break;
                                }
                            }
                            if (!tooClose)
                                picked.append(c);
                        }
                        return picked;
                    };

                    QVector<RoundCand> chosen = selectWithGap(0.14);
                    if (chosen.size() < 3)
                        chosen = selectWithGap(0.11);

                    if (chosen.size() >= 2) {
                        std::sort(chosen.begin(), chosen.end(),
                                  [](const RoundCand &a, const RoundCand &b) { return a.u < b.u; });

                        QwtValueList majList;
                        QVector<double> occupiedT;
                        for (const RoundCand &c : chosen) {
                            majList.append(c.u);
                            occupiedT.append(c.t);
                        }

                        QwtValueList minList;
                        auto tryAddMinor = [&](double mv) {
                            if (!std::isfinite(mv) || mv <= lo + eps || mv >= hi - eps)
                                return;
                            const double mt = normT(mv);
                            if (!std::isfinite(mt) || mt < 0.01 || mt > 0.99)
                                return;
                            for (double ot : occupiedT) {
                                if (std::fabs(mt - ot) < 0.032)
                                    return;
                            }
                            minList.append(mv);
                            occupiedT.append(mt);
                        };

                        QVector<double> knots;
                        knots.append(lo);
                        for (const RoundCand &c : chosen) {
                            if (c.u > knots.last() + eps)
                                knots.append(c.u);
                        }
                        if (hi > knots.last() + eps)
                            knots.append(hi);

                        for (int k = 0; k + 1 < knots.size(); ++k) {
                            const double a = knots[k];
                            const double b = knots[k + 1];
                            const double du = b - a;
                            if (!(du > 1e-30))
                                continue;
                            const double p10 =
                                    std::pow(10.0, std::floor(std::log10(du) + 1e-9));
                            const double ratio = du / p10;
                            int subDivs = 2;
                            if (std::fabs(ratio - 5.0) < 0.05 || std::fabs(ratio - 1.0) < 0.05)
                                subDivs = 5;
                            else if (std::fabs(ratio - 3.0) < 0.05)
                                subDivs = 3;
                            else if (std::fabs(ratio - 4.0) < 0.05)
                                subDivs = 4;
                            for (int s = 1; s < subDivs; ++s)
                                tryAddMinor(a + du * (double(s) / double(subDivs)));
                        }

                        QwtValueList ticks[QwtScaleDiv::NTickTypes];
                        ticks[QwtScaleDiv::MajorTick] = majList;
                        ticks[QwtScaleDiv::MinorTick] = minList;
                        QwtScaleDiv nlDiv(lo, hi, ticks);
                        if (x1 > x2 || testAttribute(QwtScaleEngine::Inverted)) {
                            if (nlDiv.lowerBound() < nlDiv.upperBound())
                                nlDiv.invert();
                        }
                        return nlDiv;
                    }
                }
            }
        }

        const int linMaxMaj = qMin(maxMaj, 5);
        QwtScaleDiv div = QwtLinearScaleEngine::divideScale(lo, hi, linMaxMaj, 2, stepSize);
        ensureLinearHalfStepMinorTicks(div);
        if (x1 > x2 || testAttribute(QwtScaleEngine::Inverted)) {
            if (div.lowerBound() < div.upperBound())
                div.invert();
        }
        return div;
    }

private:
    QVector<double> d_xp;
    QVector<double> d_uc;
    double d_parentLo;
    double d_parentHi;
};

} // namespace

namespace GraphInternal {

PiecewiseBreakScaleEngine::PiecewiseBreakScaleEngine(const Graph::AxisBreakSpec &spec)
    : d_spec(spec)
{
}

QwtScaleTransformation *PiecewiseBreakScaleEngine::transformation() const
{
        return new PiecewiseBreakScaleTransformation(d_spec);
}

QwtScaleDiv PiecewiseBreakScaleEngine::divideScale(double x1, double x2, int maxMajSteps, int maxMinSteps,
                                                   double stepSize) const
{
        const double lo = qMin(x1, x2);
        const double hi = qMax(x1, x2);
        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15))
            return QwtLinearScaleEngine::divideScale(x1, x2, maxMajSteps, maxMinSteps, stepSize);

        const int maj1 = qMax(1, qRound(qMax(2, maxMajSteps) * pos));
        const int maj2 = qMax(1, qMax(2, maxMajSteps) - maj1);

        // Per-side step guard: a tiny user step (e.g. 0.001 over 0..100) would create tens of
        // thousands of major ticks and freeze the O(n^2) label layout. Above 2000 ticks per side
        // the step falls back to automatic (0). Log sides count decades.
        auto boundedSideStep = [](double s, double a, double b, bool isLog) -> double {
            if (!std::isfinite(s) || !(s > 0.0))
                return 0.0;
            const double span = isLog ? std::log10(b / a) : (b - a);
            if (!std::isfinite(span) || std::fabs(span) / s > 2000.0)
                return 0.0;
            return s;
        };

        QwtScaleDiv div1;
        const bool logBefore = d_spec.log10Before && lo > 0.0 && bf > lo;
        if (logBefore) {
            const double logSpan1 = std::log10(bf / lo);
            const int majBefore = qMax(maj1, int(std::ceil(logSpan1)) + 2);
            const int minBefore = (maxMinSteps >= 1) ? qMax(9, maxMinSteps) : 0;
            QwtLog10ScaleEngine le1;
            div1 = le1.divideScale(lo, bf, majBefore, minBefore,
                                   boundedSideStep(d_spec.stepBefore, lo, bf, true));
        } else {
            QwtLinearScaleEngine le1;
            div1 = le1.divideScale(
                    lo, bf, maj1, maxMinSteps,
                    boundedSideStep(d_spec.stepBefore > 0.0 ? d_spec.stepBefore : stepSize, lo,
                                    bf, false));
            if (maxMinSteps == 2)
                ensureLinearHalfStepMinorTicks(div1);
        }

        QwtScaleDiv div2;
        const bool logAfter = d_spec.log10After && bt > 0.0 && hi > bt;
        if (logAfter) {
            const double logSpan2 = std::log10(hi / bt);
            const int majAfter = qMax(maj2, int(std::ceil(logSpan2)) + 2);
            const int minAfter = (maxMinSteps >= 1) ? qMax(9, maxMinSteps) : 0;
            QwtLog10ScaleEngine le2;
            div2 = le2.divideScale(bt, hi, majAfter, minAfter,
                                   boundedSideStep(d_spec.stepAfter, bt, hi, true));
        } else {
            QwtLinearScaleEngine le2;
            div2 = le2.divideScale(
                    bt, hi, maj2, maxMinSteps,
                    boundedSideStep(d_spec.stepAfter > 0.0 ? d_spec.stepAfter : stepSize, bt, hi,
                                    false));
            if (maxMinSteps == 2)
                ensureLinearHalfStepMinorTicks(div2);
        }

        const double maxAllowedBefore = logBefore
                ? (bf * std::pow(10.0, -0.03 * std::log10(bf / lo)))
                : (bf - 0.04 * (bf - lo));
        const double minAllowedAfter = logAfter
                ? (bt * std::pow(10.0, 0.03 * std::log10(hi / bt)))
                : (bt + 0.04 * (hi - bt));
        const double loEps = logBefore ? (lo * (1.0 - 1e-11)) : (lo - 1e-12 * (bf - lo));
        const double hiEps = logAfter ? (hi * (1.0 + 1e-11)) : (hi + 1e-12 * (hi - bt));

        QwtValueList mergedTicks[QwtScaleDiv::NTickTypes];
        for (int tt = 0; tt < QwtScaleDiv::NTickTypes; ++tt) {
            const QwtScaleDiv::TickType tickType = static_cast<QwtScaleDiv::TickType>(tt);
            for (double v : div1.ticks(tickType)) {
                if (v >= loEps && v <= maxAllowedBefore)
                    mergedTicks[tt].append(v);
            }
            for (double v : div2.ticks(tickType)) {
                if (v >= minAllowedAfter && v <= hiEps)
                    mergedTicks[tt].append(v);
            }
        }

        QwtScaleDiv out(lo, hi, mergedTicks);
        if (x1 > x2)
            out.invert();
        return out;
}

} // namespace GraphInternal

void Graph::initFonts(const QFont &scaleTitleFnt, const QFont &numbersFnt)
{
    QFont tf = scaleTitleFnt;
    tf.setHintingPreference(QFont::PreferNoHinting);
    QFont nf = numbersFnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_plot->setAxisFont(i, nf);
        QwtText t = d_plot->axisTitle(i);
        t.setFont(tf);
        d_plot->setAxisTitle(i, t);
    }
}

void Graph::setAxisFont(int axis, const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    d_plot->setAxisFont(axis, nf);
    d_plot->replot();
    emit modifiedGraph();
}

QFont Graph::axisFont(int axis)
{
    return d_plot->axisFont(axis);
}

void Graph::enableAxis(int axis, bool on)
{
    d_plot->enableAxis(axis, on);
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale && !scale->isColorBarEnabled() && !hasColorScaleOnAxis(axis))
        scale->setMargin(0);

    scalePicker->refresh();
}

void Graph::enableAxes(const QStringList &list)
{
    int i;
    for (i = 0; i < QwtPlot::axisCnt; i++) {
        bool ok = list[i + 1].toInt();
        d_plot->enableAxis(i, ok);
    }

    for (i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale && !scale->isColorBarEnabled() && !hasColorScaleOnAxis(i))
            scale->setMargin(0);
    }
    scalePicker->refresh();
}

void Graph::enableAxes(QVector<bool> axesOn)
{
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        d_plot->enableAxis(i, axesOn[i]);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale && !scale->isColorBarEnabled() && !hasColorScaleOnAxis(i))
            scale->setMargin(0);
    }
    scalePicker->refresh();
}

QVector<bool> Graph::enabledAxes()
{
    QVector<bool> axesOn(4);
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        axesOn[i] = d_plot->axisEnabled(i);
    return axesOn;
}

namespace {
void applyAxisBaselineDist(QwtScaleWidget *scale, int dist)
{
    if (!scale)
        return;
    if (dist < 0) {
        scale->setProperty("_toriNegBaseline", dist);
        scale->setMargin(0);
    } else {
        scale->setProperty("_toriNegBaseline", 0);
        scale->setMargin(dist);
    }
}
} // namespace

QList<int> Graph::axesBaseline()
{
    QList<int> baselineDist;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        baselineDist << readAxisBaselineDist(scale);
    }
    return baselineDist;
}

void Graph::setAxesBaseline(const QList<int> &lst)
{
    for (int i = 0; i < QwtPlot::axisCnt && i < lst.size(); i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        applyAxisBaselineDist(scale, lst[i]);
    }
    d_plot->updateLayout();
}

void Graph::setAxesBaseline(QStringList &lst)
{
    lst.removeAll(lst.first());
    for (int i = 0; i < QwtPlot::axisCnt && i < lst.size(); i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        applyAxisBaselineDist(scale, (lst[i]).toInt());
    }
    d_plot->updateLayout();
}

QList<Graph::AxisType> Graph::axesType()
{
    return axisType;
}

void Graph::setLabelsNumericFormat(int axis, int format, int prec, const QString &formula)
{
    axisType[axis] = AxisType::Numeric;
    axesFormulas[axis] = formula;

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    if (format == Plot::Superscripts) {
        QwtSupersciptsScaleDraw *sd = new QwtSupersciptsScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                formula.toUtf8().constData());
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setLabelFormat('s', prec);
        sd->setScaleDiv(div);
        d_plot->setAxisScaleDraw(axis, sd);
    } else {
        ScaleDraw *sd = new ScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                      formula.toUtf8().constData());
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setScaleDiv(div);

        if (format == Plot::Automatic)
            sd->setLabelFormat('g', prec);
        else if (format == Plot::Scientific)
            sd->setLabelFormat('e', prec);
        else if (format == Plot::Decimal)
            sd->setLabelFormat('f', prec);

        d_plot->setAxisScaleDraw(axis, sd);
    }
}

void Graph::setLabelsNumericFormat(int axis, const QStringList &l)
{
    QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
    if (!sd->hasComponent(QwtAbstractScaleDraw::Labels) || axisType[axis] != AxisType::Numeric)
        return;

    int format = l[2 * axis].toInt();
    int prec = l[2 * axis + 1].toInt();
    setLabelsNumericFormat(axis, format, prec, axesFormulas[axis]);
}

void Graph::setLabelsNumericFormat(const QStringList &l)
{
    for (int axis = 0; axis < 4; axis++)
        setLabelsNumericFormat(axis, l);
}

QStringList Graph::enabledTickLabels()
{
    QStringList lst;
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        const QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
        lst << QString::number(sd->hasComponent(QwtAbstractScaleDraw::Labels));
    }
    return lst;
}

void Graph::setEnabledTickLabels(const QStringList &labelsOn)
{
    for (int axis = 0; axis < QwtPlot::axisCnt && axis < labelsOn.size(); axis++) {
        QwtScaleWidget *sc = d_plot->axisWidget(axis);
        if (sc) {
            QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
            if (sd)
                sd->enableComponent(QwtAbstractScaleDraw::Labels, labelsOn[axis] == "1");
        }
    }
}

void Graph::setTickLabelsOnAxis(int axis, bool on)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;
    if (QwtScaleDraw *sd = d_plot->axisScaleDraw(axis))
        sd->enableComponent(QwtAbstractScaleDraw::Labels, on);
    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->update();
    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::hasTickLabelsOnAxis(int axis) const
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return false;
    const QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
    return sd ? sd->hasComponent(QwtAbstractScaleDraw::Labels) : false;
}

void Graph::setMajorTicksType(const QList<int> &lst)
{
    if (d_plot->getMajorTicksType() == lst)
        return;

    for (int i = 0; i < (int)lst.count(); i++) {
        ScaleDraw *sd = (ScaleDraw *)d_plot->axisScaleDraw(i);
        if (lst[i] == ScaleDraw::None || lst[i] == ScaleDraw::In)
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
        else {
            sd->enableComponent(QwtAbstractScaleDraw::Ticks);
            sd->setTickLength(QwtScaleDiv::MinorTick, d_plot->minorTickLength());
            sd->setTickLength(QwtScaleDiv::MediumTick, d_plot->minorTickLength());
            sd->setTickLength(QwtScaleDiv::MajorTick, d_plot->majorTickLength());
        }
        sd->setMajorTicksStyle((ScaleDraw::TicksStyle)lst[i]);
    }
}

void Graph::setMajorTicksType(const QStringList &lst)
{
    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMajorTicksType(i, lst[i].toInt());
}

void Graph::setMinorTicksType(const QList<int> &lst)
{
    if (d_plot->getMinorTicksType() == lst)
        return;

    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMinorTicksType(i, lst[i]);
}

void Graph::setMinorTicksType(const QStringList &lst)
{
    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMinorTicksType(i, lst[i].toInt());
}

int Graph::minorTickLength()
{
    return d_plot->minorTickLength();
}

int Graph::majorTickLength()
{
    return d_plot->majorTickLength();
}

void Graph::setAxisTicksLength(int axis, int majTicksType, int minTicksType, int minLength,
                               int majLength)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (!scale)
        return;

    d_plot->setTickLength(minLength, majLength);

    ScaleDraw *sd = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    sd->setMajorTicksStyle((ScaleDraw::TicksStyle)majTicksType);
    sd->setMinorTicksStyle((ScaleDraw::TicksStyle)minTicksType);

    if (majTicksType == ScaleDraw::None && minTicksType == ScaleDraw::None)
        sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
    else if (!sd->hasComponent(QwtAbstractScaleDraw::Labels)
             && (majTicksType == ScaleDraw::None || majTicksType == ScaleDraw::In)
             && (minTicksType == ScaleDraw::None || minTicksType == ScaleDraw::In))
        sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
    else
        sd->enableComponent(QwtAbstractScaleDraw::Ticks);

    if (majTicksType == ScaleDraw::None || majTicksType == ScaleDraw::In)
        majLength = minLength;
    if (minTicksType == ScaleDraw::None)
        minLength = 0;

    sd->setTickLength(QwtScaleDiv::MinorTick, minLength);
    sd->setTickLength(QwtScaleDiv::MediumTick, minLength);
    sd->setTickLength(QwtScaleDiv::MajorTick, majLength);
}

void Graph::setTicksLength(int minLength, int majLength)
{
    QList<int> majTicksType = d_plot->getMajorTicksType();
    QList<int> minTicksType = d_plot->getMinorTicksType();

    for (int i = 0; i < 4; i++)
        setAxisTicksLength(i, majTicksType[i], minTicksType[i], minLength, majLength);
}

void Graph::changeTicksLength(int minLength, int majLength)
{
    if (d_plot->minorTickLength() == minLength && d_plot->majorTickLength() == majLength)
        return;

    setTicksLength(minLength, majLength);

    d_plot->hide();
    for (int i = 0; i < 4; i++) {
        if (d_plot->axisEnabled(i)) {
            d_plot->enableAxis(i, false);
            d_plot->enableAxis(i, true);
        }
    }
    d_plot->replot();
    d_plot->show();

    emit modifiedGraph();
}

void Graph::showAxis(int axis, AxisType type, const QString &formatInfo, Table *table, bool axisOn,
                     int majTicksType, int minTicksType, bool labelsOn, const QColor &c, int format,
                     int prec, int rotation, int baselineDist, const QString &formula,
                     const QColor &labelsColor)
{
    d_plot->enableAxis(axis, axisOn);
    if (!axisOn)
        return;

    QList<int> majTicksTypeList = d_plot->getMajorTicksType();
    QList<int> minTicksTypeList = d_plot->getMinorTicksType();

    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);

    if (d_plot->axisEnabled(axis) == axisOn && majTicksTypeList[axis] == majTicksType
        && minTicksTypeList[axis] == minTicksType && axesColors()[axis] == COLORNAME(c)
        && axesNumColors()[axis] == COLORNAME(labelsColor)
        && prec == d_plot->axisLabelPrecision(axis) && format == d_plot->axisLabelFormat(axis)
        && labelsRotation(axis) == rotation && axisType[axis] == type
        && axesFormatInfo[axis] == formatInfo && axesFormulas[axis] == formula
        && readAxisBaselineDist(scale) == baselineDist
        && sclDraw->hasComponent(QwtAbstractScaleDraw::Labels) == labelsOn)
        return;

    applyAxisBaselineDist(scale, baselineDist);
    QPalette pal = scale->palette();
    if (pal.color(QPalette::Active, QPalette::WindowText) != c)
        pal.setColor(QPalette::WindowText, c);
    if (pal.color(QPalette::Active, QPalette::Text) != labelsColor)
        pal.setColor(QPalette::Text, labelsColor);
    scale->setPalette(pal);

    if (!labelsOn)
        sclDraw->enableComponent(QwtAbstractScaleDraw::Labels, false);
    else {
        switch (type) {
        case AxisType::Numeric:
            setLabelsNumericFormat(axis, format, prec, formula);
            break;
        case AxisType::Day:
            setLabelsDayFormat(axis, format);
            break;
        case AxisType::Month:
            setLabelsMonthFormat(axis, format);
            break;
        case AxisType::Time:
        case AxisType::Date:
        case AxisType::DateTime:
            setLabelsDateTimeFormat(axis, type, formatInfo);
            break;
        case AxisType::Txt:
            setLabelsTextFormat(axis, table, formatInfo);
            break;
        case AxisType::ColHeader:
            setLabelsColHeaderFormat(axis, table);
            break;
        }

        setAxisLabelRotation(axis, rotation);
    }

    sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const bool backboneOn = drawAxesBackbone && (axis >= 0 && axis < 4 ? d_axis_backbone[axis] : true);
    sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);

    setAxisTicksLength(axis, majTicksType, minTicksType, d_plot->minorTickLength(),
                       d_plot->majorTickLength());

    if (axisOn && (axis == QwtPlot::xTop || axis == QwtPlot::yRight))
        updateSecondaryAxis(axis); // synchronize scale divisions

    scalePicker->refresh();
    d_plot->updateLayout(); // This is necessary in order to enable/disable tick labels
    scale->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setLabelsDayFormat(int axis, int format)
{
    axisType[axis] = AxisType::Day;
    axesFormatInfo[axis] = QString::number(format);

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    WeekDayScaleDraw *sd =
            new WeekDayScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                 (WeekDayScaleDraw::NameFormat)format);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setScaleDiv(div);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsMonthFormat(int axis, int format)
{
    axisType[axis] = AxisType::Month;
    axesFormatInfo[axis] = QString::number(format);

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    MonthScaleDraw *sd =
            new MonthScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                               (MonthScaleDraw::NameFormat)format);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setScaleDiv(div);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsTextFormat(int axis, const Column *column, int startRow, int endRow)
{
    // TODO: The whole text labels functionality is very limited. Specifying
    // only one column for the labels will always mean that the labels
    // correspond to 1, 2, 3, 4, etc.. Other label mappings such as
    // a -> 1.5, b -> 4.5, c -> 16.5 (with step set to 0.5) or similar
    // require to have an additional column with numeric values.
    // This should be supported in our new plotting framework.
    if (!column)
        return;
    future::Table *table = qobject_cast<future::Table *>(column->parentAspect());
    if (!table)
        return;
    if (axis < 0 || axis > 3)
        return;

    axisType[axis] = AxisType::Txt;
    axesFormatInfo[axis] = table->name() + "_" + column->name();

    QMap<int, QString> list;
    for (int row = startRow; row <= endRow; row++)
        if (!column->isInvalid(row))
            list.insert(row + 1, column->textAt(row));
    QwtTextScaleDraw *sd = new QwtTextScaleDraw(
            *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)), list);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setMinorTicksStyle(ScaleDraw::None);
    d_plot->setAxisScaleDraw(axis, sd);
    d_plot->setAxisMaxMinor(axis, 0);
    d_plot->setAxisMaxMajor(axis, qMax(8, list.size() + 2));
    d_user_step[axis] = 1.0;
}

void Graph::setLabelsTextFormat(int axis, Table *table, const QString &columnName)
{
    Column *col = 0;
    if (!table || !(col = table->column(columnName))) {
        ApplicationWindow *aw = nullptr;
        for (QObject *p = parent(); p && !aw; p = p->parent())
            aw = qobject_cast<ApplicationWindow *>(p);
        if (!aw) {
            for (QWidget *tlw : QApplication::topLevelWidgets()) {
                if ((aw = qobject_cast<ApplicationWindow *>(tlw)) != nullptr)
                    break;
            }
        }
        if (aw && aw->isNonInteractive())
            return;

        QMessageBox::critical(
                this, tr("Internal Error"),
                tr("<html>Failed to set axis labels on Graph %1. Maybe you're trying to open a "
                   "corrupted"
                   " project file; or there's some problem within SciDAVis. Please report this"
                   " as a bug (together with detailed instructions how to reproduce this message or"
                   " the corrupted file).<p>"
                   "<a href=\"https://sourceforge.net/tracker/?group_id=199120&atid=968214>\">"
                   "bug tracker: "
                   "https://sourceforge.net/tracker/?group_id=199120&atid=968214</a></html>")
                        .arg(objectName()));
        return;
    }
    setLabelsTextFormat(axis, col, 0, col->rowCount() - 1);
}

void Graph::setLabelsColHeaderFormat(int axis, Table *table)
{
    if (!table)
        return;

    axisType[axis] = AxisType::ColHeader;
    axesFormatInfo[axis] = table->name();

    QMap<int, QString> list;
    for (int col = 0; col < table->columnCount(); col++)
        if (table->colPlotDesignation(col) == SciDAVis::Y)
            list.insert(col, table->colLabel(col));
    QwtTextScaleDraw *sd = new QwtTextScaleDraw(
            *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)), list);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsDateTimeFormat(int axis, AxisType type, const QString &formatInfo)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList list = formatInfo.split(";", Qt::KeepEmptyParts);
#else
    QStringList list = formatInfo.split(";", QString::KeepEmptyParts);
#endif
    if ((int)list.count() < 2 || list[1].isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Couldn't change the axis type to the requested format!"));
        }
        return;
    }

    const bool backboneOn = drawAxesBackbone && (axis >= 0 && axis < 4 ? d_axis_backbone[axis] : true);
    switch (type) {
    case AxisType::Time : {
        TimeScaleDraw *sd = new TimeScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                list[0].isEmpty() ? QTime(12, 0, 0, 0) : QTime::fromString(list[0].right(8)),
                list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    case AxisType::Date: {
        DateScaleDraw *sd =
                new DateScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                  QDate::fromString(list[0].left(10), "yyyy-MM-dd"), list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    case AxisType::DateTime: {
        DateTimeScaleDraw *sd = new DateTimeScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                QDateTime::fromString(list[0], "yyyy-MM-ddThh:mm:ss"), list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    default:
        // safeguard - such an argument is invalid for this method
        return;
    }

    axisType[axis] = type;
    axesFormatInfo[axis] = formatInfo;
}

void Graph::setAxisLabelRotation(int axis, int rotation)
{
    if (axis == QwtPlot::xBottom) {
        if (rotation > 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignRight | Qt::AlignVCenter);
        else if (rotation < 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignLeft | Qt::AlignVCenter);
        else if (rotation == 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignHCenter | Qt::AlignBottom);
    } else if (axis == QwtPlot::xTop) {
        if (rotation > 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignLeft | Qt::AlignVCenter);
        else if (rotation < 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignRight | Qt::AlignVCenter);
        else if (rotation == 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignHCenter | Qt::AlignTop);
    }
    d_plot->setAxisLabelRotation(axis, (double)rotation);
}

int Graph::labelsRotation(int axis)
{
    ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    return (int)sclDraw->labelRotation();
}

void Graph::setYAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::yLeft);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::yLeft, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setXAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::xBottom);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::xBottom, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setRightAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::yRight);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::yRight, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTopAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::xTop);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::xTop, t);
    d_plot->replot();
    emit modifiedGraph();
}

namespace {
void alignLinkedChildCanvasToParent(Graph *child)
{
    if (!child)
        return;
    Graph *parentLayer = child->linkedParentLayer();
    if (!parentLayer || parentLayer == child)
        return;
    MultiLayer *ml = child->multiLayerParent();
    if (ml && ml->isRestoringSnapshot())
        return;

    Plot *pPlot = parentLayer->plotWidget();
    Plot *cPlot = child->plotWidget();
    if (!pPlot || !cPlot)
        return;

    pPlot->updateLayout();
    QRect pCanvas = pPlot->plotLayout()->canvasRect();
    if (pCanvas.width() < 20 || pCanvas.height() < 20)
        return;

    cPlot->updateLayout();
    const QRect cCanvas0 = cPlot->plotLayout()->canvasRect();
    const int cL0 = qMax(0, cCanvas0.left());
    const int cT0 = qMax(0, cCanvas0.top());
    const int cR0 = qMax(0, cPlot->width() - (cCanvas0.left() + cCanvas0.width()));
    const int cB0 = qMax(0, cPlot->height() - (cCanvas0.top() + cCanvas0.height()));

    QRect pGeo = parentLayer->geometry();
    QRect targetInner = pCanvas.translated(pGeo.topLeft());

    int shiftTop = 0;
    if (targetInner.top() - cT0 < 4 && pGeo.height() - (4 - (targetInner.top() - cT0)) >= 80)
        shiftTop = 4 - (targetInner.top() - cT0);
    int shiftLeft = 0;
    if (targetInner.left() - cL0 < 4 && pGeo.width() - (4 - (targetInner.left() - cL0)) >= 80)
        shiftLeft = 4 - (targetInner.left() - cL0);

    int trimRight = 0;
    int trimBottom = 0;
    QWidget *hostCanvas = ml ? ml->canvasWidget() : parentLayer->parentWidget();
    if (hostCanvas && hostCanvas->width() >= 140) {
        const int overR =
                (targetInner.left() + targetInner.width() + shiftLeft + cR0) - (hostCanvas->width() - 4);
        if (overR > 0 && pGeo.width() - shiftLeft - overR >= 80)
            trimRight = overR;
    }
    if (hostCanvas && hostCanvas->height() >= 140) {
        const int overB =
                (targetInner.top() + targetInner.height() + shiftTop + cB0) - (hostCanvas->height() - 4);
        if (overB > 0 && pGeo.height() - shiftTop - overB >= 80)
            trimBottom = overB;
    }

    if (shiftTop > 0 || shiftLeft > 0 || trimRight > 0 || trimBottom > 0) {
        const bool prevPIgnore = parentLayer->ignoresResizeEvents();
        const bool prevPAuto = parentLayer->autoscaleFonts();
        parentLayer->setIgnoreResizeEvents(true);
        parentLayer->setAutoscaleFonts(false);
        pGeo = QRect(pGeo.x() + shiftLeft, pGeo.y() + shiftTop,
                     pGeo.width() - shiftLeft - trimRight,
                     pGeo.height() - shiftTop - trimBottom);
        parentLayer->setGeometry(pGeo);
        pPlot->resize(pGeo.size());
        pPlot->updateLayout();
        parentLayer->setAutoscaleFonts(prevPAuto);
        parentLayer->setIgnoreResizeEvents(prevPIgnore);
        pCanvas = pPlot->plotLayout()->canvasRect();
        targetInner = pCanvas.translated(parentLayer->pos());
    }

    if (targetInner.width() < 20 || targetInner.height() < 20)
        return;

    const bool prevCIgnore = child->ignoresResizeEvents();
    const bool prevCAuto = child->autoscaleFonts();
    child->setIgnoreResizeEvents(true);
    child->setAutoscaleFonts(false);
    for (int pass = 0; pass < 2; ++pass) {
        cPlot->updateLayout();
        const QRect cc = cPlot->plotLayout()->canvasRect();
        const int cl = qMax(0, cc.left());
        const int ct = qMax(0, cc.top());
        const int cr = qMax(0, cPlot->width() - (cc.left() + cc.width()));
        const int cb = qMax(0, cPlot->height() - (cc.top() + cc.height()));
        const QRect targetGeo(targetInner.left() - cl, targetInner.top() - ct,
                              qMax(40, targetInner.width() + cl + cr),
                              qMax(40, targetInner.height() + ct + cb));
        if (child->geometry() != targetGeo) {
            child->setGeometry(targetGeo);
            cPlot->resize(targetGeo.size());
        }
    }
    cPlot->updateLayout();
    child->setAutoscaleFonts(prevCAuto);
    child->setIgnoreResizeEvents(prevCIgnore);
}
} // namespace

void Graph::setAxisTitleFont(int axis, const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(axis);
    t.setFont(nf);
    d_plot->setAxisTitle(axis, t);
    if (d_linked_parent)
        alignLinkedChildCanvasToParent(this);
    else
        syncChildLinkedLayers();
    d_plot->replot();
    emit modifiedGraph();
}

QFont Graph::axisTitleFont(int axis)
{
    return d_plot->axisTitle(axis).font();
}

QColor Graph::axisTitleColor(int axis)
{
    QColor c;
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        c = scale->title().color();
    return c;
}

void Graph::setAxesNumColors(const QStringList &colors)
{
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QPalette pal = scale->palette();
            pal.setColor(QPalette::Text, QColor(COLORVALUE(colors[i])));
            scale->setPalette(pal);
        }
    }
}

void Graph::setAxesColors(const QStringList &colors)
{
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QPalette pal = scale->palette();
            pal.setColor(QPalette::WindowText, QColor(COLORVALUE(colors[i])));
            scale->setPalette(pal);
        }
    }
}

QStringList Graph::axesColors()
{
    QStringList colors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++)
        colors << COLORNAME(QColor(Qt::black));

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::WindowText));
        }
    }
    return colors;
}

QColor Graph::axisColor(int axis)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        return scale->palette().color(QPalette::Active, QPalette::WindowText);
    else
        return QColor(Qt::black);
}

QColor Graph::axisNumbersColor(int axis)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        return scale->palette().color(QPalette::Active, QPalette::Text);
    else
        return QColor(Qt::black);
}

QStringList Graph::axesNumColors()
{
    QStringList colors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++)
        colors << COLORNAME(QColor(Qt::black));

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::Text));
        }
    }
    return colors;
}

void Graph::setTitleColor(const QColor &c)
{
    QwtText t = d_plot->title();
    t.setColor(c);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTitleAlignment(int align)
{
    QwtText t = d_plot->title();
    t.setRenderFlags(align);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->title();
    t.setFont(nf);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setYAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::yLeft, text);
}

void Graph::setXAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::xBottom, text);
}

void Graph::setRightAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::yRight, text);
}

void Graph::setTopAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::xTop, text);
}

int Graph::axisTitleAlignment(int axis)
{
    return d_plot->axisTitle(axis).renderFlags();
}

void Graph::setAxesTitlesAlignment(const QStringList &align)
{
    for (int i = 0; i < 4; i++) {
        QwtText t = d_plot->axisTitle(i);
        t.setRenderFlags(align[i + 1].toInt());
        d_plot->setAxisTitle(i, t);
    }
}

void Graph::setXAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xBottom);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::xBottom, t);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setYAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yLeft);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::yLeft, t);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTopAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xTop);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::xTop, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setRightAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yRight);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::yRight, t);

    d_plot->replot();
    emit modifiedGraph();
}

namespace {
QString formatAxisBaseAndUnits(const QString &base, const QString &units, Column::UnitFormat fmt)
{
    auto stripOuterBrackets = [](QString s) -> QString {
        s = s.trimmed();
        while (s.size() >= 3) {
            const QChar f = s.front();
            const QChar l = s.back();
            const bool matched =
                    (f == QLatin1Char('(') && l == QLatin1Char(')'))
                    || (f == QLatin1Char('[') && l == QLatin1Char(']'))
                    || (f == QChar(0xFF08) && l == QChar(0xFF09))
                    || (f == QChar(0xFF3B) && l == QChar(0xFF3D));
            if (!matched)
                break;
            s = s.mid(1, s.size() - 2).trimmed();
        }
        return s;
    };

    const QString b = base.trimmed();
    QString u = stripOuterBrackets(units);
    if (b.isEmpty())
        return QString();
    if (u.isEmpty())
        return b;

    const QString uLower = u.toLower();
    const bool isDimensionless = (u == QLatin1String("-") || u == QStringLiteral("\u2013")
                                  || u == QStringLiteral("\u2014") || u == QStringLiteral("\u2212")
                                  || uLower == QLatin1String("none")
                                  || uLower == QLatin1String("dimensionless"));
    if (isDimensionless) {
        if (fmt == Column::UnitFormat::ISIJSlash)
            return b;
        if (fmt == kUnitFormatBracket)
            return QStringLiteral("%1 [-]").arg(b);
        return QStringLiteral("%1 (-)").arg(b);
    }

    const bool isArbUnits = (uLower == QLatin1String("a.u.") || uLower == QLatin1String("a.u")
                             || uLower == QLatin1String("au")
                             || uLower == QLatin1String("arb. units")
                             || uLower == QLatin1String("arb. unit")
                             || uLower == QLatin1String("arb.units")
                             || uLower == QLatin1String("arbitrary units"));
    if (isArbUnits)
        u = QStringLiteral("a.u.");

    // If Long Name already ends with "/ <Units>", do not duplicate.
    const int slashIdx = b.lastIndexOf(QLatin1Char('/'));
    if (slashIdx >= 0) {
        const QString tail = stripOuterBrackets(b.mid(slashIdx + 1));
        if (!tail.isEmpty() && tail.compare(u, Qt::CaseInsensitive) == 0)
            return b;
    }

    if (fmt == Column::UnitFormat::ISIJSlash)
        return QStringLiteral("%1 / %2").arg(b, u);
    if (fmt == kUnitFormatBracket)
        return QStringLiteral("%1 [%2]").arg(b, u);
    return QStringLiteral("%1 (%2)").arg(b, u);
}

QString extractSharedLongNamePrefix(const QStringList &longNames)
{
    if (longNames.size() < 2)
        return QString();

    auto extractStemBeforeDelim = [](const QString &s) -> QString {
        int cut = -1;
        for (int i = 0; i < s.size(); ++i) {
            const QChar ch = s.at(i);
            if (ch == QLatin1Char('(') || ch == QLatin1Char('_')) {
                cut = i;
                break;
            }
            if (ch == QLatin1Char('-') || ch == QChar(0x2013) || ch == QChar(0x2014)) {
                cut = i;
                break;
            }
        }
        return (cut > 0) ? s.left(cut).trimmed() : QString();
    };

    const QString firstStem = extractStemBeforeDelim(longNames.first());
    if (!firstStem.isEmpty()) {
        bool allSameStem = true;
        for (int i = 1; i < longNames.size(); ++i) {
            const QString st = extractStemBeforeDelim(longNames[i]);
            if (st.isEmpty() || st.compare(firstStem, Qt::CaseInsensitive) != 0) {
                allSameStem = false;
                break;
            }
        }
        if (allSameStem)
            return firstStem;
    }

    const QString &s0 = longNames.first();
    int commonLen = s0.size();
    for (int i = 1; i < longNames.size(); ++i) {
        const QString &si = longNames[i];
        int k = 0;
        const int lim = qMin(commonLen, si.size());
        while (k < lim && s0.at(k).toLower() == si.at(k).toLower())
            ++k;
        commonLen = k;
        if (commonLen == 0)
            return QString();
    }

    // Require that the divergence happens at or after a delimiter '(', '-', '_'
    QString rawPrefix = s0.left(commonLen);
    bool hasDelimBoundary = false;
    for (const QString &ln : longNames) {
        if (ln.size() > commonLen) {
            const QChar nextCh = ln.at(commonLen);
            if (nextCh == QLatin1Char('(') || nextCh == QLatin1Char('-')
                || nextCh == QLatin1Char('_') || nextCh == QChar(0x2013)
                || nextCh == QChar(0x2014)) {
                hasDelimBoundary = true;
            }
        }
    }
    while (!rawPrefix.isEmpty()) {
        const QChar tail = rawPrefix.at(rawPrefix.size() - 1);
        if (tail.isSpace() || tail == QLatin1Char('(') || tail == QLatin1Char('-')
            || tail == QLatin1Char('_') || tail == QChar(0x2013) || tail == QChar(0x2014)) {
            if (tail == QLatin1Char('(') || tail == QLatin1Char('-') || tail == QLatin1Char('_')
                || tail == QChar(0x2013) || tail == QChar(0x2014)) {
                hasDelimBoundary = true;
            }
            rawPrefix.chop(1);
        } else {
            break;
        }
    }
    return (hasDelimBoundary && rawPrefix.size() >= 2) ? rawPrefix.trimmed() : QString();
}

bool isSingleColumnHijackCandidate(const QList<Column *> &cols, const QString &plainText)
{
    const QString p = plainText.trimmed();
    if (p.isEmpty())
        return false;
    for (Column *col : cols) {
        if (!col)
            continue;
        const QString colBase =
                col->longName().trimmed().isEmpty() ? col->name().trimmed() : col->longName().trimmed();
        if (p.compare(col->formattedAxisTitle(Column::UnitFormat::Paren).trimmed(),
                      Qt::CaseInsensitive)
                    == 0
            || p.compare(col->formattedAxisTitle(Column::UnitFormat::ISIJSlash).trimmed(),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(col->formattedAxisTitle(kUnitFormatBracket).trimmed(),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(formatAxisBaseAndUnits(colBase, col->units(), Column::UnitFormat::Paren),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(formatAxisBaseAndUnits(colBase, col->units(), Column::UnitFormat::ISIJSlash),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(formatAxisBaseAndUnits(colBase, col->units(), kUnitFormatBracket),
                         Qt::CaseInsensitive)
                    == 0
            || (!col->longName().trimmed().isEmpty()
                && p.compare(col->longName().trimmed(), Qt::CaseInsensitive) == 0)
            || (!col->name().trimmed().isEmpty()
                && p.compare(col->name().trimmed(), Qt::CaseInsensitive) == 0)) {
            return true;
        }
    }
    return false;
}
} // namespace

bool Graph::isAuxiliaryOrFitCurve(const QwtPlotCurve *pc, bool skipHiddenTables) const
{
    if (!pc)
        return true;
    if (d_fit_curves.contains(const_cast<QwtPlotCurve *>(pc)))
        return true;
    const QString cTitle = pc->title().text().trimmed();
    if (cTitle.startsWith(QStringLiteral("RpcFitCurve")))
        return true;
    const DataCurve *dc = dynamic_cast<const DataCurve *>(pc);
    if (!dc || !dc->table())
        return false;
    if (dc->yColumnName().trimmed().startsWith(QStringLiteral("RpcFitCurve")))
        return true;
    const QString tblObj = dc->table()->objectName();
    const QString tblName = dc->table()->name();
    if (tblObj.startsWith(QStringLiteral("GadgetFit_"))
        || tblName.startsWith(QStringLiteral("GadgetFit_"))
        || tblName.startsWith(QStringLiteral("RpcFit"))) {
        return true;
    }
    if (skipHiddenTables && dc->table()->status() == MyWidget::Hidden)
        return true;
    return false;
}

QString Graph::curveCustomLabel(int curveIndex) const
{
    if (curveIndex < 0 || curveIndex >= n_curves)
        return QString();
    const DataCurve *dc = dynamic_cast<const DataCurve *>(curve(curveIndex));
    return dc ? dc->customLabel() : QString();
}

bool Graph::setCurveCustomLabel(int curveIndex, const QString &label)
{
    if (curveIndex < 0 || curveIndex >= n_curves)
        return false;
    DataCurve *dc = dynamic_cast<DataCurve *>(curve(curveIndex));
    if (!dc || !dc->table())
        return false;
    dc->setCustomLabel(label.trimmed());
    return true;
}

int Graph::curveIndexByCustomLabel(const QString &label) const
{
    const QString want = label.trimmed();
    if (want.isEmpty())
        return -1;
    for (const Qt::CaseSensitivity cs : { Qt::CaseSensitive, Qt::CaseInsensitive }) {
        for (int i = 0; i < n_curves; ++i) {
            const DataCurve *dc = dynamic_cast<const DataCurve *>(curve(i));
            if (dc && !dc->customLabel().isEmpty()
                && dc->customLabel().compare(want, cs) == 0) {
                return i;
            }
        }
    }
    return -1;
}

QString Graph::resolvedAutoCurveLabel(int curveIndex) const
{
    // 利用者が付けたラベル（RPC の label=）は曲線のタイトル（Y 列の結び付き）とは別に
    // 曲線が持つ。凡例の作り直し・フィットの追加・曲線の追加と削除で列名に戻らないよう、
    // 列から作る自動ラベルより優先する。
    const QString custom = curveCustomLabel(curveIndex);
    if (!custom.isEmpty())
        return custom;
    return columnCurveLabel(curveIndex);
}

QString Graph::columnCurveLabel(int curveIndex) const
{
    if (curveIndex < 0 || curveIndex >= n_curves)
        return QString();
    const QwtPlotCurve *c = curve(curveIndex);
    if (!c)
        return QString();

    const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
    if (!dc || !dc->table())
        return c->title().text().trimmed();

    Table *t = dc->table();
    const int yIdx = t->colIndex(dc->yColumnName());
    Column *yc = (yIdx >= 0) ? t->column(yIdx) : nullptr;

    QString cleanShort = yc ? yc->name().trimmed() : dc->yColumnName().trimmed();
    const QString prefix = QString(t->name()) + QLatin1Char('_');
    if (cleanShort.startsWith(prefix) && cleanShort.length() > prefix.length())
        cleanShort = cleanShort.mid(prefix.length());
    if (cleanShort.isEmpty()) {
        cleanShort = c->title().text().trimmed();
        if (cleanShort.startsWith(prefix) && cleanShort.length() > prefix.length())
            cleanShort = cleanShort.mid(prefix.length());
    }

    if (!yc)
        return cleanShort;

    const QString myLong = yc->longName().trimmed();
    const QString myComment = firstNonEmptyCommentLine(yc->comment());

    if (!myLong.isEmpty()) {
        int sameLongCount = 0;
        for (int j = 0; j < n_curves; ++j) {
            if (j < c_type.size() && c_type[j] == ErrorBars)
                continue;
            const QwtPlotCurve *opc = curve(j);
            if (!opc || (j != curveIndex && !opc->testItemAttribute(QwtPlotItem::Legend)))
                continue;
            if (j != curveIndex && isAuxiliaryOrFitCurve(opc, true))
                continue;
            const DataCurve *odc = dynamic_cast<const DataCurve *>(opc);
            if (!odc || !odc->table())
                continue;
            Column *oxDummy = nullptr;
            Column *oyc = nullptr;
            odc->resolveColumns(oxDummy, oyc);
            if (!oyc) {
                const int oyIdx = odc->table()->colIndex(odc->yColumnName());
                oyc = (oyIdx >= 0) ? odc->table()->column(oyIdx) : nullptr;
            }
            if (oyc && oyc->longName().trimmed().compare(myLong, Qt::CaseInsensitive) == 0)
                ++sameLongCount;
        }
        if (sameLongCount >= 2)
            return !myComment.isEmpty() ? myComment : cleanShort;

        if (!myComment.isEmpty()) {
            const int ya = (c->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight : QwtPlot::yLeft;
            QString curYPlain = unwrapNoWrapAxisTitle(axisTitle(ya)).trimmed();
            if (curYPlain.startsWith(QLatin1String("<b>"))
                && curYPlain.endsWith(QLatin1String("</b>")) && curYPlain.length() >= 7) {
                curYPlain = curYPlain.mid(3, curYPlain.length() - 7).trimmed();
            }
            const QString autoY = resolvedAutoAxisTitle(ya, d_axis_unit_format).trimmed();
            if (curYPlain.startsWith(myLong, Qt::CaseInsensitive)
                || autoY.startsWith(myLong, Qt::CaseInsensitive)) {
                return myComment;
            }
        }
    }

    if (!myLong.isEmpty())
        return myLong;
    if (!myComment.isEmpty())
        return myComment;
    return cleanShort;
}

QString Graph::resolvedAutoAxisTitle(int axis, Column::UnitFormat fmt) const
{
    if (axis < 0 || axis >= 4 || isPiePlot() || isTernaryPlot() || isPolarPlot()
        || isPoleFigurePlot())
        return QString();

    bool hasNonHiddenDataCurve = false;
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        if (isAuxiliaryOrFitCurve(pc, false))
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->table() && dc->table()->status() != MyWidget::Hidden) {
            hasNonHiddenDataCurve = true;
            break;
        }
    }

    QList<Column *> xBottomCols, xTopCols, yLeftCols, yRightCols;
    // 軸題の種類（ヒストグラムの「Counts」など）は、関数曲線・フィット曲線・計算結果の曲線を
    // 除いたデータ曲線の先頭から決める。関数曲線をヒストグラムより前に動かすと、保存して開き
    // 直したときや Redo の後に Y 軸題が「Counts」から列名に変わっていた。データ曲線が一つも
    // ないときだけ、従来どおり先頭の曲線の種類を使う。
    int detectedStyle = -1;
    int firstCurveStyle = -1;

    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        const int cType = (i < c_type.size()) ? c_type[i] : -1;
        if (cType == ErrorBars)
            continue;
        if (firstCurveStyle < 0)
            firstCurveStyle = cType;
        if (detectedStyle < 0 && cType != Function && !dynamic_cast<FunctionCurve *>(pc)
            && !isAuxiliaryOrFitCurve(pc, hasNonHiddenDataCurve))
            detectedStyle = cType;

        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (!dc || !dc->table())
            continue;
        if (isAuxiliaryOrFitCurve(pc, hasNonHiddenDataCurve)
            || (!pc->testItemAttribute(QwtPlotItem::Legend) && n_curves > 1))
            continue;
        Table *t = dc->table();
        const int xIdx = t->colIndex(dc->xColumnName());
        const int yIdx = t->colIndex(dc->yColumnName());
        if (Column *xc = (xIdx >= 0 ? t->column(xIdx) : nullptr)) {
            QList<Column *> &dst = (pc->xAxis() == QwtPlot::xTop) ? xTopCols : xBottomCols;
            if (!dst.contains(xc))
                dst << xc;
        }
        if (Column *yc = (yIdx >= 0 ? t->column(yIdx) : nullptr)) {
            QList<Column *> &dst = (pc->yAxis() == QwtPlot::yRight) ? yRightCols : yLeftCols;
            if (!dst.contains(yc))
                dst << yc;
        }
    }
    if (detectedStyle < 0)
        detectedStyle = firstCurveStyle;

    auto cleanColAxisTitle = [fmt](Column *col) -> QString {
        if (!col)
            return QString();
        const QString ln = col->longName().trimmed();
        const QString base = ln.isEmpty() ? col->name().trimmed() : ln;
        return formatAxisBaseAndUnits(base, col->units(), fmt);
    };

    auto resolveMultiCol = [&](const QList<Column *> &cols) -> QString {
        if (cols.isEmpty())
            return QString();
        if (cols.size() == 1)
            return cleanColAxisTitle(cols.first());

        // どの列からも同じ軸題（Long Name か列名＋単位）になるなら、それを使う。元の列「温度」
        // （Long Name なし）と、その名前を Long Name に引き継いだ解析結果の列が並ぶときなど、
        // Long Name の有無が混じっていても同じ量を表している。
        {
            const QString firstFull = cleanColAxisTitle(cols.first());
            bool allSameFull = !firstFull.isEmpty();
            for (int i = 1; i < cols.size() && allSameFull; ++i)
                allSameFull = cleanColAxisTitle(cols[i]).compare(firstFull, Qt::CaseInsensitive)
                        == 0;
            if (allSameFull)
                return firstFull;
        }

        QString sharedUnits;
        bool unitsConflict = false;
        QStringList longNames;
        bool allHaveLongName = true;
        bool allEmptyLongName = true;
        for (Column *col : cols) {
            const QString u = col->units().trimmed();
            if (!u.isEmpty()) {
                if (sharedUnits.isEmpty())
                    sharedUnits = u;
                else if (sharedUnits.compare(u, Qt::CaseInsensitive) != 0)
                    unitsConflict = true;
            }
            const QString ln = col->longName().trimmed();
            longNames << ln;
            if (ln.isEmpty())
                allHaveLongName = false;
            else
                allEmptyLongName = false;
        }

        // Case 1: All curves on this axis share the same non-empty Long Name (case-insensitive)
        if (allHaveLongName) {
            const QString firstLong = longNames.first();
            bool allSameLong = true;
            for (int i = 1; i < longNames.size(); ++i) {
                if (longNames[i].compare(firstLong, Qt::CaseInsensitive) != 0) {
                    allSameLong = false;
                    break;
                }
            }
            if (allSameLong) {
                return unitsConflict ? firstLong
                                     : formatAxisBaseAndUnits(firstLong, sharedUnits, fmt);
            }

            // Case 2: All curves share a non-empty common prefix before '(' / '-' / '_'
            const QString sharedPrefix = extractSharedLongNamePrefix(longNames);
            if (!sharedPrefix.isEmpty()) {
                return unitsConflict ? sharedPrefix
                                     : formatAxisBaseAndUnits(sharedPrefix, sharedUnits, fmt);
            }
        } else if (allEmptyLongName) {
            const QString firstShortFull = cleanColAxisTitle(cols.first());
            bool allSameShortFull = !firstShortFull.isEmpty();
            for (int i = 1; i < cols.size(); ++i) {
                if (cleanColAxisTitle(cols[i]).compare(firstShortFull, Qt::CaseInsensitive) != 0) {
                    allSameShortFull = false;
                    break;
                }
            }
            if (allSameShortFull)
                return firstShortFull;
        }

        // Case 3: Long Names differ completely (e.g. "Uncharged" vs "H-charged").
        // Never hijack curve 1's specimen-specific Long Name! Keep existing non-placeholder
        // axis title if not a single-curve hijack, or format cleanly without curve 1's sample label.
        QString curPlain = unwrapNoWrapAxisTitle(axisTitle(axis)).trimmed();
        if (curPlain.startsWith("<b>") && curPlain.endsWith("</b>") && curPlain.length() >= 7)
            curPlain = curPlain.mid(3, curPlain.length() - 7).trimmed();

        const QString axisSym = (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
                ? QStringLiteral("X")
                : QStringLiteral("Y");
        const QString genericParen = (!unitsConflict && !sharedUnits.isEmpty())
                ? formatAxisBaseAndUnits(axisSym, sharedUnits, Column::UnitFormat::Paren)
                : QString();
        const QString genericSlash = (!unitsConflict && !sharedUnits.isEmpty())
                ? formatAxisBaseAndUnits(axisSym, sharedUnits, Column::UnitFormat::ISIJSlash)
                : QString();
        const QString genericBracket = (!unitsConflict && !sharedUnits.isEmpty())
                ? formatAxisBaseAndUnits(axisSym, sharedUnits, kUnitFormatBracket)
                : QString();

        if (!curPlain.isEmpty() && !isAxisPlaceholderTitle(curPlain)
            && !isSingleColumnHijackCandidate(cols, curPlain)
            && curPlain != genericParen && curPlain != genericSlash
            && curPlain != genericBracket) {
            return curPlain;
        }

        if (!unitsConflict && !sharedUnits.isEmpty())
            return formatAxisBaseAndUnits(axisSym, sharedUnits, fmt);
        return QString();
    };

    auto isSelfLabeledCategoryCols = [&](int catAxis, const QList<Column *> &cols) -> bool {
        if (catAxis < 0 || catAxis >= axisType.size() || axisType[catAxis] != AxisType::Txt)
            return false;
        for (Column *col : cols) {
            if (col && (!col->longName().trimmed().isEmpty() || !col->units().trimmed().isEmpty()))
                return false;
        }
        return true;
    };

    if (detectedStyle == Histogram) {
        if (axis == QwtPlot::xBottom)
            return resolveMultiCol(yLeftCols);
        if (axis == QwtPlot::yLeft)
            return tr("Counts");
        return QString();
    }
    if (detectedStyle == Box) {
        return (axis == QwtPlot::yLeft) ? resolveMultiCol(yLeftCols) : QString();
    }
    if (detectedStyle == HorizontalBars) {
        if (axis == QwtPlot::xBottom)
            return resolveMultiCol(yLeftCols);
        if (axis == QwtPlot::yLeft)
            return isSelfLabeledCategoryCols(QwtPlot::yLeft, xBottomCols)
                    ? QString()
                    : resolveMultiCol(xBottomCols);
        return QString();
    }
    if (detectedStyle == VerticalBars && axis == QwtPlot::xBottom
        && isSelfLabeledCategoryCols(QwtPlot::xBottom, xBottomCols)) {
        return QString();
    }

    switch (axis) {
    case QwtPlot::xBottom:
        return resolveMultiCol(xBottomCols);
    case QwtPlot::xTop:
        return resolveMultiCol(xTopCols);
    case QwtPlot::yLeft:
        return resolveMultiCol(yLeftCols);
    case QwtPlot::yRight:
        return resolveMultiCol(yRightCols);
    default:
        return QString();
    }
}

bool Graph::matchesAutoAxisTitleCandidate(int axis, const QString &text) const
{
    QString plain = unwrapNoWrapAxisTitle(text).trimmed();
    if (plain.startsWith("<b>") && plain.endsWith("</b>") && plain.length() >= 7)
        plain = plain.mid(3, plain.length() - 7).trimmed();

    if (isAxisPlaceholderTitle(plain))
        return true;

    const QString parenCandidate = resolvedAutoAxisTitle(axis, Column::UnitFormat::Paren);
    if (!parenCandidate.isEmpty() && plain == parenCandidate)
        return true;

    const QString slashCandidate = resolvedAutoAxisTitle(axis, Column::UnitFormat::ISIJSlash);
    if (!slashCandidate.isEmpty() && plain == slashCandidate)
        return true;

    const QString bracketCandidate = resolvedAutoAxisTitle(axis, kUnitFormatBracket);
    if (!bracketCandidate.isEmpty() && plain == bracketCandidate)
        return true;

    const QString axisSym = (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
            ? QStringLiteral("X")
            : QStringLiteral("Y");

    for (int i = 0; i < n_curves; ++i) {
        DataCurve *dc = dynamic_cast<DataCurve *>(curve(i));
        if (!dc || !dc->table())
            continue;
        Table *t = dc->table();
        const bool isHorizBar = (dc->type() == HorizontalBars);
        const bool isHorizAxis = (axis == QwtPlot::xBottom || axis == QwtPlot::xTop);
        const QString colName = (isHorizAxis != isHorizBar) ? dc->xColumnName() : dc->yColumnName();
        const int idx = t->colIndex(colName);
        if (Column *col = (idx >= 0 ? t->column(idx) : nullptr)) {
            const QString colBase = col->longName().trimmed().isEmpty() ? col->name().trimmed()
                                                                        : col->longName().trimmed();
            if (plain == col->name().trimmed() || plain == col->longName().trimmed()
                || plain == col->formattedAxisTitle(Column::UnitFormat::Paren).trimmed()
                || plain == col->formattedAxisTitle(Column::UnitFormat::ISIJSlash).trimmed()
                || plain == col->formattedAxisTitle(kUnitFormatBracket).trimmed()
                || plain == formatAxisBaseAndUnits(colBase, col->units(), Column::UnitFormat::Paren)
                || plain == formatAxisBaseAndUnits(colBase, col->units(),
                                                   Column::UnitFormat::ISIJSlash)
                || plain == formatAxisBaseAndUnits(colBase, col->units(), kUnitFormatBracket)) {
                return true;
            }
            const QString u = col->units().trimmed();
            if (!u.isEmpty()
                && (plain == formatAxisBaseAndUnits(axisSym, u, Column::UnitFormat::Paren)
                    || plain == formatAxisBaseAndUnits(axisSym, u, Column::UnitFormat::ISIJSlash)
                    || plain == formatAxisBaseAndUnits(axisSym, u, kUnitFormatBracket))) {
                return true;
            }
        }
    }
    return false;
}

void Graph::setAxisTitleAuto(int axis, bool autoSync)
{
    if (axis >= 0 && axis < 4)
        d_axis_title_auto[axis] = autoSync;
}

bool Graph::isAxisTitleAuto(int axis) const
{
    return (axis >= 0 && axis < 4) ? d_axis_title_auto[axis] : false;
}

void Graph::setAxisUnitFormat(Column::UnitFormat fmt)
{
    if (d_axis_unit_format == fmt)
        return;
    d_axis_unit_format = fmt;
    refreshAutoAxisTitlesFromColumns();
}

void Graph::refreshAutoAxisTitlesFromColumns()
{
    if (!d_plot || isPiePlot() || isTernaryPlot())
        return;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QFont af = axisTitleFont(a);
        if (af.family().trimmed().isEmpty()
            || af.hintingPreference() != QFont::PreferNoHinting) {
            if (af.family().trimmed().isEmpty())
                af.setFamily(QStringLiteral("Arial"));
            af.setHintingPreference(QFont::PreferNoHinting);
            setAxisTitleFont(a, af);
        }
    }

    QSet<int> colorBarAxes;
    const QwtPlotItemList &items = d_plot->itemList();
    for (QwtPlotItem *it : items) {
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(it);
            if (sp->hasColorScale())
                colorBarAxes.insert(sp->colorScaleAxis());
        }
    }

    bool hasNonHiddenDataCurve = false;
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        if (isAuxiliaryOrFitCurve(pc, false))
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->table() && dc->table()->status() != MyWidget::Hidden) {
            hasNonHiddenDataCurve = true;
            break;
        }
    }

    QList<Column *> axisCols[4];
    QList<Column *> allGraphCols;
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (!dc || !dc->table())
            continue;
        if (isAuxiliaryOrFitCurve(pc, hasNonHiddenDataCurve)
            || (!pc->testItemAttribute(QwtPlotItem::Legend) && n_curves > 1))
            continue;
        Table *t = dc->table();
        const int xIdx = t->colIndex(dc->xColumnName());
        const int yIdx = t->colIndex(dc->yColumnName());
        const int xa = (pc->xAxis() == QwtPlot::xTop) ? QwtPlot::xTop : QwtPlot::xBottom;
        const int ya = (pc->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight : QwtPlot::yLeft;
        const bool isHorizBar = (dc->type() == HorizontalBars);
        if (Column *xc = (xIdx >= 0 ? t->column(xIdx) : nullptr)) {
            const int targetAx = isHorizBar ? ya : xa;
            if (!axisCols[targetAx].contains(xc))
                axisCols[targetAx] << xc;
            if (!allGraphCols.contains(xc))
                allGraphCols << xc;
        }
        if (Column *yc = (yIdx >= 0 ? t->column(yIdx) : nullptr)) {
            const int targetAy = isHorizBar ? xa : ya;
            if (!axisCols[targetAy].contains(yc))
                axisCols[targetAy] << yc;
            if (!allGraphCols.contains(yc))
                allGraphCols << yc;
        }
    }

    const bool prevRefreshing = d_refreshing_auto_titles;
    d_refreshing_auto_titles = true;
    bool anyChanged = false;

    for (int a = 0; a < 4; ++a) {
        if (!d_axis_title_auto[a] || colorBarAxes.contains(a))
            continue;
        if ((a == QwtPlot::xTop || a == QwtPlot::yRight) && !d_plot->axisEnabled(a))
            continue;

        QString nextTitle = resolvedAutoAxisTitle(a, d_axis_unit_format);
        if (nextTitle.isEmpty()) {
            QString curPlain = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
            if (curPlain.startsWith("<b>") && curPlain.endsWith("</b>") && curPlain.length() >= 7)
                curPlain = curPlain.mid(3, curPlain.length() - 7).trimmed();
            const bool shouldClearHijack = isAxisPlaceholderTitle(curPlain)
                    || (axisCols[a].size() >= 2
                        && isSingleColumnHijackCandidate(axisCols[a], curPlain))
                    || (a < axisType.size() && axisType[a] == AxisType::Txt
                        && isSingleColumnHijackCandidate(allGraphCols, curPlain))
                    || ((a == QwtPlot::yRight || a == QwtPlot::xTop) && axisCols[a].isEmpty()
                        && isSingleColumnHijackCandidate(allGraphCols, curPlain));
            if (shouldClearHijack && !curPlain.isEmpty()) {
                QwtText t = d_plot->axisTitle(a);
                t.setText(QString());
                d_plot->setAxisTitle(a, t);
                anyChanged = true;
            }
            continue;
        }

        nextTitle = formatScientificMarkupTokens(nextTitle);
        const QString wrappedNext = wrapRichAxisTitleNoWrap(nextTitle);
        const QString curRawTitle = d_plot->axisTitle(a).text();

        if (curRawTitle != wrappedNext) {
            QwtText t = d_plot->axisTitle(a);
            QFont f = t.font();
            if (f.family().trimmed().isEmpty())
                f = QFont(QStringLiteral("Arial"), 14, QFont::Normal, false);
            f.setHintingPreference(QFont::PreferNoHinting);
            t.setFont(f);
            t.setText(wrappedNext);
            d_plot->setAxisTitle(a, t);
            if (a == QwtPlot::yRight && !nextTitle.trimmed().isEmpty()) {
                if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight))
                    sw->setSpacing(6);
            }
            anyChanged = true;
        }
    }

    if (Legend *leg = legend()) {
        QStringList lines = leg->text().split('\n');
        bool legChanged = false;
        for (int li = 0; li < lines.size(); ++li) {
            const QString line = lines[li];
            if (!line.startsWith("\\c{"))
                continue;
            const int closeBrace = line.indexOf('}');
            if (closeBrace <= 3)
                continue;
            bool ok = false;
            const int oneBased = line.mid(3, closeBrace - 3).toInt(&ok);
            if (!ok || oneBased < 1 || oneBased > n_curves)
                continue;
            const int ci = oneBased - 1;
            DataCurve *dc = dynamic_cast<DataCurve *>(curve(ci));
            if (!dc || !dc->table())
                continue;
            Table *t = dc->table();
            const int yIdx = t->colIndex(dc->yColumnName());
            Column *yc = (yIdx >= 0) ? t->column(yIdx) : nullptr;
            if (!yc)
                continue;

            const QString currentLabel = line.mid(closeBrace + 1);
            const QString colShort = yc->name().trimmed();
            const QString colLong = yc->longName().trimmed();
            const QString colComment = firstNonEmptyCommentLine(yc->comment());
            const QString desiredLabel = resolvedAutoCurveLabel(ci);
            if (desiredLabel.isEmpty())
                continue;
            const QString prevAuto = d_last_auto_curve_labels.value(ci);

            const QString pfx = QString(t->name()) + "_";
            const QString yColFull = dc->yColumnName();
            const bool hasStrippedTitle =
                    yColFull.startsWith(pfx) && yColFull.length() > pfx.length();
            const QString strippedTitle = hasStrippedTitle ? yColFull.mid(pfx.length()) : QString();

            const bool hasRichMeta = !colLong.isEmpty() || !colComment.isEmpty();
            if (currentLabel == colShort || (hasStrippedTitle && currentLabel == strippedTitle)
                || (hasRichMeta && currentLabel == yColFull)
                || (!colLong.isEmpty() && currentLabel == colLong)
                || (!colComment.isEmpty() && currentLabel == colComment)
                || (!prevAuto.isEmpty() && currentLabel == prevAuto)) {
                int availCw = 320;
                if (d_plot && d_plot->plotLayout()
                    && d_plot->plotLayout()->canvasRect().width() > 20) {
                    availCw = d_plot->plotLayout()->canvasRect().width();
                } else if (d_plot && d_plot->canvas() && d_plot->canvas()->width() > 20) {
                    availCw = d_plot->canvas()->width();
                }
                QFont measureLegFnt = leg->font();
                if (availCw < 260 && measureLegFnt.pointSize() > 9)
                    measureLegFnt.setPointSize(9);
                const bool desiredOverflowsCanvas =
                        !currentLabel.trimmed().isEmpty()
                        && desiredLabel.size() > currentLabel.trimmed().size()
                        && (QFontMetrics(measureLegFnt).horizontalAdvance(desiredLabel) + 38
                            > availCw);
                if (!desiredOverflowsCanvas) {
                    d_last_auto_curve_labels[ci] = desiredLabel;
                    const QString updatedLine =
                            QStringLiteral("\\c{%1}%2").arg(oneBased).arg(desiredLabel);
                    if (lines[li] != updatedLine) {
                        lines[li] = updatedLine;
                        legChanged = true;
                    }
                }
            } else if (currentLabel == desiredLabel) {
                d_last_auto_curve_labels[ci] = desiredLabel;
            }
        }
        if (legChanged) {
            leg->setText(lines.join("\n"));
            anyChanged = true;
        }
    }

    d_refreshing_auto_titles = prevRefreshing;
    if (anyChanged) {
        d_plot->replot();
        emit modifiedGraph();
    }
}

void Graph::setAxisTitle(int axis, const QString &text)
{
    QString unesc = text;
    unesc.replace(QLatin1String("&#9;"), QLatin1String("\t"));
    unesc.replace(QLatin1String("&#60;"), QLatin1String("<"));
    const QString rawInput = unwrapNoWrapAxisTitle(unesc);
    QString finalText = rawInput;
    if (axis >= 0 && axis < 4 && !d_refreshing_auto_titles) {
        if (rawInput.trimmed().isEmpty()) {
            d_axis_title_auto[axis] = false;
        } else if (d_axis_title_auto[axis]) {
            d_axis_title_auto[axis] = matchesAutoAxisTitleCandidate(axis, rawInput);
            if (d_axis_title_auto[axis]) {
                QString stripped = rawInput.trimmed();
                if (stripped.startsWith("<b>") && stripped.endsWith("</b>")
                    && stripped.length() >= 7) {
                    stripped = stripped.mid(3, stripped.length() - 7).trimmed();
                }
                const QString autoTitle = resolvedAutoAxisTitle(axis, d_axis_unit_format);
                finalText = !autoTitle.isEmpty() ? autoTitle : stripped;
            }
        }
    }

    if (!finalText.trimmed().isEmpty())
        finalText = formatScientificMarkupTokens(finalText);

    QwtText t = d_plot->axisTitle(axis);
    QFont f = t.font();
    if (f.family().trimmed().isEmpty())
        f = QFont("Arial", 14, QFont::Normal, false);
    f.setHintingPreference(QFont::PreferNoHinting);
    t.setFont(f);
    t.setText(wrapRichAxisTitleNoWrap(finalText));
    d_plot->setAxisTitle(axis, t);
    if (axis == QwtPlot::yRight && !finalText.trimmed().isEmpty()) {
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight))
            sw->setSpacing(6);
    }
    if (d_linked_parent)
        alignLinkedChildCanvasToParent(this);
    else
        syncChildLinkedLayers();
    d_plot->replot();
    emit modifiedGraph();
}

QString Graph::axisTitle(int axis) const
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return unwrapNoWrapAxisTitle(d_plot->axisTitle(axis).text());
}

QStringList Graph::scalesTitles()
{
    QStringList scaleTitles;
    int axis;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        switch (i) {
        case 0:
            axis = 2;
            scaleTitles << axisTitle(axis);
            break;

        case 1:
            axis = 0;
            scaleTitles << axisTitle(axis);
            break;

        case 2:
            axis = 3;
            scaleTitles << axisTitle(axis);
            break;

        case 3:
            axis = 1;
            scaleTitles << axisTitle(axis);
            break;
        }
    }
    return scaleTitles;
}

void Graph::updateSecondaryAxis(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt || hasColorScaleOnAxis(axis))
        return;

    if (d_weibull_prob_axis[axis] || !d_custom_tick_positions[axis].isEmpty()
        || !d_custom_category_labels[axis].isEmpty()) {
        syncCustomAndWeibullAxes();
        return;
    }

    for (int i = 0; i < n_curves; i++) {
        QwtPlotItem *it = plotItem(i);
        if (!it)
            continue;

        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = (Spectrogram *)it;
            if (sp->colorScaleAxis() == axis)
                return;
        }

        if ((axis == QwtPlot::yRight && it->yAxis() == QwtPlot::yRight)
            || (axis == QwtPlot::xTop && it->xAxis() == QwtPlot::xTop))
            return;
    }

    int a = QwtPlot::xBottom;
    if (axis == QwtPlot::yRight)
        a = QwtPlot::yLeft;

    if (!d_plot->axisEnabled(a))
        return;

    QwtScaleEngine *se = d_plot->axisScaleEngine(a);
    const QwtScaleDiv *sd = d_plot->axisScaleDiv(a);
    if (sd && !sd->isValid()) {
        d_plot->updateAxes();
        se = d_plot->axisScaleEngine(a);
        sd = d_plot->axisScaleDiv(a);
    }
    if (!se || !sd)
        return;

    QwtScaleEngine *sc_engine = 0;
    if (d_axis_breaks[a].enabled && !d_axis_breaks[axis].enabled)
        sc_engine = new PiecewiseBreakScaleEngine(d_axis_breaks[a]);
    else if (Plot::isLog10ScaleEngine(se))
        sc_engine = new QwtLog10ScaleEngine();
    else
        sc_engine = new QwtLinearScaleEngine();

    if (se->testAttribute(QwtScaleEngine::Inverted))
        sc_engine->setAttribute(QwtScaleEngine::Inverted);

    d_plot->setAxisScaleEngine(axis, sc_engine);
    d_plot->setAxisScaleDiv(axis, *sd);

    if (dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(a)) != nullptr) {
        if (QwtScaleDraw *oppSd = d_plot->axisScaleDraw(axis))
            oppSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
        d_plot->setMinorTicksType(axis, ScaleDraw::None);
        d_plot->setMajorTicksType(axis, ScaleDraw::None);
    }

    if (ScaleDraw *mirrorSd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        const double lo = sd ? qMin(sd->lowerBound(), sd->upperBound()) : 0.0;
        const double hi = sd ? qMax(sd->lowerBound(), sd->upperBound()) : 0.0;
        const double bf = qMin(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
        const double bt = qMax(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
        if (d_axis_breaks[a].enabled && !d_axis_breaks[axis].enabled && bf > lo && bt < hi)
            mirrorSd->setAxisBreakIndicator(true, bf, bt, d_axis_breaks[a].log10Before,
                                            d_axis_breaks[a].log10After);
        else if (!d_axis_breaks[axis].enabled)
            mirrorSd->setAxisBreakIndicator(false, 0.0);
    }

    d_user_step[axis] = d_user_step[a];

    QwtScaleWidget *scale = d_plot->axisWidget(a);
    int start = scale->startBorderDist();
    int end = scale->endBorderDist();

    scale = d_plot->axisWidget(axis);
    scale->setMinBorderDist(start, end);
}

bool Graph::isAxisUserLocked(int axis) const
{
    return isToriAxisUserLocked(d_plot, axis);
}

void Graph::setAxisUserLocked(int axis, bool locked)
{
    setToriAxisUserLocked(d_plot, axis, locked);
}

void Graph::cancelInlineScaleEditor()
{
    if (scalePicker)
        scalePicker->cancelInlineScaleEditor();
}

void Graph::applyLogAutoScalePositiveRange()
{
    if (!d_plot || isPiePlot() || isTernaryPlot())
        return;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        if (isToriAxisUserLocked(d_plot, a) || !d_plot->axisAutoScale(a)
            || d_axis_breaks[a].enabled || hasColorScaleOnAxis(a)) {
            continue;
        }
        QwtScaleEngine *se = d_plot->axisScaleEngine(a);
        if (!se || !Plot::isLog10ScaleEngine(se))
            continue;

        // Same item set as QwtPlot::updateAxes(): every AutoScale item attached to this axis.
        const bool isX = (a == QwtPlot::xBottom || a == QwtPlot::xTop);
        bool hasNonPositive = false;
        double posLo = std::numeric_limits<double>::infinity();
        double posHi = 0.0;
        auto considerPos = [&](double v) {
            if (std::isfinite(v) && v > 0.0) {
                posLo = qMin(posLo, v);
                posHi = qMax(posHi, v);
            }
        };
        const QwtPlotItemList items = d_plot->itemList();
        for (QwtPlotItem *it : items) {
            if (!it || !it->testItemAttribute(QwtPlotItem::AutoScale))
                continue;
            if ((isX && it->xAxis() != a) || (!isX && it->yAxis() != a))
                continue;
            // 点の無い曲線（列が一時的に空）の範囲は Qwt の無効な矩形 (1,1,-2,-2) で、
            // そのまま読むと -1..1 になり「0 以下の値がある」と誤って数えてしまう。
            if (const QwtPlotCurve *pc = dynamic_cast<const QwtPlotCurve *>(it)) {
                if (pc->dataSize() <= 0)
                    continue;
            }
            const QwtDoubleRect r = it->boundingRect();
            const double lo = isX ? qMin(r.left(), r.right()) : qMin(r.top(), r.bottom());
            const double hi = isX ? qMax(r.left(), r.right()) : qMax(r.top(), r.bottom());
            if (!(lo <= hi))
                continue;
            if (lo > 0.0) {
                considerPos(lo);
                considerPos(hi);
                continue;
            }
            hasNonPositive = true;
            QwtPlotCurve *cv = dynamic_cast<QwtPlotCurve *>(it);
            if (!cv) {
                considerPos(hi);
                continue;
            }
            auto *eb = dynamic_cast<QwtErrorPlotCurve *>(cv);
            // Only bars drawn along this axis extend it (as in applyZeroSnapOnAutoAxes); an X
            // error bar on a Y log axis contributes its base point alone.
            if (eb
                && !((isX && eb->direction() == QwtErrorPlotCurve::Horizontal)
                     || (!isX && eb->direction() == QwtErrorPlotCurve::Vertical)))
                eb = nullptr;
            const int n = cv->dataSize();
            for (int k = 0; k < n; ++k) {
                const double base = isX ? cv->x(k) : cv->y(k);
                considerPos(base);
                if (eb) {
                    const double err = eb->errorValue(k);
                    if (std::isfinite(err)) {
                        if (eb->minusSide())
                            considerPos(base - err);
                        if (eb->plusSide())
                            considerPos(base + err);
                    }
                }
            }
        }

        auto *pe = dynamic_cast<PositiveLog10AutoScaleEngine *>(se);
        if (!hasNonPositive || !(posLo > 0.0) || !std::isfinite(posLo)) {
            // All data positive (or nothing positive at all): keep Qwt's default behaviour.
            if (pe)
                pe->clearPositiveRange();
            continue;
        }
        if (!pe) {
            pe = new PositiveLog10AutoScaleEngine();
            pe->setAttributes(se->attributes());
            pe->setReference(se->reference());
            pe->setMargins(se->lowerMargin(), se->upperMargin());
            pe->setPositiveRange(posLo, posHi);
            d_plot->setAxisScaleEngine(a, pe); // deletes the previous engine
        } else {
            pe->setPositiveRange(posLo, posHi);
        }
    }
}

void Graph::applyZeroSnapOnAutoAxes()
{
    if (!d_plot || isPiePlot() || isTernaryPlot())
        return;

    // Log axes: autoscale over the strictly positive data only (0/negative values would drag
    // the lower bound to Qwt's LOG_MIN = 1e-100).
    applyLogAutoScalePositiveRange();

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        if (isToriAxisUserLocked(d_plot, a) || !d_plot->axisAutoScale(a)
            || d_axis_breaks[a].enabled || hasColorScaleOnAxis(a)) {
            continue;
        }
        QwtScaleEngine *se = d_plot->axisScaleEngine(a);
        if (!se || Plot::isLog10ScaleEngine(se) || dynamic_cast<FormulaLinkedScaleEngine *>(se))
            continue;

        const bool isX = (a == QwtPlot::xBottom || a == QwtPlot::xTop);
        bool hasData = false;
        double dataMin = 0.0;
        double dataMax = 0.0;
        auto considerVal = [&](double v) {
            if (!std::isfinite(v))
                return;
            if (!hasData) {
                dataMin = dataMax = v;
                hasData = true;
            } else {
                if (v < dataMin)
                    dataMin = v;
                if (v > dataMax)
                    dataMax = v;
            }
        };

        for (int i = 0; i < n_curves; ++i) {
            const int cType = (i < c_type.size()) ? c_type[i] : -1;
            if (cType == Pie || cType == Box || cType == VerticalBars || cType == HorizontalBars
                || cType == Histogram) {
                continue;
            }
            QwtPlotItem *it = plotItem(i);
            if (!it || !it->isVisible() || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
                continue;
            if ((isX && it->xAxis() != a) || (!isX && it->yAxis() != a))
                continue;
            QwtPlotCurve *cv = curve(i);
            if (!cv)
                continue;
            const DataCurve *dc = dynamic_cast<const DataCurve *>(cv);
            const int n = cv->dataSize();
            if (cType == ErrorBars) {
                auto *eb = dynamic_cast<QwtErrorPlotCurve *>(cv);
                if (!eb || !eb->masterCurve())
                    continue;
                const bool matchesDir = (isX && eb->direction() == QwtErrorPlotCurve::Horizontal)
                        || (!isX && eb->direction() == QwtErrorPlotCurve::Vertical);
                if (!matchesDir)
                    continue;
                for (int k = 0; k < n; ++k) {
                    const double baseV = isX ? eb->x(k) : eb->y(k);
                    const double errV = eb->errorValue(k);
                    if (std::isfinite(baseV) && std::isfinite(errV)) {
                        if (eb->minusSide())
                            considerVal(baseV - errV);
                        if (eb->plusSide())
                            considerVal(baseV + errV);
                    }
                }
                continue;
            }
            Column *dcXCol = nullptr;
            Column *dcYCol = nullptr;
            const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
            if (dcHasMasks)
                dc->resolveColumns(dcXCol, dcYCol);
            for (int k = 0; k < n; ++k) {
                if (dcHasMasks && dc->isPointMaskedFast(k, dcXCol, dcYCol))
                    continue;
                considerVal(isX ? cv->x(k) : cv->y(k));
            }
        }

        const bool snapZero = hasData && dataMin >= 0.0 && dataMax > 0.0
                && dataMin <= 0.06 * dataMax;
        se->setReference(0.0);
        se->setAttribute(QwtScaleEngine::IncludeReference, snapZero);
    }
}

void Graph::setAutoScale()
{
    const MarkerAxisState axesBefore = markerAxisState();
    if (!d_applying_zoom_history)
        recordZoomHistorySnapshot();
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        setToriAxisUserLocked(d_plot, i, false);
        d_plot->setAxisAutoScale(i);
    }
    applyZeroSnapOnAutoAxes();

    d_plot->replot();
    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();
    updateScale(axesBefore);
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();

    emit modifiedGraph();
}

double Graph::effectiveMajorStep(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt || axis >= d_user_step.size())
        return 0.0;
    const double userStep = d_user_step[axis];
    if (!d_plot)
        return userStep;
    if (d_axis_breaks[axis].enabled || !d_custom_tick_positions[axis].isEmpty()
        || !axesFormulas[axis].isEmpty())
        return userStep;
    const QwtScaleEngine *se = d_plot->axisScaleEngine(axis);
    const QwtScaleDiv *sd = d_plot->axisScaleDiv(axis);
    // An autoscaled axis gets its ticks from autoscaling again after reopening.
    if (!se || !sd || Plot::isLog10ScaleEngine(se) || d_plot->axisAutoScale(axis))
        return userStep;
    QwtValueList cur = sd->ticks(QwtScaleDiv::MajorTick);
    if (cur.size() < 2)
        return userStep;
    std::sort(cur.begin(), cur.end());
    const double lo = qMin(sd->lowerBound(), sd->upperBound());
    const double hi = qMax(sd->lowerBound(), sd->upperBound());
    if (!(hi > lo) || !std::isfinite(lo) || !std::isfinite(hi))
        return userStep;
    const int minT = d_plot->axisMaxMinor(axis);
    const int maxMinInt = (minT >= 1) ? (minT + 1) : minT;
    const int maj = d_plot->axisMaxMajor(axis);
    // Same division as Graph::setScale (reopen) and Graph::copy (clone) would make.
    auto majorsFor = [&](double stepSize) {
        QwtLinearScaleEngine eng;
        QwtValueList v = eng.divideScale(lo, hi, maj, maxMinInt, stepSize)
                                 .ticks(QwtScaleDiv::MajorTick);
        std::sort(v.begin(), v.end());
        return v;
    };
    const double step = cur[1] - cur[0];
    if (!(step > 0.0) || !std::isfinite(step))
        return userStep;
    const double eps = 1e-6 * step;
    auto sameTicks = [&](const QwtValueList &v) {
        if (v.size() != cur.size())
            return false;
        for (int i = 0; i < v.size(); ++i) {
            if (std::fabs(v[i] - cur[i]) > eps)
                return false;
        }
        return true;
    };
    if (sameTicks(majorsFor(userStep)))
        return userStep;
    for (int i = 1; i + 1 < cur.size(); ++i) {
        if (std::fabs((cur[i + 1] - cur[i]) - step) > eps)
            return userStep; // uneven ticks cannot be stored as a step
    }
    return sameTicks(majorsFor(step)) ? step : userStep;
}

void Graph::setScale(int axis, double start, double end, double step, int majorTicks,
                     int minorTicks, int type, bool inverted)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt || !std::isfinite(start) || !std::isfinite(end))
        return;

    constexpr double kMaxSafeScaleBound = 1.0e300;
    constexpr double kClampedOverflowBound = 1.0e150;
    if (std::fabs(start) > kMaxSafeScaleBound || std::fabs(end) > kMaxSafeScaleBound
        || !std::isfinite(end - start)) {
        start = qBound(-kClampedOverflowBound, start, kClampedOverflowBound);
        end = qBound(-kClampedOverflowBound, end, kClampedOverflowBound);
        if (std::fabs(end - start) < 1e-300) {
            start -= 1.0;
            end += 1.0;
        }
    }
    if (!std::isfinite(step) || step < 0.0) {
        step = 0.0;
    } else if (step > 0.0) {
        // Tick-count guard. On a log axis the step is in decades, so count decades instead of
        // the linear width (1..1e4 with step = 1 is 4 ticks, not 9999). A non-positive log
        // bound is clamped by the engine itself, so the guard is not applied there.
        double nSteps = 0.0;
        if (type) {
            const double lo = qMin(start, end);
            const double hi = qMax(start, end);
            if (lo > 0.0)
                nSteps = std::fabs(std::log10(hi / lo)) / step;
        } else {
            nSteps = std::fabs(end - start) / step;
        }
        if (!std::isfinite(nSteps) || nSteps > 2000.0)
            step = 0.0;
    }
    majorTicks = qBound(1, majorTicks, 100);
    minorTicks = qBound(0, minorTicks, 50);

    const MarkerAxisState axesBefore = markerAxisState();
    setToriAxisUserLocked(d_plot, axis, true);
    d_plot->setAxisScale(axis, inverted ? qMax(start, end) : qMin(start, end),
                         inverted ? qMin(start, end) : qMax(start, end), step);

    QwtScaleEngine *sc_engine = 0;
    if (type)
        sc_engine = new QwtLog10ScaleEngine();
    else
        sc_engine = new QwtLinearScaleEngine();

    int max_min_intervals = minorTicks;
    if (minorTicks >= 1)
        max_min_intervals = minorTicks + 1;

    QwtScaleDiv div = sc_engine->divideScale(qMin(start, end), qMax(start, end), majorTicks,
                                             max_min_intervals, step);
    if (!type && minorTicks == 1)
        ensureLinearHalfStepMinorTicks(div);
    d_plot->setAxisMaxMajor(axis, majorTicks);
    d_plot->setAxisMaxMinor(axis, minorTicks);

    if (inverted) {
        sc_engine->setAttribute(QwtScaleEngine::Inverted);
        div.invert();
    }

    d_plot->setAxisScaleEngine(axis, sc_engine);
    d_plot->setAxisScaleDiv(axis, div);

    if (!d_weibull_prob_axis[axis] && d_custom_tick_positions[axis].isEmpty()) {
        if (type && axisType[axis] == AxisType::Numeric
            && d_plot->axisLabelFormat(axis) == Plot::Automatic
            && d_plot->axisScaleDraw(axis)->hasComponent(QwtAbstractScaleDraw::Labels)) {
            setLabelsNumericFormat(axis, Plot::Superscripts, 0, axesFormulas[axis]);
        } else if (!type && axisType[axis] == AxisType::Numeric
                   && d_plot->axisLabelFormat(axis) == Plot::Superscripts
                   && d_plot->axisScaleDraw(axis)->hasComponent(QwtAbstractScaleDraw::Labels)) {
            setLabelsNumericFormat(axis, Plot::Automatic, 4, axesFormulas[axis]);
        }
    }

    // A break that the new range cannot show is kept (suspended) instead of being dropped,
    // and restored automatically once the axis range contains it again (C2-#7).
    if (!d_axis_breaks[axis].enabled && d_suspended_axis_breaks[axis].enabled) {
        d_axis_breaks[axis] = d_suspended_axis_breaks[axis];
        d_axis_breaks[axis].enabled = true;
        d_suspended_axis_breaks[axis] = AxisBreakSpec();
    }
    if (d_axis_breaks[axis].enabled) {
        const double lo = qMin(start, end);
        const double hi = qMax(start, end);
        const double bf = qMin(d_axis_breaks[axis].breakFrom, d_axis_breaks[axis].breakTo);
        const double bt = qMax(d_axis_breaks[axis].breakFrom, d_axis_breaks[axis].breakTo);
        const bool isLog = (type != 0);
        const bool logBefore = isLog || d_axis_breaks[axis].log10Before;
        const bool logAfter = isLog || d_axis_breaks[axis].log10After;
        const bool validBreak = (bf > lo && bt < hi && bt > bf + 1e-15)
                && (!logBefore || (lo > 0.0 && bf > 0.0))
                && (!logAfter || (bt > 0.0 && hi > 0.0));
        if (!validBreak) {
            d_suspended_axis_breaks[axis] = d_axis_breaks[axis];
            d_axis_breaks[axis].enabled = false;
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
                sd->setAxisBreakIndicator(false, 0.0);
            const int mirrorAxis = (axis == QwtPlot::xBottom)
                    ? QwtPlot::xTop
                    : ((axis == QwtPlot::yLeft) ? QwtPlot::yRight : -1);
            if (mirrorAxis >= 0 && !d_axis_breaks[mirrorAxis].enabled) {
                if (ScaleDraw *msd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(mirrorAxis)))
                    msd->setAxisBreakIndicator(false, 0.0);
            }
            if (QWidget *tw = window()) {
                if (QMainWindow *mw = qobject_cast<QMainWindow *>(tw)) {
                    if (mw->statusBar())
                        mw->statusBar()->showMessage(
                                tr("Axis break is outside the current axis range; it is kept "
                                   "and will reappear when the range contains it again."),
                                6000);
                }
            }
        } else {
            if (isLog) {
                d_axis_breaks[axis].log10Before = true;
                d_axis_breaks[axis].log10After = true;
            }
            int effMaj = majorTicks;
            if (axisFont(axis).pointSize() >= 20)
                effMaj = qMin(effMaj, 5);
            const int effMinIntervals =
                    (d_axis_breaks[axis].log10Before || d_axis_breaks[axis].log10After)
                    ? qMax(9, max_min_intervals)
                    : max_min_intervals;
            PiecewiseBreakScaleEngine *brkEng = new PiecewiseBreakScaleEngine(d_axis_breaks[axis]);
            QwtScaleDiv brkDiv = brkEng->divideScale(lo, hi, effMaj, effMinIntervals, step);
            if (inverted) {
                brkEng->setAttribute(QwtScaleEngine::Inverted);
                brkDiv.invert();
            }
            d_plot->setAxisScaleEngine(axis, brkEng);
            d_plot->setAxisScaleDiv(axis, brkDiv);
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
                sd->setAxisBreakIndicator(true, bf, bt, d_axis_breaks[axis].log10Before,
                                          d_axis_breaks[axis].log10After);
            }
        }
    }

    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();

    d_user_step[axis] = step;
    syncCustomAndWeibullAxes();

    if (axis == QwtPlot::xBottom || axis == QwtPlot::yLeft) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    d_plot->updateAxes();
    if (!ignoreResize && !d_linked_parent && !d_syncing_link) {
        if (MultiLayer *ml = multiLayerParent()) {
            if (ml->keepAspectRatio()) {
                static thread_local bool s_inSetScaleArrange = false;
                if (!s_inSetScaleArrange) {
                    struct ScopedBoolReset {
                        bool &flag;
                        explicit ScopedBoolReset(bool &f) : flag(f) { flag = true; }
                        ~ScopedBoolReset() { flag = false; }
                    } guard(s_inSetScaleArrange);
                    ml->arrangeLayers(false, false);
                }
            }
        }
    }
    // keep markers on canvas area (also performs the single d_plot->replot())
    updateMarkersBoundingRect(axesBefore);
    syncChildLinkedLayers();
}

void Graph::drawAxesBackbones(bool yes)
{
    drawAxesBackbone = yes;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_axis_backbone[i] = yes;
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(i);
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, yes);
            scale->repaint();
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::loadAxesOptions(const QString &s)
{
    if (s == "1")
        return;

    drawAxesBackbone = false;

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_axis_backbone[i] = false;
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(i);
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, false);
            scale->repaint();
        }
    }
}

void Graph::setAxesLinewidth(int width)
{
    if (d_plot->axesLinewidth() == width)
        return;

    d_plot->setAxesLinewidth(width);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            scale->setPenWidth(width);
            if (scale->isColorBarEnabled()) {
                if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(i)))
                    sd->setColorBarBorder(true, scale->colorBarWidth(), qMax(1, width));
            }
            scale->repaint();
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::loadAxesLinewidth(int width)
{
    d_plot->setAxesLinewidth(width);
}

void Graph::setXAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::xBottom);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setYAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::yLeft);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setRightAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::yRight);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setTopAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::xTop);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setAxesTitleColor(QStringList l)
{
    for (int i = 0; i < int(l.count() - 1); i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QwtText title = scale->title();
            title.setColor(QColor(COLORVALUE(l[i + 1])));
            scale->setTitle(title);
        }
    }
}

QString Graph::axisFormatInfo(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    else
        return axesFormatInfo[axis];
}

int Graph::mapToQwtAxis(int axis)
{
    int a = -1;
    switch (axis) {
    case 0:
        a = QwtPlot::xBottom;
        break;
    case 1:
        a = QwtPlot::yLeft;
        break;
    case 2:
        a = QwtPlot::xTop;
        break;
    case 3:
        a = QwtPlot::yRight;
        break;
    }
    return a;
}

namespace {

class CustomPosTextScaleDraw : public ScaleDraw
{
public:
    CustomPosTextScaleDraw(const QVector<double> &positions, const QStringList &labels)
        : ScaleDraw(), d_positions(positions), d_labels(labels)
    {
    }
    CustomPosTextScaleDraw(const ScaleDraw &other, const QVector<double> &positions,
                           const QStringList &labels)
        : ScaleDraw(other), d_positions(positions), d_labels(labels)
    {
    }

    QwtText label(double value) const override
    {
        if (!std::isfinite(value) || d_positions.isEmpty())
            return QwtText();
        const QwtScaleDiv sc = scaleDiv();
        const double span = std::fabs(sc.upperBound() - sc.lowerBound());
        double tol = 1e-4 * qMax(1.0, span);
        const int n = qMin(d_positions.size(), d_labels.size());
        int bestIdx = -1;
        double bestDist = 1e300;
        for (int i = 0; i < n; ++i) {
            const double d = std::fabs(value - d_positions[i]);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
            if (i > 0) {
                const double gap = std::fabs(d_positions[i] - d_positions[i - 1]);
                if (gap > 1e-12)
                    tol = qMin(tol, 0.25 * gap);
            }
        }
        if (bestIdx >= 0 && bestDist <= qMax(1e-7, tol))
            return QwtText(d_labels[bestIdx]);
        return QwtText();
    }

private:
    QVector<double> d_positions;
    QStringList d_labels;
};

class WeibullProbScaleDraw : public ScaleDraw
{
public:
    WeibullProbScaleDraw() : ScaleDraw() { }
    explicit WeibullProbScaleDraw(const ScaleDraw &other) : ScaleDraw(other) { }

    QwtText label(double value) const override
    {
        if (!std::isfinite(value) || value < -15.0 || value > 4.0)
            return QwtText();

        struct CanonicalWeibullTick {
            double pct;
            const char *txt;
        };
        static const CanonicalWeibullTick kMaj[] = {
            { 0.1, "0.1" },   { 1.0, "1" },     { 5.0, "5" },
            { 10.0, "10" },   { 20.0, "20" },   { 63.21205588285577, "63.2" },
            { 90.0, "90" },   { 99.0, "99" },   { 99.9, "99.9" }
        };
        for (const auto & item : kMaj) {
            const double yRef = std::log(-std::log(1.0 - item.pct / 100.0));
            if (std::fabs(value - yRef) <= 2.5e-3)
                return QwtText(QString::fromLatin1(item.txt));
        }
        if (std::fabs(value - std::log(-std::log(1.0 - 0.632))) <= 2.5e-3)
            return QwtText(QStringLiteral("63.2"));

        const double p = 100.0 * (1.0 - std::exp(-std::exp(value)));
        if (!(p > 0.0 && p < 100.0) || !std::isfinite(p))
            return QwtText();
        return QwtText(QLocale::c().toString(p, 'g', 3));
    }
};

} // namespace

void Graph::setCategoryLabels(int axis, const QStringList &labels)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_custom_category_labels[axis] = labels;
    d_custom_tick_positions[axis].clear();
    d_custom_tick_labels[axis].clear();
    d_custom_tick_rotation[axis] = 0.0;
    d_weibull_prob_axis[axis] = false;
    if (labels.isEmpty())
        return;

    QMap<int, QString> map;
    for (int i = 0; i < labels.size(); ++i)
        map.insert(i + 1, labels[i]);

    axisType[axis] = AxisType::Txt;
    const ScaleDraw *prevSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis));
    QwtTextScaleDraw *tsd =
            prevSd ? new QwtTextScaleDraw(*prevSd, map) : new QwtTextScaleDraw(map);
    tsd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    tsd->setMinorTicksStyle(ScaleDraw::None);
    d_plot->setAxisScaleDraw(axis, tsd);

    double cLo = 0.5;
    double cHi = double(labels.size()) + 0.5;
    if (isToriAxisUserLocked(d_plot, axis)) {
        if (const QwtScaleDiv *curDiv = d_plot->axisScaleDiv(axis)) {
            const double curLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
            const double curHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
            if (std::isfinite(curLo) && std::isfinite(curHi) && curHi > curLo
                && curLo >= -1.0 && curLo <= 1.0
                && curHi >= double(labels.size()) && curHi <= double(labels.size()) + 4.5) {
                cLo = qMin(cLo, curLo);
                cHi = qMax(cHi, curHi);
            }
        }
    }
    const bool isInv = d_plot->axisScaleEngine(axis)
            && d_plot->axisScaleEngine(axis)->testAttribute(QwtScaleEngine::Inverted);
    QwtValueList catMaj;
    for (int i = 1; i <= labels.size(); ++i)
        catMaj += double(i);
    QwtValueList emptyTicks;
    QwtValueList catTicks[QwtScaleDiv::NTickTypes];
    catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
    catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
    catTicks[QwtScaleDiv::MajorTick] = catMaj;
    QwtScaleEngine *catEng = new QwtLinearScaleEngine();
    QwtScaleDiv catDiv(cLo, cHi, catTicks);
    if (isInv) {
        catEng->setAttribute(QwtScaleEngine::Inverted);
        catDiv.invert();
    }
    d_user_step[axis] = 1.0;
    d_plot->setAxisMaxMinor(axis, 0);
    d_plot->setMinorTicksType(axis, ScaleDraw::None);
    d_plot->setAxisMaxMajor(axis, qMax(8, labels.size() + 2));
    d_plot->setAxisScale(axis, isInv ? cHi : cLo, isInv ? cLo : cHi, 1.0);
    d_plot->setAxisScaleEngine(axis, catEng);
    d_plot->setAxisScaleDiv(axis, catDiv);

    const int opp = (axis == QwtPlot::xBottom) ? QwtPlot::xTop
            : (axis == QwtPlot::yLeft)         ? QwtPlot::yRight
                                               : -1;
    if (opp >= 0 && !d_plot->hasVisibleItemOnAxis(opp) && !hasColorScaleOnAxis(opp)
        && !d_weibull_prob_axis[opp] && d_custom_tick_positions[opp].isEmpty()) {
        if (QwtScaleDraw *oppSd = d_plot->axisScaleDraw(opp))
            oppSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
        d_plot->setMinorTicksType(opp, ScaleDraw::None);
        d_plot->setMajorTicksType(opp, ScaleDraw::None);
    }

    if (axis == QwtPlot::yLeft && n_curves > 0) {
        bool allHorizBars = true;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (bi < c_type.size() && c_type[bi] != HorizontalBars && c_type[bi] != ErrorBars) {
                allHorizBars = false;
                break;
            }
        }
        if (allHorizBars
            && (axisType[QwtPlot::xBottom] == AxisType::Txt
                || dynamic_cast<QwtTextScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::xBottom)))) {
            d_custom_category_labels[QwtPlot::xBottom].clear();
            axesFormatInfo[QwtPlot::xBottom].clear();
            setLabelsNumericFormat(QwtPlot::xBottom, Plot::Automatic, 4);
            updateSecondaryAxis(QwtPlot::xTop);
        }
    }

    const QString curPlain = unwrapNoWrapAxisTitle(axisTitle(axis)).trimmed();
    if (isAxisPlaceholderTitle(curPlain)) {
        QwtText emptyT = d_plot->axisTitle(axis);
        emptyT.setText(QString());
        d_plot->setAxisTitle(axis, emptyT);
    }

    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

QStringList Graph::categoryLabels(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QStringList();
    if (!d_custom_tick_labels[axis].isEmpty())
        return d_custom_tick_labels[axis];
    if (!d_custom_category_labels[axis].isEmpty())
        return d_custom_category_labels[axis];
    if (d_plot) {
        if (const QwtTextScaleDraw *tsd =
                    dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(axis))) {
            return tsd->labelsList();
        }
    }
    return QStringList();
}

void Graph::setCustomTickLabels(int axis, const QVector<double> &positions,
                                const QStringList &labels, double rotationDeg)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    const int m = qMin(positions.size(), labels.size());
    d_custom_category_labels[axis].clear();
    d_weibull_prob_axis[axis] = false;
    d_custom_tick_positions[axis].clear();
    d_custom_tick_labels[axis].clear();
    d_custom_tick_rotation[axis] = std::isfinite(rotationDeg) ? rotationDeg : 0.0;

    for (int i = 0; i < m; ++i) {
        if (std::isfinite(positions[i])) {
            d_custom_tick_positions[axis].append(positions[i]);
            d_custom_tick_labels[axis].append(labels[i]);
        }
    }
    if (d_custom_tick_positions[axis].isEmpty())
        return;

    axisType[axis] = AxisType::Numeric;
    if (std::fabs(d_custom_tick_rotation[axis]) > 1e-6)
        setAxisLabelRotation(axis, qRound(d_custom_tick_rotation[axis]));
    else if (labelsRotation(axis) != 0)
        setAxisLabelRotation(axis, 0);

    syncCustomAndWeibullAxes();

    const QString curPlain = unwrapNoWrapAxisTitle(axisTitle(axis)).trimmed();
    if (isAxisPlaceholderTitle(curPlain)) {
        QwtText emptyT = d_plot->axisTitle(axis);
        emptyT.setText(QString());
        d_plot->setAxisTitle(axis, emptyT);
    }

    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setWeibullProbabilityAxis(int axis, bool enabled)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_weibull_prob_axis[axis] = enabled;
    if (!enabled) {
        setLabelsNumericFormat(axis, Plot::Automatic, 4, axesFormulas[axis]);
        d_plot->updateLayout();
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    d_custom_category_labels[axis].clear();
    d_custom_tick_positions[axis].clear();
    d_custom_tick_labels[axis].clear();

    enableAxis(axis, true);
    setAxisBackboneVisible(axis, true);
    d_plot->setMajorTicksType(axis, ScaleDraw::In);
    d_plot->setMinorTicksType(axis, ScaleDraw::In);

    QFont nf = d_plot->axisFont(axis == QwtPlot::yRight ? QwtPlot::yLeft : axis);
    nf.setFamily(QStringLiteral("Arial"));
    nf.setBold(false);
    nf.setWeight(QFont::Normal);
    nf.setHintingPreference(QFont::PreferNoHinting);
    d_plot->setAxisFont(axis, nf);

    syncCustomAndWeibullAxes();

    if (axis == QwtPlot::yRight) {
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight))
            sw->setSpacing(6);
    }

    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::hasWeibullProbabilityAxis(int axis) const
{
    return (axis >= 0 && axis < 4) ? d_weibull_prob_axis[axis] : false;
}

void Graph::syncCustomAndWeibullAxes()
{
    if (!d_plot || d_is_ternary || d_is_polar || isPoleFigurePlot() || isPiePlot())
        return;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        if (!d_custom_tick_positions[a].isEmpty()
            && d_custom_tick_positions[a].size() == d_custom_tick_labels[a].size()) {
            const QVector<double> &pos = d_custom_tick_positions[a];
            const QStringList &lbls = d_custom_tick_labels[a];
            double minPos = pos.first();
            double maxPos = pos.first();
            QwtValueList maj;
            for (double pv : pos) {
                minPos = qMin(minPos, pv);
                maxPos = qMax(maxPos, pv);
                maj += pv;
            }

            const QwtScaleDiv *curDiv = d_plot->axisScaleDiv(a);
            const bool isInv = d_plot->axisScaleEngine(a)
                    && d_plot->axisScaleEngine(a)->testAttribute(QwtScaleEngine::Inverted);
            double cLo = minPos;
            double cHi = maxPos;
            if (curDiv && isToriAxisUserLocked(d_plot, a)) {
                const double dLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
                const double dHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
                if (std::isfinite(dLo) && std::isfinite(dHi) && dHi > dLo
                    && dLo <= minPos + 1e-6 && dHi >= maxPos - 1e-6) {
                    cLo = dLo;
                    cHi = dHi;
                }
            }
            if (!(cHi > cLo)) {
                cLo = minPos - 0.5;
                cHi = maxPos + 0.5;
            } else if (!isToriAxisUserLocked(d_plot, a) && pos.size() >= 2
                       && std::fabs(minPos - 1.0) <= 1e-6
                       && std::fabs(maxPos - double(pos.size())) <= 1e-6) {
                cLo = 0.5;
                cHi = double(pos.size()) + 0.5;
            }

            if (!dynamic_cast<CustomPosTextScaleDraw *>(d_plot->axisScaleDraw(a))) {
                const ScaleDraw *prevSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(a));
                CustomPosTextScaleDraw *csd = prevSd
                        ? new CustomPosTextScaleDraw(*prevSd, pos, lbls)
                        : new CustomPosTextScaleDraw(pos, lbls);
                csd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                csd->setMinorTicksStyle(ScaleDraw::None);
                d_plot->setAxisScaleDraw(a, csd);
            } else if (QwtScaleDraw *sd = d_plot->axisScaleDraw(a)) {
                sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
            }
            if (std::fabs(d_custom_tick_rotation[a]) > 1e-6)
                setAxisLabelRotation(a, qRound(d_custom_tick_rotation[a]));

            QwtValueList emptyTicks;
            QwtValueList ticks[QwtScaleDiv::NTickTypes];
            ticks[QwtScaleDiv::MinorTick] = emptyTicks;
            ticks[QwtScaleDiv::MediumTick] = emptyTicks;
            ticks[QwtScaleDiv::MajorTick] = maj;
            QwtScaleEngine *eng = new QwtLinearScaleEngine();
            QwtScaleDiv div(cLo, cHi, ticks);
            if (isInv) {
                eng->setAttribute(QwtScaleEngine::Inverted);
                div.invert();
            }
            setToriAxisUserLocked(d_plot, a, true);
            d_plot->setAxisMaxMinor(a, 0);
            d_plot->setMinorTicksType(a, ScaleDraw::None);
            d_plot->setAxisMaxMajor(a, qMax(8, pos.size() + 2));
            d_plot->setAxisScaleEngine(a, eng);
            d_plot->setAxisScaleDiv(a, div);

            const int opp = (a == QwtPlot::yLeft) ? QwtPlot::yRight
                    : (a == QwtPlot::xBottom)     ? QwtPlot::xTop
                                                  : -1;
            if (opp >= 0 && d_plot->axisEnabled(opp) && !d_plot->hasVisibleItemOnAxis(opp)
                && !hasColorScaleOnAxis(opp) && !d_weibull_prob_axis[opp]
                && d_custom_tick_positions[opp].isEmpty()
                && d_custom_category_labels[opp].isEmpty()) {
                QwtScaleEngine *oppEng = new QwtLinearScaleEngine();
                if (isInv)
                    oppEng->setAttribute(QwtScaleEngine::Inverted);
                d_plot->setAxisMaxMinor(opp, 0);
                d_plot->setMinorTicksType(opp, ScaleDraw::None);
                d_plot->setAxisScaleEngine(opp, oppEng);
                d_plot->setAxisScaleDiv(opp, div);
            }
            continue;
        }

        if (d_weibull_prob_axis[a]) {
            const Graph *srcG = linkedParentLayer() ? linkedParentLayer() : this;
            const QwtPlot *srcPlot = (srcG && srcG->plotWidget()) ? srcG->plotWidget() : d_plot;
            const int refAxis = (a == QwtPlot::yRight) ? QwtPlot::yLeft
                    : (a == QwtPlot::xTop)             ? QwtPlot::xBottom
                                                       : a;
            const QwtScaleDiv *refDiv = srcPlot->axisScaleDiv(refAxis);
            double lo = refDiv ? qMin(refDiv->lowerBound(), refDiv->upperBound()) : -5.0;
            double hi = refDiv ? qMax(refDiv->lowerBound(), refDiv->upperBound()) : 2.0;
            if (!std::isfinite(lo) || !std::isfinite(hi) || !(hi > lo)
                || (srcG != this && lo == 0.0 && hi == 1000.0)) {
                lo = -5.0;
                hi = 2.0;
            }
            const bool isInv = srcPlot->axisScaleEngine(refAxis)
                    && srcPlot->axisScaleEngine(refAxis)->testAttribute(QwtScaleEngine::Inverted);

            if (!dynamic_cast<WeibullProbScaleDraw *>(d_plot->axisScaleDraw(a))) {
                const ScaleDraw *prevSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(a));
                WeibullProbScaleDraw *wsd =
                        prevSd ? new WeibullProbScaleDraw(*prevSd) : new WeibullProbScaleDraw();
                wsd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                wsd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
                wsd->enableComponent(QwtAbstractScaleDraw::Backbone, true);
                wsd->setMajorTicksStyle(ScaleDraw::In);
                wsd->setMinorTicksStyle(ScaleDraw::In);
                d_plot->setAxisScaleDraw(a, wsd);
            } else if (ScaleDraw *wsd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(a))) {
                wsd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                wsd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
            }

            static const double kMajPcts[] = {
                0.1, 1.0, 5.0, 10.0, 20.0, 63.21205588285577, 90.0, 99.0
            };
            static const double kMinPcts[] = {
                0.2, 0.5, 2.0, 3.0, 15.0, 30.0, 40.0, 50.0, 70.0, 80.0, 95.0, 98.0
            };
            const double eps = 1e-6 * (hi - lo);
            QwtValueList majTicks, minTicks, emptyMed;
            for (double p : kMajPcts) {
                const double yv = std::log(-std::log(1.0 - p / 100.0));
                if (yv >= lo - eps && yv <= hi + eps)
                    majTicks += yv;
            }
            for (double p : kMinPcts) {
                const double yv = std::log(-std::log(1.0 - p / 100.0));
                if (yv >= lo - eps && yv <= hi + eps)
                    minTicks += yv;
            }

            QwtValueList wTicks[QwtScaleDiv::NTickTypes];
            wTicks[QwtScaleDiv::MinorTick] = minTicks;
            wTicks[QwtScaleDiv::MediumTick] = emptyMed;
            wTicks[QwtScaleDiv::MajorTick] = majTicks;
            QwtScaleEngine *wEng = new QwtLinearScaleEngine();
            QwtScaleDiv wDiv(lo, hi, wTicks);
            if (isInv) {
                wEng->setAttribute(QwtScaleEngine::Inverted);
                wDiv.invert();
            }
            enableAxis(a, true);
            d_plot->setMajorTicksType(a, ScaleDraw::In);
            d_plot->setMinorTicksType(a, ScaleDraw::In);
            d_plot->setAxisMaxMajor(a, qMax(8, majTicks.size() + 2));
            d_plot->setAxisMaxMinor(a, 2);
            d_plot->setAxisScaleEngine(a, wEng);
            d_plot->setAxisScaleDiv(a, wDiv);
        }
    }
}

void Graph::setAxisBackboneVisible(int axis, bool visible)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_axis_backbone[axis] = visible;
    bool anyVisible = false;
    for (int i = 0; i < QwtPlot::axisCnt; ++i) {
        if (d_axis_backbone[i])
            anyVisible = true;
    }
    drawAxesBackbone = anyVisible;

    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale) {
        ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
        if (sclDraw)
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, visible);
        scale->repaint();
    }

    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::isAxisBackboneVisible(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return false;
    return drawAxesBackbone && d_axis_backbone[axis];
}

bool Graph::hasColorScaleOnAxis(int axis) const
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return false;
    const QwtScaleWidget *sw = d_plot->axisWidget(axis);
    if (!sw || !sw->isColorBarEnabled())
        return false;
    const QwtPlotItemList &items = d_plot->itemList();
    for (QwtPlotItem *it : items) {
        if (!it || !it->isVisible())
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(it);
            if (sp->colorScaleAxis() == axis)
                return true;
        }
    }
    // E11: the colour bar of a colour-mapped scatter always sits on the right axis.
    if (axis == QwtPlot::yRight && d_point_colorbar_active && pointColorMapCurve())
        return true;
    return false;
}

QFont Graph::axisNumbersFont(int axis) const
{
    if (d_plot && axis >= 0 && axis < QwtPlot::axisCnt)
        return d_plot->axisFont(axis);
    return QFont(QStringLiteral("Arial"), 12, QFont::Normal);
}

QFont Graph::legendFont() const
{
    if (Legend *leg = const_cast<Graph *>(this)->legend())
        return leg->font();
    const QFont axF = axisNumbersFont(QwtPlot::yLeft);
    const double axPt = axF.pointSizeF() > 0.0
            ? axF.pointSizeF()
            : (axF.pointSize() > 0 ? double(axF.pointSize()) : 11.0);
    const double basePt = qMax(9.0, axPt - 2.0);
    QFont f(QStringLiteral("Arial"));
    f.setPointSizeF(basePt);
    f.setWeight(QFont::Normal);
    return f;
}

void Graph::syncAxisColorToCurve(int qwtAxis, int curveIndex)
{
    if (!d_plot || qwtAxis < 0 || qwtAxis >= QwtPlot::axisCnt || curveIndex < 0
        || curveIndex >= n_curves) {
        return;
    }
    const QwtPlotCurve *c = curve(curveIndex);
    if (!c)
        return;
    QColor col = c->pen().color();
    if (!col.isValid() || col.alpha() == 0)
        col = c->symbol().pen().color();
    if (!col.isValid() || col.alpha() == 0)
        col = Qt::black;

    if (QwtScaleWidget *sw = d_plot->axisWidget(qwtAxis)) {
        QPalette pal = sw->palette();
        pal.setColor(QPalette::WindowText, col);
        pal.setColor(QPalette::Text, col);
        sw->setPalette(pal);

        QwtText t = d_plot->axisTitle(qwtAxis);
        t.setColor(col);
        d_plot->setAxisTitle(qwtAxis, t);
        sw->repaint();
    }
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setLinkedParentLayer(Graph *parentLayer, int xLinkMode, const QString &xExpr,
                                 int yLinkMode, const QString &yExpr)
{
    if (parentLayer == this)
        parentLayer = nullptr;

    d_linked_parent = parentLayer;
    d_x_link_mode = xLinkMode;
    d_x_link_expr = xExpr.trimmed().isEmpty() ? QString("x") : xExpr.trimmed();
    d_y_link_mode = yLinkMode;
    d_y_link_expr = yExpr.trimmed().isEmpty() ? QString("y") : yExpr.trimmed();

    if (d_linked_parent)
        syncLinkedAxesFromParent();
}

Graph *Graph::linkedParentLayer() const
{
    return d_linked_parent.data();
}

void Graph::syncLinkedAxesFromParent()
{
    if (d_syncing_link || !d_linked_parent)
        return;

    d_syncing_link = true;
    Plot *parentPlot = d_linked_parent->plotWidget();
    if (!parentPlot) {
        d_syncing_link = false;
        return;
    }

    auto evalEndpoint = [](const QString &expr, double val, bool &ok) -> double {
        ok = false;
        if (expr.isEmpty()) {
            ok = true;
            return val;
        }
        try {
            MyParser parser;
            double v = val;
            parser.DefineVar(_T("x"), &v);
            parser.DefineVar(_T("x1"), &v);
            parser.DefineVar(_T("X"), &v);
            parser.DefineVar(_T("y"), &v);
            parser.DefineVar(_T("y1"), &v);
            parser.DefineVar(_T("Y"), &v);
            parser.SetExpr(expr);
            const double res = parser.Eval();
            if (std::isfinite(res)) {
                ok = true;
                return res;
            }
        } catch (...) {
        }
        return val;
    };

    auto syncOneLinkedPair = [&](int linkMode, const QString &expr, int srcAxis, int dstPrimary,
                                 int dstSecondary) {
        if (linkMode != 1 && linkMode != 2)
            return;
        const QwtScaleDiv *pDiv = parentPlot->axisScaleDiv(srcAxis);
        const QwtScaleEngine *pEng = parentPlot->axisScaleEngine(srcAxis);
        if (!pDiv || !pEng)
            return;

        const double pBound1 = pDiv->lowerBound();
        const double pBound2 = pDiv->upperBound();
        const double pLo = qMin(pBound1, pBound2);
        const double pHi = qMax(pBound1, pBound2);
        const bool parentInverted = pEng->testAttribute(QwtScaleEngine::Inverted);
        const int maj = parentPlot->axisMaxMajor(srcAxis);
        const int minT = parentPlot->axisMaxMinor(srcAxis);

        if (linkMode == 1) {
            if (qAbs(pHi - pLo) <= 1e-300)
                return;
            const int scaleType = (Plot::isLog10ScaleEngine(pEng) && pLo > 0.0) ? 1 : 0;
            auto applyDirect = [&](int targetAxis) {
                QwtScaleEngine *se = scaleType
                        ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                        : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
                int maxMinInt = minT;
                if (minT == 1)
                    maxMinInt = 3;
                else if (minT > 1)
                    maxMinInt = minT + 1;
                QwtScaleDiv div = se->divideScale(pLo, pHi, maj, maxMinInt, 0.0);
                if (parentInverted) {
                    se->setAttribute(QwtScaleEngine::Inverted);
                    div.invert();
                }
                d_plot->setAxisMaxMajor(targetAxis, maj);
                d_plot->setAxisMaxMinor(targetAxis, minT);
                d_plot->setAxisScaleEngine(targetAxis, se);
                d_plot->setAxisScaleDiv(targetAxis, div);
            };
            applyDirect(dstPrimary);
            applyDirect(dstSecondary);
            return;
        }

        // Formula mode (linkMode == 2): precompute 129-point monotone lookup table
        double pLoSafe = pLo;
        const double pHiSafe = pHi;
        if (pLoSafe <= 0.0 && pHiSafe > 0.0 && expr.contains(QLatin1Char('/')))
            pLoSafe = qMax(1e-3, pHiSafe * 0.02);
        if (pHiSafe - pLoSafe <= 1e-300)
            return;

        constexpr int kTablePts = 129;
        QVector<double> xpTable(kTablePts), ucTable(kTablePts);
        bool allFinite = true;
        try {
            MyParser parser;
            double v = pLoSafe;
            parser.DefineVar(_T("x"), &v);
            parser.DefineVar(_T("x1"), &v);
            parser.DefineVar(_T("X"), &v);
            parser.DefineVar(_T("y"), &v);
            parser.DefineVar(_T("y1"), &v);
            parser.DefineVar(_T("Y"), &v);
            parser.SetExpr(expr);
            for (int k = 0; k < kTablePts; ++k) {
                const double xk = pLoSafe
                        + (double(k) / double(kTablePts - 1)) * (pHiSafe - pLoSafe);
                xpTable[k] = xk;
                v = xk;
                const double uk = parser.Eval();
                if (!std::isfinite(uk)) {
                    allFinite = false;
                    break;
                }
                ucTable[k] = uk;
            }
        } catch (...) {
            allFinite = false;
        }

        if (!allFinite) {
            bool ok1 = false, ok2 = false;
            const double c1 = evalEndpoint(expr, pLoSafe, ok1);
            const double c2 = evalEndpoint(expr, pHiSafe, ok2);
            if (!ok1 || !ok2 || qAbs(c2 - c1) <= 1e-300)
                return;
            const double lo = qMin(c1, c2);
            const double hi = qMax(c1, c2);
            const bool shouldInvert = ((c1 > c2) != parentInverted);
            auto applyFallback = [&](int targetAxis) {
                QwtLinearScaleEngine *se = new QwtLinearScaleEngine();
                QwtScaleDiv div = se->divideScale(lo, hi, qMin(qMax(2, maj), 5), 3, 0.0);
                if (shouldInvert) {
                    se->setAttribute(QwtScaleEngine::Inverted, true);
                    div.invert();
                }
                d_plot->setAxisMaxMajor(targetAxis, qMin(qMax(2, maj), 5));
                d_plot->setAxisMaxMinor(targetAxis, 1);
                d_plot->setAxisScaleEngine(targetAxis, se);
                d_plot->setAxisScaleDiv(targetAxis, div);
            };
            applyFallback(dstPrimary);
            applyFallback(dstSecondary);
            return;
        }

        const double c1 = ucTable.first();
        const double c2 = ucTable.last();
        if (qAbs(c2 - c1) <= 1e-300)
            return;

        bool monoInc = true;
        bool monoDec = true;
        for (int k = 0; k < kTablePts - 1; ++k) {
            if (!(ucTable[k + 1] >= ucTable[k]))
                monoInc = false;
            if (!(ucTable[k + 1] <= ucTable[k]))
                monoDec = false;
        }
        const bool isMonotone = (monoInc || monoDec);
        const bool childDecreasing = (c1 > c2);
        const bool shouldInvert = (childDecreasing != parentInverted);
        const double uLo = qMin(c1, c2);
        const double uHi = qMax(c1, c2);
        const int maxMaj = qMin(qMax(2, maj), 5);

        auto applyFormulaAxis = [&](int targetAxis) {
            QwtScaleEngine *se = isMonotone
                    ? static_cast<QwtScaleEngine *>(
                              new FormulaLinkedScaleEngine(xpTable, ucTable, pLo, pHi))
                    : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
            if (shouldInvert)
                se->setAttribute(QwtScaleEngine::Inverted, true);
            QwtScaleDiv div = se->divideScale(uLo, uHi, maxMaj, 3, 0.0);
            if (shouldInvert && div.lowerBound() < div.upperBound())
                div.invert();
            d_plot->setAxisMaxMajor(targetAxis, maxMaj);
            d_plot->setAxisMaxMinor(targetAxis, 1);
            d_plot->setAxisScaleEngine(targetAxis, se);
            d_plot->setAxisScaleDiv(targetAxis, div);
        };

        applyFormulaAxis(dstPrimary);
        applyFormulaAxis(dstSecondary);
    };

    const int srcX = parentPlot->axisEnabled(QwtPlot::xBottom) ? QwtPlot::xBottom : QwtPlot::xTop;
    syncOneLinkedPair(d_x_link_mode, d_x_link_expr, srcX, QwtPlot::xTop, QwtPlot::xBottom);

    const int srcY = parentPlot->axisEnabled(QwtPlot::yLeft) ? QwtPlot::yLeft : QwtPlot::yRight;
    syncOneLinkedPair(d_y_link_mode, d_y_link_expr, srcY, QwtPlot::yRight, QwtPlot::yLeft);

    if ((d_x_link_mode == 1 || d_x_link_mode == 2) && d_y_link_mode == 0
        && !hasColorScaleOnAxis(QwtPlot::yRight)) {
        bool hasRightYItem = false;
        for (int i = 0; i < n_curves; ++i) {
            if (QwtPlotItem *it = plotItem(i)) {
                if (it->yAxis() == QwtPlot::yRight) {
                    hasRightYItem = true;
                    break;
                }
            }
        }
        const QwtScaleDraw *sdRight = d_plot->axisScaleDraw(QwtPlot::yRight);
        const bool rightHasLabels = d_plot->axisEnabled(QwtPlot::yRight) && sdRight
                && sdRight->hasComponent(QwtAbstractScaleDraw::Labels);
        if (!hasRightYItem && !rightHasLabels) {
            if (d_plot->axisEnabled(QwtPlot::yRight))
                d_plot->enableAxis(QwtPlot::yRight, false);
            setAxisBackboneVisible(QwtPlot::yRight, false);
        }
    }

    syncCustomAndWeibullAxes();
    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();
    alignLinkedChildCanvasToParent(this);
    d_plot->replot();
    d_syncing_link = false;
}

void Graph::syncChildLinkedLayers()
{
    if (d_syncing_link)
        return;

    MultiLayer *ml = multiLayerParent();
    if (!ml)
        return;

    struct ScopedSyncFlag {
        bool &flag;
        explicit ScopedSyncFlag(bool &f) : flag(f) { flag = true; }
        ~ScopedSyncFlag() { flag = false; }
    } syncGuard(d_syncing_link);

    const QList<Graph *> allGraphs = ml->layersList();
    for (Graph *g : allGraphs) {
        if (g && g != this && g->linkedParentLayer() == this) {
            g->syncLinkedAxesFromParent();
            g->syncCustomAndWeibullAxes();
        }
    }
}

void Graph::setAxisBreak(int axis, const AxisBreakSpec &spec)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    AxisBreakSpec norm = spec;
    if (norm.breakFrom > norm.breakTo)
        std::swap(norm.breakFrom, norm.breakTo);
    norm.positionPct = qBound(0.10, norm.positionPct, 0.90);

    if (!norm.enabled) {
        // clearAxisBreak() needs the previous (enabled) spec to restore a Log10 axis.
        clearAxisBreak(axis);
        d_axis_breaks[axis] = norm;
        return;
    }

    d_axis_breaks[axis] = norm;
    d_suspended_axis_breaks[axis] = AxisBreakSpec();

    const QwtScaleDiv *oldDiv = d_plot->axisScaleDiv(axis);
    const QwtScaleEngine *oldEng = d_plot->axisScaleEngine(axis);
    const double lo = oldDiv ? qMin(oldDiv->lowerBound(), oldDiv->upperBound()) : 0.0;
    const double hi = oldDiv ? qMax(oldDiv->lowerBound(), oldDiv->upperBound()) : 100.0;
    const bool inv = oldEng ? oldEng->testAttribute(QwtScaleEngine::Inverted) : false;

    const bool validBreak = (norm.breakFrom > lo && norm.breakTo < hi
                             && norm.breakTo > norm.breakFrom + 1e-15)
            && (!norm.log10Before || (lo > 0.0 && norm.breakFrom > 0.0))
            && (!norm.log10After || (norm.breakTo > 0.0 && hi > 0.0));
    if (oldDiv && oldDiv->isValid() && !validBreak) {
        d_suspended_axis_breaks[axis] = norm;
        d_axis_breaks[axis].enabled = false;
        if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
            sd->setAxisBreakIndicator(false, 0.0);
        const int mirrorAxis = (axis == QwtPlot::xBottom)
                ? QwtPlot::xTop
                : ((axis == QwtPlot::yLeft) ? QwtPlot::yRight : -1);
        if (mirrorAxis >= 0 && !d_axis_breaks[mirrorAxis].enabled) {
            if (ScaleDraw *msd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(mirrorAxis)))
                msd->setAxisBreakIndicator(false, 0.0);
        }
        if (QWidget *tw = window()) {
            if (QMainWindow *mw = qobject_cast<QMainWindow *>(tw)) {
                if (mw->statusBar())
                    mw->statusBar()->showMessage(
                            tr("Axis break is outside the current axis range; it is kept "
                               "and will appear once the range contains it."),
                            6000);
            }
        }
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    int maj = d_plot->axisMaxMajor(axis);
    if (axisFont(axis).pointSize() >= 20)
        maj = qMin(maj, 5);
    const int minT = d_plot->axisMaxMinor(axis);
    int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
    if (norm.log10Before || norm.log10After) {
        maxMinInt = qMax(9, maxMinInt);
        if (minT < 8)
            d_plot->setAxisMaxMinor(axis, 8);
    }

    PiecewiseBreakScaleEngine *eng = new PiecewiseBreakScaleEngine(norm);
    QwtScaleDiv div = eng->divideScale(lo, hi, maj, maxMinInt, d_user_step[axis]);
    if (inv) {
        eng->setAttribute(QwtScaleEngine::Inverted);
        div.invert();
    }

    d_plot->setAxisScaleEngine(axis, eng);
    d_plot->setAxisScaleDiv(axis, div);

    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        sd->setAxisBreakIndicator(true, norm.breakFrom, norm.breakTo, norm.log10Before,
                                  norm.log10After);
    }

    if (axis == QwtPlot::xBottom)
        updateSecondaryAxis(QwtPlot::xTop);
    else if (axis == QwtPlot::yLeft)
        updateSecondaryAxis(QwtPlot::yRight);

    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

Graph::AxisBreakSpec Graph::axisBreak(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return AxisBreakSpec();
    return d_axis_breaks[axis];
}

Graph::AxisBreakSpec Graph::suspendedAxisBreak(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return AxisBreakSpec();
    return d_suspended_axis_breaks[axis];
}

bool Graph::axisBreakSuspended(int axis) const
{
    return axis >= 0 && axis < QwtPlot::axisCnt && !d_axis_breaks[axis].enabled
            && d_suspended_axis_breaks[axis].enabled;
}

void Graph::refreshAxisBreakIndicator(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt || !d_plot)
        return;
    ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis));
    if (!sd)
        return;
    const AxisBreakSpec *brk = nullptr;
    if (d_axis_breaks[axis].enabled) {
        brk = &d_axis_breaks[axis];
    } else {
        const int primary = (axis == QwtPlot::xTop)
                ? QwtPlot::xBottom
                : ((axis == QwtPlot::yRight) ? QwtPlot::yLeft : -1);
        if (primary >= 0 && d_axis_breaks[primary].enabled && !hasColorScaleOnAxis(axis))
            brk = &d_axis_breaks[primary];
    }
    if (!brk) {
        sd->setAxisBreakIndicator(false, 0.0);
        return;
    }
    const QwtScaleDiv *div = d_plot->axisScaleDiv(axis);
    const double lo = div ? qMin(div->lowerBound(), div->upperBound()) : 0.0;
    const double hi = div ? qMax(div->lowerBound(), div->upperBound()) : 0.0;
    const double bf = qMin(brk->breakFrom, brk->breakTo);
    const double bt = qMax(brk->breakFrom, brk->breakTo);
    const bool straddles = (bf > lo && bt < hi && bt > bf + 1e-15);
    sd->setAxisBreakIndicator(straddles, bf, bt, brk->log10Before, brk->log10After);
}

void Graph::clearAxisBreak(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    // Restore the logarithmic scale type when the broken axis was log on both sides
    // (previously clearing a break always forced a linear axis).
    const AxisBreakSpec prevBrk =
            d_axis_breaks[axis].enabled ? d_axis_breaks[axis] : d_suspended_axis_breaks[axis];
    const bool prevWasBreak = d_axis_breaks[axis].enabled || d_suspended_axis_breaks[axis].enabled;
    d_axis_breaks[axis].enabled = false;
    d_suspended_axis_breaks[axis] = AxisBreakSpec();
    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
        sd->setAxisBreakIndicator(false, 0.0);

    const QwtScaleDiv *oldDiv = d_plot->axisScaleDiv(axis);
    const QwtScaleEngine *oldEng = d_plot->axisScaleEngine(axis);
    const double lo = oldDiv ? qMin(oldDiv->lowerBound(), oldDiv->upperBound()) : 0.0;
    const double hi = oldDiv ? qMax(oldDiv->lowerBound(), oldDiv->upperBound()) : 100.0;
    const bool inv = oldEng ? oldEng->testAttribute(QwtScaleEngine::Inverted) : false;
    bool wasLog = prevWasBreak ? (prevBrk.log10Before && prevBrk.log10After)
                               : (oldEng && Plot::isLog10ScaleEngine(oldEng));
    if (wasLog && !(lo > 0.0 && hi > 0.0))
        wasLog = false;

    setScale(axis, lo, hi, d_user_step[axis], d_plot->axisMaxMajor(axis),
             d_plot->axisMaxMinor(axis), wasLog ? 1 : 0, inv);

    const int mirrorAxis = (axis == QwtPlot::xBottom)
            ? QwtPlot::xTop
            : ((axis == QwtPlot::yLeft) ? QwtPlot::yRight : -1);
    if (mirrorAxis >= 0 && !d_axis_breaks[mirrorAxis].enabled) {
        if (ScaleDraw *msd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(mirrorAxis)))
            msd->setAxisBreakIndicator(false, 0.0);
    }

    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTickLabelTransform(int axis, double divideByFactor, const QString &prefix,
                                  const QString &suffix)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    if (!std::isfinite(divideByFactor) || qAbs(divideByFactor) <= 1e-300)
        divideByFactor = 1.0;

    d_tick_divide_factor[axis] = divideByFactor;
    d_tick_prefix[axis] = prefix;
    d_tick_suffix[axis] = suffix;

    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        sd->setTickLabelTransform(divideByFactor, prefix, suffix);
    }
    d_plot->updateLayout();
    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

double Graph::tickDivideFactor(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return 1.0;
    return d_tick_divide_factor[axis];
}

QString Graph::tickPrefix(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return d_tick_prefix[axis];
}

QString Graph::tickSuffix(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return d_tick_suffix[axis];
}

void Graph::setAutoScaleAxis(int qwtAxisId)
{
    if (qwtAxisId < 0 || qwtAxisId >= QwtPlot::axisCnt || !d_plot || isPiePlot()
        || isTernaryPlot()) {
        return;
    }
    const MarkerAxisState axesBefore = markerAxisState();
    if (!d_applying_zoom_history)
        recordZoomHistorySnapshot();

    int targetAxis = qwtAxisId;
    if (qwtAxisId == QwtPlot::xTop || qwtAxisId == QwtPlot::yRight) {
        bool hasOwnItem = false;
        for (int i = 0; i < n_curves; ++i) {
            QwtPlotItem *it = plotItem(i);
            if (!it || !it->isVisible())
                continue;
            if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                Spectrogram *sp = static_cast<Spectrogram *>(it);
                if (sp->colorScaleAxis() == qwtAxisId) {
                    setToriAxisUserLocked(d_plot, qwtAxisId, false);
                    d_plot->setAxisScale(qwtAxisId, sp->colorAxisRange().minValue(),
                                         sp->colorAxisRange().maxValue());
                    d_user_step[qwtAxisId] = 0.0;
                    d_plot->replot();
                    emit modifiedGraph();
                    return;
                }
            }
            if ((qwtAxisId == QwtPlot::xTop && it->xAxis() == QwtPlot::xTop)
                || (qwtAxisId == QwtPlot::yRight && it->yAxis() == QwtPlot::yRight)) {
                hasOwnItem = true;
                break;
            }
        }
        if (!hasOwnItem)
            targetAxis = (qwtAxisId == QwtPlot::xTop) ? QwtPlot::xBottom : QwtPlot::yLeft;
    }

    setToriAxisUserLocked(d_plot, targetAxis, false);
    d_user_step[targetAxis] = 0.0;
    d_plot->setAxisAutoScale(targetAxis);
    applyZeroSnapOnAutoAxes();
    d_plot->updateAxes();

    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);
    updateMarkersBoundingRect(axesBefore);
    d_zoomer[0]->setZoomBase(false);
    d_zoomer[1]->setZoomBase(false);
    updateStatReferenceLines();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();
    emit modifiedGraph();
}
