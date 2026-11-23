/***************************************************************************
    File                 : CanvasPicker.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Canvas picker

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
#ifndef CANVASPICKER_H
#define CANVASPICKER_H

#include <QObject>
#include <QPointer>
#include <QPoint>
#include "Plot.h"

class Graph;
class ArrowMarker;
class SelectionMoveResizer;
class QRubberBand;
class QTimer;

/**
 * \brief Handles parts of the user interaction for a Plot by registering itself as an event filter
 * for its QwtPlotCanvas.
 *
 * CanvasPicker relies heavily on its parent being the Graph that owns the Plot it operates on.
 * Additionally, parts of the code use Graph::plotWidget instead of CanvasPicker::plotWidget.
 */
class CanvasPicker : public QObject
{
    Q_OBJECT
public:
    CanvasPicker(Graph *plot);
    ~CanvasPicker() override;
    virtual bool eventFilter(QObject *, QEvent *);
    void selectPoints(int n);
    void selectPeak(const QPoint &p);

    //! Disable editing of #d_editing_marker on a TAB-key event.
    /*!\brief Called by Graph::focusNextPrevChild ()
     */
    void disableEditing();
    bool cancelActiveInteraction();
    static void hidePassiveHoverTooltip();

    //! Open the lightweight inline text editor for a legend / text annotation marker.
    /**
     * Shared entry point for double-click, Enter/Return and F2 so every path edits text the
     * same way. \return false when \a markerKey is not a visible text marker of \a g.
     */
    static bool openInlineTextEditorForMarker(Graph *g, long markerKey, int legendRow = -1);

    //! True for the keys that start editing the selected element (Enter, Return, F2).
    static bool isEditKey(int key)
    {
        return key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_F2;
    }

private:
    void drawTextMarker(const QPoint &);
    void drawLineMarker(const QPoint &, bool endArrow);
    void updatePassiveHoverReadout(const QPoint &canvasPos);
    void updateHoverCursor(const QPoint &canvasPos);
    void showPendingHoverTooltip();

    //! Selects and highlights the marker at the given position.
    /**
     * \return whether a marker was found at #pointSelected
     */
    bool selectMarker(const QMouseEvent *e);

    /**
     * \brief Return my parent as a Graph.
     *
     * %Note that contrary to the method name, this does NOT return the Plot I operate on.
     */
    Graph *plot() { return (Graph *)parent(); }

    /**
     * \brief The Plot I handle user interaction for.
     *
     * %Note that this has to be owned by my parent Graph.
     */
    Plot *plotWidget;
    QPoint startLinePoint, endLinePoint;

signals:
    void showPlotDialog(int);
    void viewTextDialog();
    void viewLineDialog();
    void viewImageDialog();
    void drawTextOff();
    void showMarkerPopupMenu();
    void modified();
    void selectPlot();

private:
    bool pointSelected;
    /*!\brief The marker that is currently being edited, or NULL.
     * Editing does explicitly _not_ inlude moving and resizing, which are being
     * handled by SelectionMoveResizer (see Graph::d_markers_selector).
     * Currently, only ArrowMarker provides any other form of editing, but this really
     * should be generalized. See ImageMarker for details.
     */
    ArrowMarker *d_editing_marker;
    QPointer<SelectionMoveResizer> d_forwarded_resizer;
    QPoint d_press_pos;
    bool d_left_click_pending;

    bool d_outer_legend_dragging = false;
    bool d_outer_legend_moved = false;
    long d_outer_legend_key = -1;
    QPoint d_outer_legend_press_pos;
    QPoint d_outer_legend_orig_origin;
    QString d_outer_legend_undo_before;

    QTimer *d_hover_dwell_timer = nullptr;
    QPoint d_hover_last_canvas_pos;
    int d_hover_pending_curve_key = -1;
    int d_hover_pending_point = -1;

    bool d_z_marquee_active = false;
    QPoint d_z_marquee_origin;
    QPointer<QRubberBand> d_z_marquee_band;

    bool d_ref_line_dragging = false;
    bool d_ref_line_moved = false;
    int d_ref_line_index = -1;
    double d_ref_line_orig_value = 0.0;
    QString d_ref_line_orig_stat_mode;
    QString d_ref_line_undo_before;
};

#endif // CANVASPICKER_H
