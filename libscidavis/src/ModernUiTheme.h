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

#include <QColor>
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

    // H4 / H5 accessibility colours (contrast >= 4.5:1 on the chrome backgrounds).
    static QColor disabledTextColor() { return QColor(0x5B, 0x6B, 0x80); }
    static QColor placeholderTextColor() { return QColor(0x64, 0x74, 0x8B); }
    static QColor focusRingColor() { return QColor(0x25, 0x63, 0xEB); }
    // Focus ring on a dark surface (the Focus Canvas dark pill): #2563EB is too dark there.
    static QColor focusRingColorOnDark() { return QColor(0x60, 0xA5, 0xFA); }
    // Focus ring colour to use around `w` (light chrome or the dark pill).
    static QColor focusRingColorFor(const QWidget *w);
    // Font-size steps (pt) used by the UI style sheets and the application base font:
    // caption/hint text < dense controls < standard text (= the application base font, which
    // labels, check boxes and inputs follow) < headings.
    static constexpr double kFontCaptionPt = 9.5;
    static constexpr double kFontDensePt = 10.0;
    static constexpr double kFontStandardPt = 10.5;
    static constexpr double kFontHeadingPt = 11.5;
    // True while a keyboard-focus ring is shown around `w` (tests / diagnostics).
    static bool keyboardFocusRingTargets(const QWidget *w);

private:
    ModernUiTheme() { }
};

#endif // MODERNUITHEME_H
