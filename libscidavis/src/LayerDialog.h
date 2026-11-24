/***************************************************************************
    File                 : LayerDialog.h
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
#ifndef LAYERDIALOG_H
#define LAYERDIALOG_H

#include "MultiLayer.h"

class QGroupBox;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;

//! Arrange layers dialog
class LayerDialog : public QDialog
{
    Q_OBJECT

public:
    LayerDialog(QWidget *parent = 0, Qt::WindowFlags fl = Qt::Widget);
    void setMultiLayer(MultiLayer *g);

protected slots:
    void accept() override;
    void done(int r) override;
    void update();
    void enableLayoutOptions(bool ok);
    void onAspectPresetChanged(int index);
    void onTargetLayerChanged(int index);
    void onLinkModeChanged();

private:
    void refreshLayerSelectors();
    //! Fill the X/Y/W/H spin boxes from 'geo' without marking them as user-edited.
    void loadLayerGeometryFields(const QRect &geo);
    //! Number of layers that occupy a grid cell once 'layerCount' layers exist (linked overlay
    //! layers and insets do not).
    int gridCellLayerCount(int layerCount) const;

    MultiLayer *multi_layer;

    QPushButton *buttonOk;
    QPushButton *buttonCancel;
    QPushButton *buttonApply;
    QGroupBox *GroupCanvasSize, *GroupGrid, *GroupLinkAxes;
    QSpinBox *boxX, *boxY, *boxColsGap, *boxRowsGap;
    QSpinBox *boxRightSpace, *boxLeftSpace, *boxTopSpace, *boxBottomSpace;
    QSpinBox *boxCanvasWidth, *boxCanvasHeight, *layersBox;
    QCheckBox *fitBox;
    QCheckBox *keepAspectBox;
    QComboBox *aspectPresetBox;
    QDoubleSpinBox *customAspectBox;
    QComboBox *alignHorBox, *alignVertBox;

    QComboBox *targetLayerBox;
    QComboBox *parentLayerBox;
    QComboBox *xLinkModeBox;
    QLineEdit *xFormulaEdit;
    QComboBox *yLinkModeBox;
    QLineEdit *yFormulaEdit;
    QSpinBox *layerGeoXBox, *layerGeoYBox, *layerGeoWBox, *layerGeoHBox;

    //! True only after the user edited one of the X/Y/W/H geometry spin boxes of the target layer
    //! since they were last loaded from the layer (P0/B5-1). update() applies the geometry
    //! spin boxes to the layer only in that case, so that the (stale) values loaded before an
    //! arrangeLayers() never overwrite the freshly arranged position.
    bool userEditedGeo = false;
    //! Set while the geometry spin boxes are filled programmatically (not a user edit).
    bool loadingGeo = false;
};

#endif
