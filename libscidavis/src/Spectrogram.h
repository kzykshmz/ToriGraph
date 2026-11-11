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
    Matrix *matrix() { return d_matrix; };
    const Matrix *matrix() const { return d_matrix; }

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
    //! Pointer to the source data matrix
    Matrix *d_matrix;

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
          d_m(nullptr),
          n_rows(m ? m->numRows() : 0),
          n_cols(m ? m->numCols() : 0),
          min_z(0.0),
          max_z(1.0),
          dx(1.0),
          dy(1.0),
          x_start(0.0),
          y_start(0.0),
          d_use_custom_range(useCustomRange),
          d_custom_min(customMin),
          d_custom_max(customMax)
    {
        if (!d_matrix || n_rows <= 0 || n_cols <= 0)
            return;

        d_m = new double *[n_rows];
        for (int l = 0; l < n_rows; ++l)
            d_m[l] = new double[n_cols];

        min_z = std::numeric_limits<double>::max();
        max_z = -std::numeric_limits<double>::max();
        bool anyFinite = false;
        for (int i = 0; i < n_rows; i++)
            for (int j = 0; j < n_cols; j++) {
                // replace NaNs and Infs with average of neighbours
                if (std::isfinite(d_matrix->cell(i, j))) {
                    d_m[i][j] = d_matrix->cell(i, j);
                    anyFinite = true;
                } else {
                    double av = 0;
                    unsigned cnt = 0;
                    for (int ii = -1; ii <= 1; ii += 2)
                        for (int jj = -1; jj <= 1; jj += 2)
                            if (i + ii >= 0 && i + ii < n_rows && j + jj >= 0 && j + jj < n_cols
                                && std::isfinite(d_matrix->cell(i + ii, j + jj))) {
                                av += d_matrix->cell(i + ii, j + jj);
                                cnt++;
                            }
                    if (cnt > 0)
                        av /= cnt;
                    d_m[i][j] = av;
                }
                min_z = std::min(min_z, d_m[i][j]);
                max_z = std::max(max_z, d_m[i][j]);
            }
        if (!anyFinite && min_z > max_z) {
            min_z = 0.0;
            max_z = 1.0;
        }

        x_start = d_matrix->xStart();
        dx = (d_matrix->xEnd() - x_start) / (double)n_cols;

        y_start = d_matrix->yStart();
        dy = (d_matrix->yEnd() - y_start) / (double)n_rows;
    }

    ~MatrixData()
    {
        if (d_m) {
            for (int i = 0; i < n_rows; i++)
                delete[] d_m[i];
            delete[] d_m;
        }
    };

    virtual QwtRasterData *copy() const
    {
        return new MatrixData(d_matrix, d_use_custom_range, d_custom_min, d_custom_max);
    }

    virtual QwtDoubleInterval range() const
    {
        if (d_use_custom_range && std::isfinite(d_custom_min) && std::isfinite(d_custom_max)
            && d_custom_min < d_custom_max)
            return QwtDoubleInterval(d_custom_min, d_custom_max);
        return QwtDoubleInterval(min_z, max_z);
    }

    QwtDoubleInterval rawRange() const { return QwtDoubleInterval(min_z, max_z); }

    virtual QSize rasterHint(const QwtDoubleRect &) const { return QSize(n_cols, n_rows); }

    virtual double value(double x, double y) const;

private:
    //! Pointer to the source data matrix
    Matrix *d_matrix;

    //! Vector used to store in memory the data from the source matrix window
    double **d_m;

    //! Data size
    int n_rows, n_cols;

    //! Min and max values in the source data matrix
    double min_z, max_z;

    //! Data resolution in x(columns) and y(rows)
    double dx, dy;

    //! X axis left value in the data matrix
    double x_start;

    //! Y axis bottom value in the data matrix
    double y_start;

    bool d_use_custom_range = false;
    double d_custom_min = 0.0;
    double d_custom_max = 1.0;
};

#endif
