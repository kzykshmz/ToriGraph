/***************************************************************************
    File                 : Legend.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Legend marker (extension to QwtPlotMarker)

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
#include "Legend.h"
#include "QwtPieCurve.h"
#include "QwtErrorPlotCurve.h"
#include "ScaleDraw.h"
#include "VectorCurve.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPolygon>
#include <QMessageBox>
#include <limits>

#include <qwt_plot.h>
#include <qwt_scale_widget.h>
#include <qwt_painter.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_layout_metrics.h>
#include <qwt_symbol.h>

namespace {
static int extractLegendCurveIndex(const QString &line)
{
    if (line.contains("\\c{")) {
        const int pos = line.indexOf("\\c{");
        const int pos2 = line.indexOf('}', pos + 3);
        if (pos >= 0 && pos2 > pos + 3) {
            bool ok = false;
            const int k = line.mid(pos + 3, pos2 - (pos + 3)).toInt(&ok);
            if (ok && k >= 1)
                return k - 1;
        }
    } else if (line.contains("\\l(")) {
        const int pos = line.indexOf("\\l(");
        const int pos2 = line.indexOf(')', pos + 3);
        if (pos >= 0 && pos2 > pos + 3) {
            bool ok = false;
            const int k = line.mid(pos + 3, pos2 - (pos + 3)).toInt(&ok);
            if (ok && k >= 1)
                return k - 1;
        }
    }
    return -1;
}

static QStringList splitRawLegendLines(const QString &rawText)
{
    QString norm = rawText;
    norm.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    norm.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    while (norm.endsWith(QLatin1Char('\n')) && norm.size() > 1)
        norm.chop(1);
    if (norm == QLatin1String("\n"))
        return QStringList{ QString() };
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return norm.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
#else
    return norm.split(QLatin1Char('\n'), QString::KeepEmptyParts);
#endif
}

static QStringList filterVisibleLegendLines(Plot *plot, const QString &rawText,
                                            QVector<int> *outRawIndices = nullptr)
{
    if (outRawIndices)
        outRawIndices->clear();

    const QString trimmed = rawText.trimmed();
    if (trimmed.isEmpty()) {
        if (outRawIndices)
            outRawIndices->append(0);
        return QStringList{ QString() };
    }

    Graph *g = plot ? qobject_cast<Graph *>(plot->parent()) : nullptr;
    const QStringList allLines = splitRawLegendLines(rawText);
    QStringList visible;
    visible.reserve(allLines.size());
    for (int i = 0; i < allLines.size(); ++i) {
        const QString &line = allLines[i];
        const int cv = extractLegendCurveIndex(line);
        if (cv >= 0 && g && cv < g->curves()) {
            QwtPlotItem *it = g->plotItem(cv);
            if (it && !it->isVisible())
                continue;
        }
        visible.append(line);
        if (outRawIndices)
            outRawIndices->append(i);
    }
    return visible;
}

static bool isCompactLegendFont(const QFont &fnt)
{
    const double pt = fnt.pointSizeF();
    if (pt > 0.0)
        return pt <= 9.5;
    return QFontMetricsF(fnt).height() <= 16.0;
}

static int effectiveSampleGap(const QwtText *txt, int defaultHspace)
{
    if (txt && isCompactLegendFont(txt->font()))
        return qMin(defaultHspace, 5);
    return defaultHspace;
}
} // namespace

Legend::Legend(Plot *plot) : d_plot(plot), d_frame(0), d_angle(0), d_num_columns(1)
{
    d_text = new QwtText(QString(), QwtText::RichText);
    QFont legFnt("Arial", 12, QFont::Normal, false);
    legFnt.setHintingPreference(QFont::PreferNoHinting);
    d_text->setFont(legFnt);
    d_text->setRenderFlags(Qt::AlignTop | Qt::AlignLeft);
    d_text->setBackgroundBrush(QBrush(Qt::NoBrush));
    d_text->setColor(Qt::black);
    d_text->setBackgroundPen(QPen(Qt::NoPen));
    d_text->setPaintAttribute(QwtText::PaintBackground);

    hspace = 10;
    left_margin = 9;
    top_margin = 6;
    d_shadow_size_x = 5;
    d_shadow_size_y = 5;
}

void Legend::draw(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                  const QRect &canvasRect) const
{
    if (!d_text)
        return;
    const QString rawText = d_text->text();
    if (!rawText.trimmed().isEmpty() && filterVisibleLegendLines(d_plot, rawText).isEmpty())
        return;

    const int x = xMap.transform(xValue());
    const int y = yMap.transform(yValue());

    const QwtMetricsMap savedMetricsMap = QwtPainter::metricsMap();
    struct ScopedMetricsMapRestore {
        QwtMetricsMap prev;
        ~ScopedMetricsMapRestore() { QwtPainter::setMetricsMap(prev); }
    } metricsGuard{ savedMetricsMap };
    QwtPainter::setMetricsMap(plot(), p->device());
    const QwtMetricsMap map = QwtPainter::metricsMap();

    const int symbolLineLength = symbolsMaxLineLength();

    int width = 0, height = 0;
    QwtArray<long> heights = itemsHeight(map.deviceToLayoutY(y), symbolLineLength, width, height);
    if (width <= 0 || height <= 0)
        return;

    const bool intentionallyOutside = canvasRect.isValid()
            && (x >= canvasRect.right() - 2 || y >= canvasRect.bottom() - 2
                || x < canvasRect.left() - 2 || y < canvasRect.top() - 2);

    int devX = x;
    int devY = y;
    if (!intentionallyOutside && d_frame == 0 && canvasRect.isValid()) {
        Graph *g = d_plot ? qobject_cast<Graph *>(d_plot->parent()) : nullptr;
        auto checkSdInward = [&](int axisId) -> bool {
            if (!d_plot || !d_plot->axisEnabled(axisId))
                return false;
            const ScaleDraw *sd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axisId));
            if (!sd)
                return false;
            return sd->majorTicksStyle() == ScaleDraw::In
                    || sd->majorTicksStyle() == ScaleDraw::Both
                    || sd->minorTicksStyle() == ScaleDraw::In
                    || sd->minorTicksStyle() == ScaleDraw::Both;
        };
        auto sideHasInwardTicks = [&](int ax, int mirrorAx) -> bool {
            if (g && g->hasColorScaleOnAxis(ax))
                return g->axesBackbones() && checkSdInward(mirrorAx);
            return checkSdInward(ax);
        };

        const int majLen = d_plot ? qMax(0, d_plot->majorTickLength()) : 8;
        const int reqInL = (d_frame == None) ? qMax(2, majLen + 2 - left_margin)
                                             : qMax(2, majLen + 3);
        const int reqInR = (d_frame == None && d_num_columns <= 1)
                ? qMax(2, majLen + 2)
                : ((d_frame == None) ? qMax(2, majLen + 2 - (left_margin + 4))
                                     : qMax(2, majLen + 3));
        const int reqInT = (d_frame == None) ? qMax(2, majLen + 2 - top_margin)
                                             : qMax(2, majLen + 3);
        const int reqInB = (d_frame == None) ? qMax(2, majLen + 2 - top_margin)
                                             : qMax(2, majLen + 3);
        const int layInL = map.isIdentity() ? reqInL : map.screenToLayoutX(reqInL);
        const int layInR = map.isIdentity() ? reqInR : map.screenToLayoutX(reqInR);
        const int layInT = map.isIdentity() ? reqInT : map.screenToLayoutY(reqInT);
        const int layInB = map.isIdentity() ? reqInB : map.screenToLayoutY(reqInB);
        const int layPlainX = map.isIdentity() ? 2 : map.screenToLayoutX(2);
        const int layPlainY = map.isIdentity() ? 2 : map.screenToLayoutY(2);

        const int layClearL = sideHasInwardTicks(QwtPlot::yLeft, QwtPlot::yRight) ? layInL : layPlainX;
        const int layClearR = sideHasInwardTicks(QwtPlot::yRight, QwtPlot::yLeft) ? layInR : layPlainX;
        const int layClearT = sideHasInwardTicks(QwtPlot::xTop, QwtPlot::xBottom) ? layInT : layPlainY;
        const int layClearB = sideHasInwardTicks(QwtPlot::xBottom, QwtPlot::xTop) ? layInB : layPlainY;

        int clampWidth = width;
        if (d_frame == None && d_num_columns <= 1) {
            const int trailLayout = map.isIdentity() ? (left_margin + 8)
                                                     : map.screenToLayoutX(left_margin + 8);
            clampWidth = qMax(16, width - trailLayout);
        }

        const QPoint devZero = map.layoutToDevice(QPoint(0, 0));
        const QPoint devSize = map.layoutToDevice(QPoint(clampWidth, height)) - devZero;
        const QPoint devClearLT = map.layoutToDevice(QPoint(layClearL, layClearT)) - devZero;
        const QPoint devClearRB = map.layoutToDevice(QPoint(layClearR, layClearB)) - devZero;

        const int devW = qMax(1, devSize.x());
        const int devH = qMax(1, devSize.y());
        const int clearL = qMax(0, devClearLT.x());
        const int clearT = qMax(0, devClearLT.y());
        const int clearR = qMax(0, devClearRB.x());
        const int clearB = qMax(0, devClearRB.y());

        const int minX = canvasRect.left() + clearL;
        const int maxX = qMax(minX, canvasRect.right() - devW - clearR);
        const int minY = canvasRect.top() + clearT;
        const int maxY = qMax(minY, canvasRect.bottom() - devH - clearB);

        devX = qBound(minX, x, maxX);
        devY = qBound(minY, y, maxY);
        if (devY != y)
            heights = itemsHeight(map.deviceToLayoutY(devY), symbolLineLength, width, height);
    }

    QRect rs = QRect(map.deviceToLayout(QPoint(devX, devY)), QSize(width, height));

    p->save();
    if (p->hasClipping() && intentionallyOutside)
        p->setClipping(false);

    drawFrame(p, d_frame, rs);
    drawSymbols(p, rs, heights, symbolLineLength);
    drawLegends(p, rs, heights, symbolLineLength);
    p->restore();
}

void Legend::setText(const QString &s)
{
    d_text->setText(s);
}

void Legend::setFrameStyle(int style)
{
    if (d_frame == style)
        return;

    d_frame = style;
}

void Legend::setBackgroundColor(const QColor &c)
{
    if (d_text->backgroundBrush().color() == c)
        return;

    d_text->setBackgroundBrush(QBrush(c));
}

QRect Legend::rect() const
{
    const QwtScaleMap &xMap = d_plot->canvasMap(xAxis());
    const QwtScaleMap &yMap = d_plot->canvasMap(yAxis());

    const int x = xMap.transform(xValue());
    const int y = yMap.transform(yValue());

    if (d_text && !d_text->text().trimmed().isEmpty()
        && filterVisibleLegendLines(d_plot, d_text->text()).isEmpty()) {
        return QRect(QPoint(x, y), QSize(0, 0));
    }

    int width = 0, height = 0;
    itemsHeight(y, symbolsMaxLineLength(), width, height);

    return QRect(QPoint(x, y), QSize(qMax(0, width - 1), qMax(0, height - 1)));
}

QwtDoubleRect Legend::boundingRect() const
{
    QRect bounding_rect = rect();
    const QwtScaleMap &x_map = d_plot->canvasMap(xAxis());
    const QwtScaleMap &y_map = d_plot->canvasMap(yAxis());

    double left = x_map.invTransform(bounding_rect.left());
    double right = x_map.invTransform(bounding_rect.right());
    double top = y_map.invTransform(bounding_rect.top());
    double bottom = y_map.invTransform(bounding_rect.bottom());

    return QwtDoubleRect(left, top, qAbs(right - left), qAbs(bottom - top));
}

void Legend::setTextColor(const QColor &c)
{
    if (c == d_text->color())
        return;

    d_text->setColor(c);
}

void Legend::setOrigin(const QPoint &p)
{
    d_pos = p;

    const QwtScaleMap &xMap = d_plot->canvasMap(xAxis());
    const QwtScaleMap &yMap = d_plot->canvasMap(yAxis());

    setXValue(xMap.invTransform(p.x()));
    setYValue(yMap.invTransform(p.y()));
}

void Legend::updateOrigin()
{
    if (!d_plot)
        return;

    const QwtScaleMap &xMap = d_plot->canvasMap(xAxis());
    const QwtScaleMap &yMap = d_plot->canvasMap(yAxis());

    const QwtScaleDiv *xScDiv = d_plot->axisScaleDiv(xAxis());
    double xVal = xMap.invTransform(d_pos.x());
    if (!xScDiv->contains(xVal))
        return;

    const QwtScaleDiv *yScDiv = d_plot->axisScaleDiv(yAxis());
    double yVal = yMap.invTransform(d_pos.y());
    if (!yScDiv->contains(yVal))
        return;

    setXValue(xVal);
    setYValue(yVal);
}

void Legend::setOriginCoord(double x, double y)
{
    if (xValue() == x && yValue() == y)
        return;

    setXValue(x);
    setYValue(y);

    const QwtScaleMap &xMap = d_plot->canvasMap(xAxis());
    const QwtScaleMap &yMap = d_plot->canvasMap(yAxis());

    d_pos = QPoint(xMap.transform(x), yMap.transform(y));
}

void Legend::setFont(const QFont &font)
{
    QFont resolved = font;
    if (resolved.family().trimmed().isEmpty())
        resolved.setFamily("Arial");
    resolved.setHintingPreference(QFont::PreferNoHinting);
    if (resolved == d_text->font()
        && resolved.hintingPreference() == d_text->font().hintingPreference())
        return;

    d_text->setFont(resolved);
}

void Legend::setNumColumns(int cols)
{
    const int clamped = qMax(1, cols);
    if (d_num_columns == clamped)
        return;
    d_num_columns = clamped;
}

void Legend::computeCellsLayout(int baseX, int baseY, int symbolLineLength, QVector<int> &cellX,
                                QVector<int> &cellCenterY, int &totalWidth, int &totalHeight) const
{
    totalWidth = 0;
    totalHeight = 0;

    const bool scatterOnly = isScatterOnly();
    const bool compact = d_text && isCompactLegendFont(d_text->font());
    const int sampleLen = scatterOnly ? qMax(symbolLineLength, compact ? 11 : 14) : symbolLineLength;
    const int sampleOffset = sampleLen + effectiveSampleGap(d_text, hspace);

    const QString rawText = d_text ? d_text->text() : QString();
    const QStringList titles = filterVisibleLegendLines(d_plot, rawText);
    const int n = titles.count();
    cellX.resize(n);
    cellCenterY.resize(n);
    if (n <= 0) {
        if (rawText.trimmed().isEmpty()) {
            totalWidth = 2 * left_margin;
            totalHeight = 2 * top_margin;
        }
        return;
    }

    const int cols = qBound(1, d_num_columns, n);
    const int rows = (n + cols - 1) / cols;
    QVector<int> colWidths(cols, 0);
    QVector<int> rowHeights(rows, 0);

    const QwtMetricsMap map = QwtPainter::metricsMap();
    for (int i = 0; i < n; ++i) {
        const QString &str = titles[i];
        int itemW = 0;
        if (str.contains("\\c{") || str.contains("\\p{") || str.contains("\\l("))
            itemW = sampleOffset;

        QwtText aux(parse(str));
        aux.setFont(d_text->font());
        const QSize size = aux.textSize();
        const int screenW = map.isIdentity() ? size.width() : map.layoutToScreenX(size.width());
        const int padScreen = qMax(6, qRound(0.03 * screenW) + 4);
        const int padW = map.isIdentity() ? padScreen : map.screenToLayoutX(padScreen);
        itemW += size.width() + padW;
        const int itemH = size.height();
        const int row = i / cols;
        const int col = i % cols;
        if (itemW > colWidths[col])
            colWidths[col] = itemW;
        if (itemH > rowHeights[row])
            rowHeights[row] = itemH;
    }

    const int colGap = (cols > 1) ? (2 * left_margin + 6) : 0;
    QVector<int> colOffsetX(cols, baseX + left_margin);
    for (int c = 1; c < cols; ++c)
        colOffsetX[c] = colOffsetX[c - 1] + colWidths[c - 1] + colGap;

    QVector<int> rowOffsetY(rows, baseY + top_margin);
    for (int r = 1; r < rows; ++r)
        rowOffsetY[r] = rowOffsetY[r - 1] + rowHeights[r - 1];

    for (int i = 0; i < n; ++i) {
        const int row = i / cols;
        const int col = i % cols;
        cellX[i] = colOffsetX[col];
        cellCenterY[i] = rowOffsetY[row] + rowHeights[row] / 2;
    }

    int sumW = 0;
    for (int c = 0; c < cols; ++c)
        sumW += colWidths[c];
    int sumH = 0;
    for (int r = 0; r < rows; ++r)
        sumH += rowHeights[r];

    const int rightSafety = map.isIdentity() ? 4 : map.screenToLayoutX(4);
    totalWidth = 2 * left_margin + sumW + (cols - 1) * colGap + rightSafety;
    totalHeight = 2 * top_margin + sumH;
}

int Legend::itemIndexAt(const QPoint &canvasPos) const
{
    if (!d_plot || !d_text)
        return -1;

    QVector<int> rawIndices;
    const QStringList titles = filterVisibleLegendLines(d_plot, d_text->text(), &rawIndices);
    const int n = titles.count();
    if (n <= 0)
        return -1;

    const QwtScaleMap &xMap = d_plot->canvasMap(xAxis());
    const QwtScaleMap &yMap = d_plot->canvasMap(yAxis());
    const int baseX = xMap.transform(xValue());
    const int baseY = yMap.transform(yValue());
    const int symbolLineLength = symbolsMaxLineLength();
    const bool scatterOnly = isScatterOnly();
    const bool compact = d_text && isCompactLegendFont(d_text->font());
    const int sampleLen = scatterOnly ? qMax(symbolLineLength, compact ? 11 : 14) : symbolLineLength;
    const int sampleOffset = sampleLen + effectiveSampleGap(d_text, hspace);

    const int cols = qBound(1, d_num_columns, n);
    const int rows = (n + cols - 1) / cols;
    QVector<int> colWidths(cols, 0);
    QVector<int> rowHeights(rows, 0);

    for (int i = 0; i < n; ++i) {
        const QString &str = titles[i];
        int itemW = 0;
        if (str.contains("\\c{") || str.contains("\\p{") || str.contains("\\l("))
            itemW = sampleOffset;
        QwtText aux(parse(str));
        aux.setFont(d_text->font());
        const QSize size = aux.textSize();
        const int padW = qMax(6, qRound(0.03 * size.width()) + 4);
        itemW += size.width() + padW;
        const int itemH = qMax(1, size.height());
        const int row = i / cols;
        const int col = i % cols;
        if (itemW > colWidths[col])
            colWidths[col] = itemW;
        if (itemH > rowHeights[row])
            rowHeights[row] = itemH;
    }

    const int colGap = (cols > 1) ? (2 * left_margin + 6) : 0;
    QVector<int> colOffsetX(cols, baseX + left_margin);
    for (int c = 1; c < cols; ++c)
        colOffsetX[c] = colOffsetX[c - 1] + colWidths[c - 1] + colGap;

    QVector<int> rowOffsetY(rows, baseY + top_margin);
    for (int r = 1; r < rows; ++r)
        rowOffsetY[r] = rowOffsetY[r - 1] + rowHeights[r - 1];

    int sumW = 0;
    for (int c = 0; c < cols; ++c)
        sumW += colWidths[c];
    int sumH = 0;
    for (int r = 0; r < rows; ++r)
        sumH += rowHeights[r];

    const QRect fullRect(baseX, baseY, 2 * left_margin + sumW + (cols - 1) * colGap,
                         2 * top_margin + sumH);
    if (!fullRect.adjusted(-4, -4, 4, 4).contains(canvasPos))
        return -1;

    int bestIdx = -1;
    qint64 bestDist2 = std::numeric_limits<qint64>::max();
    for (int i = 0; i < n; ++i) {
        const int row = i / cols;
        const int col = i % cols;
        const QRect cellRect(colOffsetX[col] - left_margin / 2, rowOffsetY[row],
                             colWidths[col] + left_margin, rowHeights[row]);
        if (cellRect.contains(canvasPos))
            return rawIndices[i];
        const int dx = (canvasPos.x() < cellRect.left()) ? (cellRect.left() - canvasPos.x())
                : (canvasPos.x() > cellRect.right())     ? (canvasPos.x() - cellRect.right())
                                                         : 0;
        const int dy = (canvasPos.y() < cellRect.top()) ? (cellRect.top() - canvasPos.y())
                : (canvasPos.y() > cellRect.bottom())   ? (canvasPos.y() - cellRect.bottom())
                                                        : 0;
        const qint64 d2 = qint64(dx) * dx + qint64(dy) * dy;
        if (d2 < bestDist2) {
            bestDist2 = d2;
            bestIdx = rawIndices[i];
        }
    }
    return bestIdx;
}

void Legend::drawFrame(QPainter *p, int type, const QRect &rect) const
{
    p->save();
    QPen framePen(Qt::black, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    const QwtMetricsMap map = QwtPainter::metricsMap();
    if (!map.isIdentity()) {
        const qreal scale = qreal(map.screenToLayoutX(1000)) / 1000.0;
        if (scale > 0.0)
            framePen.setWidthF(scale);
    } else {
        framePen.setWidthF(1.0);
    }
    p->setPen(framePen);
    if (type == None)
        QwtPainter::fillRect(p, rect, d_text->backgroundBrush());

    if (type == Line) {
        // drawing/filling in one go broken for PDF export:
        // pen "inherits" alpha value of background brush
        QwtPainter::fillRect(p, rect, d_text->backgroundBrush());
        QwtPainter::drawRect(p, rect);
    } else if (type == Shadow) {
        QwtPainter::fillRect(p, rect, d_text->backgroundBrush());
        QwtPainter::drawRect(p, rect);

        QRect shadow_right =
                QRect(rect.right(), rect.y() + d_shadow_size_y, d_shadow_size_x, rect.height() - 1);
        QRect shadow_bottom =
                QRect(rect.x() + d_shadow_size_x, rect.bottom(), rect.width() - 1, d_shadow_size_y);
        QwtPainter::fillRect(p, shadow_right, QBrush(Qt::black));
        QwtPainter::fillRect(p, shadow_bottom, QBrush(Qt::black));
    }
    p->restore();
}

void Legend::drawVector(QPainter *p, int x, int y, int l, int curveIndex) const
{
    Graph *g = (Graph *)d_plot->parent();
    if (!g)
        return;

    VectorCurve *v = (VectorCurve *)g->curve(curveIndex);
    if (!v)
        return;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const double sc = Plot::printDeviceScale();
    QPen pen(v->color(), qMax(1.2, double(v->width())) * sc, Qt::SolidLine, Qt::RoundCap,
             Qt::MiterJoin);
    pen.setCosmetic(false);
    p->setPen(pen);

    const double headLen = qMin(double(qMax(5, v->headLength())) * sc, 0.45 * double(qMax(8, l)));
    const double effAngle = qBound(14.0, double(v->headAngle()), 55.0);
    const double d = headLen * std::tan(3.14159265358979323846 * effAngle / 180.0);
    const double retract = v->filledArrowHead() ? (0.65 * headLen) : 0.0;
    p->drawLine(QPointF(double(x), double(y)), QPointF(double(x + l) - retract, double(y)));

    p->translate(QPointF(double(x + l), double(y)));

    QPolygonF endArray(3);
    endArray[0] = QPointF(0.0, 0.0);
    endArray[1] = QPointF(-headLen, d);
    endArray[2] = QPointF(-headLen, -d);

    p->setBrush(v->filledArrowHead() ? QBrush(pen.color(), Qt::SolidPattern) : QBrush(Qt::NoBrush));
    p->drawPolygon(endArray);
    p->restore();
}

void Legend::drawSymbols(QPainter *p, const QRect &rect, QwtArray<long> height,
                         int symbolLineLength) const
{
    Graph *g = (Graph *)d_plot->parent();
    const bool scatterOnly = isScatterOnly();
    const QwtMetricsMap map = QwtPainter::metricsMap();

    const bool compact = d_text && isCompactLegendFont(d_text->font());
    int l = scatterOnly ? qMax(symbolLineLength, compact ? 11 : 14) : symbolLineLength;

    QVector<int> cellX;
    QVector<int> cellCenterY;
    int dummyW = 0, dummyH = 0;
    computeCellsLayout(rect.x(), rect.y(), symbolLineLength, cellX, cellCenterY, dummyW, dummyH);

    const QStringList titles = filterVisibleLegendLines(d_plot, d_text ? d_text->text() : QString());

    for (int i = 0; i < (int)titles.count(); i++) {
        const int w = (i < cellX.size()) ? cellX[i] : (rect.x() + left_margin);
        const int cy = (i < cellCenterY.size()) ? cellCenterY[i] : int(height[i]);
        if (titles[i].contains("\\c{") || titles[i].contains("\\l(")) {
            QString aux;
            if (titles[i].contains("\\c{")) { // SciDAVis symbol specification
                int pos = titles[i].indexOf("{", 0);
                int pos2 = titles[i].indexOf("}", pos);
                aux = titles[i].mid(pos + 1, pos2 - pos - 1);
            } else if (titles[i].contains("\\l(")) { // Origin project legend
                int pos = titles[i].indexOf("(", 0);
                int pos2 = titles[i].indexOf(")", pos);
                aux = titles[i].mid(pos + 1, pos2 - pos - 1);
            }

            int cv = aux.toInt() - 1;
            if (cv < 0)
                continue;

            if (g && (g->curveType(cv) == Graph ::VectXYXY || g->curveType(cv) == Graph ::VectXYAM))
                drawVector(p, w, cy, l, cv);
            else {
                const QwtPlotCurve *curve = g ? g->curve(cv) : nullptr;
                if (curve && curve->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
                    QwtSymbol symb = curve->symbol();
                    const QBrush br = curve->brush();
                    QPen pen = curve->pen();

                    p->save();
                    if (curve->testRenderHint(QwtPlotItem::RenderAntialiased))
                        p->setRenderHint(QPainter::Antialiasing, true);

                    // E11: a curve with an error band shows the band colour behind its line.
                    if (DataCurve *dcBand =
                                dynamic_cast<DataCurve *>(const_cast<QwtPlotCurve *>(curve))) {
                        for (DataCurve *ec : dcBand->errorBarsList()) {
                            auto *er = dynamic_cast<QwtErrorPlotCurve *>(ec);
                            if (!er || !er->isBand() || !er->isVisible())
                                continue;
                            p->save();
                            p->setPen(Qt::NoPen);
                            p->setBrush(QBrush(er->bandFillColor()));
                            QwtPainter::drawRect(p, QRect(w, cy - 5, l, 11));
                            p->restore();
                            break;
                        }
                    }

                    if (curve->style() != 0) {
                        qreal pw = pen.widthF();
                        if (pw <= 0.0)
                            pw = qMax(1, pen.width());
                        if (!map.isIdentity() && !pen.isCosmetic()) {
                            const qreal scale = qreal(map.screenToLayoutX(1000)) / 1000.0;
                            if (scale > 0.0)
                                pw *= scale;
                        }
                        pen.setWidthF(pw);
                        if (pen.style() != Qt::NoPen && pen.isCosmetic())
                            pen.setCosmetic(false);
                        p->setPen(pen);
                        if (br.style() != Qt::NoBrush || g->curveType(cv) == Graph::Box) {
                            QRect lr = QRect(w, cy - 4, l, 10);
                            QBrush effBr = br;
                            if (effBr.style() == Qt::NoBrush && g->curveType(cv) == Graph::Box) {
                                QColor tint = pen.color();
                                tint.setAlpha(95);
                                effBr = QBrush(tint, Qt::SolidPattern);
                            }
                            p->setBrush(effBr);
                            QwtPainter::drawRect(p, lr);
                        } else {
                            const QPoint p1 = map.layoutToDevice(QPoint(w, cy), p);
                            const QPoint p2 = map.layoutToDevice(QPoint(w + l, cy), p);
                            p->drawLine(QPointF(p1), QPointF(p2));
                        }
                    }
                    if (symb.style() != QwtSymbol::NoSymbol) {
                        int symb_size = symb.size().width();
                        if (symb_size > 15)
                            symb_size = 15;
                        else if (symb_size < 3)
                            symb_size = 3;
                        QRect symRect;
                        symRect.setSize(map.screenToLayout(QSize(symb_size, symb_size)));
                        symRect.moveCenter(QPoint(w + l / 2, cy));

                        p->setBrush(symb.brush());
                        QPen symPen = QwtPainter::scaledPen(symb.pen());
                        if (symPen.style() != Qt::NoPen && symPen.isCosmetic()) {
                            symPen.setCosmetic(false);
                            if (symPen.widthF() <= 0.0)
                                symPen.setWidthF(qMax(1.0, double(symb.pen().widthF())));
                        }
                        p->setPen(symPen);
                        symb.draw(p, symRect);
                    }
                    p->restore();
                }
            }
        } else if (titles[i].contains("\\p{")) {
            int pos = titles[i].indexOf("{", 0);
            int pos2 = titles[i].indexOf("}", pos);
            QString aux = titles[i].mid(pos + 1, pos2 - pos - 1);

            int id = aux.toInt();

            Graph *g = (Graph *)d_plot->parent();
            if (g && g->isPiePlot()) {
                QwtPieCurve *curve = dynamic_cast<QwtPieCurve *>(g->curve(0));
                if (!curve)
                    curve = dynamic_cast<QwtPieCurve *>(d_plot->curve(1));
                if (curve) {
                    const QBrush br = QBrush(curve->color(id - 1), curve->pattern());
                    QPen swPen(QColor(80, 80, 80), qMax(1.0, Plot::printDeviceScale()),
                               Qt::SolidLine);
                    swPen.setCosmetic(false);

                    p->save();
                    p->setPen(swPen);
                    QRect lr = QRect(w, cy - 4, l, 10);
                    p->setBrush(br);
                    QwtPainter::drawRect(p, lr);
                    p->restore();
                }
            }
        }
    }
}

void Legend::drawLegends(QPainter *p, const QRect &rect, QwtArray<long> height,
                         int symbolLineLength) const
{
    const bool scatterOnly = isScatterOnly();
    const bool compact = d_text && isCompactLegendFont(d_text->font());
    const int sampleLen = scatterOnly ? qMax(symbolLineLength, compact ? 11 : 14) : symbolLineLength;
    const int sampleOffset = sampleLen + effectiveSampleGap(d_text, hspace);

    QVector<int> cellX;
    QVector<int> cellCenterY;
    int dummyW = 0, dummyH = 0;
    computeCellsLayout(rect.x(), rect.y(), symbolLineLength, cellX, cellCenterY, dummyW, dummyH);

    const QStringList titles = filterVisibleLegendLines(d_plot, d_text ? d_text->text() : QString());

    for (int i = 0; i < (int)titles.count(); i++) {
        QString str = titles[i];
        int x = (i < cellX.size()) ? cellX[i] : (rect.x() + left_margin);
        const int cy = (i < cellCenterY.size()) ? cellCenterY[i] : int(height[i]);
        if (str.contains("\\c{") || str.contains("\\p{") || str.contains("\\l("))
            x += sampleOffset;

        QwtText aux(parse(str));
        aux.setFont(d_text->font());
        aux.setColor(d_text->color());
        aux.setRenderFlags(Qt::AlignLeft | Qt::AlignVCenter);

        const QSize size = aux.textSize();
        const QwtMetricsMap map = QwtPainter::metricsMap();
        const int screenW = map.isIdentity() ? size.width() : map.layoutToScreenX(size.width());
        const int padScreen = qMax(6, qRound(0.03 * screenW) + 4);
        const int padW = map.isIdentity() ? padScreen : map.screenToLayoutX(padScreen);

        QRect tr = QRect(QPoint(x, cy - size.height() / 2), QSize(size.width() + padW, size.height()));
        aux.draw(p, tr);
    }
}

QwtArray<long> Legend::itemsHeight(int y, int symbolLineLength, int &width, int &height) const
{
    QVector<int> cellX;
    QVector<int> cellCenterY;
    computeCellsLayout(0, y, symbolLineLength, cellX, cellCenterY, width, height);

    QwtArray<long> heights(cellCenterY.size());
    for (int i = 0; i < cellCenterY.size(); ++i)
        heights[i] = cellCenterY[i];
    return heights;
}

int Legend::symbolsMaxLineLength() const
{
    QList<int> cvs = d_plot->curveKeys();
    Graph *g = d_plot ? qobject_cast<Graph *>(d_plot->parent()) : nullptr;

    int maxL = 0;
    const QStringList titles = filterVisibleLegendLines(d_plot, d_text ? d_text->text() : QString());
    for (int i = 0; i < (int)titles.count(); i++) {
        if ((titles[i].contains("\\c{") || titles[i].contains("\\l(")) && (int)cvs.size() > 0) {
            QString aux;
            if (titles[i].contains("\\c{")) { // SciDAVis symbol specification
                int pos = titles[i].indexOf("{", 0);
                int pos2 = titles[i].indexOf("}", pos);
                aux = titles[i].mid(pos + 1, pos2 - pos - 1);
            } else if (titles[i].contains("\\l(")) { // Origin project legend
                int pos = titles[i].indexOf("(", 0);
                int pos2 = titles[i].indexOf(")", pos);
                aux = titles[i].mid(pos + 1, pos2 - pos - 1);
            }

            int cv = aux.toInt() - 1;
            if (cv < 0)
                continue;

            const QwtPlotCurve *c = g ? g->curve(cv)
                                      : ((cv < cvs.count()) ? (const QwtPlotCurve *)d_plot->curve(cvs[cv])
                                                            : nullptr);
            if (c && c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
                int l = c->symbol().size().width();
                if (l < 3)
                    l = 3;
                else if (l > 15)
                    l = 15;
                if (l > maxL && c->symbol().style() != QwtSymbol::NoSymbol)
                    maxL = l;
            }
        }

        if (titles[i].contains("\\p{"))
            maxL = qMax(maxL, 16);
    }
    const QFont fnt = d_text ? d_text->font() : QFont(QStringLiteral("Arial"), 12);
    const bool compact = isCompactLegendFont(fnt);
    if (isScatterOnly())
        return qMax(maxL + (compact ? 2 : 0), compact ? 11 : 14);
    if (compact) {
        const int fontProportionalLen =
                qBound(14, qRound(QFontMetricsF(fnt).height() * 1.05), 34);
        return qBound(14, qMax(maxL + 6, fontProportionalLen), 34);
    }
    const int fontProportionalLen =
            qBound(16, qRound(QFontMetricsF(fnt).height() * 1.45), 34);
    return qBound(16, qMax(maxL + 8, fontProportionalLen), 34);
}

bool Legend::isScatterOnly() const
{
    if (!d_plot)
        return false;

    Graph *g = qobject_cast<Graph *>(d_plot->parent());
    QList<int> cvs = d_plot->curveKeys();

    const QStringList titles = filterVisibleLegendLines(d_plot, d_text ? d_text->text() : QString());

    bool hasScatter = false;
    for (int i = 0; i < (int)titles.count(); i++) {
        if (titles[i].contains("\\p{"))
            return false;

        if (titles[i].contains("\\c{") || titles[i].contains("\\l(")) {
            QString aux;
            if (titles[i].contains("\\c{")) {
                int pos = titles[i].indexOf("{", 0);
                int pos2 = titles[i].indexOf("}", pos);
                aux = titles[i].mid(pos + 1, pos2 - pos - 1);
            } else if (titles[i].contains("\\l(")) {
                int pos = titles[i].indexOf("(", 0);
                int pos2 = titles[i].indexOf(")", pos);
                aux = titles[i].mid(pos + 1, pos2 - pos - 1);
            }

            int cv = aux.toInt() - 1;
            if (cv < 0)
                continue;

            if (g
                && (g->curveType(cv) == Graph::VectXYXY || g->curveType(cv) == Graph::VectXYAM
                    || g->curveType(cv) == Graph::Box))
                return false;

            const QwtPlotCurve *c = g ? g->curve(cv)
                                      : ((cv < cvs.count()) ? (const QwtPlotCurve *)d_plot->curve(cvs[cv])
                                                            : nullptr);
            if (c && c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
                if (c->style() != QwtPlotCurve::NoCurve || c->brush().style() != Qt::NoBrush)
                    return false;
                if (c->symbol().style() != QwtSymbol::NoSymbol)
                    hasScatter = true;
            }
        }
    }
    return hasScatter;
}

QString Legend::parse(const QString &str) const
{
    QString s = str;
    if (s.contains("\\c{") || s.contains("\\p{") || s.contains("\\l(")) {
        int pos = s.indexOf("}", 0);
        if (s.contains("\\l("))
            pos = s.indexOf(")", 0);
        s = s.right(s.length() - pos - 1);
    }

    int searchFrom = 0;
    int pos = -1;
    while ((pos = s.indexOf("%(", searchFrom)) >= 0) { // curve name specification
        int pos2 = s.indexOf(")", pos + 2);
        if (pos2 < 0)
            break;
        bool ok = false;
        int cv = s.mid(pos + 2, pos2 - pos - 2).toInt(&ok) - 1;
        bool replaced = false;
        if (ok && cv >= 0) {
            Graph *g = d_plot ? (Graph *)d_plot->parent() : nullptr;
            if (g) {
                const QwtPlotCurve *c = (QwtPlotCurve *)g->curve(cv);
                if (c) {
                    const QString repl = c->title().text();
                    s.replace(pos, pos2 - pos + 1, repl);
                    searchFrom = pos + repl.length();
                    replaced = true;
                }
            }
        }
        if (!replaced)
            searchFrom = pos2 + 1;
    }
    return s;
}

Legend::~Legend()
{
    delete d_text;
}
