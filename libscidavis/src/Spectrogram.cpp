/***************************************************************************
        File                 : Spectrogram.cpp
        Project              : SciDAVis
--------------------------------------------------------------------
        Copyright            : (C) 2006 by Ion Vasilief
        Email (use @ for *)  : ion_vasilief*yahoo.fr
        Description          : SciDAVis's Spectrogram Class
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

#include "Spectrogram.h"
#include "ColorButton.h"
#include "Plot.h"
#include "ScaleDraw.h"
#include <algorithm>
#include <cmath>
#include <math.h>
#include <qpen.h>
#include <qmessagebox.h>
#include <QFont>
#include <QPainter>
#include <QRegularExpression>

#include <qwt_plot_layout.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>

namespace {

QString unwrapColorBarTitleNoWrap(const QString &s)
{
    QString out = s;
    const QString pre = QStringLiteral("<span style=\"white-space:nowrap;\">");
    const QString suf = QStringLiteral("</span>");
    while (out.startsWith(pre) && out.endsWith(suf) && out.size() >= pre.size() + suf.size())
        out = out.mid(pre.size(), out.size() - pre.size() - suf.size());
    return out;
}

QString wrapColorBarTitleNoWrap(const QString &raw)
{
    const QString clean = unwrapColorBarTitleNoWrap(raw);
    if (clean.trimmed().isEmpty() || clean.contains(QLatin1Char('\n'))
        || clean.contains(QLatin1String("<br"), Qt::CaseInsensitive)) {
        return clean;
    }
    static const QRegularExpression kRichTagRx(
            QStringLiteral("<\\s*(?:i|sub|sup|b|u|span|font)\\b"),
            QRegularExpression::CaseInsensitiveOption);
    if (clean.contains(kRichTagRx)) {
        return QStringLiteral("<span style=\"white-space:nowrap;\">") + clean
                + QStringLiteral("</span>");
    }
    if (clean.contains(QLatin1Char(' '))) {
        QString esc = clean;
        esc.replace(QLatin1String("&"), QLatin1String("&amp;"));
        esc.replace(QLatin1String("<"), QLatin1String("&lt;"));
        esc.replace(QLatin1String(">"), QLatin1String("&gt;"));
        return QStringLiteral("<span style=\"white-space:nowrap;\">") + esc
                + QStringLiteral("</span>");
    }
    return clean;
}

//! E11b: colour bar map for a logarithmic colour axis. The colour bar axis has a log10 scale
//! engine, so QwtScaleWidget asks for colours of data values; the colour follows log10(value)
//! between the interval ends (the data image itself is drawn from log10 values).
class LogColorBarMap : public QwtColorMap
{
public:
    explicit LogColorBarMap(const QwtColorMap &base) : QwtColorMap(QwtColorMap::RGB), d_base(base.copy()) { }
    LogColorBarMap(const LogColorBarMap &o) : QwtColorMap(QwtColorMap::RGB), d_base(o.d_base->copy()) { }
    ~LogColorBarMap() override { delete d_base; }
    QwtColorMap *copy() const override { return new LogColorBarMap(*this); }
    QRgb rgb(const QwtDoubleInterval &iv, double v) const override
    {
        const double lo = std::log10(qMax(iv.minValue(), 1e-300));
        const double hi = std::log10(qMax(iv.maxValue(), 1e-300));
        double t = 0.0;
        if (v > 0.0 && hi > lo)
            t = (std::log10(v) - lo) / (hi - lo);
        return d_base->rgb(QwtDoubleInterval(0.0, 1.0), qBound(0.0, t, 1.0));
    }
    unsigned char colorIndex(const QwtDoubleInterval &, double) const override { return 0; }

private:
    LogColorBarMap &operator=(const LogColorBarMap &) = delete;
    QwtColorMap *d_base;
};

} // namespace

Spectrogram::Spectrogram()
    : QwtPlotSpectrogram(),
      d_matrix(0),
      color_axis(QwtPlot::yRight),
      color_map_policy(Default),
      d_preset(PresetLegacyDefault),
      color_map(defaultColorMap()),
      d_base_custom_map(defaultColorMap())
{
    setRenderHint(QwtPlotItem::RenderAntialiased, true);
}

Spectrogram::Spectrogram(Matrix *m)
    : QwtPlotSpectrogram(m ? QString(m->name()) : QString()),
      d_matrix(m),
      color_axis(QwtPlot::yRight),
      color_map_policy(Default),
      d_preset(PresetLegacyDefault),
      color_map(defaultColorMap()),
      d_base_custom_map(defaultColorMap())
{
    setRenderHint(QwtPlotItem::RenderAntialiased, true);
    if (!m)
        return;
    setData(MatrixData(m));
    const QwtDoubleInterval zRange = data().range();
    const int defaultLevels = 5;
    const double step = fabs(zRange.maxValue() - zRange.minValue()) / (double)defaultLevels;

    QwtValueList contourLevels;
    if (step > 0.0 && std::isfinite(step)) {
        const double zMin = qMin(zRange.minValue(), zRange.maxValue());
        for (int i = 1; i < defaultLevels; ++i)
            contourLevels += zMin + i * step;
    }

    setContourLevels(contourLevels);
}

QwtDoubleInterval Spectrogram::rawDataRange() const
{
    if (const MatrixData *md = dynamic_cast<const MatrixData *>(&data())) {
        if (md->flags() & MatrixData::FlagLog10) {
            const QwtDoubleInterval lr = md->rawRange();
            return QwtDoubleInterval(std::pow(10.0, lr.minValue()), std::pow(10.0, lr.maxValue()));
        }
        return md->rawRange();
    }
    if (d_matrix) {
        double lo = 0.0, hi = 1.0;
        d_matrix->range(&lo, &hi);
        if (std::isfinite(lo) && std::isfinite(hi) && lo <= hi)
            return QwtDoubleInterval(lo, hi);
    }
    return data().range();
}

static QwtDoubleInterval padDegenerateColorRange(const QwtDoubleInterval &raw)
{
    double zMin = raw.minValue();
    double zMax = raw.maxValue();
    if (!std::isfinite(zMin) || !std::isfinite(zMax))
        return QwtDoubleInterval(0.0, 1.0);
    if (zMin > zMax)
        std::swap(zMin, zMax);
    if (std::abs(zMax - zMin) <= 1e-12) {
        const double z = 0.5 * (zMin + zMax);
        const double pad = std::max(0.5, 0.1 * std::abs(z));
        return QwtDoubleInterval(z - pad, z + pad);
    }
    return QwtDoubleInterval(zMin, zMax);
}

QwtDoubleInterval Spectrogram::effectiveValueRange() const
{
    if (d_log_color) // log10 units; MatrixData converts a custom (data unit) range itself
        return padDegenerateColorRange(data().range());
    if (d_custom_z_range && std::isfinite(d_custom_z_min) && std::isfinite(d_custom_z_max)
        && d_custom_z_min < d_custom_z_max)
        return padDegenerateColorRange(QwtDoubleInterval(d_custom_z_min, d_custom_z_max));
    return padDegenerateColorRange(data().range());
}

QwtDoubleInterval Spectrogram::boundingInterval(Qt::Axis axis) const
{
    if (axis == Qt::ZAxis)
        return effectiveValueRange();
    const QwtDoubleRect br = boundingRect();
    if (axis == Qt::XAxis)
        return QwtDoubleInterval(br.left(), br.right());
    return QwtDoubleInterval(br.top(), br.bottom());
}

void Spectrogram::enforceColorBarArialFont()
{
    QwtPlot *plot = this->plot();
    if (!plot || color_axis < 0 || color_axis >= QwtPlot::axisCnt)
        return;

    QFont tickFont = plot->axisFont(color_axis);
    if (tickFont.family() != QLatin1String("Arial")
        || tickFont.hintingPreference() != QFont::PreferNoHinting) {
        tickFont.setFamily(QStringLiteral("Arial"));
        if (tickFont.pointSize() <= 0 && tickFont.pixelSize() <= 0)
            tickFont.setPointSize(10);
        tickFont.setHintingPreference(QFont::PreferNoHinting);
        plot->setAxisFont(color_axis, tickFont);
    }

    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (!colorAxis)
        return;

    QFont wf = colorAxis->font();
    if (wf.family() != QLatin1String("Arial")
        || wf.hintingPreference() != QFont::PreferNoHinting) {
        wf.setFamily(QStringLiteral("Arial"));
        wf.setHintingPreference(QFont::PreferNoHinting);
        colorAxis->setFont(wf);
    }

    QwtText t = colorAxis->title();
    const QString rawTitle = !d_colorbar_title.isEmpty()
            ? d_colorbar_title
            : unwrapColorBarTitleNoWrap(t.text());
    if (!rawTitle.isEmpty())
        t.setText(wrapColorBarTitleNoWrap(rawTitle));
    QFont tf = t.font();
    tf.setFamily(QStringLiteral("Arial"));
    tf.setBold(false);
    if (tf.pointSize() <= 0 && tf.pixelSize() <= 0)
        tf.setPointSize(11);
    tf.setHintingPreference(QFont::PreferNoHinting);
    t.setFont(tf);
    colorAxis->setTitle(t);

    if (colorAxis->isColorBarEnabled()) {
        colorAxis->setMargin(4);
        colorAxis->setSpacing(0);
        colorAxis->setMinBorderDist(0, 0);
        int sDist = 0, eDist = 0;
        colorAxis->getBorderDistHint(sDist, eDist);
        colorAxis->setBorderDist(sDist, eDist);
        if (plot->plotLayout()) {
            plot->plotLayout()->setAlignCanvasToScales(true);
            plot->plotLayout()->setCanvasMargin(0);
        }
        if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(plot->axisScaleDraw(color_axis))) {
            sd->enableComponent(QwtAbstractScaleDraw::Backbone, true);
            sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
            sd->setMajorTicksStyle(ScaleDraw::Out);
            sd->setMinorTicksStyle(ScaleDraw::None);
            sd->setColorBarBorder(true, colorAxis->colorBarWidth(),
                                  qMax(1, colorAxis->penWidth()));
            QGradientStops stops;
            const QwtDoubleInterval unitIv(0.0, 1.0);
            const QwtColorMap &cm = colorMap();
            constexpr int kGradSamples = 32;
            stops.reserve(kGradSamples + 1);
            for (int k = 0; k <= kGradSamples; ++k) {
                const double t = double(k) / double(kGradSamples);
                stops.append(qMakePair(qreal(t), QColor(cm.rgb(unitIv, t))));
            }
            sd->setColorBarGradient(stops);
        }
    } else if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(plot->axisScaleDraw(color_axis))) {
        sd->setColorBarBorder(false, 0, 1);
    }
}

void Spectrogram::syncColorScale()
{
    QwtPlot *plot = this->plot();
    if (!plot || color_axis < 0 || color_axis >= QwtPlot::axisCnt)
        return;

    const QwtDoubleInterval zRange = effectiveValueRange();
    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (colorAxis) {
        if (d_log_color)
            colorAxis->setColorMap(colorAxisRange(), LogColorBarMap(colorMap()));
        else
            colorAxis->setColorMap(zRange, colorMap());
    }

    if (colorAxis && colorAxis->isColorBarEnabled()) {
        syncColorAxisEngine();
        const QwtDoubleInterval axisRange = colorAxisRange();
        plot->setAxisScale(color_axis, axisRange.minValue(), axisRange.maxValue());
        enforceColorBarArialFont();
    }
}

void Spectrogram::setColorBarTitle(const QString &title)
{
    d_colorbar_title = unwrapColorBarTitleNoWrap(title);
    QwtPlot *plot = this->plot();
    if (!plot || color_axis < 0 || color_axis >= QwtPlot::axisCnt)
        return;
    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (!colorAxis)
        return;
    QwtText t = colorAxis->title();
    t.setText(wrapColorBarTitleNoWrap(d_colorbar_title));
    QFont tf = t.font();
    tf.setFamily(QStringLiteral("Arial"));
    tf.setBold(false);
    if (tf.pointSize() <= 0 && tf.pixelSize() <= 0)
        tf.setPointSize(11);
    tf.setHintingPreference(QFont::PreferNoHinting);
    t.setFont(tf);
    colorAxis->setTitle(t);
    enforceColorBarArialFont();
    plot->updateLayout();
}

void Spectrogram::setCustomValueRange(bool enabled, double zMin, double zMax)
{
    if (enabled) {
        if (!std::isfinite(zMin))
            zMin = 0.0;
        if (!std::isfinite(zMax))
            zMax = zMin + 1.0;
        if (zMin > zMax)
            std::swap(zMin, zMax);
        if (std::fabs(zMax - zMin) < 1e-300)
            zMax = zMin + 1.0;
    }
    d_custom_z_range = enabled;
    d_custom_z_min = zMin;
    d_custom_z_max = zMax;

    const int prevLevels = qMax(2, levels());
    if (d_matrix)
        setData(MatrixData(d_matrix, d_custom_z_range, d_custom_z_min, d_custom_z_max,
                           dataFlags()));
    setLevelsNumber(prevLevels);
    syncColorScale();
    itemChanged();
}

void Spectrogram::updateData(Matrix *m)
{
    if (!m)
        return;

    d_matrix = m;
    const int prevLevels = qMax(2, levels());
    setData(MatrixData(m, d_custom_z_range, d_custom_z_min, d_custom_z_max, dataFlags()));
    setLevelsNumber(prevLevels);

    QwtPlot *plot = this->plot();
    if (!plot)
        return;

    syncColorScale();
    const QwtDoubleInterval zRange = colorAxisRange();
    plot->setAxisScale(color_axis, zRange.minValue(), zRange.maxValue());
    enforceColorBarArialFont();
    plot->replot();
}

void Spectrogram::setLevelsNumber(int levels)
{
    if (levels < 2)
        levels = 2;
    const QwtDoubleInterval zRange = effectiveValueRange();
    const double step = fabs(zRange.maxValue() - zRange.minValue()) / (double)levels;

    QwtValueList contourLevels;
    if (step > 0.0 && std::isfinite(step)) {
        const double zMin = qMin(zRange.minValue(), zRange.maxValue());
        for (int i = 1; i < levels; ++i)
            contourLevels += zMin + i * step;
    }

    setContourLevels(contourLevels);
}

bool Spectrogram::hasColorScale()
{
    QwtPlot *plot = this->plot();
    if (!plot)
        return false;

    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    return colorAxis ? colorAxis->isColorBarEnabled() : false;
}

void Spectrogram::showColorScale(int axis, bool on)
{
    QwtPlot *plot = this->plot();
    if (!plot) {
        color_axis = axis;
        return;
    }

    if (hasColorScale() == on && color_axis == axis) {
        syncColorScale();
        enforceColorBarArialFont();
        plot->updateLayout();
        return;
    }

    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (colorAxis) {
        colorAxis->setColorBarEnabled(false);
        colorAxis->setMargin(0);
    }
    if (ScaleDraw *oldSd = dynamic_cast<ScaleDraw *>(plot->axisScaleDraw(color_axis)))
        oldSd->setColorBarBorder(false, 0, 1);

    color_axis = axis;

    // We must switch main and the color scale axes and their respective scales
    int xAxis = this->xAxis();
    int yAxis = this->yAxis();
    int oldMainAxis = 0;
    if (axis == QwtPlot::xBottom || axis == QwtPlot::xTop) {
        oldMainAxis = xAxis;
        xAxis = 5 - color_axis;
    } else if (axis == QwtPlot::yLeft || axis == QwtPlot::yRight) {
        oldMainAxis = yAxis;
        yAxis = 1 - color_axis;
    }

    // First we switch axes
    setAxis(xAxis, yAxis);

    // Next we switch axes scales
    QwtScaleDiv *scDiv = plot->axisScaleDiv(oldMainAxis);
#if QWT_VERSION >= 0x050200
    if (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
        plot->setAxisScale(xAxis, scDiv->lowerBound(), scDiv->upperBound());
    else if (axis == QwtPlot::yLeft || color_axis == QwtPlot::yRight)
        plot->setAxisScale(yAxis, scDiv->lowerBound(), scDiv->upperBound());
#else
    if (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
        plot->setAxisScale(xAxis, scDiv->lBound(), scDiv->hBound());
    else if (axis == QwtPlot::yLeft || color_axis == QwtPlot::yRight)
        plot->setAxisScale(yAxis, scDiv->lBound(), scDiv->hBound());
#endif

    const QwtDoubleInterval zRange = effectiveValueRange();
    colorAxis = plot->axisWidget(color_axis);
    syncColorAxisEngine();
    plot->setAxisScale(color_axis, colorAxisRange().minValue(), colorAxisRange().maxValue());
    colorAxis->setColorBarEnabled(on);
    if (d_log_color)
        colorAxis->setColorMap(colorAxisRange(), LogColorBarMap(colorMap()));
    else
        colorAxis->setColorMap(zRange, colorMap());
    colorAxis->setMargin(on ? 4 : 0);
    if (on) {
        colorAxis->setSpacing(0);
        colorAxis->setMinBorderDist(0, 0);
        if (plot->plotLayout()) {
            plot->plotLayout()->setAlignCanvasToScales(true);
            plot->plotLayout()->setCanvasMargin(0);
        }
    } else if (plot->plotLayout()) {
        bool anyOtherColorBar = false;
        for (QwtPlotItem *it : plot->itemList()) {
            if (it && it != this && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                if (static_cast<Spectrogram *>(it)->hasColorScale()) {
                    anyOtherColorBar = true;
                    break;
                }
            }
        }
        if (!anyOtherColorBar)
            plot->plotLayout()->setAlignCanvasToScales(false);
    }
    if (ScaleDraw *newSd = dynamic_cast<ScaleDraw *>(plot->axisScaleDraw(color_axis))) {
        newSd->setColorBarBorder(on, colorAxis->colorBarWidth(),
                                 qMax(1, colorAxis->penWidth()));
    }
    if (on && !plot->axisEnabled(color_axis))
        plot->enableAxis(color_axis);
    enforceColorBarArialFont();
    if (on)
        colorAxis->show();
    plot->updateLayout();
}

int Spectrogram::colorBarWidth()
{
    QwtPlot *plot = this->plot();
    if (!plot)
        return 16;

    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    return colorAxis ? colorAxis->colorBarWidth() : 16;
}

void Spectrogram::setColorBarWidth(int width)
{
    QwtPlot *plot = this->plot();
    if (!plot)
        return;

    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (colorAxis) {
        colorAxis->setColorBarWidth(width);
        if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(plot->axisScaleDraw(color_axis))) {
            sd->setColorBarBorder(colorAxis->isColorBarEnabled(), colorAxis->colorBarWidth(),
                                  qMax(1, colorAxis->penWidth()));
        }
        plot->updateLayout();
    }
}

Spectrogram *Spectrogram::copy()
{
    Spectrogram *new_s = new Spectrogram(matrix());
    new_s->setDisplayMode(QwtPlotSpectrogram::ImageMode,
                          testDisplayMode(QwtPlotSpectrogram::ImageMode));
    new_s->setDisplayMode(QwtPlotSpectrogram::ContourMode,
                          testDisplayMode(QwtPlotSpectrogram::ContourMode));
    new_s->d_preset = d_preset;
    new_s->d_reverse_palette = d_reverse_palette;
    new_s->d_base_custom_map = d_base_custom_map;
    new_s->color_map = color_map;
    new_s->setColorMap(colorMap());
    new_s->setAxis(xAxis(), yAxis());
    new_s->setDefaultContourPen(defaultContourPen());
    new_s->d_colorbar_title = d_colorbar_title;
    if (d_custom_z_range)
        new_s->setCustomValueRange(true, d_custom_z_min, d_custom_z_max);
    new_s->d_mask_invalid = d_mask_invalid;
    new_s->d_log_color = d_log_color;
    new_s->d_cell_blocks = d_cell_blocks;
    if (d_mask_invalid || d_log_color || d_cell_blocks)
        new_s->rebuildData();
    new_s->setLevelsNumber(levels());
    new_s->color_map_policy = color_map_policy;
    return new_s;
}

QwtLinearColorMap Spectrogram::reversedColorMap(const QwtLinearColorMap &src)
{
    QwtLinearColorMap dst(src.color2(), src.color1());
    dst.setMode(src.mode());
    const QwtArray<double> stops = src.colorStops();
    const QwtDoubleInterval unit(0.0, 1.0);
    for (int i = 1; i < (int)stops.size() - 1; ++i) {
        const double p = stops[i];
        if (p > 0.0 && p < 1.0)
            dst.addColorStop(1.0 - p, QColor(src.rgb(unit, p)));
    }
    return dst;
}

QwtLinearColorMap Spectrogram::presetColorMap(ColorMapPreset preset, bool reverse)
{
    auto fromStops = [](const std::initializer_list<std::pair<double, QRgb>> &stops) {
        auto it = stops.begin();
        const QColor c0(it->second);
        auto last = stops.end();
        --last;
        const QColor c1(last->second);
        QwtLinearColorMap m(c0, c1);
        for (const auto &kv : stops) {
            if (kv.first > 0.0 && kv.first < 1.0)
                m.addColorStop(kv.first, QColor(kv.second));
        }
        return m;
    };

    QwtLinearColorMap m;
    switch (preset) {
    case PresetGrayScale:
        m = QwtLinearColorMap(Qt::black, Qt::white);
        break;
    case PresetViridis:
        m = fromStops({
                { 0.000, qRgb(68, 1, 84) },    { 0.125, qRgb(71, 44, 122) },
                { 0.250, qRgb(59, 82, 139) },  { 0.375, qRgb(44, 114, 142) },
                { 0.500, qRgb(33, 145, 140) }, { 0.625, qRgb(39, 173, 129) },
                { 0.750, qRgb(94, 201, 98) },  { 0.875, qRgb(170, 220, 50) },
                { 1.000, qRgb(253, 231, 37) },
        });
        break;
    case PresetMagma:
        m = fromStops({
                { 0.000, qRgb(0, 0, 4) },       { 0.125, qRgb(28, 16, 68) },
                { 0.250, qRgb(79, 18, 123) },   { 0.375, qRgb(129, 37, 129) },
                { 0.500, qRgb(181, 54, 122) },  { 0.625, qRgb(229, 80, 100) },
                { 0.750, qRgb(251, 135, 97) },  { 0.875, qRgb(254, 194, 135) },
                { 1.000, qRgb(252, 253, 191) },
        });
        break;
    case PresetInferno:
        m = fromStops({
                { 0.000, qRgb(0, 0, 4) },       { 0.125, qRgb(31, 12, 72) },
                { 0.250, qRgb(85, 15, 109) },   { 0.375, qRgb(136, 34, 106) },
                { 0.500, qRgb(186, 54, 85) },   { 0.625, qRgb(227, 89, 51) },
                { 0.750, qRgb(249, 140, 10) },  { 0.875, qRgb(249, 201, 50) },
                { 1.000, qRgb(252, 255, 164) },
        });
        break;
    case PresetPlasma:
        m = fromStops({
                { 0.000, qRgb(13, 8, 135) },    { 0.125, qRgb(75, 3, 161) },
                { 0.250, qRgb(125, 3, 168) },   { 0.375, qRgb(168, 34, 150) },
                { 0.500, qRgb(203, 70, 121) },  { 0.625, qRgb(229, 107, 93) },
                { 0.750, qRgb(248, 148, 65) },  { 0.875, qRgb(253, 195, 40) },
                { 1.000, qRgb(240, 249, 33) },
        });
        break;
    case PresetCividis:
        m = fromStops({
                { 0.000, qRgb(0, 32, 76) },     { 0.125, qRgb(0, 54, 111) },
                { 0.250, qRgb(42, 75, 110) },   { 0.375, qRgb(78, 96, 110) },
                { 0.500, qRgb(118, 118, 118) }, { 0.625, qRgb(155, 143, 112) },
                { 0.750, qRgb(193, 171, 98) },  { 0.875, qRgb(231, 203, 72) },
                { 1.000, qRgb(253, 234, 69) },
        });
        break;
    case PresetJet:
        m = fromStops({
                { 0.000, qRgb(0, 0, 131) },   { 0.125, qRgb(0, 60, 170) },
                { 0.375, qRgb(5, 255, 255) }, { 0.625, qRgb(255, 255, 0) },
                { 0.875, qRgb(250, 0, 0) },   { 1.000, qRgb(128, 0, 0) },
        });
        break;
    case PresetBlueWhiteRed:
        m = fromStops({
                { 0.00, qRgb(5, 48, 97) },     { 0.25, qRgb(67, 147, 195) },
                { 0.50, qRgb(247, 247, 247) }, { 0.75, qRgb(214, 96, 77) },
                { 1.00, qRgb(103, 0, 31) },
        });
        break;
    case PresetCoolWarm:
        m = fromStops({
                { 0.00, qRgb(59, 76, 192) },   { 0.25, qRgb(141, 176, 254) },
                { 0.50, qRgb(221, 220, 220) }, { 0.75, qRgb(244, 154, 123) },
                { 1.00, qRgb(180, 4, 38) },
        });
        break;
    case PresetLegacyDefault:
    case PresetCustom:
    default:
        m = defaultColorMap();
        break;
    }

    return reverse ? reversedColorMap(m) : m;
}

void Spectrogram::setColorMapPreset(ColorMapPreset preset, bool reverse)
{
    d_preset = preset;
    d_reverse_palette = reverse;

    if (preset == PresetCustom) {
        color_map = reverse ? reversedColorMap(d_base_custom_map) : d_base_custom_map;
        color_map_policy = Custom;
    } else if (preset == PresetGrayScale && !reverse) {
        color_map = presetColorMap(PresetGrayScale, false);
        color_map_policy = GrayScale;
    } else if (preset == PresetLegacyDefault && !reverse) {
        color_map = presetColorMap(PresetLegacyDefault, false);
        color_map_policy = Default;
    } else {
        color_map = presetColorMap(preset, reverse);
        color_map_policy = Custom;
    }

    setColorMap(color_map);
    syncColorScale();
}

void Spectrogram::setReversePalette(bool reverse)
{
    setColorMapPreset(d_preset, reverse);
}

void Spectrogram::setGrayScale()
{
    d_preset = PresetGrayScale;
    color_map = presetColorMap(PresetGrayScale, d_reverse_palette);
    setColorMap(color_map);
    color_map_policy = d_reverse_palette ? Custom : GrayScale;
    syncColorScale();
}

void Spectrogram::setDefaultColorMap()
{
    d_preset = PresetLegacyDefault;
    color_map = presetColorMap(PresetLegacyDefault, d_reverse_palette);
    setColorMap(color_map);
    color_map_policy = d_reverse_palette ? Custom : Default;
    syncColorScale();
}

void Spectrogram::setCustomColorMap(const QwtLinearColorMap &map)
{
    d_preset = PresetCustom;
    d_base_custom_map = map;
    color_map = d_reverse_palette ? reversedColorMap(map) : map;
    setColorMap(color_map);
    color_map_policy = Custom;
    syncColorScale();
}

QwtLinearColorMap Spectrogram::defaultColorMap()
{
    QwtLinearColorMap colorMap(Qt::blue, Qt::red);
    colorMap.addColorStop(0.25, Qt::cyan);
    colorMap.addColorStop(0.5, Qt::green);
    colorMap.addColorStop(0.75, Qt::yellow);
    return colorMap;
}

QString Spectrogram::saveToString()
{
    if (!d_matrix)
        return QString();

    QString s = "<spectrogram>\n";
    s += "\t<matrix>" + QString(d_matrix->name()) + "</matrix>\n";

    if (color_map_policy != Custom)
        s += "\t<ColorPolicy>" + QString::number(color_map_policy) + "</ColorPolicy>\n";
    else {
        const QwtLinearColorMap &mapToSave =
                (d_preset == PresetCustom) ? d_base_custom_map : color_map;
        s += "\t<ColorMap>\n";
        s += "\t\t<Mode>" + QString::number(mapToSave.mode()) + "</Mode>\n";
        s += "\t\t<MinColor>" + COLORNAME(mapToSave.color1()) + "</MinColor>\n";
        s += "\t\t<MaxColor>" + COLORNAME(mapToSave.color2()) + "</MaxColor>\n";
        QwtArray<double> colors = mapToSave.colorStops();
        int stops = (int)colors.size();
        s += "\t\t<ColorStops>" + QString::number(stops - 2) + "</ColorStops>\n";
        for (int i = 1; i < stops - 1; i++) {
            s += "\t\t<Stop>" + QString::number(colors[i]) + "\t";
            s += COLORNAME(QColor(mapToSave.rgb(QwtDoubleInterval(0, 1), colors[i])));
            s += "</Stop>\n";
        }
        s += "\t</ColorMap>\n";
    }
    s += "\t<Image>" + QString::number(testDisplayMode(QwtPlotSpectrogram::ImageMode))
            + "</Image>\n";

    bool contourLines = testDisplayMode(QwtPlotSpectrogram::ContourMode);
    s += "\t<ContourLines>" + QString::number(contourLines) + "</ContourLines>\n";
    if (contourLines) {
        s += "\t\t<Levels>" + QString::number(levels()) + "</Levels>\n";
        bool defaultPen = defaultContourPen().style() != Qt::NoPen;
        s += "\t\t<DefaultPen>" + QString::number(defaultPen) + "</DefaultPen>\n";
        if (defaultPen) {
            s += "\t\t\t<PenColor>" + COLORNAME(defaultContourPen().color()) + "</PenColor>\n";
            s += "\t\t\t<PenWidth>" + QString::number(defaultContourPen().width())
                    + "</PenWidth>\n";
            s += "\t\t\t<PenStyle>" + QString::number(defaultContourPen().style() - 1)
                    + "</PenStyle>\n";
        }
    }
    QwtScaleWidget *colorAxis = plot() ? plot()->axisWidget(color_axis) : nullptr;
    if (colorAxis && colorAxis->isColorBarEnabled()) {
        s += "\t<ColorBar>\n\t\t<axis>" + QString::number(color_axis) + "</axis>\n";
        s += "\t\t<width>" + QString::number(colorAxis->colorBarWidth()) + "</width>\n";
        s += "\t</ColorBar>\n";
    }
    s += "\t<ColorPreset>" + QString::number(static_cast<int>(d_preset)) + "</ColorPreset>\n";
    s += "\t<ReversePalette>" + QString::number(d_reverse_palette ? 1 : 0) + "</ReversePalette>\n";
    if (d_custom_z_range) {
        s += "\t<CustomZRange>1\t" + QString::number(d_custom_z_min, 'g', 15) + "\t"
                + QString::number(d_custom_z_max, 'g', 15) + "</CustomZRange>\n";
    }
    if (!d_colorbar_title.isEmpty()) {
        QString esc = d_colorbar_title;
        esc.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\n", "&#10;");
        s += "\t<ColorBarTitle>" + esc + "</ColorBarTitle>\n";
    }
    // E11b display options (older versions ignore these tags)
    if (d_mask_invalid)
        s += "\t<ToriMaskInvalid>1</ToriMaskInvalid>\n";
    if (d_log_color)
        s += "\t<ToriLogColor>1</ToriLogColor>\n";
    if (d_cell_blocks)
        s += "\t<ToriCellBlocks>1</ToriCellBlocks>\n";
    s += "\t<Visible>" + QString::number(isVisible()) + "</Visible>\n";
    return s + "</spectrogram>\n";
}

QPen Spectrogram::contourPen(double level) const
{
    const double sc = Plot::printDeviceScale();
    QPen defPen = defaultContourPen();
    if (defPen.style() != Qt::NoPen) {
        defPen.setCosmetic(false);
        const double baseW = (defPen.widthF() > 0.0) ? defPen.widthF() : 1.2;
        defPen.setWidthF(qMax(1.0, baseW) * sc);
        return defPen;
    }

    const QwtDoubleInterval zRange = effectiveValueRange();
    QColor isoColor(colorMap().rgb(zRange, level));
    if (!isoColor.isValid())
        isoColor = Qt::black;
    const bool overImage = testDisplayMode(QwtPlotSpectrogram::ImageMode);
    if (overImage)
        isoColor = isoColor.darker(145);
    QPen levelPen(isoColor, (overImage ? 1.15 : 1.55) * sc, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin);
    levelPen.setCosmetic(false);
    return levelPen;
}

void Spectrogram::drawContourLines(QPainter *painter, const QwtScaleMap &xMap,
                                   const QwtScaleMap &yMap,
                                   const QwtRasterData::ContourLines &contourLines) const
{
    if (!painter)
        return;

    const QwtValueList levelsList = contourLevels();
    if (levelsList.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    for (int l = 0; l < levelsList.size(); ++l) {
        const double level = levelsList[l];
        QPen pen = contourPen(level);
        if (pen.style() == Qt::NoPen)
            continue;
        pen.setCosmetic(false);
        painter->setPen(pen);

        const QPolygonF lines = contourLines.value(level);
        for (int i = 0; i + 1 < lines.size(); i += 2) {
            const QPointF p1(xMap.xTransform(lines[i].x()), yMap.xTransform(lines[i].y()));
            const QPointF p2(xMap.xTransform(lines[i + 1].x()), yMap.xTransform(lines[i + 1].y()));
            if (std::isfinite(p1.x()) && std::isfinite(p1.y())
                && std::isfinite(p2.x()) && std::isfinite(p2.y())) {
                painter->drawLine(p1, p2);
            }
        }
    }

    painter->restore();
}

// ============================================================================
// E11b display options: transparent missing cells, log colour scale, flat cell blocks
// ============================================================================

namespace {

//! Contour helper: missing (NaN) samples become a value outside the accepted range, so the
//! CONREC pass skips every cell that touches them (QwtRasterData::IgnoreOutOfRange).
class MaskedContourData : public QwtRasterData
{
public:
    explicit MaskedContourData(const QwtRasterData &src)
        : QwtRasterData(src.boundingRect()), d_src(src.copy())
    {
    }
    MaskedContourData(const MaskedContourData &o)
        : QwtRasterData(o.boundingRect()), d_src(o.d_src->copy())
    {
    }
    ~MaskedContourData() override { delete d_src; }
    QwtRasterData *copy() const override { return new MaskedContourData(*this); }
    QwtDoubleInterval range() const override { return QwtDoubleInterval(-1e299, 1e299); }
    void initRaster(const QwtDoubleRect &r, const QSize &s) override { d_src->initRaster(r, s); }
    void discardRaster() override { d_src->discardRaster(); }
    double value(double x, double y) const override
    {
        const double v = d_src->value(x, y);
        return std::isfinite(v) ? v : -1e300;
    }

private:
    MaskedContourData &operator=(const MaskedContourData &) = delete;
    QwtRasterData *d_src;
};

} // namespace

int Spectrogram::dataFlags() const
{
    int f = 0;
    if (d_mask_invalid)
        f |= MatrixData::FlagMaskInvalid;
    if (d_log_color)
        f |= MatrixData::FlagLog10;
    if (d_cell_blocks)
        f |= MatrixData::FlagCellBlocks;
    return f;
}

void Spectrogram::rebuildData()
{
    if (!d_matrix)
        return;
    const int prevLevels = qMax(2, levels());
    setData(MatrixData(d_matrix, d_custom_z_range, d_custom_z_min, d_custom_z_max, dataFlags()));
    setLevelsNumber(prevLevels);
    setConrecAttribute(QwtRasterData::IgnoreOutOfRange, d_mask_invalid || d_log_color);
    syncColorScale();
    itemChanged();
}

void Spectrogram::setMaskInvalidCells(bool on)
{
    if (d_mask_invalid == on)
        return;
    d_mask_invalid = on;
    rebuildData();
}

void Spectrogram::setLogColorScale(bool on)
{
    if (d_log_color == on)
        return;
    d_log_color = on;
    rebuildData();
}

void Spectrogram::setCellBlocks(bool on)
{
    if (d_cell_blocks == on)
        return;
    d_cell_blocks = on;
    rebuildData();
}

QwtDoubleInterval Spectrogram::colorAxisRange() const
{
    const QwtDoubleInterval r = effectiveValueRange();
    if (!d_log_color)
        return r;
    return QwtDoubleInterval(std::pow(10.0, r.minValue()), std::pow(10.0, r.maxValue()));
}

void Spectrogram::syncColorAxisEngine()
{
    QwtPlot *plot = this->plot();
    if (!plot || color_axis < 0 || color_axis >= QwtPlot::axisCnt)
        return;
    const bool isLog = Plot::isLog10ScaleEngine(plot->axisScaleEngine(color_axis));
    if (d_log_color && !isLog) {
        plot->setAxisScaleEngine(color_axis, new QwtLog10ScaleEngine());
        d_log_engine_applied = true;
    } else if (!d_log_color && isLog && d_log_engine_applied) {
        plot->setAxisScaleEngine(color_axis, new QwtLinearScaleEngine());
        d_log_engine_applied = false;
    } else if (d_log_color && isLog) {
        d_log_engine_applied = true;
    }
}

QImage Spectrogram::renderImage(const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                                const QwtDoubleRect &area) const
{
    if (!(d_mask_invalid || d_log_color) || colorMap().format() != QwtColorMap::RGB)
        return QwtPlotSpectrogram::renderImage(xMap, yMap, area);
    if (area.isEmpty())
        return QImage();

    // Same raster set-up as QwtPlotSpectrogram::renderImage, but NaN pixels stay transparent.
    QRect rect = transform(xMap, yMap, area);
    QwtScaleMap xxMap = xMap;
    QwtScaleMap yyMap = yMap;
    const QSize res = data().rasterHint(area);
    if (res.isValid()) {
        rect.setSize(rect.size().boundedTo(res));
        int px1 = rect.x();
        int px2 = rect.x() + rect.width();
        if (xMap.p1() > xMap.p2())
            qSwap(px1, px2);
        double sx1 = area.x();
        double sx2 = area.x() + area.width();
        if (xMap.s1() > xMap.s2())
            qSwap(sx1, sx2);
        int py1 = rect.y();
        int py2 = rect.y() + rect.height();
        if (yMap.p1() > yMap.p2())
            qSwap(py1, py2);
        double sy1 = area.y();
        double sy2 = area.y() + area.height();
        if (yMap.s1() > yMap.s2())
            qSwap(sy1, sy2);
        xxMap.setPaintInterval(px1, px2);
        xxMap.setScaleInterval(sx1, sx2);
        yyMap.setPaintInterval(py1, py2);
        yyMap.setScaleInterval(sy1, sy2);
    }
    if (rect.width() <= 0 || rect.height() <= 0)
        return QImage();

    QImage image(rect.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    const QwtDoubleInterval intensityRange = data().range();
    if (!intensityRange.isValid())
        return image;

    QwtRasterData &rd = const_cast<QwtRasterData &>(data());
    rd.initRaster(area, rect.size());
    const QwtColorMap &cm = colorMap();
    for (int y = rect.top(); y <= rect.bottom(); y++) {
        const double ty = yyMap.invTransform(y);
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y - rect.top()));
        for (int x = rect.left(); x <= rect.right(); x++) {
            const double tx = xxMap.invTransform(x);
            const double v = rd.value(tx, ty);
            *line++ = std::isfinite(v) ? (cm.rgb(intensityRange, v) | 0xFF000000u) : 0u;
        }
    }
    rd.discardRaster();
    return image;
}

QwtRasterData::ContourLines Spectrogram::renderContourLines(const QwtDoubleRect &rect,
                                                            const QSize &raster) const
{
    if (!(d_mask_invalid || d_log_color))
        return QwtPlotSpectrogram::renderContourLines(rect, raster);
    int flags = QwtRasterData::IgnoreOutOfRange;
    if (testConrecAttribute(QwtRasterData::IgnoreAllVerticesOnLevel))
        flags |= QwtRasterData::IgnoreAllVerticesOnLevel;
    MaskedContourData masked(data());
    return masked.contourLines(rect, raster, contourLevels(), flags);
}

double MatrixData::value(double x, double y) const
{
    if (d_data.isEmpty() || n_rows <= 0 || n_cols <= 0 || !std::isfinite(x) || !std::isfinite(y))
        return 0.0;

    double u = 0.0;
    if (n_cols > 1 && std::fabs(x_end - x_start) > 1e-300)
        u = ((x - x_start) / (x_end - x_start)) * double(n_cols - 1);
    u = qBound(0.0, u, double(n_cols - 1));

    double v = 0.0;
    if (n_rows > 1 && std::fabs(y_end - y_start) > 1e-300)
        v = ((y - y_start) / (y_end - y_start)) * double(n_rows - 1);
    v = qBound(0.0, v, double(n_rows - 1));

    // E11b: flat blocks (nearest cell) for binned data such as 2D histograms
    if (d_flags & FlagCellBlocks) {
        const int cn = qBound(0, int(std::floor(u + 0.5)), n_cols - 1);
        const int rn = qBound(0, int(std::floor(v + 0.5)), n_rows - 1);
        return d_data[rn * n_cols + cn];
    }

    const int c0 = qBound(0, int(std::floor(u)), n_cols - 1);
    const int c1 = qMin(c0 + 1, n_cols - 1);
    const double tx = u - double(c0);

    const int r0 = qBound(0, int(std::floor(v)), n_rows - 1);
    const int r1 = qMin(r0 + 1, n_rows - 1);
    const double ty = v - double(r0);

    const double z00 = d_data[r0 * n_cols + c0];
    const double z01 = d_data[r0 * n_cols + c1];
    const double z10 = d_data[r1 * n_cols + c0];
    const double z11 = d_data[r1 * n_cols + c1];

    if ((d_flags & (FlagMaskInvalid | FlagLog10))
        && !(std::isfinite(z00) && std::isfinite(z01) && std::isfinite(z10)
             && std::isfinite(z11))) {
        // next to a missing cell: the nearest cell decides (keeps the measured edge sharp)
        const int cn = (tx < 0.5) ? c0 : c1;
        const int rn = (ty < 0.5) ? r0 : r1;
        return d_data[rn * n_cols + cn];
    }

    const double top = z00 + tx * (z01 - z00);
    const double bot = z10 + tx * (z11 - z10);
    return top + ty * (bot - top);
}
