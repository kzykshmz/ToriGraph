/***************************************************************************
    File                 : ColorButton.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : A button used for color selection

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
#include "ColorButton.h"
#include "IconLoader.h"

#include <algorithm>
#include <iterator>

#include <QColorDialog>
#include <QPalette>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFrame>

const QColor ColorButton::colors[] = {
    QColor(Qt::black),
    QColor(Qt::red),
    QColor(Qt::green),
    QColor(Qt::blue),
    QColor(Qt::cyan),
    QColor(Qt::magenta),
    QColor(Qt::yellow),
    QColor(Qt::darkYellow),
    QColor(Qt::darkBlue),
    QColor(Qt::darkMagenta),
    QColor(Qt::darkRed),
    QColor(Qt::darkGreen),
    QColor(Qt::darkCyan),
    QColor("#0000A0"),
    QColor("#FF8000"),
    QColor("#8000FF"),
    QColor("#FF0080"),
    QColor(Qt::white),
    QColor(Qt::lightGray),
    QColor(Qt::gray),
    QColor("#FFFF80"),
    QColor("#80FFFF"),
    QColor("#FF80FF"),
    QColor(Qt::darkGray),
    // additional colors from figure 6 in doi:10.1016/j.csda.2008.11.033
    QColor("#023fa5"),
    QColor("#4a6fe3"),
    QColor("#11c638"),
    QColor("#0fcfc0"),
    QColor("#8e063b"),
    QColor("#d33f6a"),
    QColor("#ef9708"),
    QColor("#f79cd4"),
    QColor("#7d87b9"),
    QColor("#8595e1"),
    QColor("#8dd593"),
    QColor("#9cded6"),
    QColor("#bb7784"),
    QColor("#e07b91"),
    QColor("#f0b98d"),
    QColor("#f6c4e1"),
    QColor("#bec1d4"),
    QColor("#b5bbe3"),
    QColor("#c6dec7"),
    QColor("#d5eae7"),
    QColor("#d6bcc0"),
    QColor("#e6afb9"),
    QColor("#ead3c6"),
    QColor("#f3e1eb"),
};

const unsigned int ColorButton::colors_count = sizeof(colors) / sizeof(colors[0]);

namespace {
// ToriGraph standard palette (A3-6): the first row of the colour dialog's basic-colour grid.
// Origin's default line colours, which stay distinguishable in print and for colour-blind
// readers: black, red, blue, green, magenta, orange.
//
// This is deliberately NOT part of ColorButton::colors[]. Projects, imported OPJ/OPJU files and
// scripts store colours as indices into that array (ColorButton::colorIndex()/color()), so its
// order and size (colors_count also drives pie-slice and import colour cycling) must not change.
// Colours of this palette that are not in colors[] are stored by value (rgba) as usual.
const QColor kStandardPaletteFirstRow[] = {
    QColor(0x00, 0x00, 0x00), // black
    QColor(0xFF, 0x00, 0x00), // red
    QColor(0x00, 0x00, 0xFF), // blue
    QColor(0x00, 0x80, 0x00), // green
    QColor(0xFF, 0x00, 0xFF), // magenta
    QColor(0xFF, 0x80, 0x00), // orange
};
constexpr int kBasicGridCols = 6;
static_assert(sizeof(kStandardPaletteFirstRow) / sizeof(kStandardPaletteFirstRow[0])
                      == kBasicGridCols,
              "the standard palette fills exactly the first row of the 6-column basic grid");
} // namespace

ColorButton::ColorButton(QWidget *parent) : QWidget(parent)
{
    init();
}

void ColorButton::init()
{
    // transpose colors in the 6x8 basic colour grid.
    constexpr int rows = 8, cols = kBasicGridCols;
    static_assert(rows * cols <= sizeof(colors) / sizeof(colors[0]), "");
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            QColorDialog::setStandardColor(j + cols * i, colors[i + rows * j].rgb());
        }
    }
    // first row: ToriGraph standard palette (overrides the transposed entries 0..5)
    for (int j = 0; j < cols; j++)
        QColorDialog::setStandardColor(j, kStandardPaletteFirstRow[j].rgb());
    btn_size = 28;
    selectButton = new QPushButton(IconLoader::duotoneIcon("color-palette"), QString(), this);
    selectButton->setMinimumWidth(btn_size);
    selectButton->setMinimumHeight(btn_size);

    display = new QFrame(this);
    display->setLineWidth(2);
    display->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    display->setMinimumHeight(btn_size);
    display->setMinimumWidth(2 * btn_size);
    display->setAutoFillBackground(true);
    setColor(QColor(Qt::white));

    QHBoxLayout *l = new QHBoxLayout(this);
    l->setMargin(0);
    l->addWidget(display);
    l->addWidget(selectButton);

    setMaximumWidth(3 * btn_size);
    setMaximumHeight(btn_size);

    connect(selectButton, SIGNAL(clicked()), this, SLOT(pickColor()));
}

void ColorButton::setColor(const QColor &c)
{
    QPalette pal;
    pal.setColor(QPalette::Window, c);
    display->setPalette(pal);
    emit changed(c);
}

QColor ColorButton::color() const
{
    return display->palette().color(QPalette::Window);
}

unsigned int ColorButton::colorIndex(const QColor &c)
{
    const QColor *ite = std::find(std::begin(colors), std::end(colors), c);
    if (ite != std::end(colors) && ite->isValid())
        return (ite - colors);
    else
        return c.rgba();
}

QColor ColorButton::color(unsigned int colorIndex)
{
    if (colorIndex < colors_count)
        return colors[colorIndex];
    else {
        QColor qc = QColor::fromRgba(colorIndex);
        if (qc.isValid())
            return qc;
        else
            return QColor(Qt::black); // default color is black.
    }
}

bool ColorButton::isValidColor(const QColor &c)
{
    const QColor *ite = std::find(std::begin(colors), std::end(colors), c);
    if (ite != std::end(colors) && ite->isValid())
        return true;
    // the ToriGraph standard palette (first row of the colour dialog) is also a predefined set
    const QColor *pal = std::find(std::begin(kStandardPaletteFirstRow),
                                  std::end(kStandardPaletteFirstRow), c);
    return pal != std::end(kStandardPaletteFirstRow) && pal->isValid();
}

QSize ColorButton::sizeHint() const
{
    return QSize(4 * btn_size, btn_size);
}

void ColorButton::pickColor()
{
    QColor c = QColorDialog::getColor(color(), this, "Select color",
                                      QColorDialog::DontUseNativeDialog
                                              | QColorDialog::ShowAlphaChannel);
    if (!c.isValid() || c == color())
        return;
    setColor(c);
}
