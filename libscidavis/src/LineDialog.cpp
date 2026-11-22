/***************************************************************************
    File                 : LineDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Line options dialog

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
#include "LineDialog.h"
#include "ColorButton.h"
#include "ArrowMarker.h"
#include "PenWidget.h"
#include "Graph.h"
#include "Plot.h"
#include "ApplicationWindow.h"

#include <qwt_plot.h>

#include <QMessageBox>
#include <QLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QComboBox>
#include <QSignalBlocker>
#include <QTabWidget>
#include <cmath>

LineDialog::LineDialog(ArrowMarker *line, QWidget *parent, Qt::WindowFlags fl) : QDialog(parent, fl)
{
    setWindowTitle(tr("Line options"));

    lm = line;

    QGroupBox *gb1 = new QGroupBox();
    QGridLayout *gl1 = new QGridLayout();

    penWidget = new PenWidget(this, lm->linePen());
    penWidget->setPen(lm->linePen());
    gl1->addWidget(penWidget, 0, 0, 1, 2);

    startBox = new QCheckBox();
    startBox->setText(tr("Arrow at &start"));
    startBox->setChecked(lm->hasStartArrow());
    gl1->addWidget(startBox, 1, 0, Qt::AlignTop);

    endBox = new QCheckBox();
    endBox->setText(tr("Arrow at &end"));
    endBox->setChecked(lm->hasEndArrow());
    gl1->addWidget(endBox, 1, 1, Qt::AlignTop);

    gb1->setLayout(gl1);

    QHBoxLayout *hl1 = new QHBoxLayout();
    hl1->addWidget(gb1);

    options = new QWidget();
    options->setLayout(hl1);

    tw = new QTabWidget();
    tw->addTab(options, tr("Opti&ons"));

    QGroupBox *gb2 = new QGroupBox();
    QGridLayout *gl2 = new QGridLayout();

    gl2->addWidget(new QLabel(tr("Length")), 0, 0);
    boxHeadLength = new QSpinBox();
    boxHeadLength->setValue(lm->headLength());
    gl2->addWidget(boxHeadLength, 0, 1);

    gl2->addWidget(new QLabel(tr("Angle")), 1, 0);
    boxHeadAngle = new QSpinBox();
    boxHeadAngle->setRange(0, 85);
    boxHeadAngle->setSingleStep(5);
    boxHeadAngle->setValue(lm->headAngle());
    gl2->addWidget(boxHeadAngle, 1, 1);

    filledBox = new QCheckBox();
    filledBox->setText(tr("&Filled"));
    filledBox->setChecked(lm->filledArrowHead());
    gl2->addWidget(filledBox, 2, 1);

    gb2->setLayout(gl2);

    QHBoxLayout *hl2 = new QHBoxLayout();
    hl2->addWidget(gb2);

    head = new QWidget();
    head->setLayout(hl2);
    tw->addTab(head, tr("Arrow &Head"));

    initGeometryTab();

    buttonDefault = new QPushButton(tr("Set &Default"));
    buttonDefault->setAutoDefault(false);
    btnApply = new QPushButton(tr("&Apply"));
    btnApply->setAutoDefault(false);
    btnOk = new QPushButton(tr("&Ok"));
    btnOk->setDefault(true);
    btnCancel = new QPushButton(tr("&Cancel"));
    btnCancel->setAutoDefault(false);

    QBoxLayout *bl1 = new QBoxLayout(QBoxLayout::LeftToRight);
    bl1->addWidget(buttonDefault);
    bl1->addStretch();
    bl1->addWidget(btnApply);
    bl1->addWidget(btnOk);
    bl1->addWidget(btnCancel);

    QVBoxLayout *vl = new QVBoxLayout();
    vl->addWidget(tw);
    vl->addLayout(bl1);
    setLayout(vl);

    enableHeadTab();

    connect(startBox, &QCheckBox::toggled, this, [this](bool) { enableHeadTab(); });
    connect(endBox, &QCheckBox::toggled, this, [this](bool) { enableHeadTab(); });
    connect(btnOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(btnApply, SIGNAL(clicked()), this, SLOT(apply()));
    connect(btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(tw, SIGNAL(currentChanged(int)), this, SLOT(enableButtonDefault(int)));
    connect(buttonDefault, SIGNAL(clicked()), this, SLOT(setDefaultValues()));
}

void LineDialog::initGeometryTab()
{
    unitBox = new QComboBox();
    unitBox->addItem(tr("Scale Coordinates"));
    unitBox->addItem(tr("Pixels"));

    QBoxLayout *bl1 = new QBoxLayout(QBoxLayout::LeftToRight);
    bl1->addWidget(new QLabel(tr("Unit")));
    bl1->addWidget(unitBox);

    QGroupBox *gb1 = new QGroupBox(tr("Start Point"));
    xStartBox = new QLineEdit();
    yStartBox = new QLineEdit();

    QGridLayout *gl1 = new QGridLayout();
    gl1->addWidget(new QLabel(tr("X")), 0, 0);
    gl1->addWidget(xStartBox, 0, 1);
    gl1->addWidget(new QLabel(tr("Y")), 1, 0);
    gl1->addWidget(yStartBox, 1, 1);
    gb1->setLayout(gl1);

    QGroupBox *gb2 = new QGroupBox(tr("End Point"));
    xEndBox = new QLineEdit();
    yEndBox = new QLineEdit();

    QGridLayout *gl2 = new QGridLayout();
    gl2->addWidget(new QLabel(tr("X")), 0, 0);
    gl2->addWidget(xEndBox, 0, 1);
    gl2->addWidget(new QLabel(tr("Y")), 1, 0);
    gl2->addWidget(yEndBox, 1, 1);
    gb2->setLayout(gl2);

    QBoxLayout *bl2 = new QBoxLayout(QBoxLayout::LeftToRight);
    bl2->addWidget(gb1);
    bl2->addWidget(gb2);

    QVBoxLayout *vl = new QVBoxLayout();
    vl->addLayout(bl1);
    vl->addLayout(bl2);

    geometry = new QWidget();
    geometry->setLayout(vl);
    tw->addTab(geometry, tr("&Geometry"));

    connect(unitBox, SIGNAL(activated(int)), this, SLOT(displayCoordinates(int)));
    displayCoordinates(0);

    auto markGeomDirty = [this](const QString &) { d_geometryDirty = true; };
    connect(xStartBox, &QLineEdit::textChanged, this, markGeomDirty);
    connect(yStartBox, &QLineEdit::textChanged, this, markGeomDirty);
    connect(xEndBox, &QLineEdit::textChanged, this, markGeomDirty);
    connect(yEndBox, &QLineEdit::textChanged, this, markGeomDirty);
}

void LineDialog::displayCoordinates(int unit)
{
    QSignalBlocker bx1(xStartBox);
    QSignalBlocker by1(yStartBox);
    QSignalBlocker bx2(xEndBox);
    QSignalBlocker by2(yEndBox);
    if (unit == ScaleCoordinates) {
        QwtDoublePoint sp = lm->startPointCoord();
        xStartBox->setText(QString::number(sp.x(), 'g', 15));
        yStartBox->setText(QString::number(sp.y(), 'g', 15));

        QwtDoublePoint ep = lm->endPointCoord();
        xEndBox->setText(QString::number(ep.x(), 'g', 15));
        yEndBox->setText(QString::number(ep.y(), 'g', 15));
    } else {
        QPoint startPoint = lm->startPoint();
        QPoint endPoint = lm->endPoint();

        xStartBox->setText(QString::number(startPoint.x()));
        yStartBox->setText(QString::number(startPoint.y()));

        xEndBox->setText(QString::number(endPoint.x()));
        yEndBox->setText(QString::number(endPoint.y()));
    }
    d_geometryDirty = false;
}

void LineDialog::setCoordinates(int unit)
{
    auto parseDouble = [](const QString &txt, double fallback) -> double {
        const QString s = txt.trimmed();
        bool ok = false;
        double v = QLocale().toDouble(s, &ok);
        if (!ok)
            v = QLocale::c().toDouble(QString(s).replace(QLatin1Char(','), QLatin1Char('.')), &ok);
        return (ok && std::isfinite(v)) ? v : fallback;
    };

    if (unit == ScaleCoordinates) {
        const QwtDoublePoint curStart = lm->startPointCoord();
        const QwtDoublePoint curEnd = lm->endPointCoord();
        lm->setStartPoint(parseDouble(xStartBox->text(), curStart.x()),
                          parseDouble(yStartBox->text(), curStart.y()));
        lm->setEndPoint(parseDouble(xEndBox->text(), curEnd.x()),
                        parseDouble(yEndBox->text(), curEnd.y()));
    } else {
        const QPoint curStart = lm->startPoint();
        const QPoint curEnd = lm->endPoint();
        lm->setStartPoint(QPoint(qRound(parseDouble(xStartBox->text(), curStart.x())),
                                 qRound(parseDouble(yStartBox->text(), curStart.y()))));
        lm->setEndPoint(QPoint(qRound(parseDouble(xEndBox->text(), curEnd.x())),
                               qRound(parseDouble(yEndBox->text(), curEnd.y()))));
    }
}

void LineDialog::apply()
{
    if (!lm)
        return;

    if (options) {
        lm->drawEndArrow(endBox->isChecked());
        lm->drawStartArrow(startBox->isChecked());
        QPen pen = penWidget->pen();
        pen.setCosmetic(false);
        lm->setLinePen(pen);
    }

    if (head && boxHeadLength && boxHeadAngle && filledBox) {
        if (lm->headLength() != boxHeadLength->value())
            lm->setHeadLength(boxHeadLength->value());

        if (lm->headAngle() != boxHeadAngle->value())
            lm->setHeadAngle(boxHeadAngle->value());

        if (lm->filledArrowHead() != filledBox->isChecked())
            lm->fillArrowHead(filledBox->isChecked());
    }

    if (geometry && unitBox
        && (d_geometryDirty || unitBox->currentIndex() == ScaleCoordinates
            || tw->currentWidget() == geometry)) {
        setCoordinates(unitBox->currentIndex());
        d_geometryDirty = false;
    }

    if (QwtPlot *plot = lm->plot()) {
        plot->replot();
        if (Graph *g = qobject_cast<Graph *>(plot->parent()))
            g->notifyChanges();
    }

    enableHeadTab();
}

void LineDialog::accept()
{
    apply();
    QDialog::accept();
}

void LineDialog::enableHeadTab()
{
    if (startBox->isChecked() || endBox->isChecked())
        tw->setTabEnabled(tw->indexOf(head), true);
    else
        tw->setTabEnabled(tw->indexOf(head), false);
}

void LineDialog::setDefaultValues()
{
    ApplicationWindow *app = (ApplicationWindow *)this->parent();
    if (!app)
        return;

    app->setArrowDefaultSettings(penWidget->pen(), boxHeadLength->value(), boxHeadAngle->value(),
                                 filledBox->isChecked());
}

void LineDialog::enableButtonDefault(int page)
{
    QWidget *pageWidget = tw->widget(page);
    if (pageWidget == geometry)
        buttonDefault->setEnabled(false);
    else
        buttonDefault->setEnabled(true);
}
