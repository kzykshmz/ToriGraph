/***************************************************************************
    File                 : PlotCurve.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : AbstractPlotCurve and DataCurve classes

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
#ifndef PLOTCURVE_H
#define PLOTCURVE_H

#include <QLineF>
#include <QPolygonF>
#include <QRectF>
#include <QVector>
#include <qwt_plot_curve.h>
#include "Table.h"

//! Abstract 2D plot curve class
class PlotCurve : public QwtPlotCurve
{

public:
    PlotCurve(const QString &name = {})
        : QwtPlotCurve(name),
          d_type(0),
          d_stack_dx(0.0),
          d_stack_dy(0.0),
          d_applied_dx(0.0),
          d_applied_dy(0.0),
          d_norm01(false),
          d_symbol_skip(0) {};

    int type() const { return d_type; };
    void setType(int t) { d_type = t; };

    void setSymbolSkip(int everyN);
    int symbolSkip() const;

    void setStackOffset(double dx, double dy);
    double stackOffsetX() const { return d_stack_dx; }
    double stackOffsetY() const { return d_stack_dy; }
    void setStackNormalize01(bool normalize);
    bool stackNormalize01() const { return d_norm01; }
    void applyStackTransformToCurrentData();
    //! Takes over  src's stack offset / 0-1 normalisation state (including the raw Y values the
    //! normalisation works from) as it is. For a curve that already holds  src's displayed
    //! points (Graph::copy): applying the offset / normalisation again would transform them twice.
    void copyStackTransform(const PlotCurve *src);
    //! While 0-1 normalisation is active: minimum and span of the raw (untransformed) Y values
    //! the normalisation maps to 0..1. Returns false when normalisation is off or its source
    //! values are not available.
    bool stackNormalizeRange(double *minY, double *span) const;
    //! Maps a point in plot (displayed) coordinates back to the curve's own data coordinates,
    //! undoing the stack offset and, for a normalised curve, the 0-1 scaling. Returns false when
    //! the scaling cannot be undone (no raw range, or all raw Y values equal).
    bool displayToRaw(double dispX, double dispY, double *rawX, double *rawY) const;

    using QwtPlotCurve::draw;
    void draw(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override;

    QwtDoubleRect boundingRect() const override;

    //! Axis-break geometry for this curve's axes: data-space break interval plus the
    //! device-pixel gap band [xLo, xHi] / [yLo, yHi] that drawing must not bridge.
    struct AxisBreakBands {
        bool hasX = false;
        bool hasY = false;
        double xBf = 0.0, xBt = 0.0, xEps = 0.0, xBrkPx = 0.0, xLo = 0.0, xHi = 0.0;
        double yBf = 0.0, yBt = 0.0, yEps = 0.0, yBrkPx = 0.0, yLo = 0.0, yHi = 0.0;
        bool any() const { return hasX || hasY; }
        bool xValueInGap(double vx) const { return hasX && vx > xBf + xEps && vx < xBt - xEps; }
        bool yValueInGap(double vy) const { return hasY && vy > yBf + yEps && vy < yBt - yEps; }
        bool valueInGap(double vx, double vy) const { return xValueInGap(vx) || yValueInGap(vy); }
        bool xPixelInGap(double px) const { return hasX && px > xLo && px < xHi; }
        bool yPixelInGap(double py) const { return hasY && py > yLo && py < yHi; }
        bool pixelInGap(const QPointF &p) const { return xPixelInGap(p.x()) || yPixelInGap(p.y()); }
        bool xValuesStraddle(double a, double b) const
        {
            return hasX && ((a <= xBf + xEps && b >= xBt - xEps)
                            || (a >= xBt - xEps && b <= xBf + xEps));
        }
        bool yValuesStraddle(double a, double b) const
        {
            return hasY && ((a <= yBf + yEps && b >= yBt - yEps)
                            || (a >= yBt - yEps && b <= yBf + yEps));
        }
    };
    //! Half width (device px) of the visual break gap around the collapsed break position.
    static constexpr double kAxisBreakHalfGapPx = 4.5;

    AxisBreakBands axisBreakBands(const QwtScaleMap &xMap, const QwtScaleMap &yMap) const;
    //! Appends the parts of \a seg lying outside the break gap bands to \a out.
    static void appendSegmentOutsideBreaks(const QLineF &seg, const AxisBreakBands &b,
                                           QVector<QLineF> &out);
    //! Splits an open polyline into sub-polylines that do not cross the break gap bands.
    static QVector<QPolygonF> splitPolylineAtBreaks(const QPolygonF &poly, const AxisBreakBands &b);
    //! Clips a closed (fill) polygon so no piece lies inside / bridges the break gap bands.
    static QVector<QPolygonF> clipPolygonToBreaks(const QPolygonF &closedPoly,
                                                  const AxisBreakBands &b);

    //! Pen/size scale factor for device + page-zoom rendering, identical to the one used for
    //! data lines: Plot::printDeviceScale() x (on screen only) Plot::displayFontScale().
    double renderPenScale() const;

protected:
    void drawCurve(QPainter *p, int style, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                   int from, int to) const override;
    void drawSymbols(QPainter *p, const QwtSymbol &symbol, const QwtScaleMap &xMap,
                     const QwtScaleMap &yMap, int from, int to) const override;

    int d_type;
    double d_stack_dx;
    double d_stack_dy;
    double d_applied_dx;
    double d_applied_dy;
    bool d_norm01;
    int d_symbol_skip;
    QVector<double> d_raw_y_backup;
    mutable QRectF d_canvas_rect;
};

class DataCurve : public PlotCurve
{

public:
    DataCurve(Table *t, const QString &xColName, const QString &name, int startRow = 0,
              int endRow = -1);

    QString xColumnName() const { return d_x_column; };
    void setXColumnName(const QString &name) { d_x_column = name; };

    QString yColumnName() const { return title().text(); }
    void setYColumnName(const QString &name) { setTitle(name); }

    Table *table() const { return d_table; };

    int startRow() const { return d_start_row; };
    int endRow() const { return d_end_row; };
    void setRowRange(int startRow, int endRow);

    bool isFullRange();
    void setFullRange();

    virtual bool updateData(Table *t, const QString &colName);
    virtual bool loadData();

    //! True when \a col (this curve's X column if \a isXColumn, else its Y column) is a Text /
    //! DateTime column on an axis that does not show text / time labels (HorizontalBars swap
    //! the axes), i.e. plotting it would turn the axis into a text / time axis.
    bool columnShownOffAxis(const Column *col, bool isXColumn) const;
    //! Keeps the curve in its layer with no points (Graph::updateCurvesData(), when a plotted
    //! column became Text / DateTime under an axis that does not show it). Deleting the curve
    //! there was not recorded for Undo, so undoing the column type change did not bring it
    //! back. DataCurve::loadData() keeps a held curve empty until its columns fit its axes again.
    void holdEmpty();
    bool isHeldEmpty() const { return d_held_empty; }
    void releaseHeldEmpty() { d_held_empty = false; }

    QList<QVector<double>> convertData(const QList<Column *> &cols, const QList<int> &axes) const;

    //! Returns the row index in the data source table corresponding to the data point index,
    //! or -1 when the point does not correspond to a single table row.
    virtual int tableRow(int point) const;
    //! Point index -> table row mapping built by the last data load.
    const QVector<int> &rowMapping() const { return d_index_to_row; }
    //! Sets the point index -> table row mapping (for curves whose data are not loaded through
    //! convertData(), e.g. copies or vector curves).
    void setRowMapping(const QVector<int> &rows,
                       const QVector<int> &gapBefore = QVector<int>())
    {
        d_index_to_row = rows;
        d_gap_before_indices = gapBefore;
    }

    bool isPointMasked(int point) const;
    bool hasAnyMaskedPoints() const;
    void resolveColumns(Column *&xColOut, Column *&yColOut) const;
    bool isPointMaskedFast(int point, const Column *xCol, const Column *yCol) const;
    bool hasGapBefore(int point) const;
    const QVector<int> &gapBeforeIndices() const { return d_gap_before_indices; }
    void maskPoint(int point, bool mask = true);
    int maskRegion(const QRectF &dataRect, bool mask = true);
    void clearMasks();
    int maskedPointCount() const;
    using PlotCurve::draw;
    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap, int from,
              int to) const override;

    void remove();

    /**
     * \brief A list of data sources for this curve.
     *
     * Elements must be in either of the following forms:
     *  - &lt;id of X column> "(X)," &lt;id of Y column> "(Y)" [ "," &lt;id of error column>
     * ("(xErr)" | "(yErr)") ]
     *  - &lt;id of Xstart column> "(X)," &lt;id of Ystart column>"(Y)," &lt;id of Xend column>
     * "(X)," &lt;id of Yend column> "(Y)"\n (denoting start and end coordinates for the #VectXYXY
     * style)
     *  - &lt;id of Xstart column> "(X)," &lt;id of Ystart column> "(Y)," &lt;id of angle column>
     * "(A)," &lt;id of magnitude column> "(M)"\n (denoting start coordinates, angle in radians and
     * length for the #VectXYAM style)
     *
     * Column ids are of the form '&lt;name of table> "_" &lt;name of column>'.
     */
    virtual QString plotAssociation();
    virtual void updateColumnNames(const QString &oldName, const QString &newName,
                                   bool updateTableName);

    //! The list of attached error bars.
    QList<DataCurve *> errorBarsList() { return d_error_bars; };
    //! Adds a single error bars curve to the list of attached error bars.
    void addErrorBars(DataCurve *c)
    {
        if (c)
            d_error_bars << c;
    };
    //! Remove a single error bars curve from the list of attached error bars.
    void removeErrorBars(DataCurve *c);
    //! Clears the list of attached error bars.
    void clearErrorBars();

    void setVisible(bool on);

    //! 利用者が付けた凡例ラベル（空なら表の列から自動で作る）。曲線のタイトルは Y 列の
    //! 結び付き（yColumnName()）なので、ラベルはそれとは別にここで持つ。曲線と一緒に
    //! 動くので、曲線の削除・並べ替えで番号を付け直す必要はない。
    QString customLabel() const { return d_custom_label; }
    void setCustomLabel(const QString &label) { d_custom_label = label; }

    bool hasSelectedLabels();
    void setLabelsSelected(bool on = true);

    //! E11: symbol colour (and optionally size) taken from further table columns.
    struct PointColorMap {
        bool enabled = false;
        QString zColumn;     //!< full column name ("Table1_C") giving the colour value
        QString sizeColumn;  //!< optional full column name giving the symbol size
        int preset = 2;      //!< Spectrogram::ColorMapPreset (2 = Viridis)
        bool reverse = false;
        bool autoRange = true;
        double zMin = 0.0;
        double zMax = 1.0;
        double sizeMinPx = 4.0;
        double sizeMaxPx = 16.0;
        QString colorBarTitle;
        bool showColorBar = true;
    };
    void setPointColorMap(const PointColorMap &m) { d_point_cmap = m; }
    const PointColorMap &pointColorMap() const { return d_point_cmap; }
    bool hasPointColorMap() const { return d_point_cmap.enabled && !d_point_cmap.zColumn.isEmpty(); }
    //! Colour range actually used: the fixed range, or the min/max of the plotted Z values.
    bool pointColorRange(double *zMin, double *zMax) const;
    //! Z value (colour column) of plotted point \a i, NaN when missing.
    double pointColorValue(int i) const;
    //! Symbol colour of plotted point \a i (invalid QColor when the Z value is missing).
    QColor pointColor(int i) const;

protected:
    void drawSymbols(QPainter *p, const QwtSymbol &symbol, const QwtScaleMap &xMap,
                     const QwtScaleMap &yMap, int from, int to) const override;
    PointColorMap d_point_cmap;

protected:
    //! The data source table.
    Table *d_table;
    //! List of the error bar curves associated to this curve.
    QList<DataCurve *> d_error_bars;
    //!\brief The name of the column used for abscissae values.
    /*
     *The column name used for Y values is stored in title().text().
     */
    QString d_x_column;
    //! First row of #d_tabel to be plotted, provided all relevant columns have valid entries.
    int d_start_row;
    //! Last row of #d_tabel to be plotted, provided all relevant columns have valid entries.
    int d_end_row;

private:
    /**
     * \brief Mapping of curve indices to row indices in #d_table.
     *
     * Besides #d_start_row and #d_end_row, this takes into account rows which had invalid entries
     * in one of the relevant columns the last time convertData() was called. Usually, changes in
     * validity should cause the curve to be updated via updateData(); however, subclasses of
     * DataCurve may depend on more columns than X and Y (in particular, this is true of vector
     * curves); so DataCurve cannot (within the current architecture) compute this mapping on demand
     * and has to store it like this.
     */
    mutable QVector<int> d_index_to_row;
    mutable QVector<int> d_gap_before_indices;
    QString d_custom_label;
    bool d_held_empty = false;
    //! Empty data, row mapping and error bars (see loadData()).
    void setEmptyData();
    bool validCurveType();
};
#endif
