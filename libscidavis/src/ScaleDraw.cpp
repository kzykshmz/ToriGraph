/***************************************************************************
    File                 : ScaleDraw.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Extension to QwtScaleDraw

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
#include "ScaleDraw.h"
#include "MyParser.h"

#include <QDateTime>
#include <QMessageBox>
#include <QPainter>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>

#include <qwt_layout_metrics.h>
#include <qwt_painter.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>

namespace {

void stripNegativeZeroString(QString &s)
{
    if (!s.startsWith(QLatin1Char('-')))
        return;
    for (int i = 1; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('e') || ch == QLatin1Char('E'))
            break;
        if (ch != QLatin1Char('0') && ch != QLatin1Char('.'))
            return;
    }
    s.remove(0, 1);
}

QwtText formatSuperscriptScientificLabel(double transformed, char fmt, int prec,
                                         const QString &prefix, const QString &suffix)
{
    if (!std::isfinite(transformed))
        return QwtText();
    if (transformed == 0.0)
        return QwtText(prefix + QStringLiteral("0") + suffix);

    const bool autoTrimMantissa = (prec <= 0) || (fmt == 'g');
    const int effPrec = (prec <= 0) ? 6 : (fmt == 'g' ? qBound(2, prec, 6) : prec);
    QString txt = QLocale::c().toString(transformed, 'e', effPrec);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList list = txt.split(QLatin1Char('e'), Qt::SkipEmptyParts, Qt::CaseInsensitive);
#else
    QStringList list = txt.split(QLatin1Char('e'), QString::SkipEmptyParts, Qt::CaseInsensitive);
#endif
    if (list.size() < 2)
        return QwtText(prefix + txt + suffix);

    QString mantStr = list[0];
    if (autoTrimMantissa && mantStr.contains(QLatin1Char('.'))) {
        while (mantStr.endsWith(QLatin1Char('0')))
            mantStr.chop(1);
        if (mantStr.endsWith(QLatin1Char('.')))
            mantStr.chop(1);
    }

    bool okMant = false;
    double mant = QLocale::c().toDouble(mantStr, &okMant);
    if (!okMant)
        mant = mantStr.toDouble(&okMant);
    if (okMant && mant == 0.0)
        return QwtText(prefix + QStringLiteral("0") + suffix);

    QString s = list[1];
    QChar sign;
    if (!s.isEmpty() && (s[0] == QLatin1Char('+') || s[0] == QLatin1Char('-'))) {
        sign = s[0];
        s.remove(0, 1);
    }
    while (s.length() > 1 && s.startsWith(QLatin1Char('0')))
        s.remove(0, 1);
    if (sign == QLatin1Char('-'))
        s.prepend(sign);

    QString core;
    if (okMant && qAbs(mant - 1.0) < 1e-9)
        core = QStringLiteral("10<sup>") + s + QStringLiteral("</sup>");
    else if (okMant && qAbs(mant + 1.0) < 1e-9)
        core = QStringLiteral("-10<sup>") + s + QStringLiteral("</sup>");
    else
        core = mantStr + QString::fromUtf8("\xC3\x97") + QStringLiteral("10<sup>") + s
                + QStringLiteral("</sup>");

    return QwtText(prefix + core + suffix);
}

} // namespace

ScaleDraw::ScaleDraw(const QString &s)
    : formula_string(s),
      d_fmt('g'),
      d_prec(4),
      d_minTicks(In),
      d_majTicks(In),
      d_divide_factor(1.0)
{
}

ScaleDraw::ScaleDraw(const ScaleDraw &other, const QString &s)
    : QwtScaleDraw(other),
      formula_string(s),
      d_minTicks(other.minorTicksStyle()),
      d_majTicks(other.majorTicksStyle()),
      d_divide_factor(other.d_divide_factor),
      d_prefix(other.d_prefix),
      d_suffix(other.d_suffix),
      d_break_enabled(other.d_break_enabled),
      d_break_value(other.d_break_value),
      d_break_to(other.d_break_to),
      d_break_log_before(other.d_break_log_before),
      d_break_log_after(other.d_break_log_after),
      d_colorbar_border(other.d_colorbar_border),
      d_colorbar_width(other.d_colorbar_width),
      d_colorbar_pen_width(other.d_colorbar_pen_width),
      d_colorbar_stops(other.d_colorbar_stops)
{
    other.labelFormat(d_fmt, d_prec);
    invalidateCache();
}

void ScaleDraw::setTickLabelTransform(double divideByFactor, const QString &prefix,
                                      const QString &suffix)
{
    if (std::isfinite(divideByFactor) && std::fabs(divideByFactor) > 1e-300)
        d_divide_factor = divideByFactor;
    else
        d_divide_factor = 1.0;
    d_prefix = prefix;
    d_suffix = suffix;
    invalidateCache();
}

double ScaleDraw::transformValue(double value) const
{
    double res = value;
    if (!formula_string.isEmpty()) {
        try {
            MyParser parser;
            if (formula_string.contains("x"))
                parser.DefineVar(_T("x"), &value);
            else if (formula_string.contains("y"))
                parser.DefineVar(_T("y"), &value);

            parser.SetExpr(formula_string.toUtf8().constData());
            res = parser.Eval();
        } catch (mu::ParserError &) {
            return 0;
        }
    }
    if (std::isfinite(d_divide_factor) && std::fabs(d_divide_factor) > 1e-300
        && std::fabs(d_divide_factor - 1.0) > 1e-15) {
        res /= d_divide_factor;
    }
    return res;
}

/*!
  \brief Set the number format for the major scale labels

  Format character and precision have the same meaning as for
  sprintf().
  \param f format character 'e', 'f', 'g'
  \param prec
    - for 'e', 'f': the number of digits after the radix character (point)
    - for 'g': the maximum number of significant digits

  \sa labelFormat()
*/
void ScaleDraw::setLabelFormat(char f, int prec)
{
    d_fmt = f;
    d_prec = prec;
}

/*!
  \brief Return the number format for the major scale labels

  Format character and precision have the same meaning as for
  sprintf().
  \param f format character 'e', 'f' or 'g'
  \param prec
    - for 'e', 'f': the number of digits after the radix character (point)
    - for 'g': the maximum number of significant digits

  \sa setLabelFormat()
*/
void ScaleDraw::labelFormat(char &f, int &prec) const
{
    f = d_fmt;
    prec = d_prec;
}

QwtText ScaleDraw::label(double value) const
{
    double v = transformValue(value);
    if (!std::isfinite(v))
        return QwtText();

    if (d_break_enabled && (d_break_log_after || d_break_log_before)) {
        const double bf = qMin(d_break_value, d_break_to);
        const double bt = qMax(d_break_value, d_break_to);
        if (std::fabs(v) <= 1e-13 * qMax(1.0, std::fabs(bt)))
            return QwtText(d_prefix + QStringLiteral("0") + d_suffix);
        const bool inLogAfter = d_break_log_after && bt > 0.0 && value >= bt * (1.0 - 1e-11);
        const bool inLogBefore = d_break_log_before && bf > 0.0 && value > 0.0
                && value <= bf * (1.0 + 1e-11);
        if ((inLogAfter || inLogBefore) && d_fmt != 'f') {
            return formatSuperscriptScientificLabel(v, d_fmt, d_prec, d_prefix, d_suffix);
        }
    }

    const QwtScaleDiv scDiv = scaleDiv();
    const bool isLog10 = map().transformation()
            && map().transformation()->type() == QwtScaleTransformation::Log10;
    const double rawLo = qMin(scDiv.lowerBound(), scDiv.upperBound());
    const double rawHi = qMax(scDiv.lowerBound(), scDiv.upperBound());
    if (!isLog10 && rawLo <= 0.0 && rawHi >= 0.0) {
        const double tLo = transformValue(rawLo);
        const double tHi = transformValue(rawHi);
        const double spanT = (std::isfinite(tLo) && std::isfinite(tHi))
                ? std::fabs(tHi - tLo)
                : std::fabs(rawHi - rawLo);
        if (spanT > 0.0 && std::fabs(v) <= 1e-11 * spanT)
            v = 0.0;
    }

    const QLocale cLoc = QLocale::c();
    QString num;

    if (d_fmt == 'f') {
        num = cLoc.toString(v, 'f', qMax(0, d_prec));
        stripNegativeZeroString(num);
    } else if (d_fmt == 'e') {
        if (v == 0.0)
            v = 0.0;
        num = cLoc.toString(v, 'e', qMax(0, d_prec));
        stripNegativeZeroString(num);
    } else {
        bool formattedLinear = false;
        if (!isLog10) {
            const QwtValueList maj = scDiv.ticks(QwtScaleDiv::MajorTick);
            if (maj.size() >= 2) {
                QVector<double> tMaj;
                tMaj.reserve(maj.size());
                double maxAbsMaj = 0.0;
                for (int i = 0; i < maj.size(); ++i) {
                    double tv = transformValue(maj[i]);
                    if (!std::isfinite(tv))
                        continue;
                    if (rawLo <= 0.0 && rawHi >= 0.0 && std::fabs(tv) <= 1e-12)
                        tv = 0.0;
                    tMaj.append(tv);
                    maxAbsMaj = qMax(maxAbsMaj, std::fabs(tv));
                }
                std::sort(tMaj.begin(), tMaj.end());

                bool uniformStep = (tMaj.size() >= 2);
                if (uniformStep && !d_break_enabled && tMaj.size() >= 3) {
                    double minDt = 1e300;
                    double maxDt = 0.0;
                    for (int i = 0; i + 1 < tMaj.size(); ++i) {
                        const double dt = tMaj[i + 1] - tMaj[i];
                        if (dt > 1e-300) {
                            minDt = qMin(minDt, dt);
                            maxDt = qMax(maxDt, dt);
                        }
                    }
                    if (!(minDt < 1e290 && maxDt <= 1.03 * minDt))
                        uniformStep = false;
                }

                if (uniformStep && maxAbsMaj >= 1e-4 && maxAbsMaj <= 99999.5) {
                    int neededDec = -1;
                    double pow10 = 1.0;
                    const int maxDec = qMax(4, d_prec);
                    for (int d = 0; d <= qMin(6, maxDec); ++d) {
                        bool allExact = true;
                        for (double tv : tMaj) {
                            const double sc = tv * pow10;
                            if (std::fabs(sc - std::round(sc)) > 1e-5 * qMax(1.0, std::fabs(sc))) {
                                allExact = false;
                                break;
                            }
                        }
                        if (allExact) {
                            const double vSc = v * pow10;
                            if (std::fabs(vSc - std::round(vSc))
                                <= 1e-4 * qMax(1.0, std::fabs(vSc))) {
                                neededDec = d;
                            }
                            break;
                        }
                        pow10 *= 10.0;
                    }
                    if (neededDec >= 0) {
                        num = cLoc.toString(v, 'f', neededDec);
                        stripNegativeZeroString(num);
                        formattedLinear = true;
                    }
                }
            }
        }

        if (!formattedLinear) {
            if (v == 0.0) {
                num = QStringLiteral("0");
            } else if (std::fabs(v) >= 1.0 && std::fabs(v) <= 99999.5
                       && std::fabs(v - std::round(v)) <= 1e-7 * std::fabs(v)) {
                num = cLoc.toString(std::round(v), 'f', 0);
                stripNegativeZeroString(num);
            } else {
                num = cLoc.toString(v, 'g', qMax(1, d_prec));
                stripNegativeZeroString(num);
            }
        }
    }

    return QwtText(d_prefix + num + d_suffix);
}

void ScaleDraw::drawTick(QPainter *p, double value, int len) const
{
    QwtScaleDiv scDiv = scaleDiv();
    QwtValueList majTicks = scDiv.ticks(QwtScaleDiv::MajorTick);
    if (majTicks.contains(value) && (d_majTicks == In || d_majTicks == None))
        return;

    QwtValueList medTicks = scDiv.ticks(QwtScaleDiv::MediumTick);
    if (medTicks.contains(value) && (d_minTicks == In || d_minTicks == None))
        return;

    QwtValueList minTicks = scDiv.ticks(QwtScaleDiv::MinorTick);
    if (minTicks.contains(value) && (d_minTicks == In || d_minTicks == None))
        return;

    QwtScaleDraw::drawTick(p, value, len);
}

int ScaleDraw::extent(const QPen &pen, const QFont &font) const
{
    const int ext = QwtScaleDraw::extent(pen, font);
    if (d_colorbar_border) {
        const QwtMetricsMap map = QwtPainter::metricsMap();
        const int extra = map.isIdentity() ? 6 : map.screenToLayoutX(6);
        return ext + extra;
    }
    return ext;
}

static int s_suppressScaleBackboneDepth = 0;

void setToriSuppressScaleBackboneInPrint(bool suppress)
{
    if (suppress)
        ++s_suppressScaleBackboneDepth;
    else if (s_suppressScaleBackboneDepth > 0)
        --s_suppressScaleBackboneDepth;
}

void ScaleDraw::drawBackbone(QPainter *painter) const
{
    if (d_colorbar_border && d_colorbar_width > 0 && painter && length() > 1) {
        const QwtMetricsMap map = QwtPainter::metricsMap();
        const int cbW = map.isIdentity() ? d_colorbar_width : map.screenToLayoutX(d_colorbar_width);
        const int cbH = map.isIdentity() ? d_colorbar_width : map.screenToLayoutY(d_colorbar_width);
        const int layoutPw =
                map.isIdentity() ? d_colorbar_pen_width : map.screenToLayoutX(d_colorbar_pen_width);
        const int pw = qMax(1, qMax(layoutPw, painter->pen().width()));
        const QPoint p0 = pos();
        const int len = length();

        QRectF cbBase;
        switch (alignment()) {
        case RightScale:
            cbBase = QRectF(double(p0.x() - cbW), double(p0.y()), double(cbW), double(len));
            break;
        case LeftScale:
            cbBase = QRectF(double(p0.x()), double(p0.y()), double(cbW), double(len));
            break;
        case TopScale:
            cbBase = QRectF(double(p0.x()), double(p0.y()), double(len), double(cbH));
            break;
        case BottomScale:
            cbBase = QRectF(double(p0.x()), double(p0.y() - cbH), double(len), double(cbH));
            break;
        }

        const double pwF = double(pw);
        const QRectF cbOutline = cbBase.adjusted(-0.5 * pwF, -0.5 * pwF, 0.5 * pwF, 0.5 * pwF);

        painter->save();
        if (!d_colorbar_stops.isEmpty()) {
            const QwtScaleDiv scDiv = scaleDiv();
            const double loVal = qMin(scDiv.lowerBound(), scDiv.upperBound());
            const double hiVal = qMax(scDiv.lowerBound(), scDiv.upperBound());
            const double pLo = this->map().xTransform(loVal);
            const double pHi = this->map().xTransform(hiVal);
            QLinearGradient grad;
            if (alignment() == LeftScale || alignment() == RightScale) {
                grad.setStart(cbBase.left(), pLo);
                grad.setFinalStop(cbBase.left(), pHi);
            } else {
                grad.setStart(pLo, cbBase.top());
                grad.setFinalStop(pHi, cbBase.top());
            }
            grad.setStops(d_colorbar_stops);
            painter->fillRect(cbBase, grad);
        }
        QPen cbPen(painter->pen().color(), pwF, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        cbPen.setCosmetic(false);
        painter->setPen(cbPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(cbOutline);
        painter->restore();
        return;
    }

    if (s_suppressScaleBackboneDepth > 0)
        return;

    if (!d_break_enabled || !painter) {
        QwtScaleDraw::drawBackbone(painter);
        return;
    }

    const int bw2 = painter->pen().width() / 2;
    const QPoint p0 = pos();
    const int len = length() - 1;
    const int brkPx = map().transform(d_break_value);
    const int gap = 4;

    switch (alignment()) {
    case LeftScale:
    case RightScale: {
        const int x = (alignment() == LeftScale) ? (p0.x() - bw2) : (p0.x() + bw2);
        const int y0 = p0.y();
        const int y1 = p0.y() + len;
        if (brkPx > y0 + gap + 4 && brkPx < y1 - gap - 4) {
            QwtPainter::drawLine(painter, x, y0, x, brkPx - gap);
            QwtPainter::drawLine(painter, x, brkPx + gap, x, y1);
            QwtPainter::drawLine(painter, x - 5, brkPx - gap + 3, x + 5, brkPx - gap - 3);
            QwtPainter::drawLine(painter, x - 5, brkPx + gap + 3, x + 5, brkPx + gap - 3);
        } else {
            QwtScaleDraw::drawBackbone(painter);
        }
        break;
    }
    case TopScale:
    case BottomScale: {
        const int y = (alignment() == TopScale) ? (p0.y() - bw2) : (p0.y() + bw2);
        const int x0 = p0.x();
        const int x1 = p0.x() + len;
        if (brkPx > x0 + gap + 4 && brkPx < x1 - gap - 4) {
            QwtPainter::drawLine(painter, x0, y, brkPx - gap, y);
            QwtPainter::drawLine(painter, brkPx + gap, y, x1, y);
            QwtPainter::drawLine(painter, brkPx - gap - 3, y + 5, brkPx - gap + 3, y - 5);
            QwtPainter::drawLine(painter, brkPx + gap - 3, y + 5, brkPx + gap + 3, y - 5);
        } else {
            QwtScaleDraw::drawBackbone(painter);
        }
        break;
    }
    }
}

/*****************************************************************************
 *
 * Class QwtTextScaleDraw
 *
 *****************************************************************************/

QwtTextScaleDraw::QwtTextScaleDraw(const QMap<int, QString> &list) : labels(list) { }

QwtText QwtTextScaleDraw::label(double value) const
{
    if (!std::isfinite(value))
        return QwtText();
    const QwtScaleDiv scDiv = scaleDiv();
    if (!scDiv.contains(value))
        return QwtText();

    const double nearest = std::round(value);
    if (std::fabs(value - nearest) > 1e-4
        || nearest < double(std::numeric_limits<int>::min() + 1)
        || nearest > double(std::numeric_limits<int>::max() - 1)) {
        return QwtText();
    }
    const int int_value = static_cast<int>(nearest);
    return labels.contains(int_value) ? QwtText(labels.value(int_value)) : QwtText();
}

/*****************************************************************************
 *
 * Class TimeScaleDraw
 *
 *****************************************************************************/

TimeScaleDraw::TimeScaleDraw(const QTime &t, const QString &format) : t_origin(t), t_format(format)
{
}

QString TimeScaleDraw::origin()
{
    return t_origin.toString("hh:mm:ss.zzz");
}

QwtText TimeScaleDraw::label(double value) const
{
    QTime t = t_origin.addMSecs((int)value);
    return QwtText(t.toString(t_format));
}

/*****************************************************************************
 *
 * Class DateScaleDraw
 *
 *****************************************************************************/

DateScaleDraw::DateScaleDraw(const QDate &t, const QString &format) : t_origin(t), t_format(format)
{
}

QString DateScaleDraw::origin()
{
    return t_origin.toString();
}

QwtText DateScaleDraw::label(double value) const
{
    QDate t;
    if (t_origin.isValid())
        t = t_origin.addDays((int)floor(value));
    else
        t = QDate::fromJulianDay((int)floor(value));
    return QwtText(t.toString(t_format));
}

/*****************************************************************************
 *
 * Class DateTimeScaleDraw
 *
 *****************************************************************************/

DateTimeScaleDraw::DateTimeScaleDraw(const QDateTime &origin, const QString &format)
    : d_origin(origin), d_format(format)
{
}

QString DateTimeScaleDraw::origin()
{
    return d_origin.toString(d_format);
}

QwtText DateTimeScaleDraw::label(double value) const
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(round((value - 2440587.5) * 86400000.));
    return QwtText(dt.toString(d_format));
}

/*****************************************************************************
 *
 * Class WeekDayScaleDraw
 *
 *****************************************************************************/

WeekDayScaleDraw::WeekDayScaleDraw(NameFormat format) : d_format(format) { }

QwtText WeekDayScaleDraw::label(double value) const
{
    int val = int(transformValue(value)) % 7;

    if (val < 0)
        val = 7 - abs(val);
    else if (val == 0)
        val = 7;

    QString day;
    switch (d_format) {
    case ShortName:
        day = QLocale().dayName(val, QLocale::ShortFormat);
        break;

    case LongName:
        day = QLocale().dayName(val, QLocale::LongFormat);
        break;

    case Initial:
        day = (QLocale().dayName(val, QLocale::ShortFormat)).left(1);
        break;
    }
    return QwtText(day);
}

/*****************************************************************************
 *
 * Class MonthScaleDraw
 *
 *****************************************************************************/

MonthScaleDraw::MonthScaleDraw(NameFormat format) : d_format(format) { }

QwtText MonthScaleDraw::label(double value) const
{
    int val = int(transformValue(value)) % 12;

    if (val < 0)
        val = 12 - abs(val);
    else if (val == 0)
        val = 12;

    QString month;
    switch (d_format) {
    case ShortName:
        month = QLocale().monthName(val, QLocale::ShortFormat);
        break;

    case LongName:
        month = QLocale().monthName(val, QLocale::LongFormat);
        break;

    case Initial:
        month = (QLocale().monthName(val, QLocale::ShortFormat)).left(1);
        break;
    }
    return QwtText(month);
}

/*****************************************************************************
 *
 * Class QwtSupersciptsScaleDraw
 *
 *****************************************************************************/

QwtSupersciptsScaleDraw::QwtSupersciptsScaleDraw(const QString &s)
{
    setFormulaString(s);
}

QwtText QwtSupersciptsScaleDraw::label(double value) const
{
    char f;
    int prec;
    labelFormat(f, prec);

    const double transformed = transformValue(value);
    if (!std::isfinite(transformed))
        return QwtText();

    // Prevent horizontal rich-text superscript collisions on narrow sub-decade Log10 X axes
    // (e.g. 1.5x10^1 .. 3.5x10^1 on 85 mm single-column panels).
    if ((alignment() == BottomScale || alignment() == TopScale) && transformed > 0.0
        && this->map().transformation()
        && this->map().transformation()->type() == QwtScaleTransformation::Log10) {
        const QwtScaleDiv scDiv = scaleDiv();
        const double lo = qMin(scDiv.lowerBound(), scDiv.upperBound());
        const double hi = qMax(scDiv.lowerBound(), scDiv.upperBound());
        const QwtValueList maj = scDiv.ticks(QwtScaleDiv::MajorTick);
        if (lo > 0.0 && hi > lo && std::log10(hi / lo) < 1.65 && maj.size() >= 5) {
            auto mantissaOf = [](double v) -> double {
                const double e10 = std::floor(std::log10(v) + 1e-12);
                return v / std::pow(10.0, e10);
            };
            int intMantCount = 0;
            int matchIdx = -1;
            for (int i = 0; i < maj.size(); ++i) {
                const double tv = transformValue(maj[i]);
                if (tv > 0.0 && std::isfinite(tv)) {
                    const double m = mantissaOf(tv);
                    if (std::fabs(m - std::round(m)) <= 1e-4)
                        ++intMantCount;
                    if (std::fabs(tv - transformed) <= 1e-7 * qMax(1.0, std::fabs(transformed)))
                        matchIdx = i;
                }
            }
            const double curMant = mantissaOf(transformed);
            if (intMantCount >= 5) {
                const bool is125 = (std::fabs(curMant - 1.0) <= 1e-4
                                    || std::fabs(curMant - 2.0) <= 1e-4
                                    || std::fabs(curMant - 5.0) <= 1e-4);
                if (!is125)
                    return QwtText();
            } else if (intMantCount >= 2) {
                if (std::fabs(curMant - std::round(curMant)) > 1e-4)
                    return QwtText();
            } else if (matchIdx >= 0 && (matchIdx % 2) != 0) {
                return QwtText();
            }
        }
    }

    return formatSuperscriptScientificLabel(transformed, f, prec, prefix(), suffix());
}
