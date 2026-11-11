/***************************************************************************
    File                 : SmoothCurveDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Smoothing options dialog

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
#include "SmoothCurveDialog.h"
#include "Graph.h"
#include "MyParser.h"
#include "ColorButton.h"
#include "SmoothFilter.h"

#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QLayout>

SmoothCurveDialog::SmoothCurveDialog(int method, QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl), graph(nullptr)
{
    if (method < SmoothFilter::SavitzkyGolay || method > SmoothFilter::Median)
        method = SmoothFilter::SavitzkyGolay;
    smooth_method = method;

    setWindowTitle(tr("Smoothing Options"));

    QGroupBox *gb1 = new QGroupBox();
    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("Curve")), 0, 0);

    boxName = new QComboBox();
    gl1->addWidget(boxName, 0, 1);

    gl1->addWidget(new QLabel(tr("Method")), 1, 0);
    boxMethod = new QComboBox();
    boxMethod->addItem(tr("Savitzky-Golay"), static_cast<int>(SmoothFilter::SavitzkyGolay));
    boxMethod->addItem(tr("FFT Filter"), static_cast<int>(SmoothFilter::FFT));
    boxMethod->addItem(tr("Moving Window Average"), static_cast<int>(SmoothFilter::Average));
    boxMethod->addItem(tr("LOWESS (Locally Weighted)"), static_cast<int>(SmoothFilter::LOWESS));
    boxMethod->addItem(tr("Whittaker (Penalized LS)"), static_cast<int>(SmoothFilter::Whittaker));
    boxMethod->addItem(tr("Median Filter"), static_cast<int>(SmoothFilter::Median));
    boxMethod->setCurrentIndex(smooth_method - 1);
    gl1->addWidget(boxMethod, 1, 1);

    lblOrder = new QLabel(tr("Polynomial Order"));
    boxOrder = new QSpinBox();
    boxOrder->setRange(0, 9);
    boxOrder->setValue(2);
    gl1->addWidget(lblOrder, 2, 0);
    gl1->addWidget(boxOrder, 2, 1);

    lblPointsLeft = new QLabel(tr("Points to the Left"));
    boxPointsLeft = new QSpinBox();
    boxPointsLeft->setRange(1, 1000000);
    boxPointsLeft->setValue(smooth_method == SmoothFilter::SavitzkyGolay ? 2 : 5);
    gl1->addWidget(lblPointsLeft, 3, 0);
    gl1->addWidget(boxPointsLeft, 3, 1);

    lblPointsRight = new QLabel(tr("Points to the Right"));
    boxPointsRight = new QSpinBox();
    boxPointsRight->setRange(1, 25);
    boxPointsRight->setValue(2);
    gl1->addWidget(lblPointsRight, 4, 0);
    gl1->addWidget(boxPointsRight, 4, 1);

    lblLowessSpan = new QLabel(tr("LOWESS Span Fraction"));
    boxLowessSpan = new QDoubleSpinBox();
    boxLowessSpan->setRange(0.01, 1.00);
    boxLowessSpan->setDecimals(3);
    boxLowessSpan->setSingleStep(0.05);
    boxLowessSpan->setValue(0.20);
    gl1->addWidget(lblLowessSpan, 5, 0);
    gl1->addWidget(boxLowessSpan, 5, 1);

    lblWhittakerLambda = new QLabel(tr("Whittaker Penalty (Lambda)"));
    boxWhittakerLambda = new QDoubleSpinBox();
    boxWhittakerLambda->setRange(0.01, 1e9);
    boxWhittakerLambda->setDecimals(2);
    boxWhittakerLambda->setSingleStep(10.0);
    boxWhittakerLambda->setValue(100.0);
    gl1->addWidget(lblWhittakerLambda, 6, 0);
    gl1->addWidget(boxWhittakerLambda, 6, 1);

    btnColor = new ColorButton();
    btnColor->setColor(QColor(Qt::red));
    gl1->addWidget(new QLabel(tr("Color")), 7, 0);
    gl1->addWidget(btnColor, 7, 1);
    gl1->setRowStretch(8, 1);
    gl1->setColumnStretch(2, 1);

    btnSmooth = new QPushButton(tr("&Smooth"));
    btnSmooth->setDefault(true);
    buttonCancel = new QPushButton(tr("&Close"));

    QVBoxLayout *vl = new QVBoxLayout();
    vl->addWidget(btnSmooth);
    vl->addWidget(buttonCancel);
    vl->addStretch();

    QHBoxLayout *hb = new QHBoxLayout(this);
    hb->addWidget(gb1);
    hb->addLayout(vl);

    connect(btnSmooth, SIGNAL(clicked()), this, SLOT(smooth()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(boxName, SIGNAL(activated(const QString &)), this,
            SLOT(activateCurve(const QString &)));
    connect(boxMethod, SIGNAL(currentIndexChanged(int)), this,
            SLOT(updateMethodControls(int)));

    updateMethodControls(boxMethod->currentIndex());
}

void SmoothCurveDialog::updateMethodControls(int methodIndex)
{
    smooth_method = boxMethod->itemData(methodIndex).toInt();
    if (smooth_method < SmoothFilter::SavitzkyGolay || smooth_method > SmoothFilter::Median)
        smooth_method = SmoothFilter::SavitzkyGolay;

    const bool isSavGol = (smooth_method == SmoothFilter::SavitzkyGolay);
    const bool isWindowPts = (smooth_method == SmoothFilter::FFT
                              || smooth_method == SmoothFilter::Average
                              || smooth_method == SmoothFilter::Median);
    const bool isLowess = (smooth_method == SmoothFilter::LOWESS);
    const bool isWhittaker = (smooth_method == SmoothFilter::Whittaker);

    lblOrder->setVisible(isSavGol);
    boxOrder->setVisible(isSavGol);

    lblPointsLeft->setVisible(isSavGol || isWindowPts);
    boxPointsLeft->setVisible(isSavGol || isWindowPts);
    lblPointsLeft->setText(isSavGol ? tr("Points to the Left") : tr("Points"));
    if (isSavGol) {
        boxPointsLeft->setMaximum(25);
        boxPointsLeft->setSingleStep(1);
    } else if (isWindowPts) {
        boxPointsLeft->setMaximum(1000000);
        boxPointsLeft->setSingleStep(smooth_method == SmoothFilter::FFT ? 10 : 2);
    }

    lblPointsRight->setVisible(isSavGol);
    boxPointsRight->setVisible(isSavGol);

    lblLowessSpan->setVisible(isLowess);
    boxLowessSpan->setVisible(isLowess);

    lblWhittakerLambda->setVisible(isWhittaker);
    boxWhittakerLambda->setVisible(isWhittaker);

    if (graph && boxName->count() > 0)
        activateCurve(boxName->currentText());
}

void SmoothCurveDialog::smooth()
{
    if (!graph || boxName->currentText().isEmpty())
        return;

    SmoothFilter *sf = new SmoothFilter((ApplicationWindow *)this->parent(), graph,
                                        boxName->currentText(), smooth_method);
    if (smooth_method == SmoothFilter::SavitzkyGolay) {
        sf->setSmoothPoints(boxPointsLeft->value(), boxPointsRight->value());
        sf->setPolynomOrder(boxOrder->value());
    } else if (smooth_method == SmoothFilter::LOWESS) {
        sf->setLowessSpan(boxLowessSpan->value());
    } else if (smooth_method == SmoothFilter::Whittaker) {
        sf->setWhittakerLambda(boxWhittakerLambda->value());
    } else {
        sf->setSmoothPoints(boxPointsLeft->value());
    }

    sf->setColor(btnColor->color());
    sf->run();
    delete sf;
}

void SmoothCurveDialog::setGraph(Graph *g)
{
    graph = g;
    if (!g)
        return;
    boxName->clear();
    boxName->addItems(g->analysableCurvesList());
    activateCurve(boxName->currentText());
}

void SmoothCurveDialog::activateCurve(const QString &curveName)
{
    if (!graph)
        return;
    if (smooth_method == SmoothFilter::Average || smooth_method == SmoothFilter::Median) {
        QwtPlotCurve *c = graph->curve(curveName);
        if (!c || c->rtti() != QwtPlotItem::Rtti_PlotCurve)
            return;

        boxPointsLeft->setMaximum(qMax(1, c->dataSize() / 2));
    }
}
