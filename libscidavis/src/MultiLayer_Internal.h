/***************************************************************************
    File                 : MultiLayer_Internal.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Internal helpers shared across MultiLayer split TUs

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
#ifndef MULTILAYER_INTERNAL_H
#define MULTILAYER_INTERNAL_H

#include "MultiLayer.h"
#include "ApplicationWindow.h"

#include <QApplication>
#include <QColor>
#include <QFrame>
#include <QSize>
#include <QWidget>

namespace MultiLayerInternal {

inline ApplicationWindow *findMultiLayerApp(QWidget *from)
{
    for (QWidget *w = from; w != nullptr; w = w->parentWidget()) {
        if (auto *app = qobject_cast<ApplicationWindow *>(w))
            return app;
    }
    if (qApp) {
        const QWidgetList tops = QApplication::topLevelWidgets();
        for (QWidget *tw : tops) {
            if (auto *app = qobject_cast<ApplicationWindow *>(tw))
                return app;
        }
    }
    return nullptr;
}

// Per-layer state of MultiLayer::setPageZoom(): the layer rectangle at 100 % (QRectF) and the
// rectangle the last zoom gave it (QRect).
inline constexpr const char *kZoomBaseGeomProp = "_toriZoomBaseGeom";
inline constexpr const char *kZoomAppliedGeomProp = "_toriZoomAppliedGeom";

void syncMultiLayerPageHost(MultiLayer *ml, QWidget *mainWidget, QWidget *canvas,
                            MultiLayer::PageUnit pageUnit, bool pageViewMode,
                            const QSize &basePagePx, double pageZoom, const QColor &pageBgColor);

void resizeMultiLayerSubWindowForCanvas(MultiLayer *ml, QWidget *mainW, QFrame *pillFrame, int cw,
                                        int ch, bool freePixelMode);

void clampGraphCalloutMarkersInCanvas(Graph *gr);

} // namespace MultiLayerInternal

#endif // MULTILAYER_INTERNAL_H
