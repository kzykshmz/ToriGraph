/***************************************************************************
    File                 : ImageDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Image geometry dialog

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
#include "ImageDialog.h"

#include <QLayout>
#include <QGroupBox>
#include <QLabel>

ImageDialog::ImageDialog(QWidget *parent, Qt::WindowFlags fl) : QDialog(parent, fl)
{
    setWindowTitle(tr("Image Geometry"));

    QGroupBox *gb1 = new QGroupBox(tr("Origin"));
    boxX = new QSpinBox();
    boxX->setRange(0, 2000);
    boxX->setSuffix(tr(" pixels"));

    boxY = new QSpinBox();
    boxY->setRange(0, 2000);
    boxY->setSuffix(tr(" pixels"));

    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("X= ")), 0, 0);
    gl1->addWidget(boxX, 0, 1);
    gl1->addWidget(new QLabel(tr("Y= ")), 1, 0);
    gl1->addWidget(boxY, 1, 1);
    gl1->setRowStretch(2, 1);

    QGroupBox *gb2 = new QGroupBox(tr("Size"));
    boxWidth = new QSpinBox();
    boxWidth->setRange(1, 2000);
    boxWidth->setSuffix(tr(" pixels"));

    boxHeight = new QSpinBox();
    boxHeight->setRange(1, 2000);
    boxHeight->setSuffix(tr(" pixels"));

    QGridLayout *gl2 = new QGridLayout(gb2);
    gl2->addWidget(new QLabel(tr("width= ")), 0, 0);
    gl2->addWidget(boxWidth, 0, 1);

    gl2->addWidget(new QLabel(tr("height= ")), 2, 0);
    gl2->addWidget(boxHeight, 2, 1);

    keepRatioBox = new QCheckBox(tr("Keep aspect ratio"));
    keepRatioBox->setChecked(true);
    gl2->addWidget(keepRatioBox, 3, 1);

    gl2->setRowStretch(4, 1);

    QBoxLayout *bl1 = new QBoxLayout(QBoxLayout::LeftToRight);
    bl1->addWidget(gb1);
    bl1->addWidget(gb2);

    buttonApply = new QPushButton(tr("&Apply"));
    buttonOk = new QPushButton(tr("&Ok"));
    buttonCancel = new QPushButton(tr("&Cancel"));

    QBoxLayout *bl2 = new QBoxLayout(QBoxLayout::LeftToRight);
    bl2->addStretch();
    bl2->addWidget(buttonApply);
    bl2->addWidget(buttonOk);
    bl2->addWidget(buttonCancel);

    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->addLayout(bl1);
    vl->addLayout(bl2);

    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonApply, SIGNAL(clicked()), this, SLOT(update()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
}

void ImageDialog::setOrigin(const QPoint &o)
{
    boxX->setValue(o.x());
    boxY->setValue(o.y());
}

void ImageDialog::setSize(const QSize &size)
{
    const int w = qMax(1, size.width());
    const int h = qMax(1, size.height());
    boxWidth->setValue(w);
    boxHeight->setValue(h);
    aspect_ratio = double(w) / double(h);

    connect(boxWidth, SIGNAL(valueChanged(int)), this, SLOT(adjustHeight(int)));
    connect(boxHeight, SIGNAL(valueChanged(int)), this, SLOT(adjustWidth(int)));
}

void ImageDialog::adjustHeight(int width)
{
    if (keepRatioBox->isChecked()) {
        if (!(aspect_ratio > 1e-12) || !std::isfinite(aspect_ratio))
            return;
        const double nextH = double(qMax(1, width)) / aspect_ratio;
        if (!std::isfinite(nextH))
            return;
        const int clampedH = static_cast<int>(
                qBound(double(boxHeight->minimum()), std::round(nextH),
                       double(boxHeight->maximum())));
        disconnect(boxHeight, SIGNAL(valueChanged(int)), this, SLOT(adjustWidth(int)));
        boxHeight->setValue(clampedH);
        connect(boxHeight, SIGNAL(valueChanged(int)), this, SLOT(adjustWidth(int)));
    } else if (boxHeight->value() > 0 && width > 0) {
        aspect_ratio = double(width) / double(boxHeight->value());
    }
}

void ImageDialog::adjustWidth(int height)
{
    if (keepRatioBox->isChecked()) {
        if (!(aspect_ratio > 1e-12) || !std::isfinite(aspect_ratio))
            return;
        const double nextW = double(qMax(1, height)) * aspect_ratio;
        if (!std::isfinite(nextW))
            return;
        const int clampedW = static_cast<int>(
                qBound(double(boxWidth->minimum()), std::round(nextW),
                       double(boxWidth->maximum())));
        disconnect(boxWidth, SIGNAL(valueChanged(int)), this, SLOT(adjustHeight(int)));
        boxWidth->setValue(clampedW);
        connect(boxWidth, SIGNAL(valueChanged(int)), this, SLOT(adjustHeight(int)));
    } else if (height > 0 && boxWidth->value() > 0) {
        aspect_ratio = double(boxWidth->value()) / double(height);
    }
}

void ImageDialog::update()
{
    emit setGeometry(boxX->value(), boxY->value(), boxWidth->value(), boxHeight->value());
}

void ImageDialog::accept()
{
    update();
    close();
}
