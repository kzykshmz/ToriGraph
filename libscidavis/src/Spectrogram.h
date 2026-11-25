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

    // ---- E11b display options (2D density / pole figure) -------------------------------
    //! Non-finite cells are drawn transparent and skipped by the contour lines (instead of
    //! being filled from their neighbours).
    void setMaskInvalidCells(bool on);
    bool maskInvalidCells() const { return d_mask_invalid; }
    //! Logarithmic colour scale: colours follow log10(z); cells with z <= 0 are transparent and
    //! the colour bar axis gets a log10 scale engine (ticks 1, 10, 100 ...).
    void setLogColorScale(bool on);
    bool logColorScale() const { return d_log_color; }
    //! Draw each matrix cell as a flat block (no bilinear smoothing); the image then covers the
    //! full cell extent (half a cell beyond the first / last cell centres), like histogram bins.
    void setCellBlocks(bool on);
    bool cellBlocks() const { return d_cell_blocks; }
    //! Range to put on the colour bar axis (data units: 10^range in log mode).
    QwtDoubleInterval colorAxisRange() const;

    //! Used when saving a project file
    QString saveToString();

    ColorMapPolicy colorMapPolicy() { return color_map_policy; };

protected:
    void drawContourLines(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                          const QwtRasterData::ContourLines &lines) const override;
    QImage renderImage(const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                       const QwtDoubleRect &area) const override;
    QwtRasterData::ContourLines renderContourLines(const QwtDoubleRect &rect,
                                                   const QSize &raster) const override;
    int dataFlags() const;
    //! True when missing cells must stay transparent (mask / log options, or a block of
    //! missing cells too large to fill from finite neighbours).
    bool drawsMissingCellsTransparent() const;
    void rebuildData();
    void syncColorAxisEngine();
    //! Remember the matrix coordinate range the X/Y axes were last fitted to
    void rememberMatrixExtent();
    //! After the matrix coordinates changed (transpose, rotation, new coordinates ...), put the
    //! new range on the X/Y axes, except on an axis the user fixed at another range
    void followMatrixExtent();

    //! Matrix coordinate range (x min, x max, y min, y max) last remembered
    double d_matrix_extent[4] = { 0.0, 0.0, 0.0, 0.0 };
    bool d_matrix_extent_valid = false;

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
    bool d_mask_invalid = false;
    bool d_log_color = false;
    bool d_cell_blocks = false;
    bool d_log_engine_applied = false;

    QwtLinearColorMap color_map;
    QwtLinearColorMap d_base_custom_map;
};

class MatrixData : public QwtRasterData
{
public:
    //! E11b data flags (see Spectrogram::setMaskInvalidCells / setLogColorScale / setCellBlocks)
    enum DataFlag { FlagMaskInvalid = 1, FlagLog10 = 2, FlagCellBlocks = 4 };

    MatrixData(Matrix *m, bool useCustomRange = false, double customMin = 0.0,
               double customMax = 1.0, int flags = 0)
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
          d_custom_max(customMax),
          d_flags(flags)
    {
        if (!d_matrix || n_rows <= 0 || n_cols <= 0)
            return;

        d_data.resize(n_rows * n_cols);

        min_z = std::numeric_limits<double>::max();
        max_z = -std::numeric_limits<double>::max();
        bool anyFinite = false;
        const bool masked = (d_flags & (FlagMaskInvalid | FlagLog10)) != 0;
        const bool logScale = (d_flags & FlagLog10) != 0;
        for (int i = 0; i < n_rows; i++) {
            const int rowOff = i * n_cols;
            for (int j = 0; j < n_cols; j++) {
                double val = d_matrix->cell(i, j);
                if (masked) {
                    // E11b: keep missing cells as NaN (drawn transparent, no contour)
                    if (logScale)
                        val = (std::isfinite(val) && val > 0.0)
                                ? std::log10(val)
                                : std::numeric_limits<double>::quiet_NaN();
                    if (!std::isfinite(val))
                        val = std::numeric_limits<double>::quiet_NaN();
                    d_data[rowOff + j] = val;
                    if (std::isfinite(val)) {
                        anyFinite = true;
                        min_z = std::min(min_z, val);
                        max_z = std::max(max_z, val);
                    }
                    continue;
                }
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
                    if (cnt == 0) {
                        // no finite neighbour (a block of 3x3 or more missing cells): keep it
                        // missing (transparent, no contour) instead of inventing a 0 that
                        // would also stretch the colour range
                        d_data[rowOff + j] = std::numeric_limits<double>::quiet_NaN();
                        d_has_unfilled = true;
                        continue;
                    }
                    val = av / double(cnt);
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

        if (d_flags & FlagCellBlocks) {
            // E11b: blocks cover the whole cell, half a cell beyond the outer cell centres
            const double hx = 0.5 * std::fabs(dx);
            const double hy = 0.5 * std::fabs(dy);
            const QwtDoubleRect br = QwtDoubleRect(x_start, y_start, x_end - x_start,
                                                   y_end - y_start)
                                             .normalized();
            setBoundingRect(QwtDoubleRect(br.left() - hx, br.top() - hy, br.width() + 2.0 * hx,
                                          br.height() + 2.0 * hy));
        }
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
            && d_custom_min < d_custom_max) {
            if (d_flags & FlagLog10) {
                // custom range is given in data units; non-positive ends fall back to the data
                const double lo = d_custom_min > 0.0 ? std::log10(d_custom_min) : min_z;
                const double hi = d_custom_max > 0.0 ? std::log10(d_custom_max) : max_z;
                if (lo < hi)
                    return QwtDoubleInterval(lo, hi);
                return QwtDoubleInterval(min_z, max_z);
            }
            return QwtDoubleInterval(d_custom_min, d_custom_max);
        }
        return QwtDoubleInterval(min_z, max_z);
    }

    int flags() const { return d_flags; }

    //! Some missing cells had no finite neighbour and stay NaN in the default (unmasked) mode.
    bool hasUnfilledCells() const { return d_has_unfilled; }

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
    int d_flags = 0;
    bool d_has_unfilled = false;
};

#endif
