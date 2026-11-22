/***************************************************************************
    File                 : Plot.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Plot window class

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
#ifndef PLOT_H
#define PLOT_H

#include <QObject>
#include <QMap>
#include <QPoint>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_marker.h>

class Grid;
class QTimer;

//! Plot window class
class Plot : public QwtPlot
{
    Q_OBJECT

public:
    Plot(QWidget *parent = 0, QString name = QString());

    enum LabelFormat { Automatic, Decimal, Scientific, Superscripts };

    Grid *grid() { return (Grid *)d_grid; };
    QList<int> curveKeys() { return d_curves.keys(); };
    QList<QwtPlotItem *> curvesList() { return d_curves.values(); };

    int insertCurve(QwtPlotItem *c);
    void removeCurve(int index);

    int closestCurve(int xpos, int ypos, int &dist, int &point);
    int closestCurveInteractive(const QPoint &pos, int &distOut, int &nearestVertexOut) const;
    int closestDataPoint(const QPoint &pos, int &distOut, int &vertexOut) const;
    QwtPlotCurve *curve(int index);
    QwtPlotItem *plotItem(int index) { return d_curves.value(index); };
    QMap<int, QwtPlotItem *> curves() { return d_curves; };

    QwtPlotMarker *marker(int index) { return d_markers.value(index); };
    QList<int> markerKeys() { return d_markers.keys(); };
    int insertMarker(QwtPlotMarker *m);
    void removeMarker(int index);

    QList<int> getMajorTicksType();
    void setMajorTicksType(int axis, int type);

    QList<int> getMinorTicksType();
    void setMinorTicksType(int axis, int type);

    int minorTickLength() const;
    int majorTickLength() const;
    void setTickLength(int minLength, int majLength);

    int axesLinewidth() const;
    void setAxesLinewidth(int width);

    void setAxisLabelFormat(int axis, char f, int prec);
    void axisLabelFormat(int axis, char &f, int &prec) const;

    int axisLabelFormat(int axis);
    int axisLabelPrecision(int axis);

    void printFrame(QPainter *painter, const QRect &rect) const;

    QColor frameColor();
    const QColor &paletteBackgroundColor() const;

    void print(QPainter *, const QRect &rect,
               const QwtPlotPrintFilter & = QwtPlotPrintFilter()) const override;
    void updateLayout() override;
    void printTitle(QPainter *painter, const QRect &rect) const override;
    void printScale(QPainter *painter, int axisId, int startDist, int endDist, int baseDist,
                    const QRect &rect) const override;
    void replot() override;

    bool eventFilter(QObject *object, QEvent *e) override;
    void zoomAxisAroundPos(int axis, int pixelPos, double factor);
    void panAxisByPixels(int axis, int startPixel, int endPixel);
    void syncMirrorAxisIfNeeded(int changedAxis);
    bool isMirrorOppositeAxis(int primaryAxis, int secondaryAxis) const;
    bool hasVisibleItemOnAxis(int axis) const;
    bool hasAnyItemOnAxis(int axis) const;
    void setHighlightedCurveKey(int key);
    int highlightedCurveKey() const { return d_highlighted_curve_key; }
    void setHoverPoint(int curveKey, int pointIndex);
    void clearHoverPoint();
    int hoverCurveKey() const { return d_hover_curve_key; }
    int hoverPointIndex() const { return d_hover_point_index; }
    bool isZKeyPressed() const { return d_z_pressed; }
    bool isSpaceKeyPressed() const { return d_space_pressed; }
    void flushPendingWheelUndo();
    static int scaleAlignmentToAxis(int alignment);
    static bool isLog10ScaleEngine(const QwtScaleEngine *se);
    static double printDeviceScale();

    // Display-only font zoom used by the page zoom (Ctrl+wheel). Fonts handed to the setters
    // below are the stored fonts (the pt values that are saved, exported and shown in dialogs);
    // the Qwt widgets draw them multiplied by the zoom. The getters give the stored fonts back
    // unchanged, so the zoom never leaks into project files, exports or the inspector, and
    // zooming back to 100 % restores the exact original pt (no rounding drift).
    void setDisplayFontScale(double scale);
    double displayFontScale() const { return d_display_font_scale; }
    //! Stored font -> the font to draw on screen at the current display zoom.
    QFont displayFont(const QFont &stored) const;

    // These hide the QwtPlot members of the same name (which are not virtual); every ToriGraph
    // caller goes through Plot *, Qwt itself keeps seeing the zoomed widget fonts.
    void setAxisFont(int axisId, const QFont &f);
    QFont axisFont(int axisId) const;
    void setAxisTitle(int axisId, const QString &t) { QwtPlot::setAxisTitle(axisId, t); }
    void setAxisTitle(int axisId, const QwtText &t);
    QwtText axisTitle(int axisId) const;
    void setTitle(const QString &t) { QwtPlot::setTitle(t); }
    void setTitle(const QwtText &t);
    QwtText title() const;

protected:
    void drawItems(QPainter *painter, const QRect &rect, const QwtScaleMap map[axisCnt],
                   const QwtPlotPrintFilter &pfilter) const override;

    void drawInwardTicks(QPainter *painter, const QRect &rect, const QwtScaleMap &map, int axis,
                         bool min, bool maj) const;
    Grid *d_grid;
    QMap<int, QwtPlotItem *> d_curves;
    QMap<int, QwtPlotMarker *> d_markers;

    int minTickLength, majTickLength;
    int marker_key;
    int curve_key;
    bool d_is_panning;
    bool d_space_pressed;
    bool d_z_pressed;
    bool d_x_pressed;
    bool d_y_pressed;
    int d_highlighted_curve_key;
    int d_hover_curve_key;
    int d_hover_point_index;
    QTimer *d_wheel_undo_timer;
    QString d_pending_wheel_undo_snap;
    QPoint d_pan_last_pos;

private:
    struct ZoomFontSlot
    {
        bool active = false; // the widget currently shows a zoomed copy of the stored font
        bool pixel = false; // the stored font is sized in pixels, not points
        double storedSize = 0.0;
        double shownSize = 0.0;
    };
    QFont zoomFont(ZoomFontSlot &slot, const QFont &stored);
    QFont unzoomFont(const ZoomFontSlot &slot, const QFont &shown) const;

    double d_display_font_scale = 1.0;
    ZoomFontSlot d_zoom_axis_font[axisCnt];
    ZoomFontSlot d_zoom_axis_title_font[axisCnt];
    ZoomFontSlot d_zoom_title_font;
};
#endif
