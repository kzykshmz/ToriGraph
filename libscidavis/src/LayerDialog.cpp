/***************************************************************************
    File                 : LayerDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Arrange layers dialog

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
#include "LayerDialog.h"
#include "ApplicationWindow.h"

#include <QLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QFontDialog>
#include <QFont>
#include <QMessageBox>
#include <cmath>

LayerDialog::LayerDialog(QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl), multi_layer(nullptr)
{
    setWindowTitle(tr("Arrange Layers"));

    QGroupBox *gb1 = new QGroupBox(tr("Layers"));
    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("Number")), 0, 0);
    layersBox = new QSpinBox();
    layersBox->setRange(0, 100);
    gl1->addWidget(layersBox, 0, 1);

    fitBox = new QCheckBox(tr("Automatic &layout"));
    fitBox->setChecked(false);
    gl1->addWidget(fitBox, 1, 1);

    keepAspectBox = new QCheckBox(tr("Keep plot &aspect ratio"));
    keepAspectBox->setChecked(true);
    gl1->addWidget(keepAspectBox, 2, 0, 1, 2);

    gl1->addWidget(new QLabel(tr("Aspect Ratio")), 3, 0);
    aspectPresetBox = new QComboBox();
    aspectPresetBox->addItem(tr("1:1 (Square)"), 1.0);
    aspectPresetBox->addItem(tr("4:3 (Standard)"), 4.0 / 3.0);
    aspectPresetBox->addItem(tr("16:9 (Widescreen)"), 16.0 / 9.0);
    aspectPresetBox->addItem(tr("3:2"), 1.5);
    aspectPresetBox->addItem(tr("sqrt(2):1 (A-Series)"), std::sqrt(2.0));
    aspectPresetBox->addItem(tr("Custom"), -1.0);
    gl1->addWidget(aspectPresetBox, 3, 1);

    gl1->addWidget(new QLabel(tr("Ratio (W/H)")), 4, 0);
    customAspectBox = new QDoubleSpinBox();
    customAspectBox->setRange(0.1, 10.0);
    customAspectBox->setDecimals(3);
    customAspectBox->setSingleStep(0.1);
    customAspectBox->setValue(1.0);
    gl1->addWidget(customAspectBox, 4, 1);
    gl1->setRowStretch(5, 1);

    QGroupBox *gb2 = new QGroupBox(tr("Alignement"));
    QGridLayout *gl2 = new QGridLayout(gb2);
    gl2->addWidget(new QLabel(tr("Horizontal")), 0, 0);

    alignHorBox = new QComboBox();
    alignHorBox->addItem(tr("Center"));
    alignHorBox->addItem(tr("Left"));
    alignHorBox->addItem(tr("Right"));
    gl2->addWidget(alignHorBox, 0, 1);

    gl2->addWidget(new QLabel(tr("Vertical")), 1, 0);
    alignVertBox = new QComboBox();
    alignVertBox->addItem(tr("Center"));
    alignVertBox->addItem(tr("Top"));
    alignVertBox->addItem(tr("Bottom"));
    gl2->addWidget(alignVertBox, 1, 1);
    gl2->setRowStretch(2, 1);

    GroupGrid = new QGroupBox(tr("Grid"));
    QGridLayout *gl3 = new QGridLayout(GroupGrid);
    gl3->addWidget(new QLabel(tr("Columns")), 0, 0);
    boxX = new QSpinBox();
    boxX->setRange(1, 100);
    gl3->addWidget(boxX, 0, 1);
    gl3->addWidget(new QLabel(tr("Rows")), 1, 0);
    boxY = new QSpinBox();
    boxY->setRange(1, 100);
    gl3->addWidget(boxY, 1, 1);

    GroupCanvasSize = new QGroupBox(tr("&Layer Canvas Size"));
    GroupCanvasSize->setCheckable(true);
    GroupCanvasSize->setChecked(false);

    QGridLayout *gl5 = new QGridLayout(GroupCanvasSize);
    gl5->addWidget(new QLabel(tr("Width")), 0, 0);
    boxCanvasWidth = new QSpinBox();
    boxCanvasWidth->setRange(0, 10000);
    boxCanvasWidth->setSingleStep(50);
    boxCanvasWidth->setSuffix(tr(" pixels"));
    gl5->addWidget(boxCanvasWidth, 0, 1);
    gl5->addWidget(new QLabel(tr("Height")), 1, 0);
    boxCanvasHeight = new QSpinBox();
    boxCanvasHeight->setRange(0, 10000);
    boxCanvasHeight->setSingleStep(50);
    boxCanvasHeight->setSuffix(tr(" pixels"));
    gl5->addWidget(boxCanvasHeight, 1, 1);

    QGroupBox *gb4 = new QGroupBox(tr("Spacing"));
    QGridLayout *gl4 = new QGridLayout(gb4);
    gl4->addWidget(new QLabel(tr("Columns gap")), 0, 0);
    boxColsGap = new QSpinBox();
    boxColsGap->setRange(0, 1000);
    boxColsGap->setSingleStep(5);
    boxColsGap->setSuffix(tr(" pixels"));
    gl4->addWidget(boxColsGap, 0, 1);
    gl4->addWidget(new QLabel(tr("Rows gap")), 1, 0);
    boxRowsGap = new QSpinBox();
    boxRowsGap->setRange(0, 1000);
    boxRowsGap->setSingleStep(5);
    boxRowsGap->setSuffix(tr(" pixels"));
    gl4->addWidget(boxRowsGap, 1, 1);
    gl4->addWidget(new QLabel(tr("Left margin")), 2, 0);
    boxLeftSpace = new QSpinBox();
    boxLeftSpace->setRange(0, 1000);
    boxLeftSpace->setSingleStep(5);
    boxLeftSpace->setSuffix(tr(" pixels"));
    gl4->addWidget(boxLeftSpace, 2, 1);
    gl4->addWidget(new QLabel(tr("Right margin")), 3, 0);
    boxRightSpace = new QSpinBox();
    boxRightSpace->setRange(0, 1000);
    boxRightSpace->setSingleStep(5);
    boxRightSpace->setSuffix(tr(" pixels"));
    gl4->addWidget(boxRightSpace, 3, 1);
    gl4->addWidget(new QLabel(tr("Top margin")), 4, 0);
    boxTopSpace = new QSpinBox();
    boxTopSpace->setRange(0, 1000);
    boxTopSpace->setSingleStep(5);
    boxTopSpace->setSuffix(tr(" pixels"));
    gl4->addWidget(boxTopSpace, 4, 1);
    gl4->addWidget(new QLabel(tr("Bottom margin")), 5, 0);
    boxBottomSpace = new QSpinBox();
    boxBottomSpace->setRange(0, 1000);
    boxBottomSpace->setSingleStep(5);
    boxBottomSpace->setSuffix(tr(" pixels"));
    gl4->addWidget(boxBottomSpace, 5, 1);

    QVBoxLayout *vbox1 = new QVBoxLayout();
    vbox1->addWidget(GroupGrid);
    vbox1->addWidget(GroupCanvasSize);

    buttonApply = new QPushButton(tr("&Apply"));
    buttonOk = new QPushButton(tr("&OK"));
    buttonCancel = new QPushButton(tr("&Cancel"));

    QHBoxLayout *hbox1 = new QHBoxLayout();
    hbox1->addStretch();
    hbox1->addWidget(buttonApply);
    hbox1->addWidget(buttonOk);
    hbox1->addWidget(buttonCancel);

    QGridLayout *gl6 = new QGridLayout();
    gl6->addWidget(gb1, 0, 0);
    gl6->addWidget(gb2, 0, 1);
    gl6->addLayout(vbox1, 1, 0);
    gl6->addWidget(gb4, 1, 1);

    GroupLinkAxes = new QGroupBox(tr("Link Axes & Layer Geometry"));
    QGridLayout *glLink = new QGridLayout(GroupLinkAxes);
    glLink->addWidget(new QLabel(tr("Layer")), 0, 0);
    targetLayerBox = new QComboBox();
    glLink->addWidget(targetLayerBox, 0, 1);
    glLink->addWidget(new QLabel(tr("Parent Layer")), 0, 2);
    parentLayerBox = new QComboBox();
    glLink->addWidget(parentLayerBox, 0, 3);

    glLink->addWidget(new QLabel(tr("X Link Mode")), 1, 0);
    xLinkModeBox = new QComboBox();
    xLinkModeBox->addItem(tr("None"), 0);
    xLinkModeBox->addItem(tr("Straight (1:1)"), 1);
    xLinkModeBox->addItem(tr("Custom Formula"), 2);
    glLink->addWidget(xLinkModeBox, 1, 1);
    glLink->addWidget(new QLabel(tr("X Link Formula")), 1, 2);
    xFormulaEdit = new QLineEdit(QString("x"));
    xFormulaEdit->setPlaceholderText(QString("1000/(x+273.15)"));
    glLink->addWidget(xFormulaEdit, 1, 3);

    glLink->addWidget(new QLabel(tr("Y Link Mode")), 2, 0);
    yLinkModeBox = new QComboBox();
    yLinkModeBox->addItem(tr("None"), 0);
    yLinkModeBox->addItem(tr("Straight (1:1)"), 1);
    yLinkModeBox->addItem(tr("Custom Formula"), 2);
    glLink->addWidget(yLinkModeBox, 2, 1);
    glLink->addWidget(new QLabel(tr("Y Link Formula")), 2, 2);
    yFormulaEdit = new QLineEdit(QString("y"));
    yFormulaEdit->setPlaceholderText(QString("y"));
    glLink->addWidget(yFormulaEdit, 2, 3);

    QHBoxLayout *geoRow = new QHBoxLayout();
    geoRow->addWidget(new QLabel(QStringLiteral("X:")));
    layerGeoXBox = new QSpinBox();
    layerGeoXBox->setRange(-5000, 10000);
    layerGeoXBox->setSuffix(QStringLiteral(" px"));
    geoRow->addWidget(layerGeoXBox);
    geoRow->addWidget(new QLabel(QStringLiteral("Y:")));
    layerGeoYBox = new QSpinBox();
    layerGeoYBox->setRange(-5000, 10000);
    layerGeoYBox->setSuffix(QStringLiteral(" px"));
    geoRow->addWidget(layerGeoYBox);
    geoRow->addWidget(new QLabel(QStringLiteral("W:")));
    layerGeoWBox = new QSpinBox();
    layerGeoWBox->setRange(40, 10000);
    layerGeoWBox->setSuffix(QStringLiteral(" px"));
    geoRow->addWidget(layerGeoWBox);
    geoRow->addWidget(new QLabel(QStringLiteral("H:")));
    layerGeoHBox = new QSpinBox();
    layerGeoHBox->setRange(40, 10000);
    layerGeoHBox->setSuffix(QStringLiteral(" px"));
    geoRow->addWidget(layerGeoHBox);
    glLink->addLayout(geoRow, 3, 0, 1, 4);

    gl6->addWidget(GroupLinkAxes, 2, 0, 1, 2);
    gl6->setRowStretch(3, 1);

    QVBoxLayout *vbox2 = new QVBoxLayout(this);
    vbox2->addLayout(gl6);
    vbox2->addLayout(hbox1);

    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonApply, SIGNAL(clicked()), this, SLOT(update()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(fitBox, SIGNAL(toggled(bool)), this, SLOT(enableLayoutOptions(bool)));
    connect(aspectPresetBox, SIGNAL(currentIndexChanged(int)), this,
            SLOT(onAspectPresetChanged(int)));
    connect(keepAspectBox, SIGNAL(toggled(bool)), aspectPresetBox, SLOT(setEnabled(bool)));
    connect(keepAspectBox, SIGNAL(toggled(bool)), customAspectBox, SLOT(setEnabled(bool)));
    connect(targetLayerBox, SIGNAL(currentIndexChanged(int)), this,
            SLOT(onTargetLayerChanged(int)));
    connect(parentLayerBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onLinkModeChanged()));
    connect(xLinkModeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onLinkModeChanged()));
    connect(yLinkModeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onLinkModeChanged()));
    onLinkModeChanged();
}

void LayerDialog::onAspectPresetChanged(int index)
{
    if (!aspectPresetBox || !customAspectBox)
        return;
    const double ratio = aspectPresetBox->itemData(index).toDouble();
    if (ratio > 0.0) {
        customAspectBox->setValue(ratio);
        if (boxCanvasHeight && boxCanvasHeight->value() > 0 && boxCanvasWidth)
            boxCanvasWidth->setValue(qMax(50, qRound(boxCanvasHeight->value() * ratio)));
    }
}

void LayerDialog::enableLayoutOptions(bool ok)
{
    GroupGrid->setEnabled(!ok);
    GroupCanvasSize->setEnabled(!ok);
}

void LayerDialog::setMultiLayer(MultiLayer *g)
{
    multi_layer = g;

    layersBox->setValue(g->layers());
    boxX->setValue(g->getCols());
    boxY->setValue(g->getRows());
    boxColsGap->setValue(g->colsSpacing());
    boxRowsGap->setValue(g->rowsSpacing());
    boxLeftSpace->setValue(g->leftMargin());
    boxRightSpace->setValue(g->rightMargin());
    boxTopSpace->setValue(g->topMargin());
    boxBottomSpace->setValue(g->bottomMargin());
    boxCanvasWidth->setValue(g->layerCanvasSize().width());
    boxCanvasHeight->setValue(g->layerCanvasSize().height());
    alignHorBox->setCurrentIndex(g->horizontalAlignement());
    alignVertBox->setCurrentIndex(g->verticalAlignement());

    keepAspectBox->setChecked(g->keepAspectRatio());
    const double ratio = g->targetAspectRatio();
    customAspectBox->setValue(ratio > 0.05 ? ratio : 1.0);
    int matchedIdx = aspectPresetBox->count() - 1;
    for (int i = 0; i < aspectPresetBox->count() - 1; ++i) {
        const double presetRatio = aspectPresetBox->itemData(i).toDouble();
        if (std::abs(presetRatio - ratio) < 0.01) {
            matchedIdx = i;
            break;
        }
    }
    aspectPresetBox->blockSignals(true);
    aspectPresetBox->setCurrentIndex(matchedIdx);
    aspectPresetBox->blockSignals(false);
    aspectPresetBox->setEnabled(g->keepAspectRatio());
    customAspectBox->setEnabled(g->keepAspectRatio());
    refreshLayerSelectors();
}

void LayerDialog::refreshLayerSelectors()
{
    if (!multi_layer)
        return;

    targetLayerBox->blockSignals(true);
    parentLayerBox->blockSignals(true);
    targetLayerBox->clear();
    parentLayerBox->clear();
    parentLayerBox->addItem(tr("None"), 0);

    int activeIdx = 0;
    for (int i = 1; i <= multi_layer->layers(); ++i) {
        const QString lbl = tr("Layer") + " " + QString::number(i);
        targetLayerBox->addItem(lbl, i);
        parentLayerBox->addItem(lbl, i);
        if (multi_layer->layer(i) == multi_layer->activeGraph())
            activeIdx = i - 1;
    }
    targetLayerBox->setCurrentIndex(activeIdx);
    targetLayerBox->blockSignals(false);
    parentLayerBox->blockSignals(false);

    onTargetLayerChanged(activeIdx);
}

void LayerDialog::onTargetLayerChanged(int index)
{
    if (!multi_layer || index < 0)
        return;
    const int layerNum = targetLayerBox->itemData(index).toInt();
    Graph *g = multi_layer->layer(layerNum);
    if (!g)
        return;

    parentLayerBox->blockSignals(true);
    int pIdx = 0;
    if (Graph *parentG = g->linkedParentLayer()) {
        const QList<Graph *> all = multi_layer->layersList();
        const int pos = all.indexOf(parentG);
        if (pos >= 0)
            pIdx = pos + 1;
    }
    parentLayerBox->setCurrentIndex(pIdx);
    parentLayerBox->blockSignals(false);

    xLinkModeBox->blockSignals(true);
    yLinkModeBox->blockSignals(true);
    xLinkModeBox->setCurrentIndex(qBound(0, g->xLinkMode(), 2));
    yLinkModeBox->setCurrentIndex(qBound(0, g->yLinkMode(), 2));
    xLinkModeBox->blockSignals(false);
    yLinkModeBox->blockSignals(false);

    xFormulaEdit->setText(g->xLinkExpr());
    yFormulaEdit->setText(g->yLinkExpr());

    const QRect geo = g->geometry();
    layerGeoXBox->setValue(geo.x());
    layerGeoYBox->setValue(geo.y());
    layerGeoWBox->setValue(geo.width());
    layerGeoHBox->setValue(geo.height());
    onLinkModeChanged();
}

void LayerDialog::onLinkModeChanged()
{
    const bool hasParent = parentLayerBox && parentLayerBox->currentData().toInt() > 0;
    if (xLinkModeBox)
        xLinkModeBox->setEnabled(hasParent);
    if (yLinkModeBox)
        yLinkModeBox->setEnabled(hasParent);
    if (xFormulaEdit)
        xFormulaEdit->setEnabled(hasParent && xLinkModeBox->currentData().toInt() == 2);
    if (yFormulaEdit)
        yFormulaEdit->setEnabled(hasParent && yLinkModeBox->currentData().toInt() == 2);
}

void LayerDialog::update()
{
    if (!multi_layer)
        return;
    int graphs = layersBox->value();
    int old_graphs = multi_layer->layers();
    int dn = multi_layer->layers() - graphs;
    if (dn > 0
        && QMessageBox::question(0, tr("Delete Layers?"),
                                 tr("You are about to delete %1 existing layers.").arg(dn) + "\n"
                                         + tr("Are you sure you want to continue this operation?"),
                                 tr("&Continue"), tr("&Cancel"), QString(), 0, 1))
        return;

    multi_layer->setLayersNumber(graphs);

    if (!graphs)
        return;

    if (dn < 0) { // Customize new layers with user default settings
        ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
        if (app) {
            for (int i = old_graphs + 1; i <= graphs; i++)
                app->setPreferences(multi_layer->layer(i));
        }
    }

    int cols = boxX->value();
    int rows = boxY->value();

    if (cols > graphs && !fitBox->isChecked()) {
        QMessageBox::about(0, tr("Columns input error"),
                           tr("The number of columns you've entered is greater than the number of "
                              "graphs (%1)!")
                                   .arg(graphs));
        boxX->setFocus();
        return;
    }

    if (rows > graphs && !fitBox->isChecked()) {
        QMessageBox::about(
                0, tr("Rows input error"),
                tr("The number of rows you've entered is greater than the number of graphs (%1)!")
                        .arg(graphs));
        boxY->setFocus();
        return;
    }

    if (!fitBox->isChecked()) {
        multi_layer->setCols(cols);
        multi_layer->setRows(rows);
    }

    multi_layer->setKeepAspectRatio(keepAspectBox->isChecked());
    if (GroupCanvasSize->isChecked()) {
        multi_layer->setLayerCanvasSize(boxCanvasWidth->value(), boxCanvasHeight->value());
        if (boxCanvasWidth->value() > 0 && boxCanvasHeight->value() > 0)
            customAspectBox->setValue((double)boxCanvasWidth->value()
                                      / (double)boxCanvasHeight->value());
    } else if (keepAspectBox->isChecked() && customAspectBox->value() > 0.05) {
        multi_layer->setTargetAspectRatio(customAspectBox->value());
    }

    multi_layer->setAlignement(alignHorBox->currentIndex(), alignVertBox->currentIndex());

    multi_layer->setMargins(boxLeftSpace->value(), boxRightSpace->value(), boxTopSpace->value(),
                            boxBottomSpace->value());

    multi_layer->setSpacing(boxRowsGap->value(), boxColsGap->value());
    multi_layer->arrangeLayers(fitBox->isChecked(), GroupCanvasSize->isChecked());

    if (!GroupCanvasSize->isChecked()) { // show new layer canvas size
        boxCanvasWidth->setValue(multi_layer->layerCanvasSize().width());
        boxCanvasHeight->setValue(multi_layer->layerCanvasSize().height());
    }

    if (fitBox->isChecked()) { // show new grid settings
        boxX->setValue(multi_layer->getCols());
        boxY->setValue(multi_layer->getRows());
    }

    const int targetNum = targetLayerBox->currentData().toInt();
    if (Graph *targetG = multi_layer->layer(targetNum)) {
        const int parentNum = parentLayerBox->currentData().toInt();
        Graph *parentG = (parentNum > 0 && parentNum != targetNum) ? multi_layer->layer(parentNum)
                                                                   : nullptr;
        targetG->setLinkedParentLayer(parentG, xLinkModeBox->currentData().toInt(),
                                      xFormulaEdit->text(), yLinkModeBox->currentData().toInt(),
                                      yFormulaEdit->text());
        const QRect desiredGeo(layerGeoXBox->value(), layerGeoYBox->value(), layerGeoWBox->value(),
                               layerGeoHBox->value());
        if (desiredGeo != targetG->geometry() && desiredGeo.width() >= 40
            && desiredGeo.height() >= 40) {
            targetG->setGeometry(desiredGeo);
            if (targetG->plotWidget()) {
                targetG->plotWidget()->resize(desiredGeo.size());
                targetG->plotWidget()->updateLayout();
                targetG->plotWidget()->replot();
            }
        }
    }
    refreshLayerSelectors();
}

void LayerDialog::accept()
{
    update();
    close();
}
