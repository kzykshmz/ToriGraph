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

#include <QRectF>
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

    using QwtPlotCurve::draw;
    void draw(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override;

    QwtDoubleRect boundingRect() const override;

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
    QList<QVector<double>> convertData(const QList<Column *> &cols, const QList<int> &axes) const;

    //! Returns the row index in the data source table corresponding to the data point index.
    int tableRow(int point) const;

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
    bool validCurveType();
};
#endif
