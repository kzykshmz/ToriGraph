/***************************************************************************
    File                 : CurveRangeDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief
    Email (use @ for *)  : ion_vasilief*yahoo.fr
    Description          : Curve range dialog

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
#include "CurveRangeDialog.h"
#include "PlotCurve.h"
#include "Graph.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "Table.h"

#include <QGroupBox>
#include <QMessageBox>
#include <QLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>

CurveRangeDialog::CurveRangeDialog(QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl), d_curve(nullptr)
{
    setWindowTitle(tr("Plot range"));

    QGroupBox *gb1 = new QGroupBox();
    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("Data set: ")), 0, 0);

    boxName = new QLabel();
    gl1->addWidget(boxName, 0, 1);

    gl1->addWidget(new QLabel(tr("From row number")), 1, 0);
    boxStart = new QSpinBox();
    boxStart->setMinimum(1);
    gl1->addWidget(boxStart, 1, 1);

    gl1->addWidget(new QLabel(tr("To row number")), 2, 0);
    boxEnd = new QSpinBox();
    boxEnd->setMinimum(1);
    gl1->addWidget(boxEnd, 2, 1);
    gl1->setRowStretch(3, 1);

    buttonOK = new QPushButton(tr("&OK"));
    buttonOK->setDefault(true);
    buttonCancel = new QPushButton(tr("&Close"));

    QHBoxLayout *hl = new QHBoxLayout();
    hl->addStretch();
    hl->addWidget(buttonOK);
    hl->addWidget(buttonCancel);

    QVBoxLayout *vb = new QVBoxLayout(this);
    vb->addWidget(gb1);
    vb->addLayout(hl);

    connect(buttonOK, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
}

CurveRangeDialog::~CurveRangeDialog() = default;

void CurveRangeDialog::accept()
{
    if (!d_graph || !d_curve || (d_hadTable && !d_table) || !d_graph->plotWidget()
        || !d_graph->plotWidget()->itemList().contains(d_curve)
        || d_graph->curveIndex(d_curve) < 0
        || d_curve->title().text() != d_curveTitle) {
        close();
        return;
    }

    MultiLayer *ml = d_graph->multiLayerParent();
    const QString before = ml ? ml->captureUndoSnapshot() : QString();

    int start = boxStart->value() - 1;
    int end = boxEnd->value() - 1;
    d_curve->setRowRange(qMin(start, end), qMax(start, end));
    d_graph->updatePlot();
    d_graph->notifyChanges();
    if (ml && !before.isEmpty())
        ml->pushUndoSnapshot(tr("Change Curve Range"), before);
    close();
}

void CurveRangeDialog::setCurveToModify(Graph *g, int curve)
{
    d_graph = g;
    d_curve = nullptr;
    d_table = nullptr;
    d_hadTable = false;
    d_curveTitle.clear();
    if (!d_graph || curve < 0 || curve >= d_graph->curves())
        return;

    d_curve = dynamic_cast<DataCurve *>(d_graph->curve(curve));
    if (!d_curve)
        return;

    d_curveTitle = d_curve->title().text();

    Table *t = d_curve->table();
    d_table = t;
    d_hadTable = (t != nullptr);
    if (t) {
        boxStart->setMaximum(t->numRows());
        boxEnd->setMaximum(t->numRows());
    }

    boxName->setText(d_curveTitle);
    boxStart->setValue(d_curve->startRow() + 1);
    boxEnd->setValue(d_curve->endRow() + 1);
}
