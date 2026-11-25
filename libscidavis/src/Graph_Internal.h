/***************************************************************************
    File                 : Graph_Internal.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Description          : Internal helpers shared across Graph translation units

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

#ifndef GRAPH_INTERNAL_H
#define GRAPH_INTERNAL_H

#include "Graph.h"
#include "SelectionMoveResizer.h"
#include "core/column/Column.h"

#include <QByteArray>
#include <QPointer>
#include <QRegularExpression>
#include <QString>
#include <QVector>
#include <QWidget>

#include <qwt_plot.h>
#include <qwt_plot_item.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>

#include <cmath>

namespace GraphInternal {

// Cross-track contract (A3-8 / E1-4): Column::UnitFormat::Bracket == 2.
inline constexpr Column::UnitFormat kUnitFormatBracket = static_cast<Column::UnitFormat>(2);

//! Single-pass remap of 1-based \c{k}, \l(k), %(k) curve references using oldToNew index map.
inline QString remapLegendCurveTokens(const QString &src, const QVector<int> &oldToNew)
{
    static const QRegularExpression tokenRx(
            QStringLiteral(R"((\\c\{(\d+)\})|(\\l\((\d+)\))|(%\((\d+)\)))"));
    QRegularExpressionMatchIterator mit = tokenRx.globalMatch(src);
    if (!mit.hasNext())
        return src;

    const int nCurves = oldToNew.size();
    QString dst;
    dst.reserve(src.size() + 8);
    int lastPos = 0;
    bool textChanged = false;
    while (mit.hasNext()) {
        const QRegularExpressionMatch m = mit.next();
        dst.append(src.mid(lastPos, m.capturedStart() - lastPos));
        lastPos = m.capturedEnd();

        QString replacement = m.captured(0);
        auto remapIdx = [&](const QString &numStr, const QString &fmt) {
            bool ok = false;
            const int oldOneBased = numStr.toInt(&ok);
            if (ok && oldOneBased >= 1 && oldOneBased <= nCurves) {
                const int mappedZeroBased = oldToNew[oldOneBased - 1];
                if (mappedZeroBased >= 0)
                    replacement = fmt.arg(mappedZeroBased + 1);
            }
        };
        if (!m.captured(2).isEmpty())
            remapIdx(m.captured(2), QStringLiteral("\\c{%1}"));
        else if (!m.captured(4).isEmpty())
            remapIdx(m.captured(4), QStringLiteral("\\l(%1)"));
        else if (!m.captured(6).isEmpty())
            remapIdx(m.captured(6), QStringLiteral("%(%1)"));

        if (replacement != m.captured(0))
            textChanged = true;
        dst.append(replacement);
    }
    dst.append(src.mid(lastPos));
    return textChanged ? dst : src;
}

inline void detachAndDeleteMarkerSelector(QPointer<SelectionMoveResizer> &sel)
{
    if (!sel)
        return;
    SelectionMoveResizer *old = sel.data();
    sel = nullptr;
    const bool hadFocus = old->hasFocus();
    QWidget *pw = old->parentWidget();
    old->blockSignals(true);
    old->disconnect();
    if (pw)
        pw->removeEventFilter(old);
    old->hide();
    old->setParent(nullptr);
    if (pw && hadFocus)
        pw->setFocus();
    old->deleteLater();
}

inline QString unwrapNoWrapAxisTitle(const QString &s)
{
    QString out = s;
    static const QString kPrefix = QStringLiteral("<span style=\"white-space:nowrap;\">");
    static const QString kSuffix = QStringLiteral("</span>");
    while (out.startsWith(kPrefix, Qt::CaseInsensitive)
           && out.endsWith(kSuffix, Qt::CaseInsensitive)
           && out.length() >= kPrefix.length() + kSuffix.length()) {
        out = out.mid(kPrefix.length(), out.length() - kPrefix.length() - kSuffix.length());
    }
    return out;
}

inline void ensureLinearHalfStepMinorTicks(QwtScaleDiv &div)
{
    if (!div.ticks(QwtScaleDiv::MinorTick).isEmpty()
        || !div.ticks(QwtScaleDiv::MediumTick).isEmpty()) {
        return;
    }
    const QwtValueList maj = div.ticks(QwtScaleDiv::MajorTick);
    if (maj.size() < 2)
        return;
    const double step = std::fabs(maj[1] - maj[0]);
    if (!(step > 1e-300) || !std::isfinite(step))
        return;
    const double half = 0.5 * step;
    const double lo = qMin(div.lowerBound(), div.upperBound());
    const double hi = qMax(div.lowerBound(), div.upperBound());
    const double eps = 1e-9 * step;
    double firstMaj = maj.first();
    double lastMaj = maj.last();
    if (firstMaj > lastMaj)
        qSwap(firstMaj, lastMaj);

    QwtValueList mids;
    if (firstMaj - half >= lo - eps)
        mids += (firstMaj - half);
    for (int i = 0; i + 1 < maj.size(); ++i) {
        const double m = 0.5 * (maj[i] + maj[i + 1]);
        if (m >= lo - eps && m <= hi + eps)
            mids += m;
    }
    if (lastMaj + half <= hi + eps)
        mids += (lastMaj + half);
    if (!mids.isEmpty())
        div.setTicks(QwtScaleDiv::MediumTick, mids);
}

inline int readAxisBaselineDist(const QwtScaleWidget *scale)
{
    if (!scale)
        return 0;
    const int neg = scale->property("_toriNegBaseline").toInt();
    return (neg < 0) ? neg : scale->margin();
}

inline QString firstNonEmptyCommentLine(const QString &comment)
{
    QString norm = comment;
    norm.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    norm.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = norm.split(QLatin1Char('\n'));
    for (const QString &ln : lines) {
        const QString t = ln.trimmed();
        if (!t.isEmpty())
            return t;
    }
    return QString();
}

inline bool isAxisPlaceholderTitle(const QString &plain)
{
    return plain.isEmpty() || plain == QStringLiteral("X Axis Title")
            || plain == QStringLiteral("Y Axis Title") || plain == Graph::tr("X Axis Title")
            || plain == Graph::tr("Y Axis Title");
}

inline QByteArray toriUserLockedAxisProp(int axis)
{
    return QByteArray("_toriUserLockedAxis_") + QByteArray::number(axis);
}

inline bool isToriAxisUserLocked(const QwtPlot *plot, int axis)
{
    return plot && axis >= 0 && axis < QwtPlot::axisCnt
            && plot->property(toriUserLockedAxisProp(axis).constData()).toBool();
}

inline void setToriAxisUserLocked(QwtPlot *plot, int axis, bool locked)
{
    if (plot && axis >= 0 && axis < QwtPlot::axisCnt)
        plot->setProperty(toriUserLockedAxisProp(axis).constData(), locked);
}

class PiecewiseBreakScaleEngine : public QwtLinearScaleEngine
{
public:
    explicit PiecewiseBreakScaleEngine(const Graph::AxisBreakSpec &spec);

    QwtScaleTransformation *transformation() const override;
    QwtScaleDiv divideScale(double x1, double x2, int maxMajSteps, int maxMinSteps,
                            double stepSize = 0.0) const override;

private:
    Graph::AxisBreakSpec d_spec;
};

//! Log10 engine for auto-scaled axes whose data contain zero or negative values.
//! QwtLog10ScaleEngine::autoScale clamps a non-positive lower bound to LOG_MIN (1e-100), which
//! stretches the axis over ~100 decades. Graph::applyLogAutoScalePositiveRange() installs this
//! engine with the range of the strictly positive data so that autoscaling only uses those.
class PositiveLog10AutoScaleEngine : public QwtLog10ScaleEngine
{
public:
    void setPositiveRange(double lo, double hi)
    {
        d_posLo = lo;
        d_posHi = hi;
    }
    void clearPositiveRange() { d_posLo = d_posHi = 0.0; }

    void autoScale(int maxSteps, double &x1, double &x2, double &stepSize) const override
    {
        if (d_posLo > 0.0 && d_posHi >= d_posLo && std::isfinite(d_posHi)) {
            double lo = qMin(x1, x2);
            double hi = qMax(x1, x2);
            if (!(lo > 0.0) || !std::isfinite(lo))
                lo = d_posLo;
            if (!(hi > 0.0) || !std::isfinite(hi))
                hi = d_posHi;
            if (hi < lo)
                hi = lo;
            x1 = lo;
            x2 = hi;
        }
        QwtLog10ScaleEngine::autoScale(maxSteps, x1, x2, stepSize);
    }

private:
    double d_posLo = 0.0;
    double d_posHi = 0.0;
};

class TernaryDiagramItem : public QwtPlotItem
{
public:
    explicit TernaryDiagramItem(const Graph *graph);

    int rtti() const override;
    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override;

private:
    const Graph *d_graph;
};

} // namespace GraphInternal

#endif // GRAPH_INTERNAL_H
