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

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

class IconLoader
{
public:
    enum IconMode { LightDark = 0, General = 1, Indiscriminate = 2 };

    static void init();
    static QIcon load(const QString &name, const IconMode &iconMode = LightDark);

    static QIcon duotoneIcon(const QString &semanticId, int baseSize = 24,
                             bool darkSurface = false);
    static QPixmap duotonePixmap(const QString &semanticId, int pxSize = 24, qreal dpr = 0.0,
                                 bool darkSurface = false);
    static QStringList registeredIconIds();
    static QString resolveIconId(const QString &nameOrAlias);

    static int inline isLight(const QColor &col)
    {
        return static_cast<int>(0.2126 * col.red() + 0.7152 * col.blue() + 0.0722 * col.green());
    }

    static int lumen_;
    static int &lumi_;

private:
    IconLoader() { }

    enum IconType { Dark = 0, Light = 1 };

    static QList<int> sizes_;
    static QList<QString> icon_path_;
};

#endif // ICONLOADER_H
