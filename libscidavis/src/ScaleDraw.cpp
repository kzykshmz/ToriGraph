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

#include <QApplication>
#include <QDateTime>
#include <QFontMetricsF>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPair>
#include <QSet>
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

QFont makeSuperscriptFont(const QFont &base)
{
    QFont sup = base;
    if (sup.pointSizeF() > 0.0)
        sup.setPointSizeF(qMax(1.0, sup.pointSizeF() * 0.7));
    else if (sup.pixelSize() > 0)
        sup.setPixelSize(qMax(1, qRound(double(sup.pixelSize()) * 0.7)));
    return sup;
}

QString stripHtmlTags(const QString &s)
{
    QString out;
    out.reserve(s.size());
    bool inTag = false;
    for (const QChar ch : s) {
        if (ch == QLatin1Char('<')) {
            inTag = true;
        } else if (ch == QLatin1Char('>')) {
            inTag = false;
        } else if (!inTag) {
            out.append(ch);
        }
    }
    return out;
}

double measureSuperscriptLabelWidth(const QString &richText, const QFont &font, double rotDeg)
{
    const QFontMetricsF fm(font);
    const QFontMetricsF supFm(makeSuperscriptFont(font));

    double w = 0.0;
    int pos = 0;
    const QString openTag = QStringLiteral("<sup>");
    const QString closeTag = QStringLiteral("</sup>");
    while (pos < richText.size()) {
        const int openIdx = richText.indexOf(openTag, pos, Qt::CaseInsensitive);
        if (openIdx < 0) {
            w += fm.horizontalAdvance(stripHtmlTags(richText.mid(pos)));
            break;
        }
        if (openIdx > pos)
            w += fm.horizontalAdvance(stripHtmlTags(richText.mid(pos, openIdx - pos)));
        const int bodyStart = openIdx + openTag.size();
        const int closeIdx = richText.indexOf(closeTag, bodyStart, Qt::CaseInsensitive);
        if (closeIdx < 0) {
            w += supFm.horizontalAdvance(stripHtmlTags(richText.mid(bodyStart)));
            break;
        }
        w += supFm.horizontalAdvance(stripHtmlTags(richText.mid(bodyStart, closeIdx - bodyStart)));
        pos = closeIdx + closeTag.size();
    }

    if (std::fabs(rotDeg) > 1e-3) {
        constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
        const double rad = std::fabs(rotDeg) * kDegToRad;
        const double h = fm.height();
        return w * std::fabs(std::cos(rad)) + h * std::fabs(std::sin(rad));
    }
    return w;
}

double normalizedMantissa10(double v)
{
    const double e10 = std::floor(std::log10(v) + 1e-9);
    double m = v / std::pow(10.0, e10);
    if (m >= 10.0 - 1e-4)
        m /= 10.0;
    return m;
}

struct LogTickCandidate
{
    double tv;
    double px;
    double widthPx;
    bool isIntMant;
    bool is125;
    bool is15;
    bool isDecade;
};

QVector<int> selectNonCollidingLogTickIndices(const QVector<LogTickCandidate> &items, double minGap)
{
    auto collidesPair = [&](int idxA, int idxB) -> bool {
        const LogTickCandidate &a = items[idxA];
        const LogTickCandidate &b = items[idxB];
        return std::fabs(b.px - a.px) < 0.5 * (a.widthPx + b.widthPx) + minGap;
    };
    auto hasCollision = [&](const QVector<int> &idxs) -> bool {
        for (int k = 0; k + 1 < idxs.size(); ++k) {
            if (collidesPair(idxs[k], idxs[k + 1]))
                return true;
        }
        return false;
    };

    QVector<int> allIdxs(items.size());
    for (int i = 0; i < items.size(); ++i)
        allIdxs[i] = i;
    if (!hasCollision(allIdxs))
        return allIdxs;

    QVector<int> intIdxs, m125Idxs, m15Idxs, decIdxs;
    for (int i = 0; i < items.size(); ++i) {
        if (items[i].isIntMant)
            intIdxs.append(i);
        if (items[i].is125)
            m125Idxs.append(i);
        if (items[i].is15)
            m15Idxs.append(i);
        if (items[i].isDecade)
            decIdxs.append(i);
    }

    if (intIdxs.size() >= 2 && intIdxs.size() < items.size()
        && intIdxs.size() * 5 >= items.size() * 2 && !hasCollision(intIdxs)) {
        return intIdxs;
    }
    if (m125Idxs.size() >= 2 && !hasCollision(m125Idxs))
        return m125Idxs;
    if (m15Idxs.size() >= 2 && !hasCollision(m15Idxs))
        return m15Idxs;
    if (decIdxs.size() >= 2) {
        if (!hasCollision(decIdxs))
            return decIdxs;
        for (int step = 2; step <= decIdxs.size(); ++step) {
            QVector<int> strided;
            for (int k = 0; k < decIdxs.size(); k += step)
                strided.append(decIdxs[k]);
            if (strided.size() >= 2 && !hasCollision(strided))
                return strided;
        }
    }

    QVector<int> fallback;
    const int lastIdx = items.size() - 1;
    fallback.append(0);
    if (lastIdx > 0 && !collidesPair(0, lastIdx)) {
        int prev = 0;
        for (int i = 1; i < lastIdx; ++i) {
            if (!collidesPair(prev, i) && !collidesPair(i, lastIdx)) {
                fallback.append(i);
                prev = i;
            }
        }
        fallback.append(lastIdx);
    }
    return fallback;
}

QString normalizeExprDecimalCommas(const QString &expr)
{
    if (QLocale().decimalPoint() != QLatin1Char(','))
        return expr;
    static const QSet<QString> kMultiArgFns = {
        QStringLiteral("avg"),  QStringLiteral("min"),       QStringLiteral("max"),
        QStringLiteral("sum"),  QStringLiteral("beta"),      QStringLiteral("bessel_jn"),
        QStringLiteral("bessel_yn")
    };
    QVector<QPair<int, int>> skipRanges;
    const int n = expr.size();
    for (int i = 0; i < n; ++i) {
        if (!expr[i].isLetter() && expr[i] != QLatin1Char('_'))
            continue;
        const int idStart = i;
        while (i < n && (expr[i].isLetterOrNumber() || expr[i] == QLatin1Char('_')))
            ++i;
        const QString fnName = expr.mid(idStart, i - idStart).toLower();
        int j = i;
        while (j < n && expr[j].isSpace())
            ++j;
        if (j < n && expr[j] == QLatin1Char('(') && kMultiArgFns.contains(fnName)) {
            int depth = 1;
            int k = j + 1;
            bool hasCommaSpace = false;
            bool hasTightComma = false;
            while (k < n && depth > 0) {
                if (expr[k] == QLatin1Char('('))
                    ++depth;
                else if (expr[k] == QLatin1Char(')'))
                    --depth;
                else if (depth == 1 && expr[k] == QLatin1Char(',')) {
                    if ((k + 1 < n && expr[k + 1].isSpace())
                        || (k > 0 && expr[k - 1].isSpace()))
                        hasCommaSpace = true;
                    else
                        hasTightComma = true;
                }
                ++k;
            }
            if (depth == 0 && hasTightComma && !hasCommaSpace)
                skipRanges.append(qMakePair(j, k));
        }
        --i;
    }

    QString out = expr;
    for (int i = 1; i + 1 < n; ++i) {
        if (out[i] != QLatin1Char(',') || !out[i - 1].isDigit() || !out[i + 1].isDigit())
            continue;
        bool inSkip = false;
        for (const auto &rng : skipRanges) {
            if (i > rng.first && i < rng.second) {
                inSkip = true;
                break;
            }
        }
        if (!inSkip)
            out[i] = QLatin1Char('.');
    }
    return out;
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
      d_colorbar_stops(other.d_colorbar_stops),
      d_axis_font(other.d_axis_font),
      d_has_axis_font(other.d_has_axis_font)
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
        const QString normalized = normalizeExprDecimalCommas(formula_string);
        auto tryEval = [&](const QString &expr, double &outVal) -> bool {
            try {
                MyParser parser;
                if (expr.contains(QLatin1Char('x')))
                    parser.DefineVar(_T("x"), &value);
                else if (expr.contains(QLatin1Char('y')))
                    parser.DefineVar(_T("y"), &value);

                parser.SetExpr(expr);
                int nRes = 0;
                const double *vals = parser.Eval(nRes);
                if (nRes != 1 || !vals)
                    return false;
                outVal = vals[0];
                return true;
            } catch (mu::ParserError &) {
                return false;
            }
        };
        double evalRes = 0.0;
        if (tryEval(normalized, evalRes)
            || (normalized != formula_string && tryEval(formula_string, evalRes))) {
            res = evalRes;
        } else {
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
        QVector<double> tMaj; // transformed, sorted major tick values (linear axes only)
        if (!isLog10) {
            const QwtValueList maj = scDiv.ticks(QwtScaleDiv::MajorTick);
            if (maj.size() >= 2) {
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
            auto formatAuto = [&cLoc](double x, int prec) -> QString {
                QString s;
                if (x == 0.0) {
                    s = QStringLiteral("0");
                } else if (std::fabs(x) >= 1.0 && std::fabs(x) <= 99999.5
                           && std::fabs(x - std::round(x)) <= 1e-7 * std::fabs(x)) {
                    s = cLoc.toString(std::round(x), 'f', 0);
                    stripNegativeZeroString(s);
                } else {
                    s = cLoc.toString(x, 'g', qMax(1, prec));
                    stripNegativeZeroString(s);
                }
                return s;
            };

            // Neighbouring major ticks must not share a label (101300/101320/101340 all became
            // "1.013e+05"; 1.00000/1.00005/1.00010 all became "1"). If this label equals a
            // neighbour's at the configured precision, raise the significant digits to what the
            // local tick spacing needs. Labels that are already distinct are left untouched.
            int prec = d_prec;
            int idx = -1;
            for (int i = 0; tMaj.size() >= 2 && i < tMaj.size(); ++i) {
                const double gap = (i + 1 < tMaj.size()) ? (tMaj[i + 1] - tMaj[i])
                                                         : (tMaj[i] - tMaj[i - 1]);
                if (std::fabs(tMaj[i] - v) <= 1e-6 * std::fabs(gap)) {
                    idx = i;
                    break;
                }
            }
            if (idx >= 0) {
                QVector<double> nb;
                if (idx > 0)
                    nb.append(tMaj[idx - 1]);
                if (idx + 1 < tMaj.size())
                    nb.append(tMaj[idx + 1]);
                auto collides = [&](int p) {
                    const QString self = formatAuto(v, p);
                    for (double x : nb) {
                        if (formatAuto(x, p) == self)
                            return true;
                    }
                    return false;
                };
                if (collides(prec)) {
                    double minGap = 1e300;
                    double maxAbs = std::fabs(v);
                    for (double x : nb) {
                        if (std::fabs(x - v) > 0.0)
                            minGap = qMin(minGap, std::fabs(x - v));
                        maxAbs = qMax(maxAbs, std::fabs(x));
                    }
                    int need = prec + 1;
                    if (minGap < 1e300 && maxAbs > 0.0)
                        need = int(std::ceil(std::log10(maxAbs / minGap))) + 1;
                    int p = qMin(15, qMax(prec + 1, need));
                    while (p < 15 && collides(p))
                        ++p;
                    prec = p;
                }
            }
            num = formatAuto(v, prec);
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

    if (!p || len <= 0)
        return;

    if (d_colorbar_border) {
        QwtScaleDraw::drawTick(p, value, len);
        return;
    }

    // Draw outward ticks starting flush at the canvas boundary (without QwtScaleDraw's pw2 offset),
    // connecting seamlessly to the unified frame drawn inside Plot::drawItems.
    const int tval = map().transform(value);
    const QPoint p0 = pos();
    switch (alignment()) {
    case LeftScale:
        QwtPainter::drawLine(p, p0.x() + 1, tval, p0.x() - len, tval);
        break;
    case RightScale:
        QwtPainter::drawLine(p, p0.x() - 1, tval, p0.x() + len, tval);
        break;
    case BottomScale:
        QwtPainter::drawLine(p, tval, p0.y() - 1, tval, p0.y() + len);
        break;
    case TopScale:
        QwtPainter::drawLine(p, tval, p0.y() + 1, tval, p0.y() - len);
        break;
    }
}

void ScaleDraw::setAxisFontHint(const QFont &font)
{
    if (!d_has_axis_font || d_axis_font != font) {
        d_axis_font = font;
        d_has_axis_font = true;
        invalidateCache();
    }
}

QFont ScaleDraw::axisFontHint() const
{
    if (d_has_axis_font)
        return d_axis_font;
    return QApplication::font();
}

void ScaleDraw::syncLayoutMetrics(int axisLengthPx, const QFont &font)
{
    if (d_cached_layout_len != axisLengthPx || !d_has_axis_font || d_axis_font != font) {
        d_cached_layout_len = axisLengthPx;
        d_axis_font = font;
        d_has_axis_font = true;
        invalidateCache();
    }
}

int ScaleDraw::extent(const QPen &pen, const QFont &font) const
{
    const_cast<ScaleDraw *>(this)->syncLayoutMetrics(length(), font);
    const int ext = QwtScaleDraw::extent(pen, font);
    if (d_colorbar_border) {
        const QwtMetricsMap map = QwtPainter::metricsMap();
        const int extra = map.isIdentity() ? 6 : map.screenToLayoutX(6);
        return ext + extra;
    }
    return ext;
}

void ScaleDraw::drawLabel(QPainter *painter, double value) const
{
    if (painter)
        const_cast<ScaleDraw *>(this)->syncLayoutMetrics(length(), painter->font());
    QwtScaleDraw::drawLabel(painter, value);
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

    // Non-colorbar 2D axis backbones and frame corners are drawn as a unified mitered rectangle
    // inside Plot::drawItems so the 4 corners never have disconnected 2x2-pixel gaps on screen.
    return;
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
    if (!std::isfinite(value) || value < -9e18 || value > 9e18)
        return QwtText();
    // Compute in 64-bit ms modulo 24h before QTime::addMSecs(int) so spans >= 24.85 days never
    // overflow 32-bit int.
    constexpr qint64 kMsPerDay = 86400000LL;
    const qint64 ms = static_cast<qint64>(std::llround(value));
    const int dayMs = static_cast<int>(ms % kMsPerDay);
    const QTime t = t_origin.addMSecs(dayMs);
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
    if (!std::isfinite(value))
        return QwtText();

    // ColumnMode::DateTime values (PlotCurve/VectorCurve/AxesDialog) are stored as absolute
    // Julian Days (>= ~1.72e6 for AD dates, ~2.44e6 around 1970..2030), while PlotCurve also
    // records the first sample's QDateTime in d_origin. Small offsets (< 1721058.0, i.e. before
    // Julian Day 0001-01-01) represent days elapsed relative to d_origin when d_origin is valid.
    constexpr double kJulianDayAd1 = 1721058.0;
    constexpr double kJulianUnixEpoch = 2440587.5;
    constexpr double kMsPerDay = 86400000.0;

    if (d_origin.isValid() && value < kJulianDayAd1) {
        if (std::fabs(value) > 1e11)
            return QwtText();
        const qint64 offsetMs = static_cast<qint64>(std::llround(value * kMsPerDay));
        const QDateTime dt = d_origin.addMSecs(offsetMs);
        return QwtText(dt.toString(d_format));
    }

    const double epochDays = value - kJulianUnixEpoch;
    if (std::fabs(epochDays) > 1e11)
        return QwtText();
    const qint64 epochMs = static_cast<qint64>(std::llround(epochDays * kMsPerDay));
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(epochMs);
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

    // Dynamically prevent horizontal rich-text superscript collisions on Log10 X axes using actual
    // scale-map pixel positions and QFontMetricsF label widths of the axis font, keeping 1/2/5
    // preference when thinning is needed (and preserving dense ticks on wide panels).
    if ((alignment() == BottomScale || alignment() == TopScale) && transformed > 0.0
        && this->map().transformation()
        && this->map().transformation()->type() == QwtScaleTransformation::Log10) {
        const QwtScaleMap &scMap = this->map();
        const double spanPx = std::fabs(scMap.p2() - scMap.p1());
        const QwtScaleDiv scDiv = scaleDiv();
        const QwtValueList maj = scDiv.ticks(QwtScaleDiv::MajorTick);
        if (spanPx > 20.0 && maj.size() >= 2) {
            const QFont font = axisFontHint();
            const QFontMetricsF fm(font);
            const double minGap = qMax(4.0, 0.35 * fm.height());
            const double rotDeg = labelRotation();

            QVector<LogTickCandidate> items;
            items.reserve(maj.size());
            for (int i = 0; i < maj.size(); ++i) {
                const double rv = maj[i];
                if (!scDiv.contains(rv))
                    continue;
                const double tv = transformValue(rv);
                if (!(tv > 0.0) || !std::isfinite(tv))
                    continue;
                const double px = scMap.xTransform(rv);
                if (!std::isfinite(px))
                    continue;
                const QwtText candText =
                        formatSuperscriptScientificLabel(tv, f, prec, prefix(), suffix());
                const double wPx = measureSuperscriptLabelWidth(candText.text(), font, rotDeg);
                const double m = normalizedMantissa10(tv);
                const bool isInt = (std::fabs(m - std::round(m)) <= 1e-4);
                const bool is1 = (std::fabs(m - 1.0) <= 1e-4);
                const bool is2 = (std::fabs(m - 2.0) <= 1e-4);
                const bool is5 = (std::fabs(m - 5.0) <= 1e-4);
                items.append({ tv, px, wPx, isInt, is1 || is2 || is5, is1 || is5, is1 });
            }

            std::sort(items.begin(), items.end(),
                      [](const LogTickCandidate &a, const LogTickCandidate &b) {
                          return a.px < b.px;
                      });

            int matchPos = -1;
            for (int i = 0; i < items.size(); ++i) {
                if (std::fabs(items[i].tv - transformed)
                    <= 1e-7 * qMax(1.0, std::fabs(transformed))) {
                    matchPos = i;
                    break;
                }
            }

            if (matchPos >= 0 && items.size() >= 2) {
                const QVector<int> chosen = selectNonCollidingLogTickIndices(items, minGap);
                if (!chosen.contains(matchPos))
                    return QwtText();
            }
        }
    }

    return formatSuperscriptScientificLabel(transformed, f, prec, prefix(), suffix());
}
