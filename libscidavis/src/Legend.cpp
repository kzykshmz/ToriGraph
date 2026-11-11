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
#include "VectorCurve.h"

#include <QPainter>
#include <QPolygon>
#include <QMessageBox>

#include <qwt_plot.h>
#include <qwt_scale_widget.h>
#include <qwt_painter.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_layout_metrics.h>
#include <qwt_symbol.h>

Legend::Legend(Plot *plot) : d_plot(plot), d_frame(0), d_angle(0), d_num_columns(1)
{
    d_text = new QwtText(QString(), QwtText::RichText);
    d_text->setFont(QFont("Arial", 12, QFont::Normal, false));
    d_text->setRenderFlags(Qt::AlignTop | Qt::AlignLeft);
    d_text->setBackgroundBrush(QBrush(Qt::NoBrush));
    d_text->setColor(Qt::black);
    d_text->setBackgroundPen(QPen(Qt::NoPen));
    d_text->setPaintAttribute(QwtText::PaintBackground);

    hspace = 30;
    left_margin = 10;
    top_margin = 6;
    d_shadow_size_x = 5;
    d_shadow_size_y = 5;
}

void Legend::draw(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                  const QRect &) const
{
    const int x = xMap.transform(xValue());
    const int y = yMap.transform(yValue());

    QwtPainter::setMetricsMap(plot(), p->device());
    const QwtMetricsMap map = QwtPainter::metricsMap();

    const int symbolLineLength = symbolsMaxLineLength();

    int width, height;
    QwtArray<long> heights = itemsHeight(map.deviceToLayoutY(y), symbolLineLength, width, height);

    QRect rs = QRect(map.deviceToLayout(QPoint(x, y)), QSize(width, height));

    drawFrame(p, d_frame, rs);
    drawSymbols(p, rs, heights, symbolLineLength);
    drawLegends(p, rs, heights, symbolLineLength);
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

    int width, height;
    itemsHeight(y, symbolsMaxLineLength(), width, height);

    return QRect(QPoint(x, y), QSize(width - 1, height - 1));
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
    if (resolved == d_text->font())
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
    const int sampleOffset =
            scatterOnly ? (qMax(symbolLineLength, 14) + 12) : (symbolLineLength + hspace);

    QString text = d_text->text().trimmed();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList titles = text.split("\n", Qt::KeepEmptyParts);
#else
    QStringList titles = text.split("\n", QString::KeepEmptyParts);
#endif
    const int n = titles.count();
    cellX.resize(n);
    cellCenterY.resize(n);
    if (n <= 0) {
        totalWidth = 2 * left_margin;
        totalHeight = 2 * top_margin;
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
        QSize size = aux.textSize(d_text->font());
        if (!map.isIdentity()) {
            int screen_width = map.layoutToScreenX(size.width());
            screen_width -= 3;
            screen_width *= 1.1;
            size = QSize(map.screenToLayoutX(screen_width), size.height());
        }
        itemW += size.width();
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

    totalWidth = 2 * left_margin + sumW + (cols - 1) * colGap;
    totalHeight = 2 * top_margin + sumH;
}

void Legend::drawFrame(QPainter *p, int type, const QRect &rect) const
{
    p->save();
    p->setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
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

    QPen pen(v->color(), v->width(), Qt::SolidLine);
    p->setPen(pen);
    QwtPainter::drawLine(p, x, y, x + l, y);

    p->translate(x + l, y);

    double pi = 4 * atan(-1.0);
    int headLength = v->headLength();
    int d = qRound(headLength * tan(pi * (double)v->headAngle() / 180.0));

    QPolygon endArray(3);
    endArray[0] = QPoint(0, 0);
    endArray[1] = QPoint(-headLength, d);
    endArray[2] = QPoint(-headLength, -d);

    if (v->filledArrowHead())
        p->setBrush(QBrush(pen.color(), Qt::SolidPattern));

    QwtPainter::drawPolygon(p, endArray);
    p->restore();
}

void Legend::drawSymbols(QPainter *p, const QRect &rect, QwtArray<long> height,
                         int symbolLineLength) const
{
    Graph *g = (Graph *)d_plot->parent();
    const bool scatterOnly = isScatterOnly();

    int l = scatterOnly ? qMax(symbolLineLength, 14) : (symbolLineLength + 2 * left_margin);

    QVector<int> cellX;
    QVector<int> cellCenterY;
    int dummyW = 0, dummyH = 0;
    computeCellsLayout(rect.x(), rect.y(), symbolLineLength, cellX, cellCenterY, dummyW, dummyH);

    QString text = d_text->text().trimmed();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList titles = text.split("\n", Qt::KeepEmptyParts);
#else
    QStringList titles = text.split("\n", QString::KeepEmptyParts);
#endif

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

                    if (curve->style() != 0) {
                        p->setPen(pen);
                        if (br.style() != Qt::NoBrush || g->curveType(cv) == Graph::Box) {
                            QRect lr = QRect(w, cy - 4, l, 10);
                            p->setBrush(br);
                            QwtPainter::drawRect(p, lr);
                        } else
                            QwtPainter::drawLine(p, w, cy, w + l, cy);
                    }
                    int symb_size = symb.size().width();
                    if (symb_size > 15)
                        symb_size = 15;
                    else if (symb_size < 3)
                        symb_size = 3;
                    symb.setSize(symb_size);
                    symb.draw(p, w + l / 2, cy);
                    p->restore();
                }
            }
        } else if (titles[i].contains("\\p{")) {
            int pos = titles[i].indexOf("{", 0);
            int pos2 = titles[i].indexOf("}", pos);
            QString aux = titles[i].mid(pos + 1, pos2 - pos - 1);

            int id = aux.toInt();

            Graph *g = (Graph *)d_plot->parent();
            if (g->isPiePlot()) {
                QwtPieCurve *curve = (QwtPieCurve *)d_plot->curve(1);
                if (curve) {
                    const QBrush br = QBrush(curve->color(id - 1), curve->pattern());
                    QPen pen = curve->pen();

                    p->save();
                    p->setPen(QPen(pen.color(), 1, Qt::SolidLine));
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
    const int sampleOffset =
            scatterOnly ? (qMax(symbolLineLength, 14) + 12) : (symbolLineLength + hspace);

    QVector<int> cellX;
    QVector<int> cellCenterY;
    int dummyW = 0, dummyH = 0;
    computeCellsLayout(rect.x(), rect.y(), symbolLineLength, cellX, cellCenterY, dummyW, dummyH);

    QString text = d_text->text().trimmed();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList titles = text.split("\n", Qt::KeepEmptyParts);
#else
    QStringList titles = text.split("\n", QString::KeepEmptyParts);
#endif

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

        QSize size = aux.textSize();
        // bug in Qwt; workaround in QwtText::textSize() only works for short texts.
        // Thus, we work around the workaround.
        const QwtMetricsMap map = QwtPainter::metricsMap();
        if (!map.isIdentity()) {
            int screen_width = map.layoutToScreenX(size.width());
            screen_width -= 3;
            screen_width *= 1.1;
            size = QSize(map.screenToLayoutX(screen_width), size.height());
        }

        QRect tr = QRect(QPoint(x, cy - size.height() / 2), size);
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
    QString text = d_text->text().trimmed();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList titles = text.split("\n", Qt::KeepEmptyParts);
#else
    QStringList titles = text.split("\n", QString::KeepEmptyParts);
#endif
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
            maxL = 10;
    }
    return maxL;
}

bool Legend::isScatterOnly() const
{
    if (!d_plot)
        return false;

    Graph *g = qobject_cast<Graph *>(d_plot->parent());
    QList<int> cvs = d_plot->curveKeys();

    QString text = d_text->text().trimmed();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList titles = text.split("\n", Qt::KeepEmptyParts);
#else
    QStringList titles = text.split("\n", QString::KeepEmptyParts);
#endif

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
