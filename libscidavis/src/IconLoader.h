/***************************************************************************
    File                 : IconLoader.h
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2016 by Arun Narayanankutty
    Description          : 2-Color Duotone Vector SVG Icon Engine
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

#ifndef ICONLOADER_H
#define ICONLOADER_H

#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

class IconLoader
{
public:
    // All icons are 2-colour duotone SVGs drawn from the semantic id registry below
    // (kIconSpecs in IconLoader.cpp). Legacy SciDAVis resource names (":/fileopen.xpm",
    // "edit-copy", ...) are still accepted and mapped through the alias table, but new code
    // should pass the semantic id ("file-open").
    //
    // ToriGraph always runs with its light theme (ModernUiTheme), so there is no light/dark
    // icon switching. `darkSurface` is only for icons drawn on a dark chip inside the light
    // UI (focus HUD pill, the pressed state of the mini toolbar).
    static void init();
    static QIcon load(const QString &name);

    static QIcon duotoneIcon(const QString &semanticId, int baseSize = 24,
                             bool darkSurface = false);
    static QPixmap duotonePixmap(const QString &semanticId, int pxSize = 24, qreal dpr = 0.0,
                                 bool darkSurface = false);
    static QStringList registeredIconIds();
    static bool isRegisteredIconId(const QString &id);
    static QString resolveIconId(const QString &nameOrAlias);

    // Names that were neither a registered id nor an alias and therefore fell back to a
    // keyword guess or the generic graph icon (diagnostics / regression tests).
    static QStringList unresolvedIconNames();
    static void clearUnresolvedIconNames();

private:
    IconLoader() { }
};

#endif // ICONLOADER_H
