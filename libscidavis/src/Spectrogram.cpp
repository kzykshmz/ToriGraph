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
#include <math.h>
#include <qpen.h>
#include <qmessagebox.h>
#include <QFont>

#include <qwt_scale_widget.h>
#include <qwt_text.h>

Spectrogram::Spectrogram()
    : QwtPlotSpectrogram(),
      d_matrix(0),
      color_axis(QwtPlot::yRight),
      color_map_policy(Default),
      d_preset(PresetLegacyDefault),
      color_map(defaultColorMap()),
      d_base_custom_map(defaultColorMap())
{
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
    if (!m)
        return;
    setData(MatrixData(m));
    double step = fabs(data().range().maxValue() - data().range().minValue()) / 5.0;

    QwtValueList contourLevels;
    if (step > 0.0) {
        for (double level = data().range().minValue() + step; level < data().range().maxValue();
             level += step)
            contourLevels += level;
    }

    setContourLevels(contourLevels);
}

QwtDoubleInterval Spectrogram::rawDataRange() const
{
    if (const MatrixData *md = dynamic_cast<const MatrixData *>(&data()))
        return md->rawRange();
    if (d_matrix) {
        double lo = 0.0, hi = 1.0;
        d_matrix->range(&lo, &hi);
        if (std::isfinite(lo) && std::isfinite(hi) && lo <= hi)
            return QwtDoubleInterval(lo, hi);
    }
    return data().range();
}

QwtDoubleInterval Spectrogram::effectiveValueRange() const
{
    if (d_custom_z_range && std::isfinite(d_custom_z_min) && std::isfinite(d_custom_z_max)
        && d_custom_z_min < d_custom_z_max)
        return QwtDoubleInterval(d_custom_z_min, d_custom_z_max);
    return data().range();
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
    if (tickFont.family() != QLatin1String("Arial")) {
        tickFont.setFamily(QStringLiteral("Arial"));
        if (tickFont.pointSize() <= 0 && tickFont.pixelSize() <= 0)
            tickFont.setPointSize(10);
        plot->setAxisFont(color_axis, tickFont);
    }

    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (!colorAxis)
        return;

    QFont wf = colorAxis->font();
    if (wf.family() != QLatin1String("Arial")) {
        wf.setFamily(QStringLiteral("Arial"));
        colorAxis->setFont(wf);
    }

    QwtText t = colorAxis->title();
    if (!d_colorbar_title.isEmpty())
        t.setText(d_colorbar_title);
    QFont tf = t.font();
    tf.setFamily(QStringLiteral("Arial"));
    if (tf.pointSize() <= 0 && tf.pixelSize() <= 0)
        tf.setPointSize(11);
    t.setFont(tf);
    colorAxis->setTitle(t);
}

void Spectrogram::syncColorScale()
{
    QwtPlot *plot = this->plot();
    if (!plot || color_axis < 0 || color_axis >= QwtPlot::axisCnt)
        return;

    const QwtDoubleInterval zRange = effectiveValueRange();
    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (colorAxis)
        colorAxis->setColorMap(zRange, colorMap());

    if (colorAxis && colorAxis->isColorBarEnabled()) {
        plot->setAxisScale(color_axis, zRange.minValue(), zRange.maxValue());
        enforceColorBarArialFont();
    }
}

void Spectrogram::setColorBarTitle(const QString &title)
{
    d_colorbar_title = title;
    QwtPlot *plot = this->plot();
    if (!plot || color_axis < 0 || color_axis >= QwtPlot::axisCnt)
        return;
    QwtScaleWidget *colorAxis = plot->axisWidget(color_axis);
    if (!colorAxis)
        return;
    QwtText t = colorAxis->title();
    t.setText(title);
    QFont tf = t.font();
    tf.setFamily(QStringLiteral("Arial"));
    if (tf.pointSize() <= 0 && tf.pixelSize() <= 0)
        tf.setPointSize(11);
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
        setData(MatrixData(d_matrix, d_custom_z_range, d_custom_z_min, d_custom_z_max));
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
    setData(MatrixData(m, d_custom_z_range, d_custom_z_min, d_custom_z_max));
    setLevelsNumber(prevLevels);

    QwtPlot *plot = this->plot();
    if (!plot)
        return;

    syncColorScale();
    const QwtDoubleInterval zRange = effectiveValueRange();
    plot->setAxisScale(color_axis, zRange.minValue(), zRange.maxValue());
    enforceColorBarArialFont();
    plot->replot();
}

void Spectrogram::setLevelsNumber(int levels)
{
    if (levels < 2)
        levels = 2;
    const QwtDoubleInterval zRange = effectiveValueRange();
    double step = fabs(zRange.maxValue() - zRange.minValue()) / (double)levels;

    QwtValueList contourLevels;
    if (step > 0.0) {
        for (double level = zRange.minValue() + step; level < zRange.maxValue(); level += step)
            contourLevels += level;
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
    if (colorAxis)
        colorAxis->setColorBarEnabled(false);

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
    plot->setAxisScale(color_axis, zRange.minValue(), zRange.maxValue());
    colorAxis->setColorBarEnabled(on);
    colorAxis->setColorMap(zRange, colorMap());
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
    if (colorAxis)
        colorAxis->setColorBarWidth(width);
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
    s += "\t<Visible>" + QString::number(isVisible()) + "</Visible>\n";
    return s + "</spectrogram>\n";
}

double MatrixData::value(double x, double y) const
{
    int i = abs((int)floor((y - y_start) / dy - 1));
    int j = abs((int)floor((x - x_start) / dx));

    if (d_m && i < n_rows && j < n_cols)
        return d_m[i][j];
    else
        return 0.0;
}
