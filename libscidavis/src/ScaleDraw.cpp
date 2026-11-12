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
#include <cmath>

#include <qwt_layout_metrics.h>
#include <qwt_painter.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>

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
      d_colorbar_border(other.d_colorbar_border),
      d_colorbar_width(other.d_colorbar_width),
      d_colorbar_pen_width(other.d_colorbar_pen_width)
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
        const int extra = map.isIdentity() ? 10 : map.screenToLayoutX(10);
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

        QRect cbRect;
        switch (alignment()) {
        case RightScale:
            cbRect = QRect(QPoint(p0.x() - cbW, p0.y()), QPoint(p0.x(), p0.y() + len - 1));
            break;
        case LeftScale:
            cbRect = QRect(QPoint(p0.x(), p0.y()), QPoint(p0.x() + cbW, p0.y() + len - 1));
            break;
        case TopScale:
            cbRect = QRect(QPoint(p0.x(), p0.y()), QPoint(p0.x() + len - 1, p0.y() + cbH));
            break;
        case BottomScale:
            cbRect = QRect(QPoint(p0.x(), p0.y() - cbH), QPoint(p0.x() + len - 1, p0.y()));
            break;
        }

        painter->save();
        painter->setPen(QPen(painter->pen().color(), pw, Qt::SolidLine, Qt::SquareCap,
                             Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        QwtPainter::drawRect(painter, cbRect);
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
    const QwtScaleDiv scDiv = scaleDiv();
    if (!scDiv.contains(value))
        return QwtText();

    int int_value = (int)(value + value / 1e6);
    if (labels.contains(int_value))
        return QwtText(labels[int_value]);
    else
        return QwtText();
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
    if (transformed == 0.0)
        return QwtText(prefix() + QString("0") + suffix());

    QString txt = QLocale().toString(transformed, 'e', prec);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList list = txt.split('e', Qt::SkipEmptyParts, Qt::CaseInsensitive);
#else
    QStringList list = txt.split('e', QString::SkipEmptyParts, Qt::CaseInsensitive);
#endif
    if (list.size() < 2)
        return QwtText(prefix() + txt + suffix());

    bool okMant = false;
    double mant = QLocale().toDouble(list[0], &okMant);
    if (!okMant)
        mant = list[0].toDouble(&okMant);
    if (okMant && mant == 0.0)
        return QwtText(prefix() + QString("0") + suffix());

    QString s = list[1];
    QChar sign;
    if (!s.isEmpty() && (s[0] == '+' || s[0] == '-')) {
        sign = s[0];
        s.remove(0, 1);
    }

    while (s.length() > 1 && s.startsWith('0'))
        s.remove(0, 1);

    if (sign == '-')
        s.prepend(sign);

    QString core;
    if (okMant && qAbs(mant - 1.0) < 1e-9)
        core = "10<sup>" + s + "</sup>";
    else if (okMant && qAbs(mant + 1.0) < 1e-9)
        core = "-10<sup>" + s + "</sup>";
    else
        core = list[0] + QString::fromUtf8("\xC3\x97") + "10<sup>" + s + "</sup>";

    return QwtText(prefix() + core + suffix());
}
