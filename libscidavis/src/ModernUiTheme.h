/***************************************************************************
    File                 : ModernUiTheme.h
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : Modern Fluent Scientific Theme & Legacy XPM Icon Upgrader
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

#ifndef MODERNUITHEME_H
#define MODERNUITHEME_H

#include <QString>

class QAction;
class QApplication;
class QMenu;
class QToolButton;
class QWidget;

class ModernUiTheme
{
public:
    static void applyModernTheme(QApplication *app);
    static QString modernStyleSheet();
    static void upgradeAllWindowIcons(QWidget *rootWindow);
    static void upgradeMenuIcons(QMenu *menu);

    static QString semanticIconForAction(const QAction *action);
    static QString semanticIconForToolButton(const QToolButton *button);

private:
    ModernUiTheme() { }
};

#endif // MODERNUITHEME_H
