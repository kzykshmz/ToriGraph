/***************************************************************************
        File                 : Spectrogram.h
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

#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include "Matrix.h"
#include <QPointer>
#include <QVector>
#include <qwt_raster_data.h>
#include <qwt_plot.h>
#include <qwt_plot_spectrogram.h>
#include <qwt_color_map.h>
#include <cmath>
#include <limits>

class MatrixData;

class Spectrogram : public QwtPlotSpectrogram
{
public:
    Spectrogram();
    Spectrogram(Matrix *m);

    enum ColorMapPolicy { GrayScale, Default, Custom };
    enum ColorMapPreset {
        PresetLegacyDefault = 0,
        PresetGrayScale,
        PresetViridis,
        PresetMagma,
        PresetInferno,
        PresetPlasma,
        PresetCividis,
        PresetJet,
        PresetBlueWhiteRed,
        PresetCoolWarm,
        PresetCustom
    };

    Spectrogram *copy();
    Matrix *matrix() { return d_matrix.data(); };
    const Matrix *matrix() const { return d_matrix.data(); }
    QPen contourPen(double level) const override;

    int levels() { return (int)contourLevels().size() + 1; };
    void setLevelsNumber(int levels);

    bool hasColorScale();
    int colorScaleAxis() { return color_axis; };
    void showColorScale(int axis, bool on = true);

    int colorBarWidth();
    void setColorBarWidth(int width);

    void setColorBarTitle(const QString &title);
    QString colorBarTitle() const { return d_colorbar_title; }
    void enforceColorBarArialFont();
    void syncColorScale();

    void setGrayScale();
    void setDefaultColorMap();
    static QwtLinearColorMap defaultColorMap();
    static QwtLinearColorMap presetColorMap(ColorMapPreset preset, bool reverse = false);
    static QwtLinearColorMap reversedColorMap(const QwtLinearColorMap &src);

    void setColorMapPreset(ColorMapPreset preset, bool reverse = false);
    ColorMapPreset colorMapPreset() const { return d_preset; }
    void setReversePalette(bool reverse);
    bool reversePalette() const { return d_reverse_palette; }

    void setCustomColorMap(const QwtLinearColorMap &map);
    const QwtLinearColorMap &baseCustomColorMap() const { return d_base_custom_map; }
    void setCustomValueRange(bool enabled, double zMin, double zMax);
    bool hasCustomValueRange() const { return d_custom_z_range; }
    double customZMin() const { return d_custom_z_min; }
    double customZMax() const { return d_custom_z_max; }
    QwtDoubleInterval rawDataRange() const;
    QwtDoubleInterval effectiveValueRange() const;
    QwtDoubleInterval boundingInterval(Qt::Axis axis = Qt::ZAxis) const;

    void updateData(Matrix *m);

    //! Used when saving a project file
    QString saveToString();

    ColorMapPolicy colorMapPolicy() { return color_map_policy; };

protected:
    void drawContourLines(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                          const QwtRasterData::ContourLines &lines) const override;

    //! Guarded pointer to the source data matrix
    QPointer<Matrix> d_matrix;

    //! Axis used to display the color scale
    int color_axis;

    //! Flags
    ColorMapPolicy color_map_policy;
    ColorMapPreset d_preset = PresetLegacyDefault;
    bool d_reverse_palette = false;
    bool d_custom_z_range = false;
    double d_custom_z_min = 0.0;
    double d_custom_z_max = 1.0;
    QString d_colorbar_title;

    QwtLinearColorMap color_map;
    QwtLinearColorMap d_base_custom_map;
};

class MatrixData : public QwtRasterData
{
public:
    MatrixData(Matrix *m, bool useCustomRange = false, double customMin = 0.0,
               double customMax = 1.0)
        : QwtRasterData(m ? m->boundingRect() : QwtDoubleRect()),
          d_matrix(m),
          n_rows(m ? m->numRows() : 0),
          n_cols(m ? m->numCols() : 0),
          min_z(0.0),
          max_z(1.0),
          dx(1.0),
          dy(1.0),
          x_start(0.0),
          x_end(1.0),
          y_start(0.0),
          y_end(1.0),
          d_use_custom_range(useCustomRange),
          d_custom_min(customMin),
          d_custom_max(customMax)
    {
        if (!d_matrix || n_rows <= 0 || n_cols <= 0)
            return;

        d_data.resize(n_rows * n_cols);

        min_z = std::numeric_limits<double>::max();
        max_z = -std::numeric_limits<double>::max();
        bool anyFinite = false;
        for (int i = 0; i < n_rows; i++) {
            const int rowOff = i * n_cols;
            for (int j = 0; j < n_cols; j++) {
                double val = d_matrix->cell(i, j);
                if (std::isfinite(val)) {
                    anyFinite = true;
                } else {
                    double av = 0.0;
                    unsigned cnt = 0;
                    for (int ii = -1; ii <= 1; ++ii) {
                        for (int jj = -1; jj <= 1; ++jj) {
                            if ((ii == 0 && jj == 0) || i + ii < 0 || i + ii >= n_rows
                                || j + jj < 0 || j + jj >= n_cols)
                                continue;
                            const double nb = d_matrix->cell(i + ii, j + jj);
                            if (std::isfinite(nb)) {
                                av += nb;
                                cnt++;
                            }
                        }
                    }
                    val = (cnt > 0) ? (av / double(cnt)) : 0.0;
                }
                d_data[rowOff + j] = val;
                min_z = std::min(min_z, val);
                max_z = std::max(max_z, val);
            }
        }
        if (!anyFinite && min_z > max_z) {
            min_z = 0.0;
            max_z = 1.0;
        }

        x_start = d_matrix->xStart();
        x_end = d_matrix->xEnd();
        dx = (n_cols > 1) ? ((x_end - x_start) / double(n_cols - 1)) : (x_end - x_start);

        y_start = d_matrix->yStart();
        y_end = d_matrix->yEnd();
        dy = (n_rows > 1) ? ((y_end - y_start) / double(n_rows - 1)) : (y_end - y_start);
    }

    MatrixData(const MatrixData &) = default;
    MatrixData &operator=(const MatrixData &) = default;
    ~MatrixData() override = default;

    virtual QwtRasterData *copy() const override
    {
        return new MatrixData(*this);
    }

    virtual QwtDoubleInterval range() const override
    {
        if (d_use_custom_range && std::isfinite(d_custom_min) && std::isfinite(d_custom_max)
            && d_custom_min < d_custom_max)
            return QwtDoubleInterval(d_custom_min, d_custom_max);
        return QwtDoubleInterval(min_z, max_z);
    }

    QwtDoubleInterval rawRange() const { return QwtDoubleInterval(min_z, max_z); }

    virtual QSize rasterHint(const QwtDoubleRect &) const override
    {
        return QSize(qBound(1, n_cols * 4, 1024), qBound(1, n_rows * 4, 1024));
    }

    virtual double value(double x, double y) const override;

private:
    //! Guarded pointer to the source data matrix
    QPointer<Matrix> d_matrix;

    //! Implicitly-shared (COW) contiguous row-major cell buffer
    QVector<double> d_data;

    //! Data size
    int n_rows, n_cols;

    //! Min and max values in the source data matrix
    double min_z, max_z;

    //! Data resolution in x(columns) and y(rows)
    double dx, dy;

    //! Matrix coordinate bounds
    double x_start, x_end;
    double y_start, y_end;

    bool d_use_custom_range = false;
    double d_custom_min = 0.0;
    double d_custom_max = 1.0;
};

#endif
