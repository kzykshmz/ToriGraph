/***************************************************************************
    File                 : BoxCurve.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Box curve

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
#include "BoxCurve.h"
#include "Plot.h"
#include "core/column/Column.h"
#include <QPainter>
#include <QLocale>
#include <qwt_scale_map.h>
#include <qwt_symbol.h>

#include <gsl/gsl_sort.h>
#include <gsl/gsl_statistics.h>
#include <cmath>

namespace {

QPen scaledBoxPen(const QPen &src, double sc)
{
    if (src.style() == Qt::NoPen)
        return src;
    QPen out = src;
    double wf = src.widthF();
    if (wf <= 0.0)
        wf = (src.width() > 0) ? double(src.width()) : 1.0;
    out.setCosmetic(false);
    out.setWidthF(qMax(0.1, wf) * sc);
    return out;
}

void computeTukeyWhiskers(const double *dat, int size, double &wLoVal, double &wHiVal,
                          double &fenceLo, double &fenceHi, double &tol)
{
    const double q1 = gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.25);
    const double q3 = gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.75);
    const double iqr = qMax(0.0, q3 - q1);
    fenceLo = q1 - 1.5 * iqr;
    fenceHi = q3 + 1.5 * iqr;
    tol = 1e-12 * qMax(1.0, qMax(std::fabs(fenceLo), std::fabs(fenceHi)));

    wLoVal = q1;
    for (int i = 0; i < size; ++i) {
        if (dat[i] >= fenceLo - tol) {
            wLoVal = qMin(dat[i], q1);
            break;
        }
    }
    wHiVal = q3;
    for (int i = size - 1; i >= 0; --i) {
        if (dat[i] <= fenceHi + tol) {
            wHiVal = qMax(dat[i], q3);
            break;
        }
    }
}

} // namespace

BoxCurve::BoxCurve(Table *t, QString name, int startRow, int endRow)
    : DataCurve(t, QString(), name, startRow, endRow)
{
    mean_style = QwtSymbol::Rect;
    max_style = QwtSymbol::XCross;
    min_style = QwtSymbol::XCross;
    p99_style = QwtSymbol::NoSymbol;
    p1_style = QwtSymbol::NoSymbol;

    b_style = Rect;
    b_coeff = 75.0;
    b_range = r25_75;
    w_range = r5_95;
    w_coeff = 95.0;
    b_width = 80;
    d_violin_mode = false;
    d_show_data_points = false;

    setType(Graph::Box);
}

void BoxCurve::copy(const BoxCurve *b)
{
    mean_style = b->mean_style;
    max_style = b->max_style;
    min_style = b->min_style;
    p99_style = b->p99_style;
    p1_style = b->p1_style;

    b_style = b->b_style;
    b_coeff = b->b_coeff;
    b_range = b->b_range;
    w_range = b->w_range;
    w_coeff = b->w_coeff;
    b_width = b->b_width;
    d_violin_mode = b->d_violin_mode;
    d_show_data_points = b->d_show_data_points;
}

void BoxCurve::draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap, int from,
                    int to) const
{
    if (!painter || dataSize() <= 0)
        return;

    const int n = dataSize();
    if (from < 0)
        from = 0;
    if (to < 0 || to >= n)
        to = n - 1;
    const int size = to - from + 1;
    if (size <= 0)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const double sc = renderPenScale();
    painter->setPen(scaledBoxPen(QwtPlotCurve::pen(), sc));

    QVector<double> datVec(size);
    for (int i = 0; i < size; ++i)
        datVec[i] = y(from + i);
    if (from > 0 || to < n - 1)
        gsl_sort(datVec.data(), 1, size);

    drawBox(painter, xMap, yMap, datVec.data(), size);
    drawSymbols(painter, xMap, yMap, datVec.data(), size);

    painter->restore();
}

void BoxCurve::drawBox(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                       double *dat, int size) const
{
    const double sc = renderPenScale();
    QPen cPen = scaledBoxPen(QwtPlotCurve::pen(), sc);
    painter->setPen(cPen);

    const double pxF = xMap.xTransform(x(0));
    const int px = qRound(pxF);
    const double pxSpan = std::fabs(xMap.xTransform(x(0) + 0.5) - xMap.xTransform(x(0) - 0.5));
    const int box_width = qMax(4, qRound(pxSpan * double(b_width) / 100.0));
    const int hbw = d_violin_mode ? qMax(qMax(1, qRound(3.5 * sc)), box_width / 8)
                                  : qMax(qMax(1, qRound(2.0 * sc)), box_width / 2);
    const int violin_hbw = qMax(qMax(2, qRound(8.0 * sc)), box_width / 2);
    const int median = yMap.transform(gsl_stats_median_from_sorted_data(dat, 1, size));
    int b_lowerq, b_upperq;
    double sd = 0;
    double se = 0;
    double mean = 0;
    if (w_range == SD || w_range == SE || b_range == SD || b_range == SE || d_violin_mode) {
        sd = (size > 1) ? gsl_stats_sd(dat, 1, size) : 0.0;
        se = (size > 0) ? sd / sqrt((double)size) : 0.0;
        mean = gsl_stats_mean(dat, 1, size);
    }

    // 1. Draw Violin KDE contour first (background layer) when violinMode() is enabled
    double kdeBandwidth = 1.0;
    double kdeMaxDensity = 0.0;
    if (d_violin_mode && size >= 2) {
        const double q25 = gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.25);
        const double q75 = gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.75);
        const double iqr = q75 - q25;
        double spread = (iqr > 0.0 && sd > 0.0) ? qMin(sd, iqr / 1.34) : (sd > 0.0 ? sd : iqr);
        const double dataSpan = dat[size - 1] - dat[0];
        if (spread <= 0.0)
            spread = (dataSpan > 0.0) ? 0.25 * dataSpan : 1.0;
        kdeBandwidth = 0.9 * spread * std::pow(double(size), -0.2);
        if (kdeBandwidth <= 1e-12)
            kdeBandwidth = 1.0;

        const double pad = qMin(1.5 * kdeBandwidth, (dataSpan > 0.0) ? 0.16 * dataSpan : 1.0);
        const double yLo = dat[0] - pad;
        const double yHi = dat[size - 1] + pad;
        const int nSteps = 64;
        QVector<double> yGrid(nSteps + 1);
        QVector<double> densities(nSteps + 1, 0.0);

        for (int k = 0; k <= nSteps; ++k) {
            const double yv = yLo + (yHi - yLo) * double(k) / double(nSteps);
            yGrid[k] = yv;
            double dens = 0.0;
            for (int i = 0; i < size; ++i) {
                const double u = (yv - dat[i]) / kdeBandwidth;
                dens += std::exp(-0.5 * u * u);
            }
            densities[k] = dens;
            if (dens > kdeMaxDensity)
                kdeMaxDensity = dens;
        }

        if (kdeMaxDensity > 0.0) {
            QPolygonF violinPoly(2 * (nSteps + 1));
            for (int k = 0; k <= nSteps; ++k) {
                const double py = yMap.xTransform(yGrid[k]);
                const double dx = (densities[k] / kdeMaxDensity) * double(violin_hbw);
                violinPoly[k] = QPointF(pxF - dx, py);
                violinPoly[2 * nSteps + 1 - k] = QPointF(pxF + dx, py);
            }

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            QBrush vBrush = QwtPlotCurve::brush();
            if (vBrush.style() == Qt::NoBrush) {
                QColor fillCol = cPen.color();
                fillCol.setAlpha(85);
                vBrush = QBrush(fillCol, Qt::SolidPattern);
            }
            painter->setBrush(vBrush);
            painter->setPen(cPen);
            painter->drawPolygon(violinPoly);
            painter->restore();
        }
    }

    // 2. Draw deterministic jittered data points BEFORE inner box/median so quartiles & median stay legible
    if (d_show_data_points && size > 0) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        QColor ptColor = cPen.color();
        QColor fillCol = ptColor;
        fillCol.setAlpha(d_violin_mode ? 138 : 180);
        QPen dotPen(ptColor.darker(135), 0.9 * sc);
        dotPen.setCosmetic(false);
        painter->setPen(dotPen);
        painter->setBrush(QBrush(fillCol, Qt::SolidPattern));
        const double dotRadius = qMax(0.5, 2.7 * sc);

        for (int i = 0; i < size; ++i) {
            const double frac = (size == 1)
                    ? 0.0
                    : (2.0 * std::fmod((i + 1) * 0.6180339887498949, 1.0) - 1.0);
            double maxHalfJitter = 0.50 * violin_hbw;
            if (d_violin_mode && kdeMaxDensity > 0.0) {
                double localDens = 0.0;
                for (int m = 0; m < size; ++m) {
                    const double u = (dat[i] - dat[m]) / kdeBandwidth;
                    localDens += std::exp(-0.5 * u * u);
                }
                maxHalfJitter = 0.68 * (localDens / kdeMaxDensity) * violin_hbw;
            }
            const double jx = pxF + frac * maxHalfJitter;
            const double jy = yMap.xTransform(dat[i]);
            if (std::isfinite(jx) && std::isfinite(jy))
                painter->drawEllipse(QPointF(jx, jy), dotRadius, dotRadius);
        }
        painter->restore();
    }

    if (b_range == SD) {
        b_lowerq = yMap.transform(mean - sd * b_coeff);
        b_upperq = yMap.transform(mean + sd * b_coeff);
    } else if (b_range == SE) {
        b_lowerq = yMap.transform(mean - se * b_coeff);
        b_upperq = yMap.transform(mean + se * b_coeff);
    } else if (b_range == Tukey15IQR) {
        double wLoVal = 0.0, wHiVal = 0.0, fenceLo = 0.0, fenceHi = 0.0, tol = 0.0;
        computeTukeyWhiskers(dat, size, wLoVal, wHiVal, fenceLo, fenceHi, tol);
        b_lowerq = yMap.transform(wLoVal);
        b_upperq = yMap.transform(wHiVal);
    } else {
        b_lowerq = yMap.transform(
                gsl_stats_quantile_from_sorted_data(dat, 1, size, 1 - 0.01 * b_coeff));
        b_upperq =
                yMap.transform(gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.01 * b_coeff));
    }

    // 3. Draw whiskers, inner box, and median ON TOP of violin KDE & jittered points
    const int eff_box_width = 2 * hbw;
    if (w_range) {
        const int l = qMax(qMax(1, qRound(2.0 * sc)), int(0.16 * eff_box_width));
        int w_upperq, w_lowerq;
        if (w_range == SD) {
            w_lowerq = yMap.transform(mean - sd * w_coeff);
            w_upperq = yMap.transform(mean + sd * w_coeff);
        } else if (w_range == SE) {
            w_lowerq = yMap.transform(mean - se * w_coeff);
            w_upperq = yMap.transform(mean + se * w_coeff);
        } else if (w_range == Tukey15IQR) {
            double wLoVal = 0.0, wHiVal = 0.0, fenceLo = 0.0, fenceHi = 0.0, tol = 0.0;
            computeTukeyWhiskers(dat, size, wLoVal, wHiVal, fenceLo, fenceHi, tol);
            w_lowerq = yMap.transform(wLoVal);
            w_upperq = yMap.transform(wHiVal);
        } else {
            w_lowerq = yMap.transform(
                    gsl_stats_quantile_from_sorted_data(dat, 1, size, 1 - 0.01 * w_coeff));
            w_upperq = yMap.transform(
                    gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.01 * w_coeff));
        }

        painter->drawLine(px - l, w_lowerq, px + l, w_lowerq);
        painter->drawLine(px - l, w_upperq, px + l, w_upperq);

        if (b_style) {
            if (w_upperq != b_upperq)
                painter->drawLine(px, w_upperq, px, b_upperq);
            if (w_lowerq != b_lowerq)
                painter->drawLine(px, w_lowerq, px, b_lowerq);
        } else
            painter->drawLine(px, w_upperq, px, w_lowerq);
    }

    QBrush boxBrush = QwtPlotCurve::brush();
    if (d_violin_mode && b_style != NoBox) {
        boxBrush = QBrush(QColor(255, 255, 255, 232), Qt::SolidPattern);
    } else if (boxBrush.style() == Qt::NoBrush && d_show_data_points && b_style != NoBox) {
        QColor tint = cPen.color().lighter(175);
        tint.setAlpha(150);
        boxBrush = QBrush(tint, Qt::SolidPattern);
    }

    if (b_style == Rect) {
        const int topY = qMin(b_upperq, b_lowerq);
        const int botY = qMax(b_upperq, b_lowerq);
        const QRect r(px - hbw, topY, eff_box_width, qMax(1, botY - topY));
        if (boxBrush.style() != Qt::NoBrush)
            painter->fillRect(r, boxBrush);
        painter->drawRect(r);
    } else if (b_style == Diamond) {
        QPolygon pa(4);
        pa[0] = QPoint(px, b_upperq);
        pa[1] = QPoint(px + hbw, median);
        pa[2] = QPoint(px, b_lowerq);
        pa[3] = QPoint(px - hbw, median);

        painter->setBrush(boxBrush);
        painter->drawPolygon(pa);
    } else if (b_style == WindBox) {
        const int lowerq = yMap.transform(gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.25));
        const int upperq = yMap.transform(gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.75));
        QPolygon pa(8);
        pa[0] = QPoint(px + hbw, b_upperq);
        pa[1] = QPoint(int(px + 0.4 * eff_box_width), upperq);
        pa[2] = QPoint(int(px + 0.4 * eff_box_width), lowerq);
        pa[3] = QPoint(px + hbw, b_lowerq);
        pa[4] = QPoint(px - hbw, b_lowerq);
        pa[5] = QPoint(int(px - 0.4 * eff_box_width), lowerq);
        pa[6] = QPoint(int(px - 0.4 * eff_box_width), upperq);
        pa[7] = QPoint(px - hbw, b_upperq);

        painter->setBrush(boxBrush);
        painter->drawPolygon(pa);
    } else if (b_style == Notch) {
        const double q25 = gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.25);
        const double q50 = gsl_stats_median_from_sorted_data(dat, 1, size);
        const double q75 = gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.75);
        const double iqr = qMax(0.0, q75 - q25);
        const double halfNotch = (size > 0) ? (1.57 * iqr / sqrt((double)size)) : 0.0;
        const int yTop = qMin(b_upperq, b_lowerq);
        const int yBot = qMax(b_upperq, b_lowerq);
        const int lowerCI = qBound(yTop, yMap.transform(q50 - halfNotch), yBot);
        const int upperCI = qBound(yTop, yMap.transform(q50 + halfNotch), yBot);

        QPolygon pa(10);
        pa[0] = QPoint(px + hbw, b_upperq);
        pa[1] = QPoint(px + hbw, upperCI);
        pa[2] = QPoint(int(px + 0.25 * hbw), median);
        pa[3] = QPoint(px + hbw, lowerCI);
        pa[4] = QPoint(px + hbw, b_lowerq);
        pa[5] = QPoint(px - hbw, b_lowerq);
        pa[6] = QPoint(px - hbw, lowerCI);
        pa[7] = QPoint(int(px - 0.25 * hbw), median);
        pa[8] = QPoint(px - hbw, upperCI);
        pa[9] = QPoint(px - hbw, b_upperq);

        painter->setBrush(boxBrush);
        painter->drawPolygon(pa);
    }

    // Draw crisp median line above box fill
    if (b_style != Notch && b_style != NoBox) {
        painter->save();
        QPen medPen = cPen;
        if (medPen.style() != Qt::NoPen)
            medPen.setWidthF(qMax(0.1 * sc, cPen.widthF() * 1.35));
        painter->setPen(medPen);
        if (b_style == WindBox)
            painter->drawLine(int(px - 0.4 * eff_box_width), median, int(px + 0.4 * eff_box_width),
                              median);
        else
            painter->drawLine(px - hbw, median, px + hbw, median);
        painter->restore();
    }
}

void BoxCurve::drawSymbols(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                           double *dat, int size) const
{
    if (!painter || size <= 0)
        return;

    const int px = xMap.transform(x(0));
    const double sc = renderPenScale();

    QwtSymbol s = this->symbol();
    QPen symPen = s.pen();
    if (symPen.style() != Qt::NoPen) {
        double baseW = symPen.widthF();
        if (baseW <= 0.0) {
            const QPen curvePen = QwtPlotCurve::pen();
            baseW = (curvePen.widthF() > 0.0)
                    ? curvePen.widthF()
                    : ((curvePen.width() > 0) ? double(curvePen.width()) : 1.0);
        }
        symPen.setCosmetic(false);
        symPen.setWidthF(qMax(0.1, baseW) * sc);
    }
    if (!symPen.color().isValid() || symPen.color() == Qt::black)
        symPen.setColor(QwtPlotCurve::pen().color().darker(125));
    s.setPen(symPen);
    if (s.brush().style() == Qt::NoBrush)
        s.setBrush(QBrush(Qt::white, Qt::SolidPattern));
    const int baseSz = qMax(7, s.size().width());
    const int symSz = qMax(2, qRound(baseSz * sc));
    s.setSize(symSz, symSz);

    // Draw via QwtSymbol::draw(QPainter *, const QRect &) with non-cosmetic symPen already bound,
    // bypassing QwtSymbol::draw(QPainter *, int, int) -> QwtPainter::scaledPen() (which forces
    // cosmetic 1px hairlines under 600-DPI QPainter scaling).
    painter->save();
    painter->setPen(symPen);
    painter->setBrush(s.brush());

    auto drawBoxSymAt = [&](QwtSymbol::Style st, int py) {
        s.setStyle(st);
        s.draw(painter, QRect(px - symSz / 2, py - symSz / 2, symSz, symSz));
    };

    if (w_range == Tukey15IQR) {
        double wLoVal = 0.0, wHiVal = 0.0, fenceLo = 0.0, fenceHi = 0.0, tol = 0.0;
        computeTukeyWhiskers(dat, size, wLoVal, wHiVal, fenceLo, fenceHi, tol);
        for (int i = 0; i < size; ++i) {
            if (dat[i] < fenceLo - tol) {
                if (min_style != QwtSymbol::NoSymbol)
                    drawBoxSymAt(min_style, yMap.transform(dat[i]));
            } else if (dat[i] > fenceHi + tol) {
                if (max_style != QwtSymbol::NoSymbol)
                    drawBoxSymAt(max_style, yMap.transform(dat[i]));
            }
        }
    } else {
        if (min_style != QwtSymbol::NoSymbol)
            drawBoxSymAt(min_style, yMap.transform(dat[0]));
        if (max_style != QwtSymbol::NoSymbol)
            drawBoxSymAt(max_style, yMap.transform(dat[size - 1]));
    }
    if (p1_style != QwtSymbol::NoSymbol)
        drawBoxSymAt(p1_style, yMap.transform(gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.01)));
    if (p99_style != QwtSymbol::NoSymbol)
        drawBoxSymAt(p99_style, yMap.transform(gsl_stats_quantile_from_sorted_data(dat, 1, size, 0.99)));
    if (mean_style != QwtSymbol::NoSymbol)
        drawBoxSymAt(mean_style, yMap.transform(gsl_stats_mean(dat, 1, size)));

    painter->restore();
}

void BoxCurve::setBoxStyle(int style)
{
    if (b_style == style)
        return;
    b_style = style;
}

void BoxCurve::setBoxRange(int type, double coeff)
{
    if (b_style == WindBox) {
        b_range = r10_90;
        b_coeff = 90.0;
        return;
    }

    b_range = type;

    if (b_range == r25_75)
        b_coeff = 75.0;
    else if (b_range == r10_90)
        b_coeff = 90.0;
    else if (b_range == r5_95)
        b_coeff = 95.0;
    else if (b_range == r1_99)
        b_coeff = 99.0;
    else if (b_range == MinMax)
        b_coeff = 100.0;
    else if (b_range == Tukey15IQR)
        b_coeff = 1.5;
    else
        b_coeff = coeff;
}

void BoxCurve::setWhiskersRange(int type, double coeff)
{
    w_range = type;

    if (w_range == r25_75)
        w_coeff = 75.0;
    else if (w_range == r10_90)
        w_coeff = 90.0;
    else if (w_range == r5_95)
        w_coeff = 95.0;
    else if (w_range == r1_99)
        w_coeff = 99.0;
    else if (w_range == MinMax)
        w_coeff = 100.0;
    else if (w_range == Tukey15IQR)
        w_coeff = 1.5;
    else
        w_coeff = coeff;
}

QwtDoubleRect BoxCurve::boundingRect() const
{
    QwtDoubleRect rect = QwtPlotCurve::boundingRect();

    double dy = 0.2 * (rect.bottom() - rect.top());
    rect.setTop(rect.top() - dy);
    rect.setBottom(rect.bottom() + dy);

    rect.setLeft(rect.left() - 0.5);
    rect.setRight(rect.right() + 0.5);
    return rect;
}

bool BoxCurve::loadData()
{
    if (!d_table)
        return false;

    Column *x_dummy = nullptr;
    Column *y_col_ptr = nullptr;
    resolveColumns(x_dummy, y_col_ptr);
    int ycol = d_table->colIndex(title().text());
    if (!y_col_ptr && ycol >= 0 && ycol < d_table->numCols())
        y_col_ptr = d_table->column(ycol);
    if (!y_col_ptr) {
        remove();
        return false;
    }
    if (ycol < 0 || ycol >= d_table->numCols()) {
        for (int c = 0; c < d_table->numCols(); ++c) {
            if (d_table->column(c) == y_col_ptr) {
                ycol = c;
                break;
            }
        }
    }

    const int startR = qMax(0, d_start_row);
    const int endR = (d_end_row < 0) ? (y_col_ptr->rowCount() - 1)
                                     : qMin(d_end_row, y_col_ptr->rowCount() - 1);
    if (startR > endR) {
        remove();
        return false;
    }

    QVector<double> Y(endR - startR + 1);
    const auto yColType = (ycol >= 0) ? d_table->columnType(ycol) : y_col_ptr->columnMode();
    int size = 0;
    for (int row = startR; row <= endR; row++) {
        if (y_col_ptr->isInvalid(row) || y_col_ptr->isMasked(row))
            continue;
        double val = 0.0;
        if (yColType == SciDAVis::ColumnMode::Text) {
            const QString yval = y_col_ptr->textAt(row);
            bool valid_data = false;
            val = QLocale().toDouble(yval, &valid_data);
            if (!valid_data)
                val = QLocale::c().toDouble(yval, &valid_data);
            if (!valid_data)
                continue;
        } else {
            val = y_col_ptr->valueAt(row);
        }
        if (!std::isfinite(val))
            continue;
        Y[size++] = val;
    }

    if (size > 0) {
        Y.resize(size);
        gsl_sort(Y.data(), 1, size);
        setData(QwtSingleArrayData(this->x(0), Y, size));
    } else
        remove();

    return true;
}
