/***************************************************************************
    File                 : ModernRibbonAndHud_Internal.h
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : Internal cross-translation-unit helpers for ModernRibbonAndHud split files
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
#ifndef MODERN_RIBBON_AND_HUD_INTERNAL_H
#define MODERN_RIBBON_AND_HUD_INTERNAL_H

#include <QSet>

class QObject;
class ApplicationWindow;
class ModernRibbonBar;
class MultiLayer;
class MyWidget;
class Table;

namespace ModernRibbonHudInternal {

void registerRibbonCollapseTranslations();
MultiLayer *resolveTargetMultiLayer(ApplicationWindow *app, ModernRibbonBar *ribbon);
Table *sourceTableFromMultiLayer(MultiLayer *ml,
                                 const QSet<QObject *> &ignored = QSet<QObject *>());
bool isWorkbookManagedSheet(ApplicationWindow *app, MyWidget *mw);
bool isUsableRibbonWorksheet(ApplicationWindow *app, MyWidget *mw);
MultiLayer *activeMultiLayerOrUxHint(ApplicationWindow *app);

} // namespace ModernRibbonHudInternal

#endif // MODERN_RIBBON_AND_HUD_INTERNAL_H
