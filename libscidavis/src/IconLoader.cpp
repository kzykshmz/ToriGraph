/***************************************************************************
    File                 : IconLoader.cpp
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

#include "IconLoader.h"

#include <QByteArray>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPaintDevice>
#include <QPainter>
#include <QRectF>
#include <QtDebug>
#include <QtSvg/QSvgRenderer>

namespace {

enum class AccentDomain {
    IndigoWorkspace,
    BluePlot,
    CrimsonPeakFit,
    AmberGadget,
    EmeraldSignalStats,
    OrangeStyleExport,
    TealPython
};

struct DuotoneSvgSpec {
    const char *id;
    AccentDomain domain;
    const char *duotoneFill;
    const char *primaryStroke;
    const char *accentStroke;
};

static QString primaryHexColor(bool darkSurface, bool disabled)
{
    if (disabled)
        return darkSurface ? QStringLiteral("#64748B") : QStringLiteral("#94A3B8");
    return darkSurface ? QStringLiteral("#F8FAFC") : QStringLiteral("#1E293B");
}

static QString accentHexColor(AccentDomain domain, bool darkSurface, bool disabled)
{
    QColor c;
    switch (domain) {
    case AccentDomain::IndigoWorkspace:
        c = QColor(darkSurface ? QStringLiteral("#60A5FA") : QStringLiteral("#2563EB"));
        break;
    case AccentDomain::BluePlot:
        c = QColor(darkSurface ? QStringLiteral("#60A5FA") : QStringLiteral("#2563EB"));
        break;
    case AccentDomain::CrimsonPeakFit:
        c = QColor(darkSurface ? QStringLiteral("#FB7185") : QStringLiteral("#E11D48"));
        break;
    case AccentDomain::AmberGadget:
        c = QColor(darkSurface ? QStringLiteral("#FBBF24") : QStringLiteral("#D97706"));
        break;
    case AccentDomain::EmeraldSignalStats:
        c = QColor(darkSurface ? QStringLiteral("#34D399") : QStringLiteral("#059669"));
        break;
    case AccentDomain::OrangeStyleExport:
        c = QColor(darkSurface ? QStringLiteral("#FB923C") : QStringLiteral("#EA580C"));
        break;
    case AccentDomain::TealPython:
        c = QColor(darkSurface ? QStringLiteral("#38BDF8") : QStringLiteral("#0284C7"));
        break;
    }

    if (disabled) {
        const QColor neutral = darkSurface ? QColor(QStringLiteral("#475569"))
                                           : QColor(QStringLiteral("#CBD5E1"));
        const int r = (c.red() * 45 + neutral.red() * 55) / 100;
        const int g = (c.green() * 45 + neutral.green() * 55) / 100;
        const int b = (c.blue() * 45 + neutral.blue() * 55) / 100;
        return QColor(r, g, b).name(QColor::HexRgb);
    }
    return c.name(QColor::HexRgb);
}

// ============================================================================
// Distinct 24x24 Publication-Grade 2-Color Duotone Vector SVG Geometries (197 ids)
// ============================================================================
static const DuotoneSvgSpec kIconSpecs[] = {
    // --- 1. File & Workspace (33 + extras) ---
    { "file-new", AccentDomain::IndigoWorkspace,
      "<path d='M6 3h8l4 4v14H6z'/>",
      "<path d='M14 3H6a1 1 0 0 0-1 1v16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V8z'/><path d='M14 3v5h5'/>",
      "<path d='M12 11v6M9 14h6'/>" },

    { "file-open", AccentDomain::IndigoWorkspace,
      "<path d='M3 8h18l-2 11H5z'/>",
      "<path d='M3 19V6a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1v2'/>",
      "<path d='M3 10h18l-2.2 9H5.2z'/>" },

    { "file-save", AccentDomain::IndigoWorkspace,
      "<rect x='7' y='13' width='10' height='8' rx='1'/>",
      "<path d='M5 3h11l4 4v13a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z'/><path d='M8 3v5h7V3'/>",
      "<rect x='7' y='13' width='10' height='8' rx='1'/><path d='M10 16h4'/>" },

    { "file-save-as", AccentDomain::IndigoWorkspace,
      "<rect x='6' y='12' width='8' height='8' rx='1'/>",
      "<path d='M14 20H5a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1h10l4 4v5'/><path d='M8 3v4h6V3'/>",
      "<path d='M15.5 18.5L20 14l1.8 1.8-4.5 4.5-2.5.7z'/>" },

    { "import-csv", AccentDomain::IndigoWorkspace,
      "<rect x='4' y='3' width='14' height='18' rx='1.5'/>",
      "<path d='M14 3H6a1 1 0 0 0-1 1v16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-6'/><path d='M8 8h4M8 12h4M8 16h5'/>",
      "<path d='M21 9h-7m0 0l2.8-2.8M14 9l2.8 2.8'/>" },

    { "import-opj", AccentDomain::IndigoWorkspace,
      "<circle cx='12' cy='12' r='8'/>",
      "<circle cx='12' cy='12' r='8.5'/><path d='M7.5 14.5c2-4.5 4.5-4.5 9-2'/>",
      "<path d='M12 5v6m0 0l-2.5-2.5M12 11l2.5-2.5'/><circle cx='12' cy='15.5' r='1.8'/>" },

    // Excel workbook import: a worksheet grid under an import arrow, with the green Excel "X".
    { "import-xlsx", AccentDomain::EmeraldSignalStats,
      "<rect x='3' y='10' width='12' height='11' rx='1.5'/>",
      "<rect x='3' y='10' width='12' height='11' rx='1.5'/><path d='M3 13.7h12M3 17.3h12M9 10v11'/>",
      "<path d='M7 3v4.5m0 0l-2-2m2 2l2-2'/><path d='M16.5 3.5l5 6m0-6l-5 6'/>" },

    // PowerPoint export ("Send to PowerPoint"): a slide on a stand with an outgoing arrow.
    // Distinct from preset-slide (the Lab Slide style preset) and copy-ole (clipboard copy).
    { "export-pptx", AccentDomain::OrangeStyleExport,
      "<rect x='3' y='3.5' width='14' height='11' rx='1.5'/>",
      "<rect x='3' y='3.5' width='14' height='11' rx='1.5'/><path d='M10 14.5v3.5M7 18h6M6.5 8h7M6.5 11h4'/>",
      "<path d='M15.5 19h5.5m0 0l-2.6-2.6M21 19l-2.6 2.6'/>" },

    { "export-image", AccentDomain::OrangeStyleExport,
      "<rect x='3' y='5' width='15' height='14' rx='2'/>",
      "<path d='M13 5H5a2 2 0 0 0-2 2v11a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-6'/><circle cx='8' cy='10' r='1.4'/>",
      "<path d='M4 18l4.5-4.5 3 3 3.5-3.5L19 17'/><path d='M16 4h5v5M21 4l-5.5 5.5'/>" },

    { "export-pdf", AccentDomain::OrangeStyleExport,
      "<path d='M6 3h8l4 4v14H6z'/>",
      "<path d='M14 3H6a1 1 0 0 0-1 1v16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V8z'/><path d='M14 3v5h5'/>",
      "<path d='M9 17c1.8-3.8 3.2-6.5 2.4-7.5-.7-.8-1.7.8-.2 3.2 1.4 2.2 3.8 3.2 4.8 2.4.7-.6-.8-1.5-3.2-1.1-2.5.4-4.2 1.8-3.8 3z'/>" },

    { "copy-ole", AccentDomain::OrangeStyleExport,
      "<rect x='8' y='7' width='13' height='13' rx='2'/>",
      "<path d='M15 7V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2'/><rect x='8' y='7' width='13' height='13' rx='2'/>",
      "<path d='M11 16.5l2.5-3.5 2.2 2 3.3-4.5'/><circle cx='19' cy='10.5' r='1.2' fill='{{A}}' stroke='none'/>" },

    { "copy-page-image", AccentDomain::OrangeStyleExport,
      "<rect x='7' y='7' width='13' height='13' rx='2'/>",
      "<path d='M15 7V5a1.5 1.5 0 0 0-1.5-1.5h-8A1.5 1.5 0 0 0 4 5v8.5A1.5 1.5 0 0 0 5.5 15H7'/><rect x='7.5' y='7.5' width='12.5' height='12.5' rx='1.8'/>",
      "<circle cx='11.2' cy='11.5' r='1.3'/><path d='M8.5 18l3.5-3.8 2.5 2.3 2.5-3 2.5 3.5'/>" },

    { "print", AccentDomain::OrangeStyleExport,
      "<rect x='6' y='14' width='12' height='7' rx='1'/>",
      "<path d='M7 8V4h10v4'/><path d='M6 17H4a1 1 0 0 1-1-1v-6a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2v6a1 1 0 0 1-1 1h-2'/>",
      "<rect x='7' y='14' width='10' height='7' rx='0.8'/><circle cx='17' cy='11.5' r='0.9' fill='{{A}}' stroke='none'/>" },

    { "undo", AccentDomain::IndigoWorkspace,
      "<circle cx='12' cy='13' r='7'/>",
      "<path d='M9 14h6a4.5 4.5 0 1 0 0-9H5'/>",
      "<path d='M8.5 8.5L5 5l3.5-3.5'/><path d='M5 5h7'/>" },

    { "redo", AccentDomain::IndigoWorkspace,
      "<circle cx='12' cy='13' r='7'/>",
      "<path d='M15 14H9a4.5 4.5 0 1 1 0-9h10'/>",
      "<path d='M15.5 8.5L19 5l-3.5-3.5'/><path d='M19 5h-7'/>" },

    { "cut", AccentDomain::IndigoWorkspace,
      "<circle cx='6.5' cy='17.5' r='3'/><circle cx='17.5' cy='17.5' r='3'/>",
      "<circle cx='6.5' cy='17.5' r='2.8'/><circle cx='17.5' cy='17.5' r='2.8'/>",
      "<path d='M8.7 15.3L19 4M15.3 15.3L5 4'/>" },

    { "copy", AccentDomain::IndigoWorkspace,
      "<rect x='9' y='8' width='11' height='12' rx='1.8'/>",
      "<path d='M15 8V5.5A1.5 1.5 0 0 0 13.5 4h-8A1.5 1.5 0 0 0 4 5.5v9A1.5 1.5 0 0 0 5.5 16H9'/>",
      "<rect x='9' y='8' width='11' height='12' rx='1.8'/><path d='M12 12h5M12 15.5h5'/>" },

    { "paste", AccentDomain::IndigoWorkspace,
      "<rect x='5' y='6' width='14' height='15' rx='2'/>",
      "<path d='M9 5H6a1.5 1.5 0 0 0-1.5 1.5v13A1.5 1.5 0 0 0 6 21h12a1.5 1.5 0 0 0 1.5-1.5v-13A1.5 1.5 0 0 0 18 5h-3'/>",
      "<rect x='9' y='3' width='6' height='3.5' rx='1'/><path d='M8.5 11.5h7M8.5 15.5h5'/>" },

    { "clear", AccentDomain::IndigoWorkspace,
      "<path d='M6 7h12l-1 13H7z'/>",
      "<path d='M4 7h16M9 7V4.5A1.5 1.5 0 0 1 10.5 3h3A1.5 1.5 0 0 1 15 4.5V7M6.5 7l1 13a1.5 1.5 0 0 0 1.5 1.4h6a1.5 1.5 0 0 0 1.5-1.4l1-13'/>",
      "<path d='M10 11v6M14 11v6'/>" },

    { "find", AccentDomain::IndigoWorkspace,
      "<circle cx='10.5' cy='10.5' r='6.5'/>",
      "<circle cx='10.5' cy='10.5' r='6.5'/>",
      "<path d='M15.5 15.5L20.5 20.5'/><path d='M8 9.5a3 3 0 0 1 3-2'/>" },

    { "settings", AccentDomain::IndigoWorkspace,
      "<circle cx='12' cy='12' r='6.5'/>",
      "<path d='M12 3v2.2M12 18.8V21M3 12h2.2M18.8 12H21M5.6 5.6l1.6 1.6M16.8 16.8l1.6 1.6M18.4 5.6l-1.6 1.6M7.2 16.8l-1.6 1.6'/><circle cx='12' cy='12' r='6.5'/>",
      "<circle cx='12' cy='12' r='2.8'/>" },

    { "update-check", AccentDomain::OrangeStyleExport,
      "<circle cx='12' cy='12' r='8'/>",
      "<path d='M4.5 11A7.5 7.5 0 0 1 18 7.2M19.5 13A7.5 7.5 0 0 1 6 16.8'/><path d='M18.2 4v3.5h-3.5M5.8 20v-3.5h3.5'/>",
      "<path d='M12 8v5.5m0 0l-2.3-2.3M12 13.5l2.3-2.3'/><path d='M9.5 16h5'/>" },

    { "language-ja-en", AccentDomain::IndigoWorkspace,
      "<rect x='3' y='4' width='11' height='10' rx='2'/><rect x='10' y='10' width='11' height='10' rx='2'/>",
      "<path d='M4 6h9M8.5 4v2M6 6c.4 3.2 2.2 5.4 5 6.5M10.5 6c-.6 2.8-2.4 5-5.5 6.5'/>",
      "<path d='M13.5 19l3-8 3 8M14.5 16.8h4'/>" },

    { "help", AccentDomain::IndigoWorkspace,
      "<circle cx='12' cy='12' r='8.5'/>",
      "<circle cx='12' cy='12' r='8.8'/>",
      "<path d='M9.6 9.3a2.6 2.6 0 1 1 3.6 2.4c-.9.5-1.2 1.1-1.2 2.1'/><circle cx='12' cy='17' r='1' fill='{{A}}' stroke='none'/>" },

    { "split-view", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='7' height='15' rx='1.5'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2'/><path d='M11 4.5v15'/>",
      "<path d='M6 9h2.5M6 12h2.5M6 15h2.5'/><path d='M13.5 15.5l2.2-3.5 1.8 1.8 2-3.3'/>" },

    { "tab-view", AccentDomain::IndigoWorkspace,
      "<path d='M3.5 8.5h17v11h-17z'/>",
      "<rect x='3.5' y='5' width='17' height='14.5' rx='2'/><path d='M3.5 9h17'/>",
      "<path d='M3.5 9V6.5A1.5 1.5 0 0 1 5 5h4.5l1.5 4'/><path d='M14.5 5v4'/>" },

    { "mdi-view", AccentDomain::IndigoWorkspace,
      "<rect x='8.5' y='8.5' width='11.5' height='10.5' rx='1.5'/>",
      "<path d='M15 8.5V6a1.5 1.5 0 0 0-1.5-1.5h-8A1.5 1.5 0 0 0 4 6v7.5A1.5 1.5 0 0 0 5.5 15H8.5'/>",
      "<rect x='8.5' y='8.5' width='11.5' height='10.5' rx='1.5'/><path d='M8.5 11.8h11.5'/>" },

    { "focus-hud", AccentDomain::IndigoWorkspace,
      "<rect x='6' y='6' width='12' height='12' rx='2.5'/>",
      "<path d='M4 9V5.5A1.5 1.5 0 0 1 5.5 4H9M15 4h3.5A1.5 1.5 0 0 1 20 5.5V9M20 15v3.5a1.5 1.5 0 0 1-1.5 1.5H15M9 20H5.5A1.5 1.5 0 0 1 4 18.5V15'/>",
      "<path d='M8.5 14.5l2.5-3.5 2 2 2.8-3.8'/><circle cx='12' cy='7.5' r='1' fill='{{A}}' stroke='none'/>" },

    { "exit-focus-hud", AccentDomain::IndigoWorkspace,
      "<rect x='7' y='7' width='10' height='10' rx='2'/>",
      "<path d='M4 9h5V4M20 9h-5V4M4 15h5v5M20 15h-5v5'/>",
      "<rect x='8.5' y='8.5' width='7' height='7' rx='1.2'/>" },

    { "table-drawer", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='7.5' height='15' rx='1.5'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2'/><path d='M11 4.5v15'/>",
      "<path d='M5.8 8.5h3M5.8 12h3M5.8 15.5h3M14 12h4m0 0l-1.8-1.8M18 12l-1.8 1.8'/>" },

    { "search-command", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='17' height='15' rx='3'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='3'/>",
      "<path d='M7.5 9.5L10.5 12l-3 2.5M12.5 14.5h4'/>" },

    { "explorer", AccentDomain::IndigoWorkspace,
      "<path d='M3.5 7.5h17v11.5h-17z'/>",
      "<path d='M3.5 18.5V6a1.5 1.5 0 0 1 1.5-1.5h4.5l2 2.2H19A1.5 1.5 0 0 1 20.5 8.2v10.3A1.5 1.5 0 0 1 19 20H5a1.5 1.5 0 0 1-1.5-1.5z'/>",
      "<path d='M8 12h8M8 15.5h5'/>" },

    { "log-window", AccentDomain::IndigoWorkspace,
      "<rect x='4' y='4' width='16' height='16' rx='2'/>",
      "<rect x='4' y='4' width='16' height='16' rx='2'/><path d='M7.5 8.5h9M7.5 12h9'/>",
      "<path d='M7.5 15.8l2 1.8 3.8-3.6M15.5 15.5h1.5'/>" },

    { "python-console", AccentDomain::TealPython,
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2.5'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2.5'/>",
      "<path d='M7.5 9l3.2 3-3.2 3M12.5 15.5h4.2'/>" },

    { "python-run", AccentDomain::TealPython,
      "<polygon points='7,5 19,12 7,19'/>",
      "<circle cx='12' cy='12' r='9'/>",
      "<polygon points='9.8,8 16.5,12 9.8,16' fill='{{A}}'/>" },

    // --- 2. Worksheet & Matrix (12 IDs) ---
    { "new-table", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='17' height='4.5' rx='1'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='1.8'/><path d='M3.5 9.2h17M3.5 14h17M9.2 4.5v15'/>",
      "<path d='M15 4.5v9.5'/><circle cx='17.5' cy='17.5' r='3' fill='{{A}}' stroke='none'/><path d='M17.5 16v3M16 17.5h3' stroke='#FFFFFF' stroke-width='1.5'/>" },

    { "new-matrix", AccentDomain::IndigoWorkspace,
      "<rect x='5.5' y='5.5' width='13' height='13' rx='1.5'/>",
      "<path d='M6.5 4.5H4v15h2.5M17.5 4.5H20v15h-2.5'/>",
      "<circle cx='9' cy='9' r='1.2' fill='{{A}}' stroke='none'/><circle cx='15' cy='9' r='1.2' fill='{{A}}' stroke='none'/><circle cx='9' cy='15' r='1.2' fill='{{A}}' stroke='none'/><circle cx='15' cy='15' r='1.2' fill='{{A}}' stroke='none'/><path d='M12 8.5v7M8.5 12h7'/>" },

    { "new-graph", AccentDomain::BluePlot,
      "<polygon points='5,18 9.5,12.5 13.5,15 19,8 19,18'/>",
      "<path d='M4.5 4.5v14.5h15'/>",
      "<path d='M6.5 16l3.8-4.5 3.2 2.2 5-6.2'/><path d='M17 4.5v4M15 6.5h4'/>" },

    { "new-note", AccentDomain::IndigoWorkspace,
      "<path d='M5 4h14v12l-4 4H5z'/>",
      "<path d='M5 4h14v11.5L14.5 20H5a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1z'/><path d='M14 20v-4.5h5'/>",
      "<path d='M8 9h8M8 12.5h5.5'/>" },

    { "new-folder", AccentDomain::IndigoWorkspace,
      "<path d='M3.5 7.5h17v11.5h-17z'/>",
      "<path d='M3.5 18.5V6a1.5 1.5 0 0 1 1.5-1.5h4.5l2 2.2H19A1.5 1.5 0 0 1 20.5 8.2v10.3A1.5 1.5 0 0 1 19 20H5a1.5 1.5 0 0 1-1.5-1.5z'/>",
      "<path d='M12 10.5v6M9 13.5h6'/>" },

    { "add-column", AccentDomain::IndigoWorkspace,
      "<rect x='10' y='4.5' width='5.5' height='15' rx='1'/>",
      "<path d='M4 4.5h11.5v15H4zM9.8 4.5v15M4 9.5h11.5M4 14.5h11.5'/>",
      "<path d='M19 9v6M16.5 12h5'/>" },

    { "sort-table", AccentDomain::IndigoWorkspace,
      "<rect x='4' y='5' width='8' height='14' rx='1.5'/>",
      "<path d='M4 7h8M4 12h6.5M4 17h4.5'/>",
      "<path d='M16.5 5v14m0 0l-3-3m3 3l3-3'/>" },

    { "col-stats", AccentDomain::IndigoWorkspace,
      "<rect x='9' y='4' width='6' height='16' rx='1.2'/>",
      "<rect x='4' y='4' width='16' height='16' rx='1.8'/><path d='M9.3 4v16M14.7 4v16'/>",
      "<path d='M10.5 9h3l-2.2 3 2.2 3h-3'/>" },

    { "row-stats", AccentDomain::IndigoWorkspace,
      "<rect x='4' y='9' width='16' height='6' rx='1.2'/>",
      "<rect x='4' y='4' width='16' height='16' rx='1.8'/><path d='M4 9.3h16M4 14.7h16'/>",
      "<path d='M10 10.5h4l-2.8 1.5 2.8 1.5h-4'/>" },

    { "recalc-auto", AccentDomain::EmeraldSignalStats,
      "<rect x='6' y='10.5' width='12' height='9.5' rx='2'/>",
      "<rect x='6' y='10.5' width='12' height='9.5' rx='2'/><path d='M8.5 10.5V7.8a3.5 3.5 0 0 1 7 0v2.7'/>",
      "<path d='M9.8 15.2l1.6 1.7 3.2-3.4'/>" },

    { "recalc-now", AccentDomain::EmeraldSignalStats,
      "<circle cx='12' cy='12' r='8'/>",
      "<path d='M4.5 12a7.5 7.5 0 0 1 13-5M19.5 12a7.5 7.5 0 0 1-13 5'/><path d='M18 4v3.5h-3.5M6 20v-3.5h3.5'/>",
      "<polygon points='12.8,7.5 9.5,12.5 12.2,12.5 11.2,16.5 14.5,11.5 11.8,11.5' fill='{{A}}'/>" },

    { "formula", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2.5'/>",
      "<path d='M11.5 6.5c-1.5 0-2.2.8-2.5 2.5l-1.2 7c-.3 1.2-.9 1.6-1.8 1.6M7 11h4'/>",
      "<path d='M14 10.5l4.5 5.5M18.5 10.5L14 16'/>" },

    // --- 3. 2D/3D Plots (24 IDs) ---
    { "plot-line", AccentDomain::BluePlot,
      "<polygon points='4.5,19 8.5,13 13,15.5 19.5,7 19.5,19'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<path d='M6 16.5l3.5-4.8 3.8 2.5 6-7.5'/>" },

    { "plot-scatter", AccentDomain::BluePlot,
      "<circle cx='12.5' cy='11.5' r='6.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<circle cx='8' cy='15' r='1.6' fill='{{A}}' stroke='none'/><circle cx='11' cy='10.5' r='1.6' fill='{{A}}' stroke='none'/><circle cx='14.5' cy='13.2' r='1.6' fill='{{A}}' stroke='none'/><circle cx='17.8' cy='7.8' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "plot-line-symbol", AccentDomain::BluePlot,
      "<polygon points='5,19 9,12.5 14,14.5 19,8 19,19'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M6.5 16l3.8-4.5 4.2 2 4.5-6'/>",
      "<circle cx='6.5' cy='16' r='1.7' fill='{{A}}' stroke='none'/><circle cx='10.3' cy='11.5' r='1.7' fill='{{A}}' stroke='none'/><circle cx='14.5' cy='13.5' r='1.7' fill='{{A}}' stroke='none'/><circle cx='19' cy='7.5' r='1.7' fill='{{A}}' stroke='none'/>" },

    { "plot-spline", AccentDomain::BluePlot,
      "<path d='M5 18C7 8 11 8 13 13s4 4 6.5-6V19H5z'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<path d='M6 16.5C8 7.5 11.5 8 13 12.8S17 16.5 19.5 7'/>" },

    { "plot-bars", AccentDomain::BluePlot,
      "<rect x='6.5' y='11' width='3' height='8'/><rect x='15.5' y='9' width='3' height='10'/>",
      "<path d='M4 4.5v14.5h16'/><rect x='6.5' y='11.5' width='3' height='7.5'/>",
      "<rect x='11' y='7' width='3' height='12'/><rect x='15.5' y='9.5' width='3' height='9.5'/>" },

    { "plot-hbar", AccentDomain::BluePlot,
      "<rect x='4.5' y='6' width='11' height='3'/><rect x='4.5' y='15' width='9' height='3'/>",
      "<path d='M4.5 4v15.5h15.5'/><rect x='4.5' y='15' width='8.5' height='2.8'/>",
      "<rect x='4.5' y='6' width='11' height='2.8'/><rect x='4.5' y='10.5' width='14.5' height='2.8'/>" },

    { "plot-area", AccentDomain::BluePlot,
      "<polygon points='5,18.8 5,13.5 9.5,9.5 14,12.5 19.5,6.5 19.5,18.8'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<path d='M5 13.5l4.5-4 4.5 3 5.5-6v12.3H5z'/>" },

    { "plot-pie", AccentDomain::BluePlot,
      "<path d='M12 4a8 8 0 0 1 8 8h-8z'/>",
      "<path d='M11 5.2A7.8 7.8 0 1 0 18.8 13H11z'/>",
      "<path d='M13.2 3.8A7.8 7.8 0 0 1 20.2 10.8h-7z'/>" },

    { "plot-box", AccentDomain::BluePlot,
      "<rect x='7' y='8' width='4.2' height='7.5'/><rect x='13.5' y='6.5' width='4.2' height='8'/>",
      "<path d='M9.1 5v3M9.1 15.5V19M7 8h4.2v7.5H7z'/>",
      "<path d='M15.6 4v2.5M15.6 14.5V18.5M13.5 6.5h4.2v8h-4.2zM7 11.8h4.2M13.5 10.5h4.2'/>" },

    { "plot-violin", AccentDomain::BluePlot,
      "<path d='M12 4c3.5 3 4.5 5.5 4.5 8s-1 5-4.5 8c-3.5-3-4.5-5.5-4.5-8S8.5 7 12 4z'/>",
      "<path d='M12 4c3.4 3 4.5 5.5 4.5 8s-1.1 5-4.5 8c-3.4-3-4.5-5.5-4.5-8S8.6 7 12 4z'/>",
      "<rect x='10.8' y='9' width='2.4' height='6' rx='0.8'/><circle cx='12' cy='12' r='1' fill='{{A}}' stroke='none'/>" },

    { "plot-histogram", AccentDomain::BluePlot,
      "<path d='M5.5 19v-5h3.2V8.5h3.3V6h3.3v5.5h3.2V19z'/>",
      "<path d='M4 4.5v14.5h16'/><path d='M5.5 19v-5h3.2v5M8.7 19V8.5h3.3V19M15.3 19v-7.5h3.2V19'/>",
      "<rect x='12' y='6' width='3.3' height='13'/><path d='M5 15c2.5-6 4.8-9.5 7.5-9.5s5 3.5 7.5 9.5'/>" },

    { "plot-pareto", AccentDomain::BluePlot,
      "<rect x='6' y='8' width='3.2' height='11'/><rect x='10.5' y='11.5' width='3.2' height='7.5'/>",
      "<path d='M4 4.5v14.5h16'/><rect x='6' y='8' width='3' height='11'/><rect x='10.5' y='11.8' width='3' height='7.2'/><rect x='15' y='15' width='3' height='4'/>",
      "<path d='M6.5 13C10 8.5 14 6.5 19 5.5'/><circle cx='19' cy='5.5' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "plot-qq", AccentDomain::BluePlot,
      "<polygon points='5,19 19,5 19,11 11,19'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M5.5 18.5L19 5'/>",
      "<circle cx='7.5' cy='16.8' r='1.4' fill='{{A}}' stroke='none'/><circle cx='11' cy='12.5' r='1.4' fill='{{A}}' stroke='none'/><circle cx='14.5' cy='9.8' r='1.4' fill='{{A}}' stroke='none'/><circle cx='17.5' cy='6.2' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "plot-double-y", AccentDomain::BluePlot,
      "<polygon points='5,18.5 10,12 14.5,14.5 19,9 19,18.5'/>",
      "<path d='M4.5 4.5v14.5h15V4.5'/><path d='M6 16l4-5 4 2.5 4-4.5'/>",
      "<path d='M19.5 4.5v14.5'/><path d='M6 11l4.2 3 4.3-5.5 4 2'/>" },

    { "plot-waterfall", AccentDomain::BluePlot,
      "<polygon points='5,18 9,13 13,15 18,10 18,18'/>",
      "<path d='M4.5 17.5l3.8-3.8 3.5 1.8 4.2-4.5 3.5 1.5'/><path d='M5.5 13l3.8-3.8 3.5 1.8 4.2-4.5 3 1.2'/>",
      "<path d='M6.5 8.5L10.3 4.8l3.5 1.8L18 3.5l2.5 1.2'/>" },

    { "plot-vectors", AccentDomain::BluePlot,
      "<rect x='5' y='5' width='14' height='14' rx='2'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M7 16l4-4m0 0H8.5M11 12v2.5'/>",
      "<path d='M12.5 15.5l5.5-5.5m0 0h-3m3 0v3M7.5 10l4.5-3.5m0 0H9.5M12 6.5V9'/>" },

    { "plot-ternary", AccentDomain::BluePlot,
      "<polygon points='12,4.5 20,18.5 4,18.5'/>",
      "<polygon points='12,4.5 20,18.5 4,18.5'/><path d='M8 11.5h8M8 18.5l4-7 4 7'/>",
      "<circle cx='12' cy='13.2' r='1.8' fill='{{A}}' stroke='none'/><circle cx='10' cy='15.8' r='1.3' fill='{{A}}' stroke='none'/><circle cx='14.2' cy='15.2' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "plot-radar", AccentDomain::BluePlot,
      "<circle cx='12' cy='12' r='7.5'/>",
      "<circle cx='12' cy='12' r='7.5'/><circle cx='12' cy='12' r='3.5'/><path d='M12 4.5v15M4.5 12h15'/>",
      "<polygon points='12,6.5 17,11 14,17 8,14.5'/>" },

    { "plot-heatmap", AccentDomain::BluePlot,
      "<rect x='4' y='4.5' width='12.5' height='15' rx='1.5'/>",
      "<rect x='4' y='4.5' width='12.5' height='15' rx='1.5'/><path d='M8.2 4.5v15M12.3 4.5v15M4 9.5h12.5M4 14.5h12.5'/>",
      "<rect x='18.2' y='4.5' width='2.2' height='15' rx='1'/><rect x='8.2' y='9.5' width='4.1' height='5' fill='{{A}}'/>" },

    { "plot-contour", AccentDomain::BluePlot,
      "<ellipse cx='12' cy='12' rx='6.5' ry='4.5'/>",
      "<rect x='3.8' y='4.2' width='16.4' height='15.6' rx='2'/><ellipse cx='12' cy='12' rx='7' ry='5'/>",
      "<ellipse cx='12.5' cy='12' rx='4' ry='2.6'/><circle cx='13' cy='12' r='1' fill='{{A}}' stroke='none'/>" },

    { "plot-3d-surface", AccentDomain::BluePlot,
      "<path d='M4 15l8-4 8 4-8 4.5z'/>",
      "<path d='M4 15.5l8 4 8-4M4 15.5V8l8-3.5L20 8v7.5'/>",
      "<path d='M4.5 12c3.5-4.5 6.5-4.5 8-1s4.5 3.5 7-1'/><path d='M8 10.2c2.5-2.5 5.5-2.5 8 0'/>" },

    { "plot-3d-wireframe", AccentDomain::BluePlot,
      "<polygon points='12,4.5 20,9 12,13.5 4,9'/>",
      "<polygon points='12,4.5 20,9 20,15.5 12,20 4,15.5 4,9'/><path d='M4 9l8 4.5L20 9M12 13.5V20'/>",
      "<path d='M8 6.8l8 4.5M16 6.8l-8 4.5'/>" },

    { "plot-3d-bars", AccentDomain::BluePlot,
      "<rect x='10.2' y='6' width='4' height='12'/>",
      "<path d='M3.5 16.5L11.5 20l8.5-3.5'/><rect x='5.5' y='10.5' width='3.2' height='6.5'/><rect x='15.2' y='8.5' width='3.2' height='8.5'/>",
      "<rect x='10.4' y='5.5' width='3.4' height='12.5'/>" },

    { "plot-3d-scatter", AccentDomain::BluePlot,
      "<polygon points='12,5 19.5,9.5 12,14 4.5,9.5'/>",
      "<path d='M12 4.5v8.5M12 13L4.5 17.5M12 13l7.5 4.5'/>",
      "<circle cx='8.5' cy='9' r='1.6' fill='{{A}}' stroke='none'/><circle cx='15.5' cy='8' r='1.7' fill='{{A}}' stroke='none'/><circle cx='12.5' cy='16.5' r='1.6' fill='{{A}}' stroke='none'/><circle cx='16.5' cy='13.2' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "plot-3d-ribbon", AccentDomain::BluePlot,
      "<path d='M4.5 15.5c3-5 6-5 8.5-1s4 3 6.5-2l-2 4.5c-2.5 4-4 2-6.5-1s-4.5-2-6.5-.5z'/>",
      "<path d='M4 17.5l7.5 2.5 8.5-3.5M4 17.5V7'/>",
      "<path d='M5.5 14c2.8-5 5.5-5 8-1.2s3.8 3.2 6-1.8'/><path d='M7 16.5c2.8-5 5.5-5 8-1.2s3.2 2.8 4.8-1'/>" },

    // --- 4. Peaks & Fitting (18 IDs) ---
    { "peak-analyzer", AccentDomain::CrimsonPeakFit,
      "<path d='M4.5 18.5c2.5 0 3.5-11 6.2-11s2.5 6.5 4.3 6.5 1.8-4.5 3.2-4.5 1.5 9 2.8 9z'/>",
      "<path d='M3.5 18.5h17'/><path d='M4.5 18.5c2.5 0 3.5-11.5 6.2-11.5s2.6 7 4.3 7 1.7-4.8 3.2-4.8 1.5 9.3 2.8 9.3'/>",
      "<path d='M10.7 4.2v3M10.7 11v7.5' stroke-dasharray='2 2'/><circle cx='10.7' cy='7' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "peak-gauss", AccentDomain::CrimsonPeakFit,
      "<path d='M4 18.5c3.5 0 4.8-12 8-12s4.5 12 8 12z'/>",
      "<path d='M3.5 18.5h17'/>",
      "<path d='M4 18.5c3.5 0 4.8-12 8-12s4.5 12 8 12'/><path d='M8.5 12.5h7'/>" },

    { "peak-lorentz", AccentDomain::CrimsonPeakFit,
      "<path d='M4 18.5c4.5-.5 6.2-3.5 8-13 1.8 9.5 3.5 12.5 8 13z'/>",
      "<path d='M3.5 18.5h17'/>",
      "<path d='M4 18c4.5-.6 6.4-3.8 8-12.8 1.6 9 3.5 12.2 8 12.8'/><circle cx='12' cy='5.2' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "peak-voigt", AccentDomain::CrimsonPeakFit,
      "<path d='M4 18.5c3.8-.3 5.5-12.5 8-12.5s4.2 12.2 8 12.5z'/>",
      "<path d='M3.5 18.5h17'/><path d='M5.5 18.5c2.8 0 4-8.5 6.5-8.5s3.7 8.5 6.5 8.5'/>",
      "<path d='M4 18.2c3.8-.4 5.6-12.4 8-12.4s4.2 12 8 12.4'/>" },

    { "peak-batch", AccentDomain::CrimsonPeakFit,
      "<path d='M4 18c2.5 0 3.8-8.5 6.2-8.5S13.8 18 16.5 18z'/>",
      "<path d='M3.5 19h17'/><path d='M7.5 16c2.2 0 3.2-8.5 5.5-8.5s3.3 8.5 5.8 8.5'/>",
      "<path d='M4.2 18.5c2.4 0 3.6-9.5 6-9.5s3.5 9.5 6 9.5'/>" },

    { "peak-2d", AccentDomain::CrimsonPeakFit,
      "<ellipse cx='12' cy='12.5' rx='7' ry='4.5'/>",
      "<ellipse cx='12' cy='13' rx='7.5' ry='4.8'/><path d='M4.5 13c3-6.5 5.2-8.5 7.5-8.5s4.5 2 7.5 8.5'/>",
      "<ellipse cx='12' cy='12.5' rx='3.8' ry='2.2'/><circle cx='12' cy='9.5' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "fit-linear", AccentDomain::CrimsonPeakFit,
      "<polygon points='5,18.5 19,6.5 19,12.5 9,18.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><circle cx='7.5' cy='14.5' r='1.2'/><circle cx='12.5' cy='13' r='1.2'/><circle cx='16.5' cy='7.5' r='1.2'/>",
      "<path d='M5.5 17.5L19.5 6'/>" },

    { "fit-york", AccentDomain::CrimsonPeakFit,
      "<circle cx='12' cy='12' r='5.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M9 12h6M12 9v6'/>",
      "<path d='M5.5 17.5L19.5 6'/><circle cx='12' cy='12' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "fit-poly", AccentDomain::CrimsonPeakFit,
      "<path d='M5 17c3-10 6.5-10 9 0s3.5 2 5.5-8v10H5z'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<path d='M5.5 16.5c2.8-9.5 6-9.5 8.5-.5s3.5 2.5 5.5-8'/>" },

    { "fit-nonlinear", AccentDomain::CrimsonPeakFit,
      "<path d='M5 17.5c4 0 5-10.5 14.5-10.5v11.5H5z'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><circle cx='7.5' cy='16.5' r='1.1'/><circle cx='12' cy='12' r='1.1'/><circle cx='17' cy='7.8' r='1.1'/>",
      "<path d='M5.5 17c4.5 0 5.5-10 14-10'/>" },

    { "fit-global", AccentDomain::CrimsonPeakFit,
      "<circle cx='12' cy='12' r='7'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M6 17c3.5-4.5 7.5-7 13.5-7.5'/>",
      "<path d='M6 13.5c3.5-4.5 7.5-7 13.5-7.5'/><path d='M12 7.5v5.5'/>" },

    { "fit-piecewise", AccentDomain::CrimsonPeakFit,
      "<polygon points='5,18 12,9.5 19.5,7.5 19.5,18'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M12 6v12' stroke-dasharray='2 2'/>",
      "<path d='M5.8 17L12 9.8l7.5-2.3'/><circle cx='12' cy='9.8' r='1.8' fill='{{A}}' stroke='none'/>" },

    { "fit-robust", AccentDomain::CrimsonPeakFit,
      "<polygon points='5.5,16.5 19,8.5 19,13 8,18.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><circle cx='8' cy='6.5' r='1.5'/><path d='M6.8 5.3l2.4 2.4'/>",
      "<path d='M5.8 16.5L19.5 8.5'/><circle cx='10' cy='14' r='1.3' fill='{{A}}' stroke='none'/><circle cx='15.5' cy='10.8' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "fit-odr", AccentDomain::CrimsonPeakFit,
      "<ellipse cx='12.5' cy='11.8' rx='6' ry='4.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<ellipse cx='12.5' cy='11.8' rx='5.8' ry='4.2'/><path d='M9 8.5l2.2 2.2M16 15l-2.2-2.2'/>" },

    { "fit-logistic", AccentDomain::CrimsonPeakFit,
      "<path d='M5 17.5h4c3 0 3-10.5 6-10.5h4.5v10.5z'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M5.5 7h14' stroke-dasharray='2 2'/>",
      "<path d='M5.5 17c4.5 0 4.5-10 9-10h5'/>" },

    { "fit-mlr", AccentDomain::CrimsonPeakFit,
      "<polygon points='5,16 12,11 19.5,6.5 19.5,15 12,18.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M6 13l13-3.5'/>",
      "<path d='M6 17.2L19.2 6.8'/><circle cx='12.5' cy='12' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "fit-surface-3d", AccentDomain::CrimsonPeakFit,
      "<polygon points='4,14 12,8.5 20,14 12,19'/>",
      "<path d='M4 15l8 4.2 8-4.2M4 15V8.5L12 4.5l8 4V15'/>",
      "<path d='M5 12.5c3.5-4.5 6-4.5 7-2s3.5 2.5 7-2'/>" },

    { "fit-compare", AccentDomain::CrimsonPeakFit,
      "<rect x='4.5' y='5.5' width='15' height='13' rx='2'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M6 17c4 0 5.5-8.5 13.5-9'/>",
      "<path d='M6 15.5L19 7'/><path d='M15.5 13.5l1.5 1.5 3-3'/>" },

    // --- 5. Gadgets, Masks & Tools (25 IDs) ---
    { "pointer", AccentDomain::AmberGadget,
      "<polygon points='6,4 17.5,13 11.5,14 14.5,20 11.8,21 9,15 6,18'/>",
      "<polygon points='6,4 17.5,13 11.8,14 14.5,19.8 12,20.8 9.2,15 6,18.2'/>",
      "<circle cx='16.5' cy='6.5' r='1.8'/>" },

    { "zoom-in", AccentDomain::AmberGadget,
      "<circle cx='10.5' cy='10.5' r='6.5'/>",
      "<circle cx='10.5' cy='10.5' r='6.5'/><path d='M15.3 15.3L20.5 20.5'/>",
      "<path d='M10.5 7.5v6M7.5 10.5h6'/>" },

    { "zoom-out", AccentDomain::AmberGadget,
      "<circle cx='10.5' cy='10.5' r='6.5'/>",
      "<circle cx='10.5' cy='10.5' r='6.5'/><path d='M15.3 15.3L20.5 20.5'/>",
      "<path d='M7.5 10.5h6'/>" },

    { "zoom-auto", AccentDomain::AmberGadget,
      "<rect x='6.5' y='6.5' width='11' height='11' rx='2'/>",
      "<path d='M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5'/>",
      "<path d='M7.5 14.5l3-4 2.5 2.5 3.5-4.5'/>" },

    { "data-reader", AccentDomain::AmberGadget,
      "<circle cx='12' cy='11' r='5.5'/>",
      "<path d='M4 17c3-4.5 5.5-6 8-6s5 1.5 8 6'/>",
      "<path d='M12 4v14M6 11h12'/><circle cx='12' cy='11' r='2.2'/>" },

    { "screen-reader", AccentDomain::AmberGadget,
      "<circle cx='12' cy='12' r='6'/>",
      "<circle cx='12' cy='12' r='7.5'/><path d='M12 2.5v3M12 18.5v3M2.5 12h3M18.5 12h3'/>",
      "<circle cx='12' cy='12' r='2' fill='{{A}}' stroke='none'/>" },

    { "range-selector", AccentDomain::AmberGadget,
      "<rect x='7.5' y='4.5' width='9' height='15' rx='1'/>",
      "<path d='M4 16l4-4.5 4 2.5 4-5 4 3'/>",
      "<path d='M7.5 4.5v15M16.5 4.5v15M5.5 7l2-2.5 2 2.5M14.5 7l2-2.5 2 2.5'/>" },

    { "move-points", AccentDomain::AmberGadget,
      "<circle cx='12' cy='12' r='6.5'/>",
      "<path d='M12 3.5v17M3.5 12h17M9.5 6L12 3.5 14.5 6M9.5 18L12 20.5 14.5 18M6 9.5L3.5 12 6 14.5M18 9.5L20.5 12 18 14.5'/>",
      "<circle cx='12' cy='12' r='2.2' fill='{{A}}' stroke='none'/>" },

    { "remove-points", AccentDomain::AmberGadget,
      "<circle cx='12' cy='12' r='7'/>",
      "<path d='M4.5 17l4.2-4.8 3.3 2 7.5-7.5'/>",
      "<circle cx='12' cy='14' r='4.2'/><path d='M10.4 12.4l3.2 3.2M13.6 12.4l-3.2 3.2'/>" },

    { "mask-points", AccentDomain::AmberGadget,
      "<rect x='5' y='5' width='14' height='14' rx='3'/>",
      "<path d='M4 12c2.5-4.5 5.2-6.5 8-6.5s5.5 2 8 6.5c-2.5 4.5-5.2 6.5-8 6.5S6.5 16.5 4 12z'/><circle cx='12' cy='12' r='2.5'/>",
      "<path d='M4.5 19.5L19.5 4.5'/>" },

    { "unmask-points", AccentDomain::AmberGadget,
      "<ellipse cx='12' cy='12' rx='7.5' ry='5.5'/>",
      "<path d='M3.5 12c2.6-4.6 5.4-6.8 8.5-6.8s5.9 2.2 8.5 6.8c-2.6 4.6-5.4 6.8-8.5 6.8S6.1 16.6 3.5 12z'/>",
      "<circle cx='12' cy='12' r='2.8'/>" },

    { "clear-masks", AccentDomain::AmberGadget,
      "<rect x='5' y='6' width='14' height='12' rx='2.5'/>",
      "<path d='M4 12c2.3-4 4.8-6 7.5-6 1.6 0 3.1.7 4.5 2M4 12c1.8 3.2 3.8 5 6 5.7'/>",
      "<path d='M14 14.5l2 2 4-4'/>" },

    { "gadget-roi", AccentDomain::AmberGadget,
      "<rect x='6.5' y='5' width='11' height='14' rx='1.5'/>",
      "<path d='M3.5 16.5l4-4 3.5 2.5 4.5-6 5 4.5'/>",
      "<rect x='6.5' y='5' width='11' height='14' rx='1.5' stroke-dasharray='3 2'/>" },

    { "gadget-cursor", AccentDomain::AmberGadget,
      "<rect x='9.5' y='4' width='5' height='16' rx='1'/>",
      "<path d='M4 15.5c3-5 5.5-5 8-1.5s4.5 2.5 8-3.5'/>",
      "<path d='M12 3.5v17'/><circle cx='12' cy='14' r='2' fill='{{A}}' stroke='none'/>" },

    { "gadget-delta", AccentDomain::AmberGadget,
      "<rect x='7' y='4.5' width='10' height='15'/>",
      "<path d='M3.5 16l4.5-4.5 4 2.5 4.5-5.5 4 3'/>",
      "<path d='M7 4.5v15M17 4.5v15M9.2 8.5h5.6'/>" },

    { "gadget-quick-fit", AccentDomain::AmberGadget,
      "<rect x='6' y='5' width='12' height='14' rx='1.5'/>",
      "<rect x='6' y='5' width='12' height='14' rx='1.5' stroke-dasharray='3 2'/>",
      "<path d='M4.5 17.5L19.5 6.5'/><circle cx='9.5' cy='14' r='1.3' fill='{{A}}' stroke='none'/><circle cx='14.5' cy='10' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "gadget-quick-peaks", AccentDomain::AmberGadget,
      "<rect x='5.5' y='5' width='13' height='14' rx='1.5'/>",
      "<rect x='5.5' y='5' width='13' height='14' rx='1.5' stroke-dasharray='3 2'/>",
      "<path d='M4.5 17.5c3 0 4.5-10 7.5-10s4.5 10 7.5 10'/><circle cx='12' cy='7.5' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "gadget-integrate", AccentDomain::AmberGadget,
      "<path d='M6.5 18.5v-5.5c2.5-5 5-6 11-1.5v7z'/>",
      "<path d='M4 18.5h16'/><path d='M6.5 5v13.5M17.5 5v13.5' stroke-dasharray='2.5 2'/>",
      "<path d='M5 14c3-6 6-7 14-2'/>" },

    { "gadget-diff", AccentDomain::AmberGadget,
      "<rect x='6' y='5' width='12' height='14' rx='1.5'/>",
      "<path d='M4.5 16.5c3.5-8 7.5-8 15-2'/>",
      "<path d='M6.5 15.5L17.5 6.5'/><circle cx='11.5' cy='11.4' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "gadget-intersect", AccentDomain::AmberGadget,
      "<circle cx='12' cy='12' r='6'/>",
      "<path d='M4.5 17.5L19.5 6.5M4.5 7.5L19.5 16.5'/>",
      "<circle cx='12' cy='12' r='2.8'/>" },

    { "gadget-fft", AccentDomain::AmberGadget,
      "<rect x='5.5' y='5' width='13' height='14' rx='1.5'/>",
      "<rect x='5.5' y='5' width='13' height='14' rx='1.5' stroke-dasharray='3 2'/>",
      "<path d='M8.5 17v-5M11.5 17V8M14.5 17v-7M17 17v-3'/>" },

    { "gadget-risetime", AccentDomain::AmberGadget,
      "<polygon points='7,17.5 13,7 19,7 19,17.5'/>",
      "<path d='M4 17.5h4l6-10.5h6'/>",
      "<path d='M8.8 15.5h5.5M11 7v10.5' stroke-dasharray='2 2'/>" },

    { "gadget-profile-2d", AccentDomain::AmberGadget,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/><circle cx='12' cy='12' r='3.8'/>",
      "<path d='M4.5 12h15M12 4.5v15'/>" },

    { "gadget-cluster", AccentDomain::AmberGadget,
      "<ellipse cx='12' cy='12' rx='7' ry='5.5'/>",
      "<ellipse cx='12' cy='12' rx='7.2' ry='5.6' stroke-dasharray='3 2'/>",
      "<circle cx='9.5' cy='11' r='1.5' fill='{{A}}' stroke='none'/><circle cx='13.5' cy='10' r='1.5' fill='{{A}}' stroke='none'/><circle cx='12' cy='13.8' r='1.5' fill='{{A}}' stroke='none'/>" },

    { "gadget-clear", AccentDomain::AmberGadget,
      "<rect x='5' y='5' width='14' height='14' rx='2'/>",
      "<rect x='5' y='5' width='14' height='14' rx='2' stroke-dasharray='3 2'/>",
      "<path d='M9 9l6 6M15 9l-6 6'/>" },

    // --- 6. Signal, Curve Math & Stats (36 IDs) ---
    { "signal-smooth", AccentDomain::EmeraldSignalStats,
      "<path d='M4 16c3-7 6-7 8-2s5 5 8-4v9H4z'/>",
      "<path d='M4 14l2-4 2 5 2-7 2 6 2-3 2 4 2-6 2 3' stroke-opacity='0.55'/>",
      "<path d='M4 15.5c3.2-7.5 6.2-7.5 8.2-2.2s4.6 4.8 7.8-4.3'/>" },

    { "signal-savgol", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 17c3-9 6.5-9 9.5-2s3.5 3 5.5-5v8.5h-15z'/>",
      "<circle cx='6' cy='14' r='1'/><circle cx='10' cy='8.5' r='1'/><circle cx='14.5' cy='14.5' r='1'/><circle cx='18.5' cy='9.5' r='1'/>",
      "<path d='M4.5 16.5c3-8.8 6.5-8.8 9.2-2s3.6 3.2 5.8-5.5'/>" },

    { "signal-fft", AccentDomain::EmeraldSignalStats,
      "<polygon points='5,19 9.5,19 9.5,6 12.5,6 12.5,19 19,19'/>",
      "<path d='M4 19h16'/>",
      "<path d='M7 19v-5M10.5 19V5.5M14 19v-8.5M17.5 19v-3.5'/>" },

    { "signal-filter", AccentDomain::EmeraldSignalStats,
      "<polygon points='4,5 20,5 14,12.5 14,19 10,17 10,12.5'/>",
      "<polygon points='4,5 20,5 14,12.5 14,19 10,17 10,12.5'/>",
      "<path d='M7.5 8.5h9'/>" },

    { "signal-iir", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 7.5h7c2.5 0 3.2 11 5.8 11h2.7v-11z'/>",
      "<path d='M4 4.5v14.5h16'/>",
      "<path d='M5 7.5h6.5c2.8 0 3.2 10.5 6.2 10.5h2.3'/>" },

    { "signal-stft", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/><path d='M4.5 9.5h15M4.5 14.5h15M9.5 4.5v15M14.5 4.5v15'/>",
      "<rect x='9.5' y='9.5' width='5' height='5' fill='{{A}}'/><rect x='4.5' y='14.5' width='5' height='5' fill='{{A}}'/>" },

    { "signal-wavelet", AccentDomain::EmeraldSignalStats,
      "<circle cx='12' cy='12' r='6.5'/>",
      "<path d='M3 12h2.5l1.5-2.5 2 5.5 3-10 3 10 2-5.5L18.5 12H21'/>",
      "<path d='M9 15l3-10 3 10'/>" },

    { "signal-psd", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 18.5V12l3.5-5.5 3.5 4.5 3-2.5 5 6v4z'/>",
      "<path d='M4 4.5v14.5h16'/>",
      "<path d='M5 13l3.2-6.5 3.3 4.8 3.2-2.8 4.8 6'/>" },

    { "signal-envelope", AccentDomain::EmeraldSignalStats,
      "<path d='M4 12c3.5-6.5 6.5-6.5 8-3s4.5-3.5 8 3c-3.5 6.5-6.5 6.5-8 3s-4.5 3.5-8-3z'/>",
      "<path d='M4.5 12l2-4 2 8 2-6 2 4 2-6 2 8 2-4'/>",
      "<path d='M4 11c3.5-6 6.5-6 8-2.5S16.5 5 20 11'/><path d='M4 13c3.5 6 6.5 6 8 2.5s4.5 3.5 8-2.5'/>" },

    { "signal-decimate", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='6' width='15' height='12' rx='2'/>",
      "<path d='M5 17V9M8.5 17V7M12 17v-6M15.5 17V8M19 17v-5'/>",
      "<circle cx='5' cy='9' r='1.5' fill='{{A}}' stroke='none'/><circle cx='12' cy='11' r='1.5' fill='{{A}}' stroke='none'/><circle cx='19' cy='12' r='1.5' fill='{{A}}' stroke='none'/>" },

    { "signal-fsd", AccentDomain::EmeraldSignalStats,
      "<path d='M5 18.5c2 0 2.8-12 4.2-12s2 12 3.3 12 1.6-10 3-10 1.8 10 3.5 10z'/>",
      "<path d='M3.5 18.5h17'/><path d='M4.5 18.5c2.8-9 5.5-9 7.5-6.5s4.7-2.5 7.5 6.5' stroke-dasharray='2 2'/>",
      "<path d='M5.5 18.5c1.6 0 2.3-12.5 3.7-12.5s1.8 12.5 3.1 12.5 1.5-10.5 2.9-10.5 1.7 10.5 3.3 10.5'/>" },

    { "matrix-fft2d", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/><path d='M12 4.5v15M4.5 12h15'/>",
      "<circle cx='12' cy='12' r='2.5' fill='{{A}}' stroke='none'/><circle cx='8' cy='12' r='1.1' fill='{{A}}' stroke='none'/><circle cx='16' cy='12' r='1.1' fill='{{A}}' stroke='none'/><circle cx='12' cy='8' r='1.1' fill='{{A}}' stroke='none'/><circle cx='12' cy='16' r='1.1' fill='{{A}}' stroke='none'/>" },

    { "matrix-filter2d", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/><path d='M9.5 4.5v15M14.5 4.5v15M4.5 9.5h15M4.5 14.5h15'/>",
      "<circle cx='12' cy='12' r='4.2'/>" },

    { "curvemath", AccentDomain::EmeraldSignalStats,
      "<rect x='4' y='4.5' width='16' height='15' rx='2.5'/>",
      "<path d='M4.5 16c3.5-5 6.5-5 9.5-1s3.5 2 5.5-3'/>",
      "<path d='M8 6v4M6 8h4M14.5 8h4'/>" },

    { "curve-average", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 10c4-4 8-4 15-1v7c-7-3-11-3-15 1z'/>",
      "<path d='M4.5 9.5c4.5-3.5 8.5-3.5 15-.8M4.5 16.5c4.5-3.5 8.5-3.5 15-.8' stroke-dasharray='2 2'/>",
      "<path d='M4.5 13c4.5-3.5 8.5-3.5 15-.8'/>" },

    { "curve-normalize", AccentDomain::EmeraldSignalStats,
      "<rect x='5' y='6' width='14' height='12' rx='1.5'/>",
      "<path d='M4.5 6h15M4.5 18h15' stroke-dasharray='2.5 2'/>",
      "<path d='M5 18c3 0 4.5-12 7-12s4 12 7 12'/>" },

    { "curve-fill", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 15c4-6 8-6 15-2v4c-6 2-10 2-15-2z'/>",
      "<path d='M4.5 15c4-6.5 8.5-6.5 15-2.5'/>",
      "<path d='M4.5 16.5c5 2.5 9.5 2.5 15-1'/><path d='M9.5 11.2v6M14 11.2v5.2'/>" },

    { "arrhenius", AccentDomain::EmeraldSignalStats,
      "<polygon points='5.5,6.5 19,17.5 5.5,17.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<path d='M6.5 6.5L18.5 17'/><circle cx='9' cy='8.7' r='1.3' fill='{{A}}' stroke='none'/><circle cx='14' cy='13.1' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "tauc", AccentDomain::EmeraldSignalStats,
      "<polygon points='11,18.5 18.5,6 18.5,18.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M5.5 18c4 0 5.8-.5 7.5-3.2L18.5 6'/>",
      "<path d='M11 18.8L18.5 6' stroke-dasharray='2.2 1.8'/><circle cx='11' cy='18.5' r='1.7' fill='{{A}}' stroke='none'/>" },

    { "eis-nyquist", AccentDomain::EmeraldSignalStats,
      "<path d='M5.5 17.5a5.5 5.5 0 0 1 11 0z'/>",
      "<path d='M4 4.5v14h16'/>",
      "<path d='M5.8 18.2a5.2 5.2 0 0 1 10.4 0l3.5-3.5'/>" },

    { "stats-anova", AccentDomain::EmeraldSignalStats,
      "<rect x='5.5' y='11' width='3.5' height='7.5'/><rect x='14.5' y='8.5' width='3.5' height='10'/>",
      "<path d='M4 18.5h16'/><rect x='5.8' y='11.5' width='3.4' height='7'/><rect x='14.8' y='9' width='3.4' height='9.5'/>",
      "<path d='M7.5 8.5V6h9v1.5M12 4.2v1.8'/>" },

    { "stats-twoway", AccentDomain::EmeraldSignalStats,
      "<polygon points='5.5,16 12,11 18.5,7 18.5,16 12,13'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M6 8.5L18.5 16'/>",
      "<path d='M6 16.5L18.5 7.5'/><circle cx='6' cy='16.5' r='1.4' fill='{{A}}' stroke='none'/><circle cx='18.5' cy='7.5' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "stats-nonparam", AccentDomain::EmeraldSignalStats,
      "<rect x='5.5' y='8' width='5' height='8' rx='1'/><rect x='13.5' y='6.5' width='5' height='8' rx='1'/>",
      "<rect x='5.5' y='8.5' width='5' height='7.5' rx='1'/><rect x='13.5' y='6.5' width='5' height='7.5' rx='1'/>",
      "<path d='M5.5 12.2h5M13.5 10.2h5M8 5.5v3M8 16v3M16 4v2.5M16 14v3.5'/>" },

    { "stats-normality", AccentDomain::EmeraldSignalStats,
      "<path d='M3.5 18.5c4 0 5-12 8.5-12s4.5 12 8.5 12z'/>",
      "<path d='M3 18.5h18'/>",
      "<path d='M3.5 18.5c4 0 5-12 8.5-12s4.5 12 8.5 12'/><path d='M12 6.5v12' stroke-dasharray='2 2'/>" },

    { "stats-outlier", AccentDomain::EmeraldSignalStats,
      "<circle cx='9.5' cy='14' r='4.5'/>",
      "<circle cx='7.5' cy='14.5' r='1.3'/><circle cx='10.5' cy='13' r='1.3'/><circle cx='9.5' cy='16' r='1.3'/>",
      "<circle cx='17.5' cy='6.5' r='2.5'/><path d='M17.5 11v2'/>" },

    { "stats-paired", AccentDomain::EmeraldSignalStats,
      "<polygon points='6.5,8 17.5,11 17.5,17 6.5,14'/>",
      "<path d='M6.5 5v14M17.5 5v14'/>",
      "<path d='M6.5 8.5l11 3M6.5 14l11 3'/><circle cx='6.5' cy='8.5' r='1.5' fill='{{A}}' stroke='none'/><circle cx='17.5' cy='11.5' r='1.5' fill='{{A}}' stroke='none'/>" },

    { "stats-bland-altman", AccentDomain::EmeraldSignalStats,
      "<rect x='5' y='7.5' width='14.5' height='9'/>",
      "<path d='M4.5 7.5h15M4.5 16.5h15' stroke-dasharray='2.5 2'/>",
      "<path d='M4.5 12h15'/><circle cx='8.5' cy='10.2' r='1.3' fill='{{A}}' stroke='none'/><circle cx='13' cy='13.8' r='1.3' fill='{{A}}' stroke='none'/><circle cx='16.8' cy='11.2' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "stats-bablok", AccentDomain::EmeraldSignalStats,
      "<polygon points='5,19 16,5 20,8 8,19'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M5.5 18.5L18.5 5.5' stroke-dasharray='2 2'/>",
      "<path d='M5.5 17L19.5 7'/><circle cx='11' cy='13' r='1.5' fill='{{A}}' stroke='none'/>" },

    { "stats-weibull", AccentDomain::EmeraldSignalStats,
      "<polygon points='5.5,18.5 18.5,5.5 18.5,11.5 10.5,18.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M4.5 9h15M4.5 14h15' stroke-opacity='0.45'/>",
      "<path d='M6 18L18.5 5.5'/><circle cx='9.5' cy='14.5' r='1.4' fill='{{A}}' stroke='none'/><circle cx='15' cy='9' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "stats-survival", AccentDomain::EmeraldSignalStats,
      "<polygon points='5,7 10,7 10,11 15,11 15,15.5 19.5,15.5 19.5,18.8 5,18.8'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<path d='M5 7h5v4h5v4.5h4.5'/><path d='M12.5 9.5v3'/>" },

    { "stats-roc", AccentDomain::EmeraldSignalStats,
      "<path d='M5 18.5C5.5 8.5 9.5 5.5 19 5.5L5 18.5z'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M5 18.5L19 5' stroke-dasharray='2 2'/>",
      "<path d='M5 18.5C5.5 8.2 9.5 5.2 19.2 5.2'/>" },

    { "stats-pca", AccentDomain::EmeraldSignalStats,
      "<ellipse cx='12' cy='12' rx='7' ry='4' transform='rotate(-30 12 12)'/>",
      "<ellipse cx='12' cy='12' rx='7.2' ry='4.2' transform='rotate(-30 12 12)'/>",
      "<path d='M12 12l5.8-3.4m0 0l-2.4-.3m2.4.3l-.8 2.3M12 12L9.5 7.7'/>" },

    { "stats-corr", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/><path d='M9.5 4.5v15M14.5 4.5v15M4.5 9.5h15M4.5 14.5h15'/>",
      "<rect x='4.5' y='4.5' width='5' height='5' fill='{{A}}'/><rect x='9.5' y='9.5' width='5' height='5' fill='{{A}}'/><rect x='14.5' y='14.5' width='5' height='5' fill='{{A}}'/>" },

    { "stats-cluster", AccentDomain::EmeraldSignalStats,
      "<circle cx='8' cy='9' r='4'/><circle cx='16' cy='15' r='4'/>",
      "<path d='M7 5.5v4h10v-4M12 9.5v4'/>",
      "<circle cx='7.5' cy='16' r='1.7' fill='{{A}}' stroke='none'/><circle cx='11.5' cy='17.5' r='1.7' fill='{{A}}' stroke='none'/><circle cx='17' cy='15.5' r='1.7' fill='{{A}}' stroke='none'/>" },

    { "stats-ellipse", AccentDomain::EmeraldSignalStats,
      "<ellipse cx='12' cy='12' rx='7.5' ry='4.5' transform='rotate(-25 12 12)'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<ellipse cx='12.5' cy='11.5' rx='6.5' ry='3.8' transform='rotate(-25 12.5 11.5)'/><circle cx='12.5' cy='11.5' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "digitizer", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/>",
      "<path d='M4.5 8V5.5a1 1 0 0 1 1-1H8M16 4.5h2.5a1 1 0 0 1 1 1V8M19.5 16v2.5a1 1 0 0 1-1 1H16M8 19.5H5.5a1 1 0 0 1-1-1V16'/>",
      "<path d='M7 15.5l3.5-4.5 3.2 2 3.8-4.5'/><circle cx='10.5' cy='11' r='1.6' fill='{{A}}' stroke='none'/><circle cx='17.5' cy='8.5' r='1.6' fill='{{A}}' stroke='none'/>" },

    // --- 7. Graph Page, Axes, Layers & Style (29 IDs) ---
    { "preset-isij", AccentDomain::OrangeStyleExport,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='1.2'/>",
      "<rect x='4.5' y='4.5' width='15' height='15'/><path d='M4.5 9.5h2M4.5 14.5h2M17.5 9.5h2M17.5 14.5h2M9.5 4.5v2M14.5 4.5v2M9.5 17.5v2M14.5 17.5v2'/>",
      "<path d='M7.2 16c2.5-5 5-7 9.5-8'/>" },

    { "preset-slide", AccentDomain::OrangeStyleExport,
      "<rect x='3' y='5.5' width='18' height='13' rx='2'/>",
      "<rect x='3' y='5.5' width='18' height='13' rx='2'/>",
      "<path d='M6.5 15l3.8-4.5 3.2 2.2 4-4.7' stroke-width='2.4'/>" },

    { "preset-nature", AccentDomain::OrangeStyleExport,
      "<polygon points='5,5 5,19 19,19'/>",
      "<path d='M5 5v14h14'/>",
      "<path d='M7.5 16c2.2-5.5 5.5-8 10.5-8.5'/><circle cx='12' cy='10.2' r='1.5' fill='{{A}}' stroke='none'/>" },

    { "format-copy", AccentDomain::OrangeStyleExport,
      "<rect x='6' y='4' width='12' height='6' rx='1.5'/>",
      "<rect x='6' y='4' width='12' height='6' rx='1.5'/><path d='M18 7h2v4h-8v3'/>",
      "<rect x='10.5' y='14' width='3' height='6.5' rx='1'/>" },

    { "format-paste", AccentDomain::OrangeStyleExport,
      "<rect x='5' y='6' width='14' height='14' rx='2'/>",
      "<path d='M9 5.5H6.5A1.5 1.5 0 0 0 5 7v11.5A1.5 1.5 0 0 0 6.5 20h11a1.5 1.5 0 0 0 1.5-1.5V7a1.5 1.5 0 0 0-1.5-1.5H15'/><rect x='9' y='3.5' width='6' height='3.2' rx='1'/>",
      "<path d='M8 15.5l2.8-3.8 2.4 1.8 3.2-4'/>" },

    { "batch-plot", AccentDomain::OrangeStyleExport,
      "<rect x='7.5' y='7.5' width='12.5' height='12' rx='1.8'/>",
      "<path d='M15.5 7.5V5.5A1.5 1.5 0 0 0 14 4H5.5A1.5 1.5 0 0 0 4 5.5V14a1.5 1.5 0 0 0 1.5 1.5h2'/><rect x='7.5' y='7.5' width='12.5' height='12' rx='1.8'/>",
      "<path d='M10 16.5l2.5-3.5 2.2 1.8 3-4'/>" },

    { "page-setup", AccentDomain::OrangeStyleExport,
      "<path d='M6 3.5h8l4 4v13H6z'/>",
      "<path d='M14 3.5H6a1 1 0 0 0-1 1v15a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V8.5z'/><path d='M14 3.5v5h5'/>",
      "<rect x='8' y='11' width='8' height='6' rx='0.8'/>" },

    { "fit-page", AccentDomain::OrangeStyleExport,
      "<rect x='6.5' y='6.5' width='11' height='11' rx='1.5'/>",
      "<rect x='3.5' y='3.5' width='17' height='17' rx='2'/>",
      "<path d='M7 10V7h3M17 10V7h-3M7 14v3h3M17 14v3h-3'/>" },

    { "layer-add", AccentDomain::BluePlot,
      "<polygon points='12,4.5 20,9 12,13.5 4,9'/>",
      "<polygon points='12,4.5 20,9 12,13.5 4,9'/><path d='M4 13.5l8 4.5 4-2.2'/>",
      "<path d='M18.5 14.5v5M16 17h5'/>" },

    { "layer-linked", AccentDomain::BluePlot,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='1.5'/>",
      "<path d='M4.5 6.5v13h13'/>",
      "<path d='M6.5 4.5h13v13'/><path d='M9.5 14.5l5-5'/>" },

    { "layer-inset", AccentDomain::BluePlot,
      "<rect x='11' y='5.5' width='7.5' height='6.5' rx='1'/>",
      "<rect x='4' y='4.5' width='16' height='15' rx='1.5'/><path d='M6 16.5l4-3.5 3.5 1.5 4.5-3'/>",
      "<rect x='11' y='6' width='7' height='6' rx='0.8'/><path d='M9.5 13.5L11 12'/>" },

    { "layer-arrange", AccentDomain::BluePlot,
      "<rect x='4' y='4.5' width='7' height='6.5' rx='1'/><rect x='13' y='13' width='7' height='6.5' rx='1'/>",
      "<rect x='4' y='4.5' width='7' height='6.5' rx='1'/><rect x='13' y='4.5' width='7' height='6.5' rx='1'/><rect x='4' y='13' width='7' height='6.5' rx='1'/>",
      "<rect x='13' y='13' width='7' height='6.5' rx='1'/>" },

    { "frame-box-inward", AccentDomain::BluePlot,
      "<rect x='4.5' y='4.5' width='15' height='15'/>",
      "<rect x='4.5' y='4.5' width='15' height='15'/>",
      "<path d='M4.5 9.5H7M4.5 14.5H7M19.5 9.5H17M19.5 14.5H17M9.5 4.5V7M14.5 4.5V7M9.5 19.5V17M14.5 19.5V17'/>" },

    { "frame-box-plain", AccentDomain::BluePlot,
      "<rect x='4.5' y='4.5' width='15' height='15'/>",
      "<rect x='4.5' y='4.5' width='15' height='15'/>",
      "<path d='M4.5 9.5H3M4.5 14.5H3M9.5 19.5V21M14.5 19.5V21'/>" },

    { "frame-open-l", AccentDomain::BluePlot,
      "<polygon points='5,4.5 5,19.5 19.5,19.5'/>",
      "<path d='M5 4.5v15h14.5'/>",
      "<path d='M5 9.5H3.2M5 14.5H3.2M10 19.5v1.8M15 19.5v1.8'/>" },

    { "frame-zero", AccentDomain::BluePlot,
      "<circle cx='12' cy='12' r='6.5'/>",
      "<path d='M3.5 12h17M12 3.5v17'/>",
      "<path d='M18 9.8L20.5 12 18 14.2M9.8 6L12 3.5 14.2 6'/>" },

    { "axis-scale", AccentDomain::BluePlot,
      "<rect x='4.5' y='9' width='15' height='6' rx='1.5'/>",
      "<path d='M3.5 15h17M6 15v-3.5M10 15v-2M14 15v-3.5M18 15v-2'/>",
      "<path d='M5 8.5h14M7.5 6.5L5 8.5l2.5 2M16.5 6.5l2.5 2-2.5 2'/>" },

    { "axis-log", AccentDomain::BluePlot,
      "<rect x='4.5' y='5' width='15' height='14' rx='2'/>",
      "<path d='M4 18h16'/>",
      "<path d='M5.5 18v-5M8.5 18v-3.5M10.5 18v-3M12 18v-2.5M13.2 18v-2.5M18.5 18v-6'/><path d='M5.5 11C8 6.5 12.5 5.5 18.5 5.5'/>" },

    { "axis-ticks-in", AccentDomain::BluePlot,
      "<rect x='5' y='5' width='14' height='14' rx='1.5'/>",
      "<path d='M5 4.5v15h14.5'/>",
      "<path d='M5 8.5h3.2M5 13h3.2M9.5 19.5v-3.2M14 19.5v-3.2'/>" },

    { "axis-grid", AccentDomain::BluePlot,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='1.5'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='1.5'/>",
      "<path d='M9.5 4.5v15M14.5 4.5v15M4.5 9.5h15M4.5 14.5h15' stroke-dasharray='2 2'/>" },

    { "color-palette", AccentDomain::OrangeStyleExport,
      "<path d='M12 3.5a8.5 8.5 0 1 0 0 17c1.4 0 2.5-1 2.5-2.3 0-.6-.2-1.1-.6-1.5-.4-.4-.5-.9-.5-1.4 0-1.3 1-2.3 2.3-2.3H18a2.5 2.5 0 0 0 2.5-2.5A7 7 0 0 0 12 3.5z'/>",
      "<path d='M12 3.5a8.5 8.5 0 1 0 0 17c1.4 0 2.5-1 2.5-2.3 0-.6-.2-1.1-.6-1.5-.4-.4-.5-.9-.5-1.4 0-1.3 1-2.3 2.3-2.3H18a2.5 2.5 0 0 0 2.5-2.5A7 7 0 0 0 12 3.5z'/>",
      "<circle cx='8' cy='10.5' r='1.3' fill='{{A}}' stroke='none'/><circle cx='11' cy='7.5' r='1.3' fill='{{A}}' stroke='none'/><circle cx='15' cy='8.5' r='1.3' fill='{{A}}' stroke='none'/><circle cx='8.5' cy='14.5' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "legend", AccentDomain::BluePlot,
      "<rect x='4.5' y='5' width='15' height='14' rx='2'/>",
      "<rect x='4.5' y='5' width='15' height='14' rx='2'/><path d='M11.5 9.5h5.5M11.5 14.5h5.5'/>",
      "<path d='M7 9.5h2.8M7 14.5h2.8'/><circle cx='8.4' cy='9.5' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "text-label", AccentDomain::BluePlot,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2.5'/>",
      "<path d='M6 6.5h12M12 6.5v11.5M9.5 18h5'/>",
      "<path d='M6 6.5V8.5M18 6.5V8.5'/>" },

    { "line-arrow", AccentDomain::BluePlot,
      "<circle cx='12' cy='12' r='6.5'/>",
      "<path d='M5 19L19 5'/>",
      "<path d='M13 5h6v6'/><circle cx='5' cy='19' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "font-inc", AccentDomain::BluePlot,
      "<rect x='3.5' y='5' width='12' height='14' rx='2'/>",
      "<path d='M5 18l4.5-11.5L14 18M6.5 14.5h6'/>",
      "<path d='M18 8v5M15.5 10.5h5'/>" },

    { "font-dec", AccentDomain::BluePlot,
      "<rect x='3.5' y='5' width='12' height='14' rx='2'/>",
      "<path d='M5 18l4.5-11.5L14 18M6.5 14.5h6'/>",
      "<path d='M15.5 10.5h5'/>" },

    { "width-inc", AccentDomain::BluePlot,
      "<rect x='4' y='5' width='11' height='14' rx='2'/>",
      "<path d='M4.5 7.5h9.5' stroke-width='1.2'/><path d='M4.5 12h9.5' stroke-width='2.0'/><path d='M4.5 16.8h9.5' stroke-width='3.0'/>",
      "<path d='M18 9.5v5M15.5 12h5'/>" },

    { "width-dec", AccentDomain::BluePlot,
      "<rect x='4' y='5' width='11' height='14' rx='2'/>",
      "<path d='M4.5 7.5h9.5' stroke-width='1.2'/><path d='M4.5 12h9.5' stroke-width='2.0'/><path d='M4.5 16.8h9.5' stroke-width='3.0'/>",
      "<path d='M15.5 12h5'/>" },

    { "error-bars", AccentDomain::BluePlot,
      "<rect x='6' y='5.5' width='12' height='13' rx='2'/>",
      "<path d='M8.5 6.5v11M6.5 6.5h4M6.5 17.5h4M15.5 5v9.5M13.5 5h4M13.5 14.5h4'/>",
      "<circle cx='8.5' cy='12' r='2' fill='{{A}}' stroke='none'/><circle cx='15.5' cy='9.8' r='2' fill='{{A}}' stroke='none'/><path d='M8.5 12l7-2.2'/>" },

    // --- 8. Bonus Scientific, Mini-Toolbar, 3D & Navigation Icons (18 IDs -> 195 Total) ---
    { "symbol-inc", AccentDomain::BluePlot,
      "<circle cx='9.5' cy='12' r='5'/>",
      "<circle cx='9.5' cy='12' r='4.8'/>",
      "<path d='M18 9.5v5M15.5 12h5'/><circle cx='9.5' cy='12' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "symbol-dec", AccentDomain::BluePlot,
      "<circle cx='9.5' cy='12' r='5'/>",
      "<circle cx='9.5' cy='12' r='4.8'/>",
      "<path d='M15.5 12h5'/><circle cx='9.5' cy='12' r='1.6' fill='{{A}}' stroke='none'/>" },

    { "aspect-square", AccentDomain::BluePlot,
      "<rect x='5' y='5' width='14' height='14' rx='1.5'/>",
      "<rect x='5' y='5' width='14' height='14' rx='1.5'/>",
      "<path d='M5 19L19 5' stroke-dasharray='2.5 2'/>" },

    { "mean-line", AccentDomain::BluePlot,
      "<rect x='4.5' y='8.5' width='15' height='7' rx='1.5'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M6 16l3.5-6 3.5 4.5 3-5.5 3 3'/>",
      "<path d='M5 12h14.5' stroke-dasharray='3 2'/>" },

    { "signal-lowess", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 16.5c4-7 7.5-7 15-3v4.5h-15z'/>",
      "<circle cx='6.5' cy='15.5' r='1'/><circle cx='10' cy='10.5' r='1'/><circle cx='14' cy='13.5' r='1'/><circle cx='18' cy='11' r='1'/>",
      "<path d='M4.5 16.5c4-6.8 7.8-6.8 15-3.2'/>" },

    { "signal-whittaker", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 17c3.5-8 8-9 15-4.5v6h-15z'/>",
      "<path d='M4.5 15l2.5-3.5 2.5 3 2.5-5 2.5 3.5 2.5-3 2.5 2' stroke-opacity='0.55'/>",
      "<path d='M4.5 16.5c3.5-7.8 8-8.5 15-4.2'/>" },

    { "plot-droplines", AccentDomain::BluePlot,
      "<rect x='5.5' y='8' width='13' height='11' rx='1'/>",
      "<path d='M4 4.5v14.5h16'/><path d='M7.5 19v-7M12 19V8M16.5 19v-9'/>",
      "<circle cx='7.5' cy='12' r='1.7' fill='{{A}}' stroke='none'/><circle cx='12' cy='8' r='1.7' fill='{{A}}' stroke='none'/><circle cx='16.5' cy='10' r='1.7' fill='{{A}}' stroke='none'/>" },

    { "plot-steps", AccentDomain::BluePlot,
      "<polygon points='5,19 5,14.5 9.5,14.5 9.5,10 14.5,10 14.5,6.5 19.5,6.5 19.5,19'/>",
      "<path d='M4.5 4.5v14.5h15.5'/>",
      "<path d='M5 14.5h4.5V10h5V6.5h5'/>" },

    { "function-plot", AccentDomain::BluePlot,
      "<path d='M4.5 12c3-7 5-7 7.5 0s4.5 7 7.5 0z'/>",
      "<path d='M4 12h16M12 4v16' stroke-opacity='0.5'/>",
      "<path d='M4.5 12c2.8-7 5-7 7.5 0s4.7 7 7.5 0'/>" },

    { "lock", AccentDomain::IndigoWorkspace,
      "<rect x='6' y='10.5' width='12' height='9.5' rx='2'/>",
      "<rect x='6' y='10.5' width='12' height='9.5' rx='2'/><path d='M8.5 10.5V7.5a3.5 3.5 0 0 1 7 0v3'/>",
      "<circle cx='12' cy='15' r='1.5' fill='{{A}}' stroke='none'/><path d='M12 15.5v2'/>" },

    { "unlock", AccentDomain::IndigoWorkspace,
      "<rect x='6' y='10.5' width='12' height='9.5' rx='2'/>",
      "<rect x='6' y='10.5' width='12' height='9.5' rx='2'/><path d='M8.5 10.5V7.5a3.5 3.5 0 0 1 6.7-1.2'/>",
      "<circle cx='12' cy='15' r='1.5' fill='{{A}}' stroke='none'/>" },

    { "duplicate", AccentDomain::IndigoWorkspace,
      "<rect x='8.5' y='8.5' width='11.5' height='11.5' rx='2'/>",
      "<path d='M15.5 8.5V6A1.5 1.5 0 0 0 14 4.5H6A1.5 1.5 0 0 0 4.5 6v8A1.5 1.5 0 0 0 6 15.5h2.5'/>",
      "<rect x='8.5' y='8.5' width='11.5' height='11.5' rx='2'/><path d='M14.2 11.8v5M11.8 14.2h5'/>" },

    { "close", AccentDomain::CrimsonPeakFit,
      "<circle cx='12' cy='12' r='8.5'/>",
      "<circle cx='12' cy='12' r='8.5'/>",
      "<path d='M9 9l6 6M15 9l-6 6'/>" },

    { "go-next", AccentDomain::IndigoWorkspace,
      "<circle cx='12' cy='12' r='8.5'/>",
      "<circle cx='12' cy='12' r='8.5'/>",
      "<path d='M8 12h8m0 0l-3.2-3.2M16 12l-3.2 3.2'/>" },

    { "go-previous", AccentDomain::IndigoWorkspace,
      "<circle cx='12' cy='12' r='8.5'/>",
      "<circle cx='12' cy='12' r='8.5'/>",
      "<path d='M16 12H8m0 0l3.2-3.2M8 12l3.2 3.2'/>" },

    { "python-stop", AccentDomain::CrimsonPeakFit,
      "<circle cx='12' cy='12' r='8.5'/>",
      "<circle cx='12' cy='12' r='8.8'/>",
      "<rect x='9' y='9' width='6' height='6' rx='1' fill='{{A}}'/>" },

    { "download-install", AccentDomain::OrangeStyleExport,
      "<rect x='5' y='14' width='14' height='6' rx='1.5'/>",
      "<path d='M4.5 15.5v3A1.5 1.5 0 0 0 6 20h12a1.5 1.5 0 0 0 1.5-1.5v-3'/>",
      "<path d='M12 4v10.5m0 0L8.5 11M12 14.5l3.5-3.5'/>" },

    { "classic-toolbars", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='17' height='5' rx='1.2'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2'/><path d='M3.5 9.5h17'/>",
      "<circle cx='7' cy='7' r='1' fill='{{A}}' stroke='none'/><circle cx='10.5' cy='7' r='1' fill='{{A}}' stroke='none'/><circle cx='14' cy='7' r='1' fill='{{A}}' stroke='none'/>" },

    // --- 9. Workspace Layouts, Delete/Trash, Folders, Text Format & Curve/Table Helpers (18 IDs -> 213 Total) ---
    { "delete", AccentDomain::CrimsonPeakFit,
      "<path d='M6.5 7.5h11l-.9 11.5a1.5 1.5 0 0 1-1.5 1.4H8.9a1.5 1.5 0 0 1-1.5-1.4z'/>",
      "<path d='M4.5 7h15M6.5 7l.9 12a1.5 1.5 0 0 0 1.5 1.4h6.2a1.5 1.5 0 0 0 1.5-1.4L17.5 7'/>",
      "<path d='M9.2 7V4.8a1 1 0 0 1 1-1h3.6a1 1 0 0 1 1 1V7M10 10.5v6M14 10.5v6'/>" },

    { "layout-tabs", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='8.5' width='17' height='11' rx='1.8'/>",
      "<path d='M3.5 8.5h17v9.2a1.8 1.8 0 0 1-1.8 1.8H5.3a1.8 1.8 0 0 1-1.8-1.8z'/><path d='M10.5 8.5V6a1.2 1.2 0 0 1 1.2-1.2h3.6A1.2 1.2 0 0 1 16.5 6v2.5'/>",
      "<path d='M3.5 8.5V6a1.2 1.2 0 0 1 1.2-1.2h4.6A1.2 1.2 0 0 1 10.5 6v2.5'/>" },

    { "layout-split", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='7.5' height='15' rx='1.5'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2'/>",
      "<path d='M11 4.5v15M5.5 9h3.5M5.5 12.5h3.5M13 15.5l2.2-3.5 2 2 1.8-3.5'/>" },

    { "layout-mdi", AccentDomain::IndigoWorkspace,
      "<rect x='8.5' y='8.5' width='12' height='10.5' rx='1.6'/>",
      "<path d='M8.5 15H5A1.5 1.5 0 0 1 3.5 13.5v-7A1.5 1.5 0 0 1 5 5h9.5A1.5 1.5 0 0 1 16 6.5v2'/>",
      "<rect x='8.5' y='8.5' width='12' height='10.5' rx='1.6'/><path d='M8.5 11.8h12'/>" },

    { "folder-open", AccentDomain::IndigoWorkspace,
      "<path d='M3.5 10.5h16.8l-2.1 8.5H5.2z'/>",
      "<path d='M3.5 18.5V6.5a1.5 1.5 0 0 1 1.5-1.5h4.2l2 2.2H18a1.5 1.5 0 0 1 1.5 1.5v1.8'/>",
      "<path d='M3.5 19l2.1-8.2h15.2L18.7 19z'/>" },

    { "folder-closed", AccentDomain::IndigoWorkspace,
      "<path d='M3.5 7.5h17v11a1.5 1.5 0 0 1-1.5 1.5H5a1.5 1.5 0 0 1-1.5-1.5z'/>",
      "<path d='M3.5 6.5A1.5 1.5 0 0 1 5 5h4.5l2 2.3H19A1.5 1.5 0 0 1 20.5 8.8v9.7A1.5 1.5 0 0 1 19 20H5a1.5 1.5 0 0 1-1.5-1.5z'/>",
      "<path d='M3.5 10.2h17'/>" },

    { "text-subscript", AccentDomain::BluePlot,
      "<rect x='3.5' y='5' width='11' height='12' rx='2'/>",
      "<path d='M4.5 6.5l8 10M12.5 6.5l-8 10'/>",
      "<path d='M15.2 14.5c0-1.2 1-2 2.3-2 1.2 0 2.1.7 2.1 1.7 0 .8-.5 1.4-1.4 2.1L15.2 19h4.6'/>" },

    { "text-superscript", AccentDomain::BluePlot,
      "<rect x='3.5' y='7' width='11' height='12' rx='2'/>",
      "<path d='M4.5 8.5l8 10M12.5 8.5l-8 10'/>",
      "<path d='M15.2 6.5c0-1.2 1-2 2.3-2 1.2 0 2.1.7 2.1 1.7 0 .8-.5 1.4-1.4 2.1L15.2 11h4.6'/>" },

    { "text-bold", AccentDomain::BluePlot,
      "<path d='M7 5h6.2a3.3 3.3 0 0 1 0 6.6H7zm0 6.6h7a3.6 3.6 0 0 1 0 7.4H7z'/>",
      "<path d='M7 5v14M7 5h6.2a3.3 3.3 0 0 1 0 6.6H7m6.2 0H14a3.6 3.6 0 0 1 0 7.4H7'/>",
      "<path d='M9.5 5v14'/>" },

    { "text-italic", AccentDomain::BluePlot,
      "<polygon points='10,5 17,5 14,19 7,19'/>",
      "<path d='M10 5h7M7 19h7'/>",
      "<path d='M13.5 5l-3 14'/>" },

    { "text-underline", AccentDomain::BluePlot,
      "<path d='M7 5v6.5a5 5 0 0 0 10 0V5z'/>",
      "<path d='M7 5v6.5a5 5 0 0 0 10 0V5'/>",
      "<path d='M5.5 19.5h13'/>" },

    { "text-greek", AccentDomain::BluePlot,
      "<circle cx='11' cy='12.5' r='5.5'/>",
      "<path d='M17.5 7.5c-1.5 3-3.2 9.5-6.8 9.5-2.8 0-4.7-2.2-4.7-4.8s1.9-4.7 4.7-4.7c3.6 0 5.1 6.8 7.3 9.5'/>",
      "<path d='M15.5 5h4M17.5 5v2'/>" },

    { "curve-add", AccentDomain::BluePlot,
      "<circle cx='17' cy='16.5' r='4.5'/>",
      "<path d='M4 5v14h7.5'/><path d='M5.5 15.5c2.5-6.5 6-7.5 12-6'/>",
      "<path d='M17 14v5M14.5 16.5h5'/>" },

    { "sample-gallery", AccentDomain::IndigoWorkspace,
      "<rect x='3.5' y='4.5' width='7.5' height='7' rx='1.5'/><rect x='13' y='12.5' width='7.5' height='7' rx='1.5'/>",
      "<rect x='3.5' y='4.5' width='7.5' height='7' rx='1.5'/><rect x='13' y='4.5' width='7.5' height='7' rx='1.5'/><rect x='3.5' y='12.5' width='7.5' height='7' rx='1.5'/><rect x='13' y='12.5' width='7.5' height='7' rx='1.5'/>",
      "<path d='M5 9.8l2-2.8 1.5 1.5 1.3-2M14.5 17.8l2-2.8 1.5 1.6 1.4-2.2'/>" },

    { "kissinger-tds", AccentDomain::EmeraldSignalStats,
      "<path d='M4.5 18.5c2.5 0 4-9.5 6.8-9.5s3.8 9.5 8.2 9.5z'/>",
      "<path d='M4 4.5v14.5h16'/><path d='M5 18.5c2.4 0 3.9-9.5 6.5-9.5s3.7 9.5 8 9.5'/>",
      "<path d='M6.5 6.5L18.5 16' stroke-dasharray='2.2 1.8'/><circle cx='9.5' cy='9' r='1.4' fill='{{A}}' stroke='none'/><circle cx='14.5' cy='12.8' r='1.4' fill='{{A}}' stroke='none'/>" },

    { "work-hardening", AccentDomain::EmeraldSignalStats,
      "<path d='M5 6.5L18.5 17.5H5z'/>",
      "<path d='M4.5 4.5v14.5h15.5'/><path d='M5.5 17.5c1.8-6.2 5.8-8.8 13.5-9.5'/>",
      "<path d='M5.5 6.5L17.5 18'/><circle cx='11.8' cy='12.4' r='1.8' fill='{{A}}' stroke='none'/>" },

    { "xrd-peaks", AccentDomain::CrimsonPeakFit,
      "<polygon points='5,18.5 8.2,18.5 9.5,6 10.8,18.5 14.2,18.5 15.3,10 16.4,18.5 19.5,18.5'/>",
      "<path d='M4 4.5v14.5h16'/>",
      "<path d='M5 18.5h3.2L9.5 5.5l1.3 13h3.4l1.1-8.5 1.1 8.5h3.1'/>" },

    { "vasp-bands", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='9.5' width='15' height='5' rx='1'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='1.5'/><path d='M9.5 4.5v15M14.5 4.5v15' stroke-opacity='0.45'/>",
      "<path d='M4.5 12h15' stroke-dasharray='2.2 1.8'/><path d='M4.5 8.5c2.5-2.5 5 1.5 10-1.5 2-.9 3.5-.5 5 1M4.5 16c2.5 2 5-1.8 10 1.2 2 .8 3.5.5 5-.8'/>" },

    { "code-export", AccentDomain::OrangeStyleExport,
      "<rect x='3.5' y='4.5' width='14.5' height='13' rx='2'/>",
      "<path d='M18 11V6.5A2 2 0 0 0 16 4.5H5.5A2 2 0 0 0 3.5 6.5v9A2 2 0 0 0 5.5 17.5H12'/><path d='M7.8 8.8L5.6 11l2.2 2.2M11.8 8.8l2.2 2.2-2.2 2.2'/>",
      "<path d='M14.5 16.5h6m0 0l-2.5-2.5M20.5 16.5l-2.5 2.5'/>" },

    { "curve-remove", AccentDomain::CrimsonPeakFit,
      "<circle cx='17' cy='16.5' r='4.5'/>",
      "<path d='M4 5v14h7.5'/><path d='M5.5 15.5c2.5-6.5 6-7.5 12-6'/>",
      "<path d='M14.5 16.5h5'/>" },

    { "row-numbers", AccentDomain::IndigoWorkspace,
      "<rect x='11' y='5' width='9' height='14' rx='1.5'/>",
      "<path d='M11.5 7.5h8M11.5 12h8M11.5 16.5h8'/>",
      "<path d='M5.5 6v4M4.5 10h2M4.5 13.2h2.6l-2.6 2.8h2.8'/>" },

    { "random-numbers", AccentDomain::EmeraldSignalStats,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='3'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='3'/>",
      "<circle cx='8.5' cy='8.5' r='1.3' fill='{{A}}' stroke='none'/><circle cx='12' cy='12' r='1.3' fill='{{A}}' stroke='none'/><circle cx='15.5' cy='8.5' r='1.3' fill='{{A}}' stroke='none'/><circle cx='8.5' cy='15.5' r='1.3' fill='{{A}}' stroke='none'/><circle cx='15.5' cy='15.5' r='1.3' fill='{{A}}' stroke='none'/>" },

    { "table-dimensions", AccentDomain::IndigoWorkspace,
      "<rect x='4.5' y='4.5' width='11.5' height='11.5' rx='1.5'/>",
      "<rect x='4.5' y='4.5' width='11.5' height='11.5' rx='1.5'/><path d='M4.5 10.2h11.5M10.2 4.5v11.5'/>",
      "<path d='M19.5 8v11.5H8M17 10.5L19.5 8M10.5 17L8 19.5'/>" },

    { "matrix-transpose", AccentDomain::IndigoWorkspace,
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/>",
      "<rect x='4.5' y='4.5' width='15' height='15' rx='2'/><path d='M5.5 5.5l13 13' stroke-dasharray='2 2'/>",
      "<path d='M14 7.5h3v3M10 16.5H7v-3M16.8 7.7L13 11.5M7.2 16.3L11 12.5'/>" },

    // Show / hide toggle (graph object manager) and the inspector dock toggle.
    { "visibility", AccentDomain::IndigoWorkspace,
      "<path d='M2.8 12c2.3-4.2 5.4-6.3 9.2-6.3s6.9 2.1 9.2 6.3c-2.3 4.2-5.4 6.3-9.2 6.3S5.1 16.2 2.8 12z'/>",
      "<path d='M2.8 12c2.3-4.2 5.4-6.3 9.2-6.3s6.9 2.1 9.2 6.3c-2.3 4.2-5.4 6.3-9.2 6.3S5.1 16.2 2.8 12z'/>",
      "<circle cx='12' cy='12' r='2.8'/>" },

    { "inspector-panel", AccentDomain::IndigoWorkspace,
      "<rect x='14' y='4.5' width='6' height='15' rx='1'/>",
      "<rect x='3.5' y='4.5' width='17' height='15' rx='2'/><path d='M14 4.5v15'/>",
      "<path d='M16 8.5h2.5M16 12h2.5M16 15.5h2.5'/>" }
};

struct AliasPair {
    const char *alias;
    const char *canonical;
};

static const AliasPair kAliasTable[] = {
    // Freedesktop & IconLoader::load legacy names
    { "edit-undo", "undo" },
    { "edit-redo", "redo" },
    { "edit-cut", "cut" },
    { "edit-copy", "copy" },
    { "edit-paste", "paste" },
    { "edit-clear", "clear" },
    { "edit-delete", "delete" },
    { "edit-find", "find" },
    { "edit-select-all", "table-drawer" },
    { "document-new", "file-new" },
    { "document-open", "file-open" },
    { "document-save", "file-save" },
    { "document-save-as", "file-save-as" },
    { "document-print", "print" },
    { "document-export", "export-image" },
    { "zoom-original", "zoom-auto" },
    { "zoom-fit-best", "zoom-auto" },
    { "list-add", "add-column" },
    { "list-remove", "delete" },
    { "go-up", "sort-table" },
    { "go-down", "sort-table" },
    { "applications-utilities", "settings" },
    { "preferences-system", "settings" },
    { "help-contents", "help" },
    { "help-about", "help" },
    { "view-refresh", "recalc-now" },
    { "media-playback-start", "python-run" },
    { "media-playback-stop", "python-stop" },
    { "system-software-update", "update-check" },
    { "text-x-python", "python-console" },
    { "folder", "folder-closed" },
    { "folder-new", "new-folder" },
    { "open-folder", "folder-open" },
    { "closed-folder", "folder-closed" },

    // Legacy SciDAVis .xpm / .png basenames. The raster files themselves were removed from
    // icons.qrc (H8); the names stay mapped so old call sites and saved states still resolve.
    { "new", "file-new" },
    { "fileopen", "file-open" },
    { "filesave", "file-save" },
    { "fileprint", "print" },
    { "pdf", "export-pdf" },
    { "import", "import-csv" },
    { "multiload", "import-csv" },
    { "open-template", "file-open" },
    { "save-template", "file-save-as" },
    { "table", "new-table" },
    { "worksheet", "new-table" },
    { "matrix", "new-matrix" },
    { "graph", "new-graph" },
    { "graph-disabled", "new-graph" },
    { "note", "new-note" },
    { "newfolder", "new-folder" },
    { "new-aspect", "file-new" },
    { "newf", "function-plot" },
    { "newfxy", "plot-3d-surface" },
    { "fx", "formula" },
    { "formula-editor", "formula" },
    { "recalculate", "recalc-now" },
    { "addcol", "add-column" },
    { "add-columns", "add-column" },
    { "insert-column", "add-column" },
    { "remove-column", "delete" },
    { "add-rows", "new-table" },
    { "insert-row", "new-table" },
    { "remove-row", "delete" },
    { "clear-table", "clear" },
    { "clear-row", "clear" },
    { "clear-column", "clear" },
    { "sort", "sort-table" },
    { "col-stat", "col-stats" },
    { "stat-rows", "row-stats" },
    { "lplot", "plot-line" },
    { "pplot", "plot-scatter" },
    { "lpplot", "plot-line-symbol" },
    { "linesymbol", "plot-line-symbol" },
    { "spline", "plot-spline" },
    { "droplines", "plot-droplines" },
    { "hor-steps", "plot-steps" },
    { "vert-steps", "plot-steps" },
    { "vertbars", "plot-bars" },
    { "plot-bars", "plot-bars" },
    { "bars", "plot-3d-bars" },
    { "hbars", "plot-hbar" },
    { "area", "plot-area" },
    { "pie", "plot-pie" },
    { "boxplot", "plot-box" },
    { "histogram", "plot-histogram" },
    { "stacked-hist", "plot-histogram" },
    { "stacked", "plot-waterfall" },
    { "panel-v2", "layer-arrange" },
    { "panel-h2", "layer-arrange" },
    { "panel-4", "layer-arrange" },
    { "vectxyxy", "plot-vectors" },
    { "vectxyam", "plot-vectors" },
    { "color-map", "plot-heatmap" },
    { "contour-map", "plot-contour" },
    { "gray-map", "plot-heatmap" },
    { "linemesh", "plot-3d-wireframe" },
    { "grid-poly", "plot-3d-surface" },
    { "grid-only", "plot-3d-wireframe" },
    { "no-grid", "plot-3d-surface" },
    { "pointsmesh", "plot-3d-scatter" },
    { "scatter", "plot-3d-scatter" },
    { "ribbon", "plot-3d-ribbon" },
    { "trajectory", "plot-3d-ribbon" },
    { "cones", "plot-3d-bars" },
    { "crosses", "plot-3d-scatter" },
    { "3dplots", "plot-3d-surface" },
    { "perspective", "plot-3d-wireframe" },
    { "reset-rotation", "recalc-now" },
    { "fit-frame", "fit-page" },
    { "movie", "python-run" },
    { "floor", "plot-heatmap" },
    { "isolines", "plot-contour" },
    { "no-floor", "frame-box-plain" },
    { "box", "frame-box-plain" },
    { "free-axes", "frame-open-l" },
    { "no-axes", "clear" },
    { "frontgrid", "axis-grid" },
    { "backgrid", "axis-grid" },
    { "leftgrid", "axis-grid" },
    { "rightgrid", "axis-grid" },
    { "ceilgrid", "axis-grid" },
    { "floorgrid", "axis-grid" },
    { "zoom", "zoom-in" },
    { "zoomout", "zoom-out" },
    { "unzoom", "zoom-auto" },
    { "lens", "zoom-in" },
    { "cursor", "data-reader" },
    { "cursor-16", "screen-reader" },
    { "select", "data-reader" },
    { "cursors", "gadget-delta" },
    { "vizor", "range-selector" },
    { "hand", "move-points" },
    { "gomme", "remove-points" },
    { "erase", "clear" },
    { "mask", "mask-points" },
    { "unmask", "unmask-points" },
    { "wizard", "peak-analyzer" },
    { "fit", "fit-nonlinear" },
    { "curves", "plot-line-symbol" },
    { "config-curves", "plot-line-symbol" },
    { "configtable", "new-table" },
    { "general", "settings" },
    { "newlayer", "layer-add" },
    { "arrangelayers", "layer-arrange" },
    { "auto-layout", "layer-arrange" },
    { "layer-enabled", "layer-add" },
    { "layer-disabled", "layer-add" },
    { "resize", "fit-page" },
    { "text", "text-label" },
    { "arrow", "line-arrow" },
    { "xerror", "error-bars" },
    { "yerror", "error-bars" },
    { "x-error", "error-bars" },
    { "y-error", "error-bars" },
    { "palette", "color-palette" },
    { "pixelprofile", "gadget-profile-2d" },
    { "monalisa", "export-image" },
    { "clock", "log-window" },
    { "log", "log-window" },
    { "python", "python-console" },
    { "quit", "close" },
    { "prev", "go-previous" },
    { "next", "go-next" },

    // Common Ribbon / HUD / MiniToolbar / Updater / Python Console semantic aliases
    { "roi-stats", "gadget-roi" },
    { "region-stats", "gadget-roi" },
    { "vertical-cursor", "gadget-cursor" },
    { "delta-cursor", "gadget-delta" },
    { "image-profile", "gadget-profile-2d" },
    { "profile-2d", "gadget-profile-2d" },
    { "2d-profile", "gadget-profile-2d" },
    { "quick-fit", "gadget-quick-fit" },
    { "quick-peaks", "gadget-quick-peaks" },
    { "integrate", "gadget-integrate" },
    { "differentiate", "gadget-diff" },
    { "intersect", "gadget-intersect" },
    { "rise-time", "gadget-risetime" },
    { "mask-roi", "mask-points" },
    { "unmask-roi", "unmask-points" },
    { "savgol", "signal-savgol" },
    { "savitzky-golay", "signal-savgol" },
    { "lowess", "signal-lowess" },
    { "whittaker", "signal-whittaker" },
    { "fft", "signal-fft" },
    { "iir", "signal-iir" },
    { "iir-filter", "signal-iir" },
    { "stft", "signal-stft" },
    { "wavelet", "signal-wavelet" },
    { "welch-psd", "signal-psd" },
    { "psd", "signal-psd" },
    { "envelope", "signal-envelope" },
    { "decimate", "signal-decimate" },
    { "fsd", "signal-fsd" },
    { "fft2d", "matrix-fft2d" },
    { "filter2d", "matrix-filter2d" },
    { "matrix-2d-filter", "matrix-filter2d" },
    { "fill-between", "curve-fill" },
    { "normalize", "curve-normalize" },
    { "eis", "eis-nyquist" },
    { "anova", "stats-anova" },
    { "twoway-anova", "stats-twoway" },
    { "nonparametric", "stats-nonparam" },
    { "normality", "stats-normality" },
    { "outlier", "stats-outlier" },
    { "paired", "stats-paired" },
    { "bland-altman", "stats-bland-altman" },
    { "passing-bablok", "stats-bablok" },
    { "weibull", "stats-weibull" },
    { "kaplan-meier", "stats-survival" },
    { "roc", "stats-roc" },
    { "pca", "stats-pca" },
    { "correlation", "stats-corr" },
    { "clustering", "stats-cluster" },
    { "confidence-ellipse", "stats-ellipse" },
    { "isij", "preset-isij" },
    { "isij-85mm", "preset-isij" },
    { "slide", "preset-slide" },
    { "slide-28pt", "preset-slide" },
    { "nature", "preset-nature" },
    { "nature-89mm", "preset-nature" },
    { "box-inward", "frame-box-inward" },
    { "box-frame", "frame-box-inward" },
    { "open-l", "frame-open-l" },
    { "opposite-axis", "frame-box-plain" },
    { "major-grid", "axis-grid" },
    { "grid", "axis-grid" },
    { "rescale", "zoom-auto" },
    { "1-1", "aspect-square" },
    { "square", "aspect-square" },
    { "transparent", "layer-linked" },
    { "reconstruct-legend", "legend" },
    { "ole", "copy-ole" },
    { "copy-image", "copy-page-image" },
    { "export-graph", "export-image" },
    { "command-palette", "search-command" },
    { "ctrl-k", "search-command" },
    { "focus", "focus-hud" },
    { "focus-mode", "focus-hud" },
    { "exit-focus", "exit-focus-hud" },
    { "worksheet-drawer", "table-drawer" },
    { "drawer", "table-drawer" },
    { "preferences", "settings" },
    { "check-updates", "update-check" },
    { "updater", "update-check" },
    { "download", "download-install" },
    { "language", "language-ja-en" },
    { "ja-en", "language-ja-en" },
    { "run", "python-run" },
    { "play", "python-run" },
    { "stop", "python-stop" },
    { "toolbars", "classic-toolbars" },
    { "sample", "sample-gallery" },
    { "samples", "sample-gallery" },
    { "actionsamplegallery", "sample-gallery" },
    { "kissinger", "kissinger-tds" },
    { "tds", "kissinger-tds" },
    { "actionkissingerdialog", "kissinger-tds" },
    { "kocks-mecking", "work-hardening" },
    { "considere", "work-hardening" },
    { "actionworkhardeningdialog", "work-hardening" },
    { "xrd", "xrd-peaks" },
    { "williamson-hall", "xrd-peaks" },
    { "actionxrdanalyzerdialog", "xrd-peaks" },
    { "vasp", "vasp-bands" },
    { "dos", "vasp-bands" },
    { "bands", "vasp-bands" },
    { "actionvaspplotterdialog", "vasp-bands" },
    { "export_python_script", "code-export" },
    { "actionexportpythonscript", "code-export" },
    { "python-export", "code-export" },
    { "to-python", "code-export" },

    // Delete / trash / layout / folder / Table & Matrix actionManager / TextFormatButtons / CurvesDialog aliases
    { "trash", "delete" },
    { "remove", "delete" },
    { "delete-layer", "delete" },
    { "remove-layer", "delete" },
    { "delete-selection", "delete" },
    { "workspace-tabs", "layout-tabs" },
    { "workspace-split", "layout-split" },
    { "workspace-mdi", "layout-mdi" },
    { "free-mdi", "mdi-view" },
    { "cut-selection", "cut" },
    { "copy-selection", "copy" },
    { "paste-into-selection", "paste" },
    { "paste-into-selection-transposed", "paste" },
    { "mask-selection", "mask-points" },
    { "unmask-selection", "unmask-points" },
    { "set-formula", "formula" },
    { "formula-mode", "formula" },
    { "clear-selection", "clear" },
    { "fill-row-numbers", "row-numbers" },
    { "rownumbers", "row-numbers" },
    { "row-number", "row-numbers" },
    { "fill-random", "random-numbers" },
    { "randomnumbers", "random-numbers" },
    { "random", "random-numbers" },
    { "toggle-comments", "new-table" },
    { "table-header", "new-table" },
    { "toggle-tabbar", "settings" },
    { "table-options", "settings" },
    { "select-all", "table-drawer" },
    { "export-to-tex", "export-pdf" },
    { "tex", "export-pdf" },
    { "go-to-cell", "find" },
    { "dimensions-dialog", "table-dimensions" },
    { "dimensions", "table-dimensions" },
    { "insert-columns", "add-column" },
    { "remove-columns", "delete" },
    { "clear-columns", "clear" },
    { "set-as-x", "axis-scale" },
    { "set-as-y", "plot-line" },
    { "set-as-z", "plot-3d-surface" },
    { "set-as-xerr", "error-bars" },
    { "set-as-yerr", "error-bars" },
    { "set-as-none", "clear" },
    { "normalize-columns", "curve-normalize" },
    { "normalize-selection", "curve-normalize" },
    { "sort-columns", "sort-table" },
    { "statistics-columns", "col-stats" },
    { "statistics-rows", "row-stats" },
    { "type-format", "settings" },
    { "column-format-type", "settings" },
    { "edit-description", "text-label" },
    { "column-description", "text-label" },
    { "edit-column-labels-formula", "formula" },
    { "insert-rows", "new-table" },
    { "remove-rows", "delete" },
    { "clear-rows", "clear" },
    { "clear-matrix", "clear" },
    { "transpose", "matrix-transpose" },
    { "mirror-horizontally", "matrix-transpose" },
    { "mirror-vertically", "matrix-transpose" },
    { "import-image", "import-csv" },
    { "edit-coordinates", "axis-scale" },
    { "edit-format", "settings" },
    { "numerictype", "new-table" },
    { "texttype", "text-label" },
    { "datetype", "log-window" },
    { "equals", "formula" },
    { "bottom-scl", "axis-scale" },
    { "top-scl", "axis-scale" },
    { "left-scl", "axis-scale" },
    { "right-scl", "axis-scale" },
    { "project-properties", "settings" },
    { "properties", "settings" },
    { "appicon", "new-graph" },
    { "index", "text-subscript" },
    { "subscript", "text-subscript" },
    { "sub", "text-subscript" },
    { "format-text-subscript", "text-subscript" },
    { "exp", "text-superscript" },
    { "superscript", "text-superscript" },
    { "sup", "text-superscript" },
    { "format-text-superscript", "text-superscript" },
    { "bold", "text-bold" },
    { "format-text-bold", "text-bold" },
    { "italic", "text-italic" },
    { "italics", "text-italic" },
    { "format-text-italic", "text-italic" },
    { "underline", "text-underline" },
    { "format-text-underline", "text-underline" },
    { "greek", "text-greek" },
    { "lower-greek", "text-greek" },
    { "upper-greek", "text-greek" },
    { "alpha", "text-greek" },
    { "omega", "text-greek" },
    { "sigma", "text-greek" },
    { "symbol", "text-greek" },
    { "math-symbols", "formula" },
    { "arrow-symbols", "line-arrow" },
    { "add-curve", "curve-add" },
    { "curve-plus", "curve-add" },
    { "remove-curve", "curve-remove" },
    { "delete-curve", "curve-remove" },
    { "curve-minus", "curve-remove" },

    // Ids used by the graph inspector, axis context menu and View menu
    { "eye", "visibility" },
    { "show-hide", "visibility" },
    { "inspector", "inspector-panel" },
    { "graph-inspector", "inspector-panel" },
    { "plus", "curve-add" },
    { "aspect-ratio", "aspect-square" },
    { "frame-box", "frame-box-inward" },
    { "export", "export-image" },
    { "layers", "layer-arrange" },
    { "draw-line", "mean-line" },
    { "reference-line", "mean-line" },
    { "grid-major", "axis-grid" },
    { "merge-graphs", "layer-arrange" },

    // Excel import and PowerPoint export have their own icons (they used to share import-csv,
    // preset-slide and copy-ole with CSV import, the Lab Slide preset and the OLE copy).
    { "xlsx", "import-xlsx" },
    { "xlsm", "import-xlsx" },
    { "excel", "import-xlsx" },
    { "import-excel", "import-xlsx" },
    { "import-workbook", "import-xlsx" },
    { "excel-workbook", "import-xlsx" },
    { "pptx", "export-pptx" },
    { "powerpoint", "export-pptx" },
    { "export-powerpoint", "export-pptx" },
    { "send-to-powerpoint", "export-pptx" },
    { "sendtopowerpoint", "export-pptx" },
    { "send-pptx", "export-pptx" },
    { "export-slides", "export-pptx" }
};

struct IconRegistryState {
    QMutex mutex;
    bool initialized = false;
    QHash<QString, int> specIndexById;
    QHash<QString, QString> aliasMap;
    QStringList orderedIds;
    QHash<QString, QPixmap> pixmapCache;
    QHash<QString, QIcon> iconCache;
    QStringList unresolved;
};

static IconRegistryState &registry()
{
    static IconRegistryState s;
    if (!s.initialized) {
        const int specCount = static_cast<int>(sizeof(kIconSpecs) / sizeof(kIconSpecs[0]));
        s.specIndexById.reserve(specCount * 2);
        s.orderedIds.reserve(specCount);
        for (int i = 0; i < specCount; ++i) {
            const QString key = QString::fromLatin1(kIconSpecs[i].id);
            if (!s.specIndexById.contains(key)) {
                s.specIndexById.insert(key, i);
                s.orderedIds.append(key);
            }
        }
        const int aliasCount = static_cast<int>(sizeof(kAliasTable) / sizeof(kAliasTable[0]));
        s.aliasMap.reserve(aliasCount * 2);
        for (int i = 0; i < aliasCount; ++i) {
            s.aliasMap.insert(QString::fromLatin1(kAliasTable[i].alias),
                              QString::fromLatin1(kAliasTable[i].canonical));
        }
        s.initialized = true;
    }
    return s;
}

static QString normalizeCandidateKey(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.endsWith(QStringLiteral(":dark"), Qt::CaseInsensitive))
        s.chop(5);
    if (s.startsWith(QStringLiteral(":/")))
        s = s.mid(2);
    else if (s.startsWith(QStringLiteral(":icons/")))
        s = s.mid(7);
    const int slashPos = s.lastIndexOf(QLatin1Char('/'));
    if (slashPos >= 0)
        s = s.mid(slashPos + 1);
    const int dotPos = s.lastIndexOf(QLatin1Char('.'));
    if (dotPos > 0) {
        const QString ext = s.mid(dotPos + 1).toLower();
        if (ext == QStringLiteral("xpm") || ext == QStringLiteral("svg")
            || ext == QStringLiteral("png") || ext == QStringLiteral("ico")) {
            s = s.left(dotPos);
        }
    }
    s = s.toLower();
    s.replace(QLatin1Char('_'), QLatin1Char('-'));
    s.replace(QLatin1Char(' '), QLatin1Char('-'));
    return s;
}

static QString resolveCanonicalUnlocked(IconRegistryState &st, const QString &rawId)
{
    const QString direct = rawId.trimmed();
    if (st.specIndexById.contains(direct))
        return direct;

    const QString norm = normalizeCandidateKey(rawId);
    if (st.specIndexById.contains(norm))
        return norm;

    const auto aliasIt = st.aliasMap.constFind(norm);
    if (aliasIt != st.aliasMap.cend() && st.specIndexById.contains(aliasIt.value()))
        return aliasIt.value();

    // Neither an id nor an alias: remember the name (Suite 70 requires this list to stay
    // empty) and guess from keywords so the caller still gets an icon.
    if (!st.unresolved.contains(rawId))
        st.unresolved.append(rawId);

    // Keyword-aware semantic fallback so even arbitrary caller IDs render crisp 2-color SVGs
    if (norm.contains(QStringLiteral("pptx")) || norm.contains(QStringLiteral("powerpoint")))
        return QStringLiteral("export-pptx");
    if (norm.contains(QStringLiteral("xlsx")) || norm.contains(QStringLiteral("excel")))
        return QStringLiteral("import-xlsx");
    if (norm.contains(QStringLiteral("delete")) || norm.contains(QStringLiteral("trash"))
        || norm.contains(QStringLiteral("remove")))
        return QStringLiteral("delete");
    if (norm.contains(QStringLiteral("folder")))
        return norm.contains(QStringLiteral("open")) ? QStringLiteral("folder-open")
                                                     : QStringLiteral("folder-closed");
    if (norm.contains(QStringLiteral("mdi")))
        return QStringLiteral("mdi-view");
    if (norm.contains(QStringLiteral("split")))
        return QStringLiteral("split-view");
    if (norm.contains(QStringLiteral("tab")))
        return QStringLiteral("tab-view");
    if (norm.contains(QStringLiteral("python")) || norm.contains(QStringLiteral("script")))
        return QStringLiteral("python-console");
    if (norm.contains(QStringLiteral("peak")))
        return QStringLiteral("peak-analyzer");
    if (norm.contains(QStringLiteral("fit")) || norm.contains(QStringLiteral("regress")))
        return QStringLiteral("fit-nonlinear");
    if (norm.contains(QStringLiteral("gadget")) || norm.contains(QStringLiteral("roi")))
        return QStringLiteral("gadget-roi");
    if (norm.contains(QStringLiteral("cursor")))
        return QStringLiteral("gadget-cursor");
    if (norm.contains(QStringLiteral("mask")))
        return QStringLiteral("mask-points");
    if (norm.contains(QStringLiteral("fft")) || norm.contains(QStringLiteral("spectr")))
        return QStringLiteral("signal-fft");
    if (norm.contains(QStringLiteral("filter")) || norm.contains(QStringLiteral("smooth")))
        return QStringLiteral("signal-smooth");
    if (norm.contains(QStringLiteral("stat")) || norm.contains(QStringLiteral("anova"))
        || norm.contains(QStringLiteral("test")))
        return QStringLiteral("stats-anova");
    if (norm.contains(QStringLiteral("3d")) || norm.contains(QStringLiteral("surface")))
        return QStringLiteral("plot-3d-surface");
    if (norm.contains(QStringLiteral("matrix")) || norm.contains(QStringLiteral("heatmap")))
        return QStringLiteral("new-matrix");
    if (norm.contains(QStringLiteral("table")) || norm.contains(QStringLiteral("col"))
        || norm.contains(QStringLiteral("row")))
        return QStringLiteral("new-table");
    if (norm.contains(QStringLiteral("layer")))
        return QStringLiteral("layer-add");
    if (norm.contains(QStringLiteral("axis")) || norm.contains(QStringLiteral("tick"))
        || norm.contains(QStringLiteral("scale")))
        return QStringLiteral("axis-scale");
    if (norm.contains(QStringLiteral("frame")))
        return QStringLiteral("frame-box-inward");
    if (norm.contains(QStringLiteral("export")) || norm.contains(QStringLiteral("save")))
        return QStringLiteral("export-image");
    if (norm.contains(QStringLiteral("update")))
        return QStringLiteral("update-check");
    if (norm.contains(QStringLiteral("plot")) || norm.contains(QStringLiteral("graph")))
        return QStringLiteral("plot-line-symbol");

    return QStringLiteral("new-graph");
}

static QByteArray buildSvgDocument(const DuotoneSvgSpec &spec, bool darkSurface, bool disabled)
{
    const QString prim = primaryHexColor(darkSurface, disabled);
    const QString acc = accentHexColor(spec.domain, darkSurface, disabled);
    const QString fillOp = disabled ? QStringLiteral("0.09")
                                    : (darkSurface ? QStringLiteral("0.22")
                                                   : QStringLiteral("0.16"));
    const QString strokeOp = disabled ? QStringLiteral("0.62") : QStringLiteral("1.0");

    QString fillLayer = QString::fromUtf8(spec.duotoneFill);
    fillLayer.replace(QStringLiteral("{{A}}"), acc);
    fillLayer.replace(QStringLiteral("{{P}}"), prim);

    QString primLayer = QString::fromUtf8(spec.primaryStroke);
    primLayer.replace(QStringLiteral("{{A}}"), acc);
    primLayer.replace(QStringLiteral("{{P}}"), prim);

    QString accLayer = QString::fromUtf8(spec.accentStroke);
    accLayer.replace(QStringLiteral("{{A}}"), acc);
    accLayer.replace(QStringLiteral("{{P}}"), prim);

    const QString svg = QStringLiteral(
            "<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24'>"
            "<g fill='%1' fill-opacity='%2' stroke='none'>%3</g>"
            "<g fill='none' stroke='%4' stroke-width='1.75' stroke-linecap='round' stroke-linejoin='round'>%5</g>"
            "<g fill='none' stroke='%1' stroke-opacity='%6' stroke-width='1.75' stroke-linecap='round' stroke-linejoin='round'>%7</g>"
            "</svg>")
                                .arg(acc, fillOp, fillLayer, prim, primLayer, strokeOp, accLayer);
    return svg.toUtf8();
}

static QPixmap renderCachedPixmap(const QString &canonicalId, int logicalPx, qreal dpr,
                                  bool darkSurface, bool disabled)
{
    const int safeLogicalPx = qMax(8, logicalPx > 0 ? logicalPx : 24);
    qreal effDpr = dpr;
    if (effDpr <= 0.0) {
        if (const QGuiApplication *app = qobject_cast<const QGuiApplication *>(
                    QCoreApplication::instance())) {
            effDpr = app->devicePixelRatio();
        }
        if (effDpr <= 0.0)
            effDpr = 1.0;
    }

    const int dprKey = qMax(100, qRound(effDpr * 100.0));
    const int physPx = qMax(8, qRound(safeLogicalPx * (dprKey / 100.0)));
    const QString cacheKey = QStringLiteral("%1|%2|%3|%4|%5")
                                     .arg(canonicalId)
                                     .arg(safeLogicalPx)
                                     .arg(dprKey)
                                     .arg(darkSurface ? 1 : 0)
                                     .arg(disabled ? 1 : 0);

    IconRegistryState &st = registry();
    {
        QMutexLocker locker(&st.mutex);
        const auto hit = st.pixmapCache.constFind(cacheKey);
        if (hit != st.pixmapCache.cend())
            return hit.value();
    }

    int specIdx = 0;
    {
        QMutexLocker locker(&st.mutex);
        specIdx = st.specIndexById.value(canonicalId, 0);
    }

    const QByteArray svgBytes = buildSvgDocument(kIconSpecs[specIdx], darkSurface, disabled);
    QImage img(physPx, physPx, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QSvgRenderer renderer(svgBytes);
    if (renderer.isValid()) {
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter, QRectF(0.0, 0.0, physPx, physPx));
        painter.end();
    }

    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(dprKey / 100.0);

    {
        QMutexLocker locker(&st.mutex);
        st.pixmapCache.insert(cacheKey, pm);
    }
    return pm;
}

class DuotoneSvgIconEngine : public QIconEngine
{
public:
    DuotoneSvgIconEngine(const QString &canonicalId, int baseSize, bool darkSurface)
        : d_canonicalId(canonicalId),
          d_baseSize(qMax(12, baseSize > 0 ? baseSize : 24)),
          d_darkSurface(darkSurface)
    {
    }

    QString key() const override { return QStringLiteral("ToriGraphDuotoneSvg"); }

    QString iconName() const override
    {
        return d_darkSurface ? (d_canonicalId + QStringLiteral(":dark")) : d_canonicalId;
    }

    QIconEngine *clone() const override
    {
        return new DuotoneSvgIconEngine(d_canonicalId, d_baseSize, d_darkSurface);
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override
    {
        if (size.isValid() && !size.isEmpty())
            return size;
        return QSize(d_baseSize, d_baseSize);
    }

    QList<QSize> availableSizes(QIcon::Mode = QIcon::Normal,
                                QIcon::State = QIcon::Off) const override
    {
        QList<QSize> list;
        list << QSize(16, 16) << QSize(20, 20) << QSize(24, 24) << QSize(32, 32) << QSize(48, 48);
        const QSize base(d_baseSize, d_baseSize);
        if (!list.contains(base))
            list.append(base);
        return list;
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State) override
    {
        const int px = (size.isValid() && !size.isEmpty())
                ? qMin(size.width(), size.height())
                : d_baseSize;
        return renderCachedPixmap(d_canonicalId, px, 1.0, d_darkSurface, mode == QIcon::Disabled);
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        if (!painter || rect.isEmpty())
            return;
        qreal dpr = 1.0;
        if (QPaintDevice *dev = painter->device())
            dpr = qMax<qreal>(1.0, dev->devicePixelRatioF());
        const int side = qMin(rect.width(), rect.height());
        const QPixmap pm =
                renderCachedPixmap(d_canonicalId, side, dpr, d_darkSurface, mode == QIcon::Disabled);
        const int x = rect.x() + (rect.width() - side) / 2;
        const int y = rect.y() + (rect.height() - side) / 2;
        painter->drawPixmap(QRect(x, y, side, side), pm);
    }

    void virtual_hook(int id, void *data) override
    {
        if (id == QIconEngine::IsNullHook && data) {
            *reinterpret_cast<bool *>(data) = false;
            return;
        }
        if (id == QIconEngine::AvailableSizesHook && data) {
            auto *arg = reinterpret_cast<QIconEngine::AvailableSizesArgument *>(data);
            arg->sizes = availableSizes(arg->mode, arg->state);
            return;
        }
        if (id == QIconEngine::IconNameHook && data) {
            *reinterpret_cast<QString *>(data) = iconName();
            return;
        }
        if (id == QIconEngine::ScaledPixmapHook && data) {
            auto *arg = reinterpret_cast<QIconEngine::ScaledPixmapArgument *>(data);
            const int px = (arg->size.isValid() && !arg->size.isEmpty())
                    ? qMin(arg->size.width(), arg->size.height())
                    : d_baseSize;
            const qreal scale = qMax<qreal>(1.0, arg->scale);
            arg->pixmap = renderCachedPixmap(d_canonicalId, px, scale, d_darkSurface,
                                             arg->mode == QIcon::Disabled);
            return;
        }
        QIconEngine::virtual_hook(id, data);
    }

private:
    QString d_canonicalId;
    int d_baseSize;
    bool d_darkSurface;
};

} // namespace

void IconLoader::init()
{
    (void)registry();
}

QStringList IconLoader::unresolvedIconNames()
{
    IconRegistryState &st = registry();
    QMutexLocker locker(&st.mutex);
    return st.unresolved;
}

void IconLoader::clearUnresolvedIconNames()
{
    IconRegistryState &st = registry();
    QMutexLocker locker(&st.mutex);
    st.unresolved.clear();
}

QStringList IconLoader::registeredIconIds()
{
    IconRegistryState &st = registry();
    QMutexLocker locker(&st.mutex);
    return st.orderedIds;
}

QString IconLoader::resolveIconId(const QString &nameOrAlias)
{
    IconRegistryState &st = registry();
    QMutexLocker locker(&st.mutex);
    return resolveCanonicalUnlocked(st, nameOrAlias);
}

QPixmap IconLoader::duotonePixmap(const QString &semanticId, int pxSize, qreal dpr,
                                  bool darkSurface)
{
    const QString canonical = resolveIconId(semanticId);
    return renderCachedPixmap(canonical, pxSize > 0 ? pxSize : 24, dpr, darkSurface, false);
}

QIcon IconLoader::duotoneIcon(const QString &semanticId, int baseSize, bool darkSurface)
{
    const QString canonical = resolveIconId(semanticId);
    const int safeBase = qMax(12, baseSize > 0 ? baseSize : 24);
    const QString key =
            QStringLiteral("%1|%2|%3").arg(canonical).arg(safeBase).arg(darkSurface ? 1 : 0);

    IconRegistryState &st = registry();
    {
        QMutexLocker locker(&st.mutex);
        const auto hit = st.iconCache.constFind(key);
        if (hit != st.iconCache.cend())
            return hit.value();
    }

    QIcon icon(new DuotoneSvgIconEngine(canonical, safeBase, darkSurface));
    {
        QMutexLocker locker(&st.mutex);
        st.iconCache.insert(key, icon);
    }
    return icon;
}

QIcon IconLoader::load(const QString &name)
{
    if (name.trimmed().isEmpty())
        return QIcon();
    return duotoneIcon(name, 24, false);
}
