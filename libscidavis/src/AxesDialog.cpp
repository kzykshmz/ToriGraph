/***************************************************************************
    File                 : AxesDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : General plot options dialog

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
#include "ApplicationWindow.h"
#include "AxesDialog.h"
#include "TextDialog.h"
#include "Grid.h"
#include "Plot.h"
#include "MyParser.h"
#include "ColorButton.h"
#include "TextFormatButtons.h"

#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QLayout>
#include <QMessageBox>
#include <QFontDialog>
#include <QDate>
#include <QList>
#include <QListWidget>
#include <QVector>
#include <QGroupBox>
#include <QRadioButton>

#include <qwt_plot.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>

#ifndef M_PI
#define M_PI 3.141592653589793238462643
#endif

AxesDialog::AxesDialog()
    : xBottomFont(QFont("Arial", 12, QFont::Normal, false)),
      yLeftFont(QFont("Arial", 12, QFont::Normal, false)),
      xTopFont(QFont("Arial", 12, QFont::Normal, false)),
      yRightFont(QFont("Arial", 12, QFont::Normal, false)),
      xAxisOn(true),
      yAxisOn(true),
      topAxisOn(false),
      rightAxisOn(false),
      xBottomLabelsRotation(0),
      xTopLabelsRotation(0),
      d_graph(nullptr)
{
    QPixmap image4(":/image4.xpm");
    QPixmap image5(":/image5.xpm");
    QPixmap image6(":/image6.xpm");
    QPixmap image7(":/image7.xpm");
    setWindowTitle(tr("General Plot Options"));

    initScalesPage();
    initGridPage();
    initAxesPage();
    initFramePage();
    initReferenceLinesPage();
    initAxisBreakPage();

    auto bottomButtons = new QHBoxLayout();
    bottomButtons->addStretch();

    buttonApply = new QPushButton();
    buttonApply->setText(tr("&Apply"));
    bottomButtons->addWidget(buttonApply);

    buttonOk = new QPushButton();
    buttonOk->setText(tr("&OK"));
    buttonOk->setDefault(true);
    bottomButtons->addWidget(buttonOk);

    buttonCancel = new QPushButton();
    buttonCancel->setText(tr("&Cancel"));
    bottomButtons->addWidget(buttonCancel);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(&generalDialog);
    mainLayout->addLayout(bottomButtons);

    lastPage = scalesPage;

    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(buttonApply, SIGNAL(clicked()), this, SLOT(updatePlot()));
    connect(&generalDialog, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));
}

void AxesDialog::initScalesPage()
{
    scalesPage = new QWidget();

    QGroupBox *middleBox = new QGroupBox(QString());
    QGridLayout *middleLayout = new QGridLayout(middleBox);

    middleLayout->addWidget(new QLabel(tr("From")), 0, 0);
    boxStart = new QLineEdit();
    middleLayout->addWidget(boxStart, 0, 1);

    middleLayout->addWidget(new QLabel(tr("To")), 1, 0);
    boxEnd = new QLineEdit();
    middleLayout->addWidget(boxEnd, 1, 1);

    boxScaleTypeLabel = new QLabel(tr("Type"));
    boxScaleType = new QComboBox();
    boxScaleType->addItem(tr("linear"));
    boxScaleType->addItem(tr("logarithmic"));
    middleLayout->addWidget(boxScaleTypeLabel, 2, 0);
    middleLayout->addWidget(boxScaleType, 2, 1);

    btnInvert = new QCheckBox();
    btnInvert->setText(tr("Inverted"));
    btnInvert->setChecked(false);
    middleLayout->addWidget(btnInvert, 3, 1);

    middleLayout->setRowStretch(4, 1);

    QGroupBox *rightBox = new QGroupBox(QString());
    QGridLayout *rightLayout = new QGridLayout(rightBox);
    QWidget *stepWidget = new QWidget();
    QVBoxLayout *stepWidgetLayout = new QVBoxLayout(stepWidget);

    btnStep = new QRadioButton(rightBox);
    btnStep->setText(tr("Step"));
    btnStep->setChecked(true);
    rightLayout->addWidget(btnStep, 0, 0);

    boxStep = new QLineEdit();
    stepWidgetLayout->addWidget(boxStep);
    boxUnit = new QComboBox();
    boxUnit->hide();
    stepWidgetLayout->addWidget(boxUnit);

    rightLayout->addWidget(stepWidget, 0, 1);

    btnMajor = new QRadioButton(rightBox);
    btnMajor->setText(tr("Major Ticks"));
    rightLayout->addWidget(btnMajor, 1, 0);

    boxMajorValue = new QSpinBox();
    boxMajorValue->setDisabled(true);
    rightLayout->addWidget(boxMajorValue, 1, 1);

    minorBoxLabel = new QLabel(tr("Minor Ticks"));
    rightLayout->addWidget(minorBoxLabel, 2, 0);

    boxMinorValue = new QComboBox();
    boxMinorValue->setEditable(true);
    boxMinorValue->addItems(QStringList() << "0"
                                          << "1"
                                          << "4"
                                          << "9"
                                          << "14"
                                          << "19");
    rightLayout->addWidget(boxMinorValue, 2, 1);

    rightLayout->setRowStretch(3, 1);

    QPixmap image0(":/bottom_scl.xpm");
    QPixmap image1(":/left_scl.xpm");
    QPixmap image2(":/top_scl.xpm");
    QPixmap image3(":/right_scl.xpm");

    axesList = new QListWidget();
    axesList->addItem(new QListWidgetItem(image0, tr("Bottom")));
    axesList->addItem(new QListWidgetItem(image1, tr("Left")));
    axesList->addItem(new QListWidgetItem(image2, tr("Top")));
    axesList->addItem(new QListWidgetItem(image3, tr("Right")));
    axesList->setIconSize(image0.size());
    axesList->setCurrentRow(-1);

    // calculate a sensible width for the items list
    // (default QListWidget size is 256 which looks too big)
    QFontMetrics fm(axesList->font());
    int width = 32, i;
    for (i = 0; i < axesList->count(); i++) {
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
        auto newWidth = fm.width(axesList->item(i)->text());
#else
        auto newWidth = fm.horizontalAdvance(axesList->item(i)->text());
#endif
        if (newWidth > width)
            width = newWidth;
    }

    axesList->setMaximumWidth(axesList->iconSize().width() + width + 50);
    // resize the list to the maximum width
    axesList->resize(axesList->maximumWidth(), axesList->height());

    QHBoxLayout *mainLayout = new QHBoxLayout(scalesPage);
    mainLayout->addWidget(axesList);
    mainLayout->addWidget(middleBox);
    mainLayout->addWidget(rightBox);

    generalDialog.addTab(scalesPage, tr("Scale"));

    connect(btnInvert, SIGNAL(clicked()), this, SLOT(updatePlot()));
    connect(axesList, SIGNAL(currentRowChanged(int)), this, SLOT(updateScale()));
    connect(boxScaleType, SIGNAL(activated(int)), this, SLOT(updateMinorTicksList(int)));

    connect(btnStep, SIGNAL(toggled(bool)), boxStep, SLOT(setEnabled(bool)));
    connect(btnStep, SIGNAL(toggled(bool)), boxUnit, SLOT(setEnabled(bool)));
    connect(btnMajor, SIGNAL(toggled(bool)), boxMajorValue, SLOT(setEnabled(bool)));
}

void AxesDialog::initGridPage()
{
    gridPage = new QWidget();

    QGroupBox *rightBox = new QGroupBox(QString());
    QGridLayout *rightLayout = new QGridLayout(rightBox);

    boxMajorGrid = new QCheckBox();
    boxMajorGrid->setText(tr("Major Grids"));
    boxMajorGrid->setChecked(true);
    rightLayout->addWidget(boxMajorGrid, 0, 1);

    boxMinorGrid = new QCheckBox();
    boxMinorGrid->setText(tr("Minor Grids"));
    boxMinorGrid->setChecked(false);
    rightLayout->addWidget(boxMinorGrid, 0, 2);

    rightLayout->addWidget(new QLabel(tr("Line Color")), 1, 0);

    boxColorMajor = new ColorButton();
    rightLayout->addWidget(boxColorMajor, 1, 1);

    boxColorMinor = new ColorButton();
    boxColorMinor->setDisabled(true);
    rightLayout->addWidget(boxColorMinor, 1, 2);

    rightLayout->addWidget(new QLabel(tr("Line Type")), 2, 0);

    boxTypeMajor = new QComboBox();
    boxTypeMajor->addItem("_____");
    boxTypeMajor->addItem("- - -");
    boxTypeMajor->addItem(".....");
    boxTypeMajor->addItem("_._._");
    boxTypeMajor->addItem("_.._..");
    rightLayout->addWidget(boxTypeMajor, 2, 1);

    boxTypeMinor = new QComboBox();
    boxTypeMinor->addItem("_____");
    boxTypeMinor->addItem("- - -");
    boxTypeMinor->addItem(".....");
    boxTypeMinor->addItem("_._._");
    boxTypeMinor->addItem("_.._..");
    boxTypeMinor->setDisabled(true);
    rightLayout->addWidget(boxTypeMinor, 2, 2);

    rightLayout->addWidget(new QLabel(tr("Thickness")), 3, 0);

    boxWidthMajor = new QSpinBox();
    boxWidthMajor->setRange(1, 20);
    boxWidthMajor->setValue(1);
    rightLayout->addWidget(boxWidthMajor, 3, 1);

    boxWidthMinor = new QSpinBox();
    boxWidthMinor->setRange(1, 20);
    boxWidthMinor->setValue(1);
    boxWidthMinor->setDisabled(true);
    rightLayout->addWidget(boxWidthMinor, 3, 2);

    rightLayout->addWidget(new QLabel(tr("Axes")), 4, 0);

    boxGridXAxis = new QComboBox();
    boxGridXAxis->addItem(tr("Bottom"));
    boxGridXAxis->addItem(tr("Top"));
    rightLayout->addWidget(boxGridXAxis, 4, 1);

    boxGridYAxis = new QComboBox();
    boxGridYAxis->addItem(tr("Left"));
    boxGridYAxis->addItem(tr("Right"));
    rightLayout->addWidget(boxGridYAxis, 4, 2);

    rightLayout->addWidget(new QLabel(tr("Additional lines")), 5, 0);

    boxXLine = new QCheckBox();
    boxXLine->setText(tr("X=0"));
    boxXLine->setDisabled(true);
    rightLayout->addWidget(boxXLine, 5, 1);

    boxYLine = new QCheckBox();
    boxYLine->setText(tr("Y=0"));
    rightLayout->addWidget(boxYLine, 5, 2);

    rightLayout->setRowStretch(6, 1);
    rightLayout->setColumnStretch(4, 1);

    QPixmap image2(":/image2.xpm");
    QPixmap image3(":/image3.xpm");

    axesGridList = new QListWidget();
    axesGridList->addItem(new QListWidgetItem(image3, tr("Horizontal")));
    axesGridList->addItem(new QListWidgetItem(image2, tr("Vertical")));
    axesGridList->setIconSize(image3.size());
    axesGridList->setCurrentRow(-1);

    // calculate a sensible width for the items list
    // (default QListWidget size is 256 which looks too big)
    QFontMetrics fm(axesGridList->font());
    int width = 32, i;
    for (i = 0; i < axesGridList->count(); i++) {
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
        auto newWidth = fm.width(axesGridList->item(i)->text());
#else
        auto newWidth = fm.horizontalAdvance(axesGridList->item(i)->text());
#endif
        if (newWidth > width)
            width = newWidth;
    }
    axesGridList->setMaximumWidth(axesGridList->iconSize().width() + width + 50);
    // resize the list to the maximum width
    axesGridList->resize(axesGridList->maximumWidth(), axesGridList->height());

    QHBoxLayout *mainLayout2 = new QHBoxLayout(gridPage);
    mainLayout2->addWidget(axesGridList);
    mainLayout2->addWidget(rightBox);

    generalDialog.addTab(gridPage, tr("Grid"));

    // grid page slot connections
    connect(axesGridList, SIGNAL(currentRowChanged(int)), this, SLOT(showGridOptions(int)));

    connect(boxMajorGrid, SIGNAL(toggled(bool)), this, SLOT(majorGridEnabled(bool)));
    connect(boxMinorGrid, SIGNAL(toggled(bool)), this, SLOT(minorGridEnabled(bool)));
    connect(boxColorMajor, SIGNAL(changed(QColor)), this, SLOT(updateGrid()));
    connect(boxColorMinor, SIGNAL(changed(QColor)), this, SLOT(updateGrid()));
    connect(boxTypeMajor, SIGNAL(activated(int)), this, SLOT(updateGrid()));
    connect(boxTypeMinor, SIGNAL(activated(int)), this, SLOT(updateGrid()));
    connect(boxWidthMajor, SIGNAL(valueChanged(int)), this, SLOT(updateGrid()));
    connect(boxWidthMinor, SIGNAL(valueChanged(int)), this, SLOT(updateGrid()));
    connect(boxXLine, SIGNAL(clicked()), this, SLOT(updatePlot()));
    connect(boxYLine, SIGNAL(clicked()), this, SLOT(updatePlot()));
}

void AxesDialog::initAxesPage()
{
    // axes page
    QPixmap image4(":/image4.xpm");
    QPixmap image5(":/image5.xpm");
    QPixmap image6(":/image6.xpm");
    QPixmap image7(":/image7.xpm");

    axesPage = new QWidget();

    boxAxisType = new QComboBox();
    boxAxisType->addItem(tr("Numeric"), (int)Graph::AxisType::Numeric);
    boxAxisType->addItem(tr("Text from table"), (int)Graph::AxisType::Txt);
    boxAxisType->addItem(tr("Day of the week"), (int)Graph::AxisType::Day);
    boxAxisType->addItem(tr("Month"), (int)Graph::AxisType::Month);
    boxAxisType->addItem(tr("Time"), (int)Graph::AxisType::Time);
    boxAxisType->addItem(tr("Date"), (int)Graph::AxisType::Date);
    boxAxisType->addItem(tr("Date & Time"), (int)Graph::AxisType::DateTime);
    boxAxisType->addItem(tr("Column Headings"), (int)Graph::AxisType::ColHeader);

    axesTitlesList = new QListWidget();
    axesTitlesList->addItem(new QListWidgetItem(image4, tr("Bottom")));
    axesTitlesList->addItem(new QListWidgetItem(image5, tr("Left")));
    axesTitlesList->addItem(new QListWidgetItem(image6, tr("Top")));
    axesTitlesList->addItem(new QListWidgetItem(image7, tr("Right")));
    axesTitlesList->setIconSize(image6.size());
    axesTitlesList->setMaximumWidth((int)(image6.width() * 1.5));
    axesTitlesList->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    axesTitlesList->setCurrentRow(-1);

    // calculate a sensible width for the items list
    // (default QListWidget size is 256 which looks too big)
    QFontMetrics fm(axesTitlesList->font());
    int width = 32, i;
    for (i = 0; i < axesTitlesList->count(); i++) {
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
        auto newWidth = fm.width(axesTitlesList->item(i)->text());
#else
        auto newWidth = fm.horizontalAdvance(axesTitlesList->item(i)->text());
#endif
        if (newWidth > width)
            width = newWidth;
    }
    axesTitlesList->setMaximumWidth(axesTitlesList->iconSize().width() + width + 50);
    // resize the list to the maximum width
    axesTitlesList->resize(axesTitlesList->maximumWidth(), axesTitlesList->height());

    QHBoxLayout *topLayout = new QHBoxLayout();

    QVBoxLayout *showAxisCol = new QVBoxLayout();
    boxShowAxis = new QCheckBox(tr("Show"));
    boxShowAxis->setChecked(true);
    showAxisCol->addWidget(boxShowAxis);
    boxAxisBackbone = new QCheckBox(tr("Show Axis Line (Backbone)"));
    boxAxisBackbone->setChecked(true);
    showAxisCol->addWidget(boxAxisBackbone);
    showAxisCol->addStretch();
    topLayout->addLayout(showAxisCol);

    labelBox = new QGroupBox(tr("Title"));
    topLayout->addWidget(labelBox);

    QVBoxLayout *labelBoxLayout = new QVBoxLayout(labelBox);
    labelBoxLayout->setSpacing(2);

    boxTitle = new QTextEdit();
    boxTitle->setAcceptRichText(false);
    QFontMetrics metrics(this->font());
    boxTitle->setMaximumHeight(3 * metrics.height());
    labelBoxLayout->addWidget(boxTitle);

    QHBoxLayout *hl = new QHBoxLayout();
    hl->setMargin(0);
    hl->setSpacing(2);
    buttonLabelFont = new QPushButton(tr("&Font"));
    hl->addWidget(buttonLabelFont);

    formatButtons = new TextFormatButtons(boxTitle);
    formatButtons->toggleCurveButton(false);
    hl->addWidget(formatButtons);
    hl->addStretch();

    boxTitle->setMaximumWidth(buttonLabelFont->width() + formatButtons->width());
    labelBoxLayout->addLayout(hl);

    QHBoxLayout *bottomLayout = new QHBoxLayout();

    QGroupBox *leftBox = new QGroupBox(QString());
    bottomLayout->addWidget(leftBox);
    QGridLayout *leftBoxLayout = new QGridLayout(leftBox);

    leftBoxLayout->addWidget(new QLabel(tr("Type")), 0, 0);

    leftBoxLayout->addWidget(boxAxisType, 0, 1);

    leftBoxLayout->addWidget(new QLabel(tr("Font")), 1, 0);

    btnAxesFont = new QPushButton();
    btnAxesFont->setText(tr("Axis &Font"));
    leftBoxLayout->addWidget(btnAxesFont, 1, 1);

    leftBoxLayout->addWidget(new QLabel(tr("Color")), 2, 0);
    boxAxisColor = new ColorButton();
    leftBoxLayout->addWidget(boxAxisColor, 2, 1);

    leftBoxLayout->addWidget(new QLabel(tr("Major Ticks")), 3, 0);

    boxMajorTicksType = new QComboBox();
    boxMajorTicksType->addItem(tr("None"));
    boxMajorTicksType->addItem(tr("Out"));
    boxMajorTicksType->addItem(tr("In & Out"));
    boxMajorTicksType->addItem(tr("In"));
    leftBoxLayout->addWidget(boxMajorTicksType, 3, 1);

    leftBoxLayout->addWidget(new QLabel(tr("Minor Ticks")), 4, 0);

    boxMinorTicksType = new QComboBox();
    boxMinorTicksType->addItem(tr("None"));
    boxMinorTicksType->addItem(tr("Out"));
    boxMinorTicksType->addItem(tr("In & Out"));
    boxMinorTicksType->addItem(tr("In"));
    leftBoxLayout->addWidget(boxMinorTicksType, 4, 1);

    leftBoxLayout->addWidget(new QLabel(tr("Stand-off")), 5, 0);
    boxBaseline = new QSpinBox();
    boxBaseline->setRange(0, 1000);
    leftBoxLayout->addWidget(boxBaseline);

    boxShowLabels = new QGroupBox(tr("Show Labels"));
    boxShowLabels->setCheckable(true);
    boxShowLabels->setChecked(true);

    bottomLayout->addWidget(boxShowLabels);
    QGridLayout *rightBoxLayout = new QGridLayout(boxShowLabels);

    label1 = new QLabel(tr("Column"));
    rightBoxLayout->addWidget(label1, 0, 0);

    boxColName = new QComboBox();
    rightBoxLayout->addWidget(boxColName, 0, 1);

    labelTable = new QLabel(tr("Table"));
    rightBoxLayout->addWidget(labelTable, 1, 0);

    boxTableName = new QComboBox();
    rightBoxLayout->addWidget(boxTableName, 1, 1);

    label2 = new QLabel(tr("Format"));
    rightBoxLayout->addWidget(label2, 2, 0);

    boxFormat = new QComboBox();
    boxFormat->setDuplicatesEnabled(false);
    rightBoxLayout->addWidget(boxFormat, 2, 1);

    label3 = new QLabel(tr("Precision"));
    rightBoxLayout->addWidget(label3, 3, 0);
    boxPrecision = new QSpinBox();
    boxPrecision->setRange(0, 10);
    rightBoxLayout->addWidget(boxPrecision, 3, 1);

    rightBoxLayout->addWidget(new QLabel(tr("Angle")), 4, 0);

    boxAngle = new QSpinBox();
    boxAngle->setRange(-90, 90);
    boxAngle->setSingleStep(5);
    rightBoxLayout->addWidget(boxAngle, 4, 1);

    rightBoxLayout->addWidget(new QLabel(tr("Color")), 5, 0);
    boxAxisNumColor = new ColorButton();
    rightBoxLayout->addWidget(boxAxisNumColor, 5, 1);

    boxShowFormula = new QCheckBox(tr("For&mula"));
    rightBoxLayout->addWidget(boxShowFormula, 6, 0);

    boxFormula = new QTextEdit();
    boxFormula->setAcceptRichText(false);
    boxFormula->setMaximumHeight(3 * metrics.height());
    boxFormula->hide();
    rightBoxLayout->addWidget(boxFormula, 6, 1);

    labelDivideFactor = new QLabel(tr("Divide by Factor:"));
    rightBoxLayout->addWidget(labelDivideFactor, 7, 0);
    boxDivideFactor = new QLineEdit("1");
    rightBoxLayout->addWidget(boxDivideFactor, 7, 1);

    labelTickPrefix = new QLabel(tr("Prefix:"));
    rightBoxLayout->addWidget(labelTickPrefix, 8, 0);
    boxTickPrefix = new QLineEdit();
    rightBoxLayout->addWidget(boxTickPrefix, 8, 1);

    labelTickSuffix = new QLabel(tr("Suffix:"));
    rightBoxLayout->addWidget(labelTickSuffix, 9, 0);
    boxTickSuffix = new QLineEdit();
    rightBoxLayout->addWidget(boxTickSuffix, 9, 1);

    rightBoxLayout->setRowStretch(10, 1);

    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addLayout(topLayout);
    rightLayout->addLayout(bottomLayout);
    rightLayout->addStretch(1);

    QHBoxLayout *mainLayout3 = new QHBoxLayout(axesPage);
    mainLayout3->addWidget(axesTitlesList);
    mainLayout3->addLayout(rightLayout);

    generalDialog.addTab(axesPage, tr("Axis"));

    connect(buttonLabelFont, SIGNAL(clicked()), this, SLOT(customAxisLabelFont()));

    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(updateShowBox(int)));
    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(updateAxisColor(int)));
    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(updateTitleBox(int)));
    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(setTicksType(int)));
    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(setAxisType(int)));
    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(setBaselineDist(int)));
    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(updateLabelsFormat(int)));

    connect(boxShowLabels, SIGNAL(clicked(bool)), this, SLOT(updateTickLabelsList(bool)));

    connect(boxAxisColor, SIGNAL(clicked()), this, SLOT(pickAxisColor()));
    connect(boxAxisNumColor, SIGNAL(clicked()), this, SLOT(pickAxisNumColor()));
    connect(boxShowFormula, SIGNAL(clicked()), this, SLOT(showFormulaBox()));

    connect(boxMajorTicksType, SIGNAL(activated(int)), this, SLOT(updateMajTicksType(int)));
    connect(boxMinorTicksType, SIGNAL(activated(int)), this, SLOT(updateMinTicksType(int)));

    connect(boxShowAxis, SIGNAL(clicked()), this, SLOT(showAxis()));
    connect(boxAxisBackbone, SIGNAL(toggled(bool)), this, SLOT(toggleAxisBackbone(bool)));
    connect(boxFormat, SIGNAL(activated(int)), this, SLOT(setLabelsNumericFormat(int)));

    connect(btnAxesFont, SIGNAL(clicked()), this, SLOT(customAxisFont()));
    connect(boxBaseline, SIGNAL(valueChanged(int)), this, SLOT(changeBaselineDist(int)));
    connect(boxAxisType, SIGNAL(activated(int)), this, SLOT(showAxisFormatOptions(int)));
    connect(boxPrecision, SIGNAL(valueChanged(int)), this, SLOT(setLabelsNumericFormat(int)));
    connect(boxDivideFactor, SIGNAL(editingFinished()), this, SLOT(applyTickLabelTransformFromUi()));
    connect(boxTickPrefix, SIGNAL(editingFinished()), this, SLOT(applyTickLabelTransformFromUi()));
    connect(boxTickSuffix, SIGNAL(editingFinished()), this, SLOT(applyTickLabelTransformFromUi()));
}

void AxesDialog::initFramePage()
{
    frame = new QWidget();

    boxFramed = new QGroupBox(tr("Canvas frame"));
    boxFramed->setCheckable(true);

    QGridLayout *boxFramedLayout = new QGridLayout(boxFramed);
    boxFramedLayout->addWidget(new QLabel(tr("Color")), 0, 0);
    boxFrameColor = new ColorButton(boxFramed);
    boxFramedLayout->addWidget(boxFrameColor, 0, 1);

    boxFramedLayout->addWidget(new QLabel(tr("Width")), 1, 0);
    boxFrameWidth = new QSpinBox();
    boxFrameWidth->setMinimum(1);
    boxFramedLayout->addWidget(boxFrameWidth, 1, 1);

    boxFramedLayout->setRowStretch(2, 1);

    QGroupBox *boxAxes = new QGroupBox(tr("Axes"));
    QGridLayout *boxAxesLayout = new QGridLayout(boxAxes);
    boxBackbones = new QCheckBox();
    boxBackbones->setText(tr("Draw backbones"));
    boxAxesLayout->addWidget(boxBackbones, 0, 0);

    boxAxesLayout->addWidget(new QLabel(tr("Line Width")), 1, 0);
    boxAxesLinewidth = new QSpinBox();
    boxAxesLinewidth->setRange(1, 100);
    boxAxesLayout->addWidget(boxAxesLinewidth, 1, 1);

    boxAxesLayout->addWidget(new QLabel(tr("Major ticks length")), 2, 0);
    boxMajorTicksLength = new QSpinBox();
    boxMajorTicksLength->setRange(0, 1000);
    boxAxesLayout->addWidget(boxMajorTicksLength, 2, 1);

    boxAxesLayout->addWidget(new QLabel(tr("Minor ticks length")), 3, 0);
    boxMinorTicksLength = new QSpinBox();
    boxMinorTicksLength->setRange(0, 1000);
    boxAxesLayout->addWidget(boxMinorTicksLength, 3, 1);

    boxAxesLayout->setRowStretch(4, 1);

    QGroupBox *boxPresets = new QGroupBox(tr("OriginPro Frame Presets"));
    QVBoxLayout *presetsLayout = new QVBoxLayout(boxPresets);
    btnPresetBoxInward = new QPushButton(tr("4-Sided Box [Inward Ticks]"));
    btnPresetBoxPlain = new QPushButton(tr("4-Sided Box [Plain Top/Right]"));
    btnPresetOpenL = new QPushButton(tr("Open L-Frame"));
    btnPresetZeroCrossed = new QPushButton(tr("Zero-Crossed"));
    presetsLayout->addWidget(btnPresetBoxInward);
    presetsLayout->addWidget(btnPresetBoxPlain);
    presetsLayout->addWidget(btnPresetOpenL);
    presetsLayout->addWidget(btnPresetZeroCrossed);
    presetsLayout->addStretch(1);

    QHBoxLayout *mainLayout = new QHBoxLayout(frame);
    mainLayout->addWidget(boxFramed);
    mainLayout->addWidget(boxAxes);
    mainLayout->addWidget(boxPresets);

    generalDialog.addTab(frame, tr("General"));

    connect(boxFrameColor, SIGNAL(clicked()), this, SLOT(pickCanvasFrameColor()));
    connect(boxBackbones, SIGNAL(toggled(bool)), this, SLOT(drawAxesBackbones(bool)));
    connect(boxFramed, SIGNAL(toggled(bool)), this, SLOT(drawFrame(bool)));
    connect(boxFrameWidth, SIGNAL(valueChanged(int)), this, SLOT(updateFrame(int)));
    connect(boxAxesLinewidth, SIGNAL(valueChanged(int)), this, SLOT(changeAxesLinewidth(int)));
    connect(boxMajorTicksLength, SIGNAL(valueChanged(int)), this,
            SLOT(changeMajorTicksLength(int)));
    connect(boxMinorTicksLength, SIGNAL(valueChanged(int)), this,
            SLOT(changeMinorTicksLength(int)));
    connect(btnPresetBoxInward, SIGNAL(clicked()), this, SLOT(applyPresetBoxInward()));
    connect(btnPresetBoxPlain, SIGNAL(clicked()), this, SLOT(applyPresetBoxPlain()));
    connect(btnPresetOpenL, SIGNAL(clicked()), this, SLOT(applyPresetOpenL()));
    connect(btnPresetZeroCrossed, SIGNAL(clicked()), this, SLOT(applyPresetZeroCrossed()));
}

void AxesDialog::initReferenceLinesPage()
{
    refLinesPage = new QWidget();

    QGroupBox *listGroup = new QGroupBox(tr("Reference Lines"));
    QVBoxLayout *listLayout = new QVBoxLayout(listGroup);
    refLinesList = new QListWidget();
    listLayout->addWidget(refLinesList);

    QHBoxLayout *listBtns = new QHBoxLayout();
    btnRefAdd = new QPushButton(tr("Add"));
    btnRefUpdate = new QPushButton(tr("Apply"));
    btnRefRemove = new QPushButton(tr("Remove"));
    btnRefClear = new QPushButton(tr("Clear"));
    listBtns->addWidget(btnRefAdd);
    listBtns->addWidget(btnRefUpdate);
    listBtns->addWidget(btnRefRemove);
    listBtns->addWidget(btnRefClear);
    listLayout->addLayout(listBtns);

    QGroupBox *propsGroup = new QGroupBox(tr("Properties"));
    QGridLayout *gl = new QGridLayout(propsGroup);

    gl->addWidget(new QLabel(tr("Orientation")), 0, 0);
    refOrientBox = new QComboBox();
    refOrientBox->addItem(tr("Horizontal (Y)"), 0);
    refOrientBox->addItem(tr("Vertical (X)"), 1);
    gl->addWidget(refOrientBox, 0, 1);

    gl->addWidget(new QLabel(tr("Mode")), 1, 0);
    refStatBox = new QComboBox();
    refStatBox->addItem(tr("Constant Value"), QString());
    refStatBox->addItem(tr("Mean"), QString("Mean"));
    refStatBox->addItem(tr("Median"), QString("Median"));
    refStatBox->addItem(tr("+SD"), QString("Mean+SD"));
    refStatBox->addItem(tr("-SD"), QString("Mean-SD"));
    refStatBox->addItem(tr("Min"), QString("Min"));
    refStatBox->addItem(tr("Max"), QString("Max"));
    gl->addWidget(refStatBox, 1, 1);

    gl->addWidget(new QLabel(tr("Value")), 2, 0);
    refValueBox = new QDoubleSpinBox();
    refValueBox->setRange(-1e12, 1e12);
    refValueBox->setDecimals(4);
    refValueBox->setValue(0.0);
    gl->addWidget(refValueBox, 2, 1);

    gl->addWidget(new QLabel(tr("Line Color")), 3, 0);
    refColorBtn = new ColorButton();
    refColorBtn->setColor(QColor(30, 102, 184));
    gl->addWidget(refColorBtn, 3, 1);

    gl->addWidget(new QLabel(tr("Line Type")), 4, 0);
    refStyleBox = new QComboBox();
    refStyleBox->addItem(tr("Solid Line"), (int)Qt::SolidLine);
    refStyleBox->addItem(tr("Dash Line"), (int)Qt::DashLine);
    refStyleBox->addItem(tr("Dot Line"), (int)Qt::DotLine);
    refStyleBox->addItem(tr("Dash-Dot Line"), (int)Qt::DashDotLine);
    refStyleBox->setCurrentIndex(1);
    gl->addWidget(refStyleBox, 4, 1);

    gl->addWidget(new QLabel(tr("Thickness")), 5, 0);
    refWidthBox = new QDoubleSpinBox();
    refWidthBox->setRange(0.5, 20.0);
    refWidthBox->setSingleStep(0.5);
    refWidthBox->setDecimals(1);
    refWidthBox->setValue(1.5);
    gl->addWidget(refWidthBox, 5, 1);

    gl->addWidget(new QLabel(tr("Label")), 6, 0);
    refLabelEdit = new QLineEdit();
    gl->addWidget(refLabelEdit, 6, 1);

    refFillBox = new QCheckBox(tr("Fill to Axis"));
    gl->addWidget(refFillBox, 7, 0);
    refFillColorBtn = new ColorButton();
    refFillColorBtn->setColor(QColor(30, 102, 184, 32));
    gl->addWidget(refFillColorBtn, 7, 1);
    gl->setRowStretch(8, 1);

    QHBoxLayout *mainLayout = new QHBoxLayout(refLinesPage);
    mainLayout->addWidget(listGroup, 1);
    mainLayout->addWidget(propsGroup, 1);

    generalDialog.addTab(refLinesPage, tr("Reference Lines"));

    connect(refLinesList, SIGNAL(currentRowChanged(int)), this, SLOT(selectReferenceLine(int)));
    connect(refStatBox, SIGNAL(currentIndexChanged(int)), this, SLOT(refStatModeChanged(int)));
    connect(btnRefAdd, SIGNAL(clicked()), this, SLOT(addRefLineFromUi()));
    connect(btnRefUpdate, SIGNAL(clicked()), this, SLOT(updateSelectedRefLineFromUi()));
    connect(btnRefRemove, SIGNAL(clicked()), this, SLOT(removeSelectedRefLine()));
    connect(btnRefClear, SIGNAL(clicked()), this, SLOT(clearAllRefLines()));
}

void AxesDialog::initAxisBreakPage()
{
    axisBreakPage = new QWidget();

    QPixmap image0(":/bottom_scl.xpm");
    QPixmap image1(":/left_scl.xpm");
    QPixmap image2(":/top_scl.xpm");
    QPixmap image3(":/right_scl.xpm");

    axisBreakAxesList = new QListWidget();
    axisBreakAxesList->addItem(new QListWidgetItem(image0, tr("Bottom")));
    axisBreakAxesList->addItem(new QListWidgetItem(image1, tr("Left")));
    axisBreakAxesList->addItem(new QListWidgetItem(image2, tr("Top")));
    axisBreakAxesList->addItem(new QListWidgetItem(image3, tr("Right")));
    axisBreakAxesList->setIconSize(image0.size());
    axisBreakAxesList->setCurrentRow(-1);

    QFontMetrics fm(axisBreakAxesList->font());
    int width = 32;
    for (int i = 0; i < axisBreakAxesList->count(); i++) {
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
        auto newWidth = fm.width(axisBreakAxesList->item(i)->text());
#else
        auto newWidth = fm.horizontalAdvance(axisBreakAxesList->item(i)->text());
#endif
        if (newWidth > width)
            width = newWidth;
    }
    axisBreakAxesList->setMaximumWidth(axisBreakAxesList->iconSize().width() + width + 50);
    axisBreakAxesList->resize(axisBreakAxesList->maximumWidth(), axisBreakAxesList->height());

    QGroupBox *breakBox = new QGroupBox(tr("Axis Breaks"));
    QGridLayout *gl = new QGridLayout(breakBox);

    boxEnableAxisBreak = new QCheckBox(tr("Enable Axis Break"));
    boxEnableAxisBreak->setChecked(false);
    gl->addWidget(boxEnableAxisBreak, 0, 0, 1, 3);

    gl->addWidget(new QLabel(tr("Break From:")), 1, 0);
    boxBreakFrom = new QLineEdit("0");
    gl->addWidget(boxBreakFrom, 1, 1, 1, 2);

    gl->addWidget(new QLabel(tr("Break To:")), 2, 0);
    boxBreakTo = new QLineEdit("0");
    gl->addWidget(boxBreakTo, 2, 1, 1, 2);

    gl->addWidget(new QLabel(tr("Break Position (%):")), 3, 0);
    boxBreakPositionPct = new QDoubleSpinBox();
    boxBreakPositionPct->setRange(10.0, 90.0);
    boxBreakPositionPct->setSingleStep(5.0);
    boxBreakPositionPct->setDecimals(1);
    boxBreakPositionPct->setSuffix("%");
    boxBreakPositionPct->setValue(50.0);
    gl->addWidget(boxBreakPositionPct, 3, 1, 1, 2);

    gl->addWidget(new QLabel(tr("Step Before:")), 4, 0);
    boxBreakStepBefore = new QLineEdit("0");
    gl->addWidget(boxBreakStepBefore, 4, 1, 1, 2);

    gl->addWidget(new QLabel(tr("Step After:")), 5, 0);
    boxBreakStepAfter = new QLineEdit("0");
    gl->addWidget(boxBreakStepAfter, 5, 1, 1, 2);

    gl->addWidget(new QLabel(tr("Type")), 6, 0);
    boxBreakTypeBefore = new QComboBox();
    boxBreakTypeBefore->addItem(tr("linear"));
    boxBreakTypeBefore->addItem(tr("logarithmic"));
    gl->addWidget(boxBreakTypeBefore, 6, 1);

    boxBreakTypeAfter = new QComboBox();
    boxBreakTypeAfter->addItem(tr("linear"));
    boxBreakTypeAfter->addItem(tr("logarithmic"));
    gl->addWidget(boxBreakTypeAfter, 6, 2);

    gl->setRowStretch(7, 1);

    QHBoxLayout *mainLayout = new QHBoxLayout(axisBreakPage);
    mainLayout->addWidget(axisBreakAxesList);
    mainLayout->addWidget(breakBox, 1);

    generalDialog.addTab(axisBreakPage, tr("Axis Break"));

    connect(axisBreakAxesList, SIGNAL(currentRowChanged(int)), this, SLOT(updateAxisBreakPage(int)));
    connect(boxEnableAxisBreak, SIGNAL(toggled(bool)), boxBreakFrom, SLOT(setEnabled(bool)));
    connect(boxEnableAxisBreak, SIGNAL(toggled(bool)), boxBreakTo, SLOT(setEnabled(bool)));
    connect(boxEnableAxisBreak, SIGNAL(toggled(bool)), boxBreakPositionPct, SLOT(setEnabled(bool)));
    connect(boxEnableAxisBreak, SIGNAL(toggled(bool)), boxBreakStepBefore, SLOT(setEnabled(bool)));
    connect(boxEnableAxisBreak, SIGNAL(toggled(bool)), boxBreakStepAfter, SLOT(setEnabled(bool)));
    connect(boxEnableAxisBreak, SIGNAL(toggled(bool)), boxBreakTypeBefore, SLOT(setEnabled(bool)));
    connect(boxEnableAxisBreak, SIGNAL(toggled(bool)), boxBreakTypeAfter, SLOT(setEnabled(bool)));

    boxBreakFrom->setEnabled(false);
    boxBreakTo->setEnabled(false);
    boxBreakPositionPct->setEnabled(false);
    boxBreakStepBefore->setEnabled(false);
    boxBreakStepAfter->setEnabled(false);
    boxBreakTypeBefore->setEnabled(false);
    boxBreakTypeAfter->setEnabled(false);
}

void AxesDialog::changeMinorTicksLength(int minLength)
{
    if (generalDialog.currentWidget() != frame)
        return;

    d_graph->changeTicksLength(minLength, boxMajorTicksLength->value());
    boxMajorTicksLength->setMinimum(minLength);
}

void AxesDialog::changeMajorTicksLength(int majLength)
{
    if (generalDialog.currentWidget() != frame)
        return;

    d_graph->changeTicksLength(boxMinorTicksLength->value(), majLength);
    boxMinorTicksLength->setMaximum(majLength);
}

void AxesDialog::drawAxesBackbones(bool draw)
{
    if (generalDialog.currentWidget() != frame)
        return;

    d_graph->drawAxesBackbones(draw);
}

void AxesDialog::changeAxesLinewidth(int width)
{
    if (generalDialog.currentWidget() != frame)
        return;

    d_graph->setAxesLinewidth(width);
}

void AxesDialog::drawFrame(bool framed)
{
    if (generalDialog.currentWidget() != frame)
        return;

    d_graph->drawCanvasFrame(framed, boxFrameWidth->value(), boxFrameColor->color());
}

void AxesDialog::updateFrame(int width)
{
    if (generalDialog.currentWidget() != frame)
        return;

    d_graph->drawCanvasFrame(boxFramed->isChecked(), width, boxFrameColor->color());
}

void AxesDialog::pickCanvasFrameColor()
{
    QColor c = QColorDialog::getColor(boxFrameColor->color(), this);
    if (!c.isValid() || c == boxFrameColor->color())
        return;

    boxFrameColor->setColor(c);
    d_graph->drawCanvasFrame(boxFramed->isChecked(), boxFrameWidth->value(), c);
}

void AxesDialog::showAxisFormatOptions(int format)
{
    int axis = mapToQwtAxisId();

    boxFormat->clear();
    boxFormat->setEditable(false);
    boxFormat->hide();
    boxPrecision->hide();
    boxColName->hide();
    label1->hide();
    label2->hide();
    label3->hide();
    boxShowFormula->hide();
    boxFormula->hide();
    boxTableName->hide();
    labelTable->hide();
    labelDivideFactor->hide();
    boxDivideFactor->hide();

    switch (static_cast<Graph::AxisType>(boxAxisType->itemData(format).toInt())) {
    case Graph::AxisType::Numeric:
        label2->show();
        boxFormat->show();
        boxFormat->addItem(tr("Automatic"));
        boxFormat->addItem(tr("Decimal: 100.0"));
        boxFormat->addItem(tr("Scientific: 1e2"));
        boxFormat->addItem(tr("Scientific: 10^2"));
        boxFormat->setCurrentIndex(d_graph->plotWidget()->axisLabelFormat(axis));

        label3->show();
        boxPrecision->show();
        boxShowFormula->show();
        labelDivideFactor->show();
        boxDivideFactor->show();

        showAxisFormula(mapToQwtAxisId());
        break;

    case Graph::AxisType::Txt:
        label1->show();
        boxColName->show();
        break;

    case Graph::AxisType::Day: {
        int day = (QDate::currentDate()).dayOfWeek();
        label2->show();
        boxFormat->show();
        boxFormat->addItem(QLocale().dayName(day, QLocale::ShortFormat));
        boxFormat->addItem(QLocale().dayName(day, QLocale::LongFormat));
        boxFormat->addItem((QLocale().dayName(day, QLocale::ShortFormat)).left(1));
        boxFormat->setCurrentIndex(formatInfo[axis].toInt());
    } break;

    case Graph::AxisType::Month: {
        int month = (QDate::currentDate()).month();
        label2->show();
        boxFormat->show();
        boxFormat->addItem(QLocale().monthName(month, QLocale::ShortFormat));
        boxFormat->addItem(QLocale().monthName(month, QLocale::LongFormat));
        boxFormat->addItem((QLocale().monthName(month, QLocale::ShortFormat)).left(1));
        boxFormat->setCurrentIndex(formatInfo[axis].toInt());
    } break;

    case Graph::AxisType::Time: {
        label2->show();
        boxFormat->show();
        boxFormat->setEditable(true);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = formatInfo[axis].split(";", Qt::KeepEmptyParts);
#else
        QStringList lst = formatInfo[axis].split(";", QString::KeepEmptyParts);
#endif
        if (lst.count() == 2) {
            boxFormat->addItem(lst[1]);
            boxFormat->setItemText(boxFormat->currentIndex(), lst[1]);
        }

        boxFormat->addItem("h");
        boxFormat->addItem("h ap");
        boxFormat->addItem("h AP");
        boxFormat->addItem("h:mm");
        boxFormat->addItem("h:mm ap");
        boxFormat->addItem("hh:mm");
        boxFormat->addItem("h:mm:ss");
        boxFormat->addItem("h:mm:ss.zzz");
        boxFormat->addItem("mm:ss");
        boxFormat->addItem("mm:ss.zzz");
        boxFormat->addItem("hmm");
        boxFormat->addItem("hmmss");
        boxFormat->addItem("hhmmss");
    } break;

    case Graph::AxisType::Date: {
        label2->show();
        boxFormat->show();
        boxFormat->setEditable(true);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = formatInfo[axis].split(";", Qt::KeepEmptyParts);
#else
        QStringList lst = formatInfo[axis].split(";", QString::KeepEmptyParts);
#endif
        if (lst.count() == 2) {
            boxFormat->addItem(lst[1]);
            boxFormat->setItemText(boxFormat->currentIndex(), lst[1]);
        }
        boxFormat->addItem("yyyy-MM-dd");
        boxFormat->addItem("dd.MM.yyyy");
        boxFormat->addItem("ddd MMMM d yy");
        boxFormat->addItem("dd/MM/yyyy");
    } break;

    case Graph::AxisType::DateTime: {
        label2->show();
        boxFormat->show();
        boxFormat->setEditable(true);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = formatInfo[axis].split(";", Qt::KeepEmptyParts);
#else
        QStringList lst = formatInfo[axis].split(";", QString::KeepEmptyParts);
#endif
        if (lst.count() == 2) {
            boxFormat->addItem(lst[1]);
            boxFormat->setItemText(boxFormat->currentIndex(), lst[1]);
        }

        const char *date_strings[] = {
            "yyyy-MM-dd", "yyyy/MM/dd", "dd/MM/yyyy", "dd/MM/yy", "dd.MM.yyyy",
            "dd.MM.yy",   "MM/yyyy",    "dd.MM.",     "yyyyMMdd", 0
        };

        const char *time_strings[] = {
            "hh",           "hh ap",        "hh:mm",     "hh:mm ap", "hh:mm:ss",
            "hh:mm:ss.zzz", "hh:mm:ss:zzz", "mm:ss.zzz", "hhmmss",   0
        };
        int j, i;
        for (i = 0; date_strings[i] != 0; i++)
            for (j = 0; time_strings[j] != 0; j++)
                boxFormat->addItem(
                        QString("%1 %2").arg(date_strings[i]).arg(time_strings[j]),
                        QVariant(QString(date_strings[i]) + " " + QString(time_strings[j])));
    } break;

    case Graph::AxisType::ColHeader: {
        labelTable->show();
        if (tablesList.contains(formatInfo[axis]))
            boxTableName->setItemText(boxTableName->currentIndex(), formatInfo[axis]);
        boxTableName->show();
    } break;
    }
}

void AxesDialog::insertColList(const QStringList &cols)
{
    boxColName->addItems(cols);
}

void AxesDialog::showAxis()
{
    bool ok = boxShowAxis->isChecked();
    boxTitle->setEnabled(ok);
    boxAxisColor->setEnabled(ok);
    boxAxisNumColor->setEnabled(ok);
    btnAxesFont->setEnabled(ok);
    boxShowLabels->setEnabled(ok);
    boxMajorTicksType->setEnabled(ok);
    boxMinorTicksType->setEnabled(ok);
    boxAxisType->setEnabled(ok);
    boxBaseline->setEnabled(ok);
    labelBox->setEnabled(ok);

    int axis = -1;
    int a = axesTitlesList->currentRow();
    switch (a) {
    case 0: {
        axis = QwtPlot::xBottom;
        xAxisOn = ok;
        break;
    }
    case 1: {
        axis = QwtPlot::yLeft;
        yAxisOn = ok;
        break;
    }
    case 2: {
        axis = QwtPlot::xTop;
        topAxisOn = ok;
        break;
    }
    case 3: {
        axis = QwtPlot::yRight;
        rightAxisOn = ok;
        break;
    }
    }

    bool labels = false;
    if (tickLabelsOn[axis] == "1")
        labels = true;

    boxFormat->setEnabled(labels && ok);
    boxColName->setEnabled(labels && ok);
    boxShowFormula->setEnabled(labels && ok);
    boxFormula->setEnabled(labels && ok);
    boxDivideFactor->setEnabled(labels && ok);
    boxTickPrefix->setEnabled(labels && ok);
    boxTickSuffix->setEnabled(labels && ok);

    if (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
        boxAngle->setEnabled(labels && ok);
    else
        boxAngle->setDisabled(true);

    bool userFormat = true;
    if (boxFormat->currentIndex() == 0)
        userFormat = false;
    boxPrecision->setEnabled(labels && ok && userFormat);

    QString formula = boxFormula->toPlainText();
    if (!boxShowFormula->isChecked())
        formula = QString();

    showAxis(axis, currentSelectedAxisType(), boxColName->currentText(), ok,
             boxMajorTicksType->currentIndex(), boxMinorTicksType->currentIndex(),
             boxShowLabels->isChecked(), boxAxisColor->color(), boxFormat->currentIndex(),
             boxPrecision->value(), boxAngle->value(), boxBaseline->value(), formula,
             boxAxisNumColor->color());
}

void AxesDialog::updateShowBox(int axis)
{
    if (tickLabelsOn.size() < 4)
        return;

    switch (axis) {
    case 0: {
        boxShowAxis->setChecked(xAxisOn);
        int labelsOn = tickLabelsOn[2].toInt();
        boxShowLabels->setChecked(labelsOn);
        boxAngle->setEnabled(labelsOn && xAxisOn);
        boxFormat->setEnabled(labelsOn && xAxisOn);
        boxAngle->setValue(xBottomLabelsRotation);
        break;
    }
    case 1: {
        boxShowAxis->setChecked(yAxisOn);
        int labelsOn = tickLabelsOn[0].toInt();
        boxShowLabels->setChecked(labelsOn);
        boxFormat->setEnabled(labelsOn && yAxisOn);
        boxAngle->setEnabled(false);
        boxAngle->setValue(0);
        break;
    }
    case 2: {
        boxShowAxis->setChecked(topAxisOn);

        int labelsOn = tickLabelsOn[3].toInt();
        boxShowLabels->setChecked(labelsOn);
        boxFormat->setEnabled(labelsOn && topAxisOn);
        boxAngle->setEnabled(labelsOn && topAxisOn);
        boxAngle->setValue(xTopLabelsRotation);
        break;
    }
    case 3: {
        boxShowAxis->setChecked(rightAxisOn);
        int labelsOn = tickLabelsOn[1].toInt();
        boxShowLabels->setChecked(labelsOn);
        boxFormat->setEnabled(labelsOn && rightAxisOn);
        boxAngle->setEnabled(false);
        boxAngle->setValue(0);
        break;
    }
    }

    if (d_graph && axis >= 0 && axis < 4) {
        const int qwtAxis = Graph::mapToQwtAxis(axis);
        boxAxisBackbone->blockSignals(true);
        boxAxisBackbone->setChecked(d_graph->isAxisBackboneVisible(qwtAxis));
        boxAxisBackbone->blockSignals(false);

        boxDivideFactor->blockSignals(true);
        boxTickPrefix->blockSignals(true);
        boxTickSuffix->blockSignals(true);
        boxDivideFactor->setText(QString::number(d_graph->tickDivideFactor(qwtAxis), 'g', 12));
        boxTickPrefix->setText(d_graph->tickPrefix(qwtAxis));
        boxTickSuffix->setText(d_graph->tickSuffix(qwtAxis));
        boxDivideFactor->blockSignals(false);
        boxTickPrefix->blockSignals(false);
        boxTickSuffix->blockSignals(false);
    }

    bool ok = boxShowAxis->isChecked();
    bool labels = boxShowLabels->isChecked();
    boxDivideFactor->setEnabled(labels && ok);
    boxTickPrefix->setEnabled(labels && ok);
    boxTickSuffix->setEnabled(labels && ok);
    boxTitle->setEnabled(ok);
    boxAxisColor->setEnabled(ok);
    boxAxisNumColor->setEnabled(ok);
    btnAxesFont->setEnabled(ok);
    boxShowLabels->setEnabled(ok);
    boxMajorTicksType->setEnabled(ok);
    boxMinorTicksType->setEnabled(ok);
    boxAxisType->setEnabled(ok);
    boxBaseline->setEnabled(ok);
    labelBox->setEnabled(ok);
}

void AxesDialog::customAxisFont()
{
    if (!d_graph)
        return;

    bool okF = false;
    int axis = -1;
    QFont fnt;
    switch (axesTitlesList->currentRow()) {
    case 0: {
        axis = QwtPlot::xBottom;
        fnt = QFontDialog::getFont(&okF, xBottomFont, this);
        if (okF)
            xBottomFont = fnt;
        break;
    }
    case 1: {
        axis = QwtPlot::yLeft;
        fnt = QFontDialog::getFont(&okF, yLeftFont, this);
        if (okF)
            yLeftFont = fnt;
        break;
    }
    case 2: {
        axis = QwtPlot::xTop;
        fnt = QFontDialog::getFont(&okF, xTopFont, this);
        if (okF)
            xTopFont = fnt;
        break;
    }
    case 3: {
        axis = QwtPlot::yRight;
        fnt = QFontDialog::getFont(&okF, yRightFont, this);
        if (okF)
            yRightFont = fnt;
        break;
    }
    }

    if (okF && axis >= 0)
        d_graph->setAxisFont(axis, fnt);
}

void AxesDialog::accept()
{
    if (updatePlot())
        close();
}

void AxesDialog::majorGridEnabled(bool on)
{
    boxTypeMajor->setEnabled(on);
    boxColorMajor->setEnabled(on);
    boxWidthMajor->setEnabled(on);

    updateGrid();
}

void AxesDialog::minorGridEnabled(bool on)
{
    boxTypeMinor->setEnabled(on);
    boxColorMinor->setEnabled(on);
    boxWidthMinor->setEnabled(on);

    updateGrid();
}

void AxesDialog::updateGrid()
{
    if (generalDialog.currentWidget() != gridPage)
        return;

    Grid *grid = (Grid *)d_graph->plotWidget()->grid();
    if (!grid)
        return;

    if (axesGridList->currentRow() == 1) {
        grid->enableX(boxMajorGrid->isChecked());
        grid->enableXMin(boxMinorGrid->isChecked());

        grid->setMajPenX(QPen(boxColorMajor->color(), boxWidthMajor->value(),
                              Graph::getPenStyle(boxTypeMajor->currentIndex())));
        grid->setMinPenX(QPen(boxColorMinor->color(), boxWidthMinor->value(),
                              Graph::getPenStyle(boxTypeMinor->currentIndex())));
    } else {
        grid->enableY(boxMajorGrid->isChecked());
        grid->enableYMin(boxMinorGrid->isChecked());

        grid->setMajPenY(QPen(boxColorMajor->color(), boxWidthMajor->value(),
                              Graph::getPenStyle(boxTypeMajor->currentIndex())));
        grid->setMinPenY(QPen(boxColorMinor->color(), boxWidthMinor->value(),
                              Graph::getPenStyle(boxTypeMinor->currentIndex())));
    }

    grid->enableZeroLineX(boxXLine->isChecked());
    grid->enableZeroLineY(boxYLine->isChecked());

    grid->setAxis(boxGridXAxis->currentIndex() + 2, boxGridYAxis->currentIndex());
    d_graph->replot();
    d_graph->notifyChanges();
}

void AxesDialog::showGridOptions(int axis)
{
    Grid *grd = (Grid *)d_graph->plotWidget()->grid();
    if (!grd)
        return;

    disconnect(boxMajorGrid, SIGNAL(toggled(bool)), this, SLOT(majorGridEnabled(bool)));
    disconnect(boxMinorGrid, SIGNAL(toggled(bool)), this, SLOT(minorGridEnabled(bool)));

    if (axis == 1) {
        boxMajorGrid->setChecked(grd->xEnabled());
        boxMinorGrid->setChecked(grd->xMinEnabled());

        boxXLine->setEnabled(true);
        boxYLine->setDisabled(true);

        boxGridXAxis->setEnabled(true);
        boxGridYAxis->setDisabled(true);

        QPen majPenX = grd->majPenX();
        boxTypeMajor->setCurrentIndex(majPenX.style() - 1);
        boxColorMajor->setColor(majPenX.color());
        boxWidthMajor->setValue(majPenX.width());

        QPen minPenX = grd->minPenX();
        boxTypeMinor->setCurrentIndex(minPenX.style() - 1);
        boxColorMinor->setColor(minPenX.color());
        boxWidthMinor->setValue(minPenX.width());
    } else if (axis == 0) {
        boxMajorGrid->setChecked(grd->yEnabled());
        boxMinorGrid->setChecked(grd->yMinEnabled());

        boxXLine->setDisabled(true);
        boxYLine->setEnabled(true);

        boxGridXAxis->setDisabled(true);
        boxGridYAxis->setEnabled(true);

        QPen majPenY = grd->majPenY();
        boxTypeMajor->setCurrentIndex(majPenY.style() - 1);
        boxColorMajor->setColor(majPenY.color());
        boxWidthMajor->setValue(majPenY.width());

        QPen minPenY = grd->minPenY();
        boxTypeMinor->setCurrentIndex(minPenY.style() - 1);
        boxColorMinor->setColor(minPenY.color());
        boxWidthMinor->setValue(minPenY.width());
    }

    bool majorOn = boxMajorGrid->isChecked();
    boxTypeMajor->setEnabled(majorOn);
    boxColorMajor->setEnabled(majorOn);
    boxWidthMajor->setEnabled(majorOn);

    bool minorOn = boxMinorGrid->isChecked();
    boxTypeMinor->setEnabled(minorOn);
    boxColorMinor->setEnabled(minorOn);
    boxWidthMinor->setEnabled(minorOn);

    boxGridXAxis->setCurrentIndex(grd->xAxis() - 2);
    boxGridYAxis->setCurrentIndex(grd->yAxis());

    boxXLine->setChecked(grd->xZeroLineEnabled());
    boxYLine->setChecked(grd->yZeroLineEnabled());

    connect(boxMajorGrid, SIGNAL(toggled(bool)), this, SLOT(majorGridEnabled(bool)));
    connect(boxMinorGrid, SIGNAL(toggled(bool)), this, SLOT(minorGridEnabled(bool)));
}

void AxesDialog::stepEnabled()
{
    boxStep->setEnabled(btnStep->isChecked());
    boxUnit->setEnabled(btnStep->isChecked());
    boxMajorValue->setDisabled(btnStep->isChecked());
    btnMajor->setChecked(!btnStep->isChecked());
}

void AxesDialog::stepDisabled()
{
    boxStep->setDisabled(btnMajor->isChecked());
    boxUnit->setDisabled(btnMajor->isChecked());
    boxMajorValue->setEnabled(btnMajor->isChecked());
    btnStep->setChecked(!btnMajor->isChecked());
}

void AxesDialog::updateAxisColor(int)
{
    int a = mapToQwtAxisId();
    boxAxisColor->setColor(d_graph->axisColor(a));
    boxAxisNumColor->setColor(d_graph->axisNumbersColor(a));
}

void AxesDialog::changeBaselineDist(int baseline)
{
    int axis = mapToQwtAxisId();
    axesBaseline[axis] = baseline;

    if (d_graph->axisTitle(axis) != boxTitle->toPlainText())
        d_graph->setAxisTitle(axis, boxTitle->toPlainText());

    QString formula = boxFormula->toPlainText();
    if (!boxShowFormula->isChecked())
        formula = QString();
    showAxis(axis, currentSelectedAxisType(), formatInfo[axis], boxShowAxis->isChecked(),
             boxMajorTicksType->currentIndex(), boxMinorTicksType->currentIndex(),
             boxShowLabels->isChecked(), boxAxisColor->color(), boxFormat->currentIndex(),
             boxPrecision->value(), boxAngle->value(), baseline, formula, boxAxisNumColor->color());
}

bool AxesDialog::updatePlot()
{
    if (!d_graph)
        return false;

    if (generalDialog.currentWidget() == (QWidget *)scalesPage) {
        QString from = boxStart->text().toLower();
        QString to = boxEnd->text().toLower();
        QString step = boxStep->text().toLower();
        int a = Graph::mapToQwtAxis(axesList->currentRow());
        switch (axesType[a]) {
        case Graph::AxisType::Time:
        case Graph::AxisType::Date:
        case Graph::AxisType::DateTime: {
            QDateTime qdt = QDateTime::fromString(from, "yyyy-MM-dd hh:mm:ss");
            from = QString::number(qdt.toMSecsSinceEpoch() / 86400000. + 2440587.5, 'f', 16);
            qdt = QDateTime::fromString(to, "yyyy-MM-dd hh:mm:ss");
            to = QString::number(qdt.toMSecsSinceEpoch() / 86400000. + 2440587.5, 'f', 16);
            break;
        }
        default: {
        }
        }

        double start, end, stp = 0;
        try {
            MyParser parser;
            parser.SetExpr(from);
            start = parser.Eval();
        } catch (mu::ParserError &e) {
            QMessageBox::critical(0, tr("Start limit error"), QStringFromString(e.GetMsg()));
            boxStart->setFocus();
            return false;
        }
        try {
            MyParser parser;
            parser.SetExpr(to);
            end = parser.Eval();
        } catch (mu::ParserError &e) {
            QMessageBox::critical(0, tr("End limit error"), QStringFromString(e.GetMsg()));
            boxEnd->setFocus();
            return false;
        }
        if (btnStep->isChecked()) {
            try {
                MyParser parser;
                parser.SetExpr(step.toUtf8().constData());
                stp = parser.Eval();
            } catch (mu::ParserError &e) {
                QMessageBox::critical(0, tr("Step input error"), QStringFromString(e.GetMsg()));
                boxStep->setFocus();
                return false;
            }

            if (stp <= 0) {
                QMessageBox::critical(0, tr("Step input error"),
                                      tr("Please enter a positive step value!"));
                boxStep->setFocus();
                return false;
            }

            if (axesType[a] == Graph::AxisType::Time) {
                switch (boxUnit->currentIndex()) {
                case 0:
                    break;
                case 1:
                    stp *= 1e3;
                    break;
                case 2:
                    stp *= 6e4;
                    break;
                case 3:
                    stp *= 36e5;
                    break;
                }
            } else if (axesType[a] == Graph::AxisType::Date) {
                switch (boxUnit->currentIndex()) {
                case 0:
                    break;
                case 1:
                    stp *= 7;
                    break;
                }
            } else if (axesType[a] == Graph::AxisType::DateTime) {
                switch (boxUnit->currentIndex()) {
                case 0: // milliseconds
                    stp *= 1. / 86400000.;
                    break;
                case 1:
                    stp *= 1. / 86400.;
                    break;
                case 2:
                    stp *= 1. / 1440.;
                    break;
                case 3:
                    stp *= 1. / 24.;
                    break;
                case 4:
                    break;
                case 5:
                    stp *= 7;
                    break;
                }
            }
        }

        const Graph::AxisBreakSpec prevBrk = d_graph->axisBreak(a);
        d_graph->setScale(a, start, end, stp, boxMajorValue->value(),
                          boxMinorValue->currentText().toInt(), boxScaleType->currentIndex(),
                          btnInvert->isChecked());
        if (prevBrk.enabled) {
            const double lo = std::min(start, end);
            const double hi = std::max(start, end);
            if (lo < prevBrk.breakFrom && prevBrk.breakFrom < prevBrk.breakTo && prevBrk.breakTo < hi)
                d_graph->setAxisBreak(a, prevBrk);
        }
        d_graph->notifyChanges();
    } else if (generalDialog.currentWidget() == gridPage) {
        updateGrid();
    } else if (generalDialog.currentWidget() == (QWidget *)axesPage) {
        int axis = mapToQwtAxisId();
        auto format = currentSelectedAxisType();
        axesType[axis] = format;

        int baseline = boxBaseline->value();
        axesBaseline[axis] = baseline;

        if (format == Graph::AxisType::Numeric) {
            if (boxShowFormula->isChecked()) {
                QString formula = boxFormula->toPlainText().toLower();
                try {
                    double value = 1.0;
                    MyParser parser;
                    if (formula.contains("x"))
                        parser.DefineVar(_T("x"), &value);
                    else if (formula.contains("y"))
                        parser.DefineVar(_T("y"), &value);
                    parser.SetExpr(formula.toUtf8().constData());
                    parser.Eval();
                } catch (mu::ParserError &e) {
                    QMessageBox::critical(0, tr("Formula input error"),
                                          QStringFromString(e.GetMsg()) + "\n"
                                                  + tr("Valid variables are 'x' for Top/Bottom "
                                                       "axes and 'y' for Left/Right axes!"));
                    boxFormula->setFocus();
                    return false;
                }
            }
        } else if (format == Graph::AxisType::Time || format == Graph::AxisType::Date || format == Graph::AxisType::DateTime) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList lst = formatInfo[axis].split(";", Qt::KeepEmptyParts);
#else
            QStringList lst = formatInfo[axis].split(";", QString::KeepEmptyParts);
#endif
            if (lst.size() < 2 || lst[0].isEmpty()) {
                lst = QStringList();
                if (format == Graph::AxisType::Time)
                    lst << QTime(0, 0, 0).toString();
                else if (format == Graph::AxisType::Date)
                    lst << QDate(1900, 1, 1).toString("yyyy-MM-dd");
                else
                    lst << QDateTime(QDate(1900, 1, 1), QTime(0, 0, 0))
                                    .toString("yyyy-MM-ddThh:mm:ss");
                lst << boxFormat->currentText();
            } else
                lst[1] = boxFormat->currentText();
            formatInfo[axis] = lst.join(";");
        } else if (format == Graph::AxisType::Day || format == Graph::AxisType::Month)
            formatInfo[axis] = QString::number(boxFormat->currentIndex());
        else if (format == Graph::AxisType::ColHeader)
            formatInfo[axis] = boxTableName->currentText();
        else
            formatInfo[axis] = boxColName->currentText();

        if (d_graph->axisTitle(axis) != boxTitle->toPlainText())
            d_graph->setAxisTitle(axis, boxTitle->toPlainText());

        if (axis == QwtPlot::xBottom)
            xBottomLabelsRotation = boxAngle->value();
        else if (axis == QwtPlot::xTop)
            xTopLabelsRotation = boxAngle->value();

        QString formula = boxFormula->toPlainText();
        if (!boxShowFormula->isChecked())
            formula = QString();
        showAxis(axis, format, formatInfo[axis], boxShowAxis->isChecked(),
                 boxMajorTicksType->currentIndex(), boxMinorTicksType->currentIndex(),
                 boxShowLabels->isChecked(), boxAxisColor->color(), boxFormat->currentIndex(),
                 boxPrecision->value(), boxAngle->value(), baseline, formula,
                 boxAxisNumColor->color());
        d_graph->setAxisBackboneVisible(axis, boxAxisBackbone->isChecked());
        applyTickLabelTransformFromUi();
    } else if (generalDialog.currentWidget() == (QWidget *)frame) {
        d_graph->setAxesLinewidth(boxAxesLinewidth->value());
        d_graph->changeTicksLength(boxMinorTicksLength->value(), boxMajorTicksLength->value());
        d_graph->drawCanvasFrame(boxFramed->isChecked(), boxFrameWidth->value(),
                                 boxFrameColor->color());
        d_graph->drawAxesBackbones(boxBackbones->isChecked());
    } else if (generalDialog.currentWidget() == refLinesPage) {
        if (refLinesList && refLinesList->currentRow() >= 0)
            updateSelectedRefLineFromUi();
        d_graph->updateStatReferenceLines();
    } else if (generalDialog.currentWidget() == axisBreakPage) {
        int axisRow = axisBreakAxesList ? axisBreakAxesList->currentRow() : 0;
        if (axisRow < 0)
            axisRow = 0;
        const int qwtAxis = Graph::mapToQwtAxis(axisRow);
        if (!boxEnableAxisBreak->isChecked()) {
            d_graph->clearAxisBreak(qwtAxis);
        } else {
            auto parseExpr = [](const QString &txt, double fallback, bool *okOut) -> double {
                const QString trimmed = txt.trimmed();
                if (trimmed.isEmpty()) {
                    if (okOut)
                        *okOut = true;
                    return fallback;
                }
                bool numOk = false;
                double v = QLocale::c().toDouble(trimmed, &numOk);
                if (!numOk)
                    v = trimmed.toDouble(&numOk);
                if (numOk) {
                    if (okOut)
                        *okOut = true;
                    return v;
                }
                try {
                    MyParser parser;
                    parser.SetExpr(trimmed.toUtf8().constData());
                    v = parser.Eval();
                    if (okOut)
                        *okOut = std::isfinite(v);
                    return v;
                } catch (...) {
                    if (okOut)
                        *okOut = false;
                    return fallback;
                }
            };

            bool okFrom = false, okTo = false, okStepB = false, okStepA = false;
            Graph::AxisBreakSpec spec;
            spec.enabled = true;
            spec.breakFrom = parseExpr(boxBreakFrom->text(), 0.0, &okFrom);
            spec.breakTo = parseExpr(boxBreakTo->text(), 0.0, &okTo);
            spec.positionPct = qBound(0.10, boxBreakPositionPct->value() / 100.0, 0.90);
            spec.stepBefore = qMax(0.0, parseExpr(boxBreakStepBefore->text(), 0.0, &okStepB));
            spec.stepAfter = qMax(0.0, parseExpr(boxBreakStepAfter->text(), 0.0, &okStepA));
            spec.log10Before = (boxBreakTypeBefore->currentIndex() == 1);
            spec.log10After = (boxBreakTypeAfter->currentIndex() == 1);

            if (!okFrom || !okTo || !(spec.breakFrom < spec.breakTo) || !okStepB || !okStepA) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("Break From:") + " < " + tr("Break To:"));
                boxBreakFrom->setFocus();
                return false;
            }
            d_graph->setAxisBreak(qwtAxis, spec);
        }
        d_graph->notifyChanges();
    }

    return true;
}

void AxesDialog::setGraph(Graph *g)
{
    if (!g)
        return;

    d_graph = g;
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    if (app) {
        tablesList = app->tableWindows();
        boxTableName->clear();
        boxTableName->addItems(tablesList);
        boxColName->clear();
        boxColName->addItems(app->columnsList());
    }

    refreshGraphState();
    refreshReferenceLinesList();
}

void AxesDialog::refreshGraphState()
{
    if (!d_graph)
        return;

    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    xAxisOn = p->axisEnabled(QwtPlot::xBottom);
    yAxisOn = p->axisEnabled(QwtPlot::yLeft);
    topAxisOn = p->axisEnabled(QwtPlot::xTop);
    rightAxisOn = p->axisEnabled(QwtPlot::yRight);

    xBottomFont = p->axisFont(QwtPlot::xBottom);
    if (xBottomFont.family().isEmpty())
        xBottomFont.setFamily("Arial");
    yLeftFont = p->axisFont(QwtPlot::yLeft);
    if (yLeftFont.family().isEmpty())
        yLeftFont.setFamily("Arial");
    xTopFont = p->axisFont(QwtPlot::xTop);
    if (xTopFont.family().isEmpty())
        xTopFont.setFamily("Arial");
    yRightFont = p->axisFont(QwtPlot::yRight);
    if (yRightFont.family().isEmpty())
        yRightFont.setFamily("Arial");

    majTicks = p->getMajorTicksType();
    minTicks = p->getMinorTicksType();

    formatInfo = d_graph->axesLabelsFormatInfo();
    int curAxisRow = axesTitlesList->currentRow();
    updateTitleBox(curAxisRow >= 0 ? curAxisRow : 0);

    xBottomLabelsRotation = d_graph->labelsRotation(QwtPlot::xBottom);
    xTopLabelsRotation = d_graph->labelsRotation(QwtPlot::xTop);

    tickLabelsOn = d_graph->enabledTickLabels();
    axesBaseline = d_graph->axesBaseline();
    axesType = d_graph->axesType();

    boxAxesLinewidth->blockSignals(true);
    boxBackbones->blockSignals(true);
    boxFramed->blockSignals(true);
    boxFrameWidth->blockSignals(true);
    boxMinorTicksLength->blockSignals(true);
    boxMajorTicksLength->blockSignals(true);

    boxAxesLinewidth->setValue(p->axesLinewidth());
    boxBackbones->setChecked(d_graph->axesBackbones());

    boxFramed->setChecked(d_graph->framed());
    boxFrameColor->setColor(d_graph->canvasFrameColor());
    boxFrameWidth->setValue(d_graph->canvasFrameWidth());

    boxMinorTicksLength->setValue(p->minorTickLength());
    boxMajorTicksLength->setValue(p->majorTickLength());

    boxAxesLinewidth->blockSignals(false);
    boxBackbones->blockSignals(false);
    boxFramed->blockSignals(false);
    boxFrameWidth->blockSignals(false);
    boxMinorTicksLength->blockSignals(false);
    boxMajorTicksLength->blockSignals(false);

    if (curAxisRow >= 0) {
        updateShowBox(curAxisRow);
        setTicksType(curAxisRow);
    }
    showGridOptions(axesGridList->currentRow());
    if (axisBreakAxesList)
        updateAxisBreakPage(axisBreakAxesList->currentRow() >= 0 ? axisBreakAxesList->currentRow() : 0);
}

int AxesDialog::mapToQwtAxisId()
{
    return Graph::mapToQwtAxis(axesTitlesList->currentRow());
}

void AxesDialog::updateScale()
{
    int axis = axesList->currentRow();

    boxStart->clear();
    boxEnd->clear();
    boxStep->clear();
    boxUnit->hide();
    boxUnit->clear();

    Plot *d_plot = d_graph->plotWidget();
    int a = Graph::mapToQwtAxis(axis);
    const QwtScaleDiv *scDiv = d_plot->axisScaleDiv(a);
    double astart = std::min(scDiv->lowerBound(), scDiv->upperBound());
    double aend = std::max(scDiv->lowerBound(), scDiv->upperBound());
    double astep = d_graph->axisStep(a);

    switch (axesType[a]) {
    case Graph::AxisType::Time:
    case Graph::AxisType::Date:
    case Graph::AxisType::DateTime: {
        QDateTime qdt = QDateTime::fromMSecsSinceEpoch(round((astart - 2440587.5) * 86400000.));
        boxStart->setText(qdt.toString("yyyy-MM-dd hh:mm:ss"));
        qdt = QDateTime::fromMSecsSinceEpoch(round((aend - 2440587.5) * 86400000.));
        boxEnd->setText(qdt.toString("yyyy-MM-dd hh:mm:ss"));
        break;
    }
    default: {
        boxStart->setText(QString::number(astart));
        boxEnd->setText(QString::number(aend));
    }
    }

    QwtValueList lst = scDiv->ticks(QwtScaleDiv::MajorTick);
    boxStep->setText(QString::number(d_graph->axisStep(a)));
    boxMajorValue->setValue(lst.count());

    if (axesType[a] == Graph::AxisType::Time) {
        boxUnit->show();
        boxUnit->addItem(tr("millisec."));
        boxUnit->addItem(tr("sec."));
        boxUnit->addItem(tr("min."));
        boxUnit->addItem(tr("hours"));
    } else if (axesType[a] == Graph::AxisType::Date) {
        boxUnit->show();
        boxUnit->addItem(tr("days"));
        boxUnit->addItem(tr("weeks"));
    } else if (axesType[a] == Graph::AxisType::DateTime) {
        boxUnit->show();
        boxUnit->addItem(tr("millisec."));
        boxUnit->addItem(tr("sec."));
        boxUnit->addItem(tr("min."));
        boxUnit->addItem(tr("hours"));
        boxUnit->addItem(tr("days"));
        boxUnit->addItem(tr("weeks"));
        boxUnit->setCurrentIndex(4); // days
    }

    if (d_graph->axisStep(a) != 0.0) {
        btnStep->setChecked(true);
        boxStep->setEnabled(true);
        boxUnit->setEnabled(true);
        if (axesType[a] == Graph::AxisType::DateTime) {
            if (abs(astep * 24. - round(astep * 24.)) < 1e-6 * astep) {
                astep *= 24;
                boxUnit->setCurrentIndex(3); // hours
            } else if (abs(astep * 1440. - round(astep * 1440.)) < 1e-6 * astep) {
                astep *= 1440.;
                boxUnit->setCurrentIndex(2); // minutes
            } else if (abs(astep * 86400. - round(astep * 86400.)) < 1e-6 * astep) {
                astep *= 86400.;
                boxUnit->setCurrentIndex(1); // seconds
            }
            boxStep->setText(QString::number(astep));
        }

        btnMajor->setChecked(false);
        boxMajorValue->setEnabled(false);
    } else {
        btnStep->setChecked(false);
        boxStep->setEnabled(false);
        boxUnit->setEnabled(false);
        btnMajor->setChecked(true);
        boxMajorValue->setEnabled(true);
    }

    const QwtScaleEngine *sc_eng = d_plot->axisScaleEngine(a);
    btnInvert->setChecked(sc_eng->testAttribute(QwtScaleEngine::Inverted));

    QwtScaleTransformation *tr = sc_eng->transformation();
    const bool isLog10 = (tr && tr->type() == QwtScaleTransformation::Log10);
    boxScaleType->setCurrentIndex(isLog10 ? 1 : 0);

    boxMinorValue->clear();
    if (isLog10) // log scale
        boxMinorValue->addItems(QStringList() << "0"
                                              << "2"
                                              << "4"
                                              << "8");
    else
        boxMinorValue->addItems(QStringList() << "0"
                                              << "1"
                                              << "4"
                                              << "9"
                                              << "14"
                                              << "19");

    boxMinorValue->setEditText(QString::number(d_plot->axisMaxMinor(a)));
}

void AxesDialog::updateTitleBox(int axis)
{
    int axisId = Graph::mapToQwtAxis(axis);
    boxTitle->setText(d_graph->axisTitle(axisId));
}

void AxesDialog::pickAxisColor()
{
    QColor c = QColorDialog::getColor(boxAxisColor->color(), this);
    if (!c.isValid() || c == boxAxisColor->color())
        return;

    boxAxisColor->setColor(c);

    int axis = mapToQwtAxisId();
    QString formula = boxFormula->toPlainText();
    if (!boxShowFormula->isChecked())
        formula = QString();

    showAxis(axis, currentSelectedAxisType(), formatInfo[axis], boxShowAxis->isChecked(),
             boxMajorTicksType->currentIndex(), boxMinorTicksType->currentIndex(),
             boxShowLabels->isChecked(), c, boxFormat->currentIndex(), boxPrecision->value(),
             boxAngle->value(), boxBaseline->value(), formula, boxAxisNumColor->color());
}

void AxesDialog::pickAxisNumColor()
{
    QColor c = QColorDialog::getColor(boxAxisNumColor->color(), this);
    if (!c.isValid() || c == boxAxisNumColor->color())
        return;

    boxAxisNumColor->setColor(c);
    int axis = mapToQwtAxisId();
    QString formula = boxFormula->toPlainText();
    if (!boxShowFormula->isChecked())
        formula = QString();

    showAxis(axis, currentSelectedAxisType(), formatInfo[axis], boxShowAxis->isChecked(),
             boxMajorTicksType->currentIndex(), boxMinorTicksType->currentIndex(),
             boxShowLabels->isChecked(), boxAxisColor->color(), boxFormat->currentIndex(),
             boxPrecision->value(), boxAngle->value(), boxBaseline->value(), formula, c);
}

void AxesDialog::setAxisType(int)
{
    int a = mapToQwtAxisId();
    auto type = d_graph->axesType()[a];

    boxAxisType->setCurrentIndex(boxAxisType->findData(static_cast<int>(type)));
    showAxisFormatOptions(boxAxisType->findData(static_cast<int>(type)));

    if (type == Graph::AxisType::Txt)
        boxColName->setItemText(boxColName->currentIndex(), formatInfo[a]);
}

void AxesDialog::setBaselineDist(int)
{
    int a = mapToQwtAxisId();
    boxBaseline->setValue(axesBaseline[a]);
}

void AxesDialog::setTicksType(int)
{
    int a = mapToQwtAxisId();
    boxMajorTicksType->setCurrentIndex(majTicks[a]);
    boxMinorTicksType->setCurrentIndex(minTicks[a]);
}

void AxesDialog::updateMajTicksType(int)
{
    int axis = mapToQwtAxisId();
    int type = boxMajorTicksType->currentIndex();
    if (majTicks[axis] == type)
        return;

    majTicks[axis] = type;
    QString formula = boxFormula->toPlainText();
    if (!boxShowFormula->isChecked())
        formula = QString();

    showAxis(axis, currentSelectedAxisType(), formatInfo[axis], boxShowAxis->isChecked(), type,
             boxMinorTicksType->currentIndex(), boxShowLabels->isChecked(), boxAxisColor->color(),
             boxFormat->currentIndex(), boxPrecision->value(), boxAngle->value(),
             boxBaseline->value(), formula, boxAxisNumColor->color());
}

void AxesDialog::updateMinTicksType(int)
{
    int axis = mapToQwtAxisId();
    int type = boxMinorTicksType->currentIndex();
    if (minTicks[axis] == type)
        return;

    minTicks[axis] = type;
    QString formula = boxFormula->toPlainText();

    if (!boxShowFormula->isChecked())
        formula = QString();

    showAxis(axis, currentSelectedAxisType(), formatInfo[axis], boxShowAxis->isChecked(),
             boxMajorTicksType->currentIndex(), type, boxShowLabels->isChecked(),
             boxAxisColor->color(), boxFormat->currentIndex(), boxPrecision->value(),
             boxAngle->value(), boxBaseline->value(), formula, boxAxisNumColor->color());
}

void AxesDialog::updateTickLabelsList(bool on)
{
    int axis = mapToQwtAxisId();
    if (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
        boxAngle->setEnabled(on);

    bool userFormat = true;
    if (boxFormat->currentIndex() == 0)
        userFormat = false;
    boxPrecision->setEnabled(on && userFormat);

    if (tickLabelsOn[axis] == QString::number(on))
        return;
    tickLabelsOn[axis] = QString::number(on);

    auto type = currentSelectedAxisType();
    if (type == Graph::AxisType::Day || type == Graph::AxisType::Month)
        formatInfo[axis] = QString::number(boxFormat->currentIndex());
    else if (type == Graph::AxisType::Time || type == Graph::AxisType::Date || type == Graph::AxisType::DateTime) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = formatInfo[axis].split(";", Qt::SkipEmptyParts);
#else
        QStringList lst = formatInfo[axis].split(";", QString::SkipEmptyParts);
#endif
        if (lst.size() < 2 || lst[0].isEmpty()) {
            lst = QStringList();
            if (type == Graph::AxisType::Time)
                lst << QTime(0, 0, 0).toString();
            else if (type == Graph::AxisType::Date)
                lst << QDate(1900, 1, 1).toString("yyyy-MM-dd");
            else
                lst << QDateTime(QDate(1900, 1, 1), QTime(0, 0, 0)).toString("yyyy-MM-ddThh:mm:ss");
            lst << boxFormat->currentText();
        } else
            lst[1] = boxFormat->currentText();
        formatInfo[axis] = lst.join(";");
    } else
        formatInfo[axis] = boxColName->currentText();

    QString formula = boxFormula->toPlainText();
    if (!boxShowFormula->isChecked())
        formula = QString();

    showAxis(axis, type, formatInfo[axis], boxShowAxis->isChecked(),
             boxMajorTicksType->currentIndex(), boxMinorTicksType->currentIndex(),
             boxShowLabels->isChecked(), boxAxisColor->color(), boxFormat->currentIndex(),
             boxPrecision->value(), boxAngle->value(), boxBaseline->value(), formula,
             boxAxisNumColor->color());
}

void AxesDialog::setCurrentScale(int axisPos)
{
    int axis = -1;
    switch (axisPos) {
    case QwtScaleDraw::LeftScale:
        axis = 1;
        break;
    case QwtScaleDraw::BottomScale:
        axis = 0;
        break;
    case QwtScaleDraw::RightScale:
        axis = 3;
        break;
    case QwtScaleDraw::TopScale:
        axis = 2;
        break;
    default:
        if (axisPos >= 0 && axisPos < 4)
            axis = axisPos;
        break;
    }
    if (axis >= 0) {
        axesList->setCurrentRow(axis);
        axesTitlesList->setCurrentRow(axis);
        if (axisBreakAxesList)
            axisBreakAxesList->setCurrentRow(axis);
    }
}

void AxesDialog::showAxesPage()
{
    if (generalDialog.currentWidget() != (QWidget *)axesPage)
        generalDialog.setCurrentWidget(axesPage);
}

void AxesDialog::showGridPage()
{
    if (generalDialog.currentWidget() != (QWidget *)gridPage)
        generalDialog.setCurrentWidget(gridPage);
}

void AxesDialog::setLabelsNumericFormat(int)
{
    int axis = mapToQwtAxisId();
    auto type = currentSelectedAxisType();
    int prec = boxPrecision->value();
    int format = boxFormat->currentIndex();

    Plot *plot = d_graph->plotWidget();

    if (type == Graph::AxisType::Numeric) {
        if (plot->axisLabelFormat(axis) == format && plot->axisLabelPrecision(axis) == prec)
            return;

        if (format == 0)
            boxPrecision->setEnabled(false);
        else
            boxPrecision->setEnabled(true);
    } else if (type == Graph::AxisType::Day || type == Graph::AxisType::Month)
        formatInfo[axis] = QString::number(format);
    else if (type == Graph::AxisType::Time || type == Graph::AxisType::Date || type == Graph::AxisType::DateTime) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = formatInfo[axis].split(";", Qt::KeepEmptyParts);
#else
        QStringList lst = formatInfo[axis].split(";", QString::KeepEmptyParts);
#endif
        if (lst.size() < 2 || lst[0].isEmpty()) {
            lst = QStringList();
            if (type == Graph::AxisType::Time)
                lst << QTime(0, 0, 0).toString();
            else if (type == Graph::AxisType::Date)
                lst << QDate(1900, 1, 1).toString("yyyy-MM-dd");
            else
                lst << QDateTime(QDate(1900, 1, 1), QTime(0, 0, 0)).toString("yyyy-MM-ddThh:mm:ss");
            lst << boxFormat->currentText();
        } else
            lst[1] = boxFormat->currentText();
        formatInfo[axis] = lst.join(";");
    } else
        formatInfo[axis] = boxColName->currentText();

    QString formula = boxFormula->toPlainText();
    if (!boxShowFormula->isChecked())
        formula = QString();

    showAxis(axis, type, formatInfo[axis], boxShowAxis->isChecked(),
             boxMajorTicksType->currentIndex(), boxMinorTicksType->currentIndex(),
             boxShowLabels->isChecked(), boxAxisColor->color(), format, prec, boxAngle->value(),
             boxBaseline->value(), formula, boxAxisNumColor->color());

    axesType[axis] = type;
}

void AxesDialog::showAxisFormula(int axis)
{
    QStringList l = d_graph->getAxesFormulas();
    QString formula = l[axis];
    if (!formula.isEmpty()) {
        boxShowFormula->setChecked(true);
        boxFormula->show();
        boxFormula->setText(formula);
    } else {
        boxShowFormula->setChecked(false);
        boxFormula->clear();
        boxFormula->hide();
    }
}

void AxesDialog::updateLabelsFormat(int)
{
    int a = mapToQwtAxisId();
    if (d_graph && a >= 0 && a < QwtPlot::axisCnt) {
        boxDivideFactor->blockSignals(true);
        boxTickPrefix->blockSignals(true);
        boxTickSuffix->blockSignals(true);
        boxDivideFactor->setText(QString::number(d_graph->tickDivideFactor(a), 'g', 12));
        boxTickPrefix->setText(d_graph->tickPrefix(a));
        boxTickSuffix->setText(d_graph->tickSuffix(a));
        boxDivideFactor->blockSignals(false);
        boxTickPrefix->blockSignals(false);
        boxTickSuffix->blockSignals(false);
    }

    if (currentSelectedAxisType() != Graph::AxisType::Numeric)
        return;

    int format = d_graph->plotWidget()->axisLabelFormat(a);
    boxFormat->setCurrentIndex(format);
    boxPrecision->setValue(d_graph->plotWidget()->axisLabelPrecision(a));

    if (format == 0)
        boxPrecision->setEnabled(false);
    else
        boxPrecision->setEnabled(true);

    QStringList l = d_graph->getAxesFormulas();
    QString formula = l[a];
    if (!formula.isEmpty()) {
        boxShowFormula->setChecked(true);
        boxFormula->show();
        boxFormula->setText(formula);
    } else {
        boxShowFormula->setChecked(false);
        boxFormula->clear();
        boxFormula->hide();
    }
}

void AxesDialog::showGeneralPage()
{
    generalDialog.setCurrentIndex(generalDialog.indexOf(frame));
}

void AxesDialog::showFormulaBox()
{
    if (boxShowFormula->isChecked())
        boxFormula->show();
    else
        boxFormula->hide();
}

void AxesDialog::customAxisLabelFont()
{
    if (!d_graph)
        return;

    int axis = mapToQwtAxisId();
    bool okF = false;
    QFont oldFont = d_graph->axisTitleFont(axis);
    if (oldFont.family().isEmpty())
        oldFont.setFamily("Arial");
    QFont fnt = QFontDialog::getFont(&okF, oldFont, this);
    if (okF && fnt != oldFont)
        d_graph->setAxisTitleFont(axis, fnt);
}

void AxesDialog::toggleAxisBackbone(bool checked)
{
    if (!d_graph)
        return;
    int axis = mapToQwtAxisId();
    if (axis >= 0 && axis < QwtPlot::axisCnt)
        d_graph->setAxisBackboneVisible(axis, checked);
}

void AxesDialog::applyPresetBoxInward()
{
    if (!d_graph)
        return;
    d_graph->applyFramePreset(Graph::FrameBoxInwardTicks);
    refreshGraphState();
}

void AxesDialog::applyPresetBoxPlain()
{
    if (!d_graph)
        return;
    d_graph->applyFramePreset(Graph::FrameBoxPlainOpposite);
    refreshGraphState();
}

void AxesDialog::applyPresetOpenL()
{
    if (!d_graph)
        return;
    d_graph->applyFramePreset(Graph::FrameOpenL);
    refreshGraphState();
}

void AxesDialog::applyPresetZeroCrossed()
{
    if (!d_graph)
        return;
    d_graph->applyFramePreset(Graph::FrameZeroCrossed);
    refreshGraphState();
}

void AxesDialog::refreshReferenceLinesList()
{
    if (!refLinesList)
        return;
    int prevRow = refLinesList->currentRow();
    refLinesList->blockSignals(true);
    refLinesList->clear();
    if (d_graph) {
        const QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
        for (int i = 0; i < lines.size(); ++i) {
            const Graph::ReferenceLineSpec &sp = lines[i];
            const QString orientTag = (sp.orientation == 1) ? QString("[X] ") : QString("[Y] ");
            QString dispStat = sp.statMode;
            if (dispStat == "Mean")
                dispStat = tr("Mean");
            else if (dispStat == "Median")
                dispStat = tr("Median");
            else if (dispStat == "Mean+SD" || dispStat == "+SD")
                dispStat = tr("+SD");
            else if (dispStat == "Mean-SD" || dispStat == "-SD")
                dispStat = tr("-SD");
            else if (dispStat == "Min")
                dispStat = tr("Min");
            else if (dispStat == "Max")
                dispStat = tr("Max");
            const QString modeText = sp.statMode.isEmpty()
                    ? QString::number(sp.value, 'g', 6)
                    : QString("%1 (%2)").arg(dispStat).arg(sp.value, 0, 'g', 5);
            const QString lbl = sp.label.isEmpty()
                    ? (orientTag + modeText)
                    : (orientTag + modeText + " - " + sp.label);
            refLinesList->addItem(lbl);
        }
    }
    refLinesList->blockSignals(false);
    if (refLinesList->count() > 0) {
        int sel = qBound(0, prevRow >= 0 ? prevRow : 0, refLinesList->count() - 1);
        refLinesList->setCurrentRow(sel);
        selectReferenceLine(sel);
    }
}

void AxesDialog::selectReferenceLine(int row)
{
    if (!d_graph)
        return;
    const QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
    if (row < 0 || row >= lines.size())
        return;

    const Graph::ReferenceLineSpec &sp = lines[row];
    refOrientBox->setCurrentIndex(sp.orientation == 1 ? 1 : 0);
    int statIdx = refStatBox->findData(sp.statMode);
    refStatBox->setCurrentIndex(statIdx >= 0 ? statIdx : 0);
    refValueBox->setValue(sp.value);
    refValueBox->setEnabled(sp.statMode.isEmpty());
    refColorBtn->setColor(sp.color);
    int styleIdx = refStyleBox->findData((int)sp.style);
    refStyleBox->setCurrentIndex(styleIdx >= 0 ? styleIdx : 1);
    refWidthBox->setValue(sp.width);
    refLabelEdit->setText(sp.label);
    refFillBox->setChecked(sp.fillToAxis);
    refFillColorBtn->setColor(sp.fillColor);
}

void AxesDialog::refStatModeChanged(int index)
{
    if (!refStatBox || !refValueBox)
        return;
    const QString mode = refStatBox->itemData(index).toString();
    refValueBox->setEnabled(mode.isEmpty());
}

void AxesDialog::addRefLineFromUi()
{
    if (!d_graph)
        return;
    Graph::ReferenceLineSpec sp;
    sp.orientation = refOrientBox->currentData().toInt();
    sp.statMode = refStatBox->currentData().toString();
    sp.value = refValueBox->value();
    sp.color = refColorBtn->color();
    sp.style = static_cast<Qt::PenStyle>(refStyleBox->currentData().toInt());
    sp.width = refWidthBox->value();
    sp.label = refLabelEdit->text().trimmed();
    sp.fillToAxis = refFillBox->isChecked();
    sp.fillColor = refFillColorBtn->color();
    if (sp.fillToAxis && sp.fillColor.alpha() == 255)
        sp.fillColor.setAlpha(36);
    d_graph->addReferenceLine(sp);
    refreshReferenceLinesList();
    if (refLinesList->count() > 0)
        refLinesList->setCurrentRow(refLinesList->count() - 1);
}

void AxesDialog::updateSelectedRefLineFromUi()
{
    if (!d_graph || !refLinesList)
        return;
    int row = refLinesList->currentRow();
    QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
    if (row < 0 || row >= lines.size())
        return;

    Graph::ReferenceLineSpec &sp = lines[row];
    sp.orientation = refOrientBox->currentData().toInt();
    sp.statMode = refStatBox->currentData().toString();
    sp.value = refValueBox->value();
    sp.color = refColorBtn->color();
    sp.style = static_cast<Qt::PenStyle>(refStyleBox->currentData().toInt());
    sp.width = refWidthBox->value();
    sp.label = refLabelEdit->text().trimmed();
    sp.fillToAxis = refFillBox->isChecked();
    sp.fillColor = refFillColorBtn->color();
    if (sp.fillToAxis && sp.fillColor.alpha() == 255)
        sp.fillColor.setAlpha(36);

    d_graph->setReferenceLines(lines);
    refreshReferenceLinesList();
}

void AxesDialog::removeSelectedRefLine()
{
    if (!d_graph || !refLinesList)
        return;
    int row = refLinesList->currentRow();
    QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
    if (row < 0 || row >= lines.size())
        return;
    lines.removeAt(row);
    d_graph->setReferenceLines(lines);
    refreshReferenceLinesList();
}

void AxesDialog::clearAllRefLines()
{
    if (!d_graph)
        return;
    d_graph->clearReferenceLines();
    refreshReferenceLinesList();
}

void AxesDialog::updateAxisBreakPage(int row)
{
    if (!d_graph || !axisBreakAxesList)
        return;
    if (row < 0)
        row = axisBreakAxesList->currentRow();
    if (row < 0 || row >= 4)
        row = 0;

    const int qwtAxis = Graph::mapToQwtAxis(row);
    const Graph::AxisBreakSpec spec = d_graph->axisBreak(qwtAxis);
    const QwtScaleDiv *scDiv = d_graph->plotWidget() ? d_graph->plotWidget()->axisScaleDiv(qwtAxis) : nullptr;
    double defFrom = 0.0, defTo = 0.0;
    if (scDiv) {
        const double lo = std::min(scDiv->lowerBound(), scDiv->upperBound());
        const double hi = std::max(scDiv->lowerBound(), scDiv->upperBound());
        defFrom = lo + 0.35 * (hi - lo);
        defTo = lo + 0.65 * (hi - lo);
    }

    boxEnableAxisBreak->setChecked(spec.enabled);
    boxBreakFrom->setText(QString::number(spec.enabled ? spec.breakFrom : defFrom, 'g', 10));
    boxBreakTo->setText(QString::number(spec.enabled ? spec.breakTo : defTo, 'g', 10));
    boxBreakPositionPct->setValue(qBound(10.0, spec.positionPct * 100.0, 90.0));
    boxBreakStepBefore->setText(QString::number(spec.stepBefore, 'g', 10));
    boxBreakStepAfter->setText(QString::number(spec.stepAfter, 'g', 10));
    boxBreakTypeBefore->setCurrentIndex(spec.log10Before ? 1 : 0);
    boxBreakTypeAfter->setCurrentIndex(spec.log10After ? 1 : 0);

    const bool en = spec.enabled;
    boxBreakFrom->setEnabled(en);
    boxBreakTo->setEnabled(en);
    boxBreakPositionPct->setEnabled(en);
    boxBreakStepBefore->setEnabled(en);
    boxBreakStepAfter->setEnabled(en);
    boxBreakTypeBefore->setEnabled(en);
    boxBreakTypeAfter->setEnabled(en);
}

void AxesDialog::applyTickLabelTransformFromUi()
{
    if (!d_graph)
        return;
    const int axis = mapToQwtAxisId();
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    double factor = 1.0;
    const QString factorTxt = boxDivideFactor ? boxDivideFactor->text().trimmed() : QString();
    if (!factorTxt.isEmpty()) {
        bool ok = false;
        double v = QLocale::c().toDouble(factorTxt, &ok);
        if (!ok)
            v = factorTxt.toDouble(&ok);
        if (!ok) {
            try {
                MyParser parser;
                parser.SetExpr(factorTxt.toUtf8().constData());
                v = parser.Eval();
                ok = std::isfinite(v);
            } catch (...) {
                ok = false;
            }
        }
        if (ok && std::isfinite(v) && std::fabs(v) > 1e-300)
            factor = v;
    }

    const QString pfx = boxTickPrefix ? boxTickPrefix->text() : QString();
    const QString sfx = boxTickSuffix ? boxTickSuffix->text() : QString();
    if (std::fabs(d_graph->tickDivideFactor(axis) - factor) > 1e-15
        || d_graph->tickPrefix(axis) != pfx
        || d_graph->tickSuffix(axis) != sfx) {
        d_graph->setTickLabelTransform(axis, factor, pfx, sfx);
    }
}

void AxesDialog::pageChanged(int page)
{
    QWidget *pageWidget = generalDialog.widget(page);
    int syncRow = -1;
    if (lastPage == scalesPage && axesList)
        syncRow = axesList->currentRow();
    else if (lastPage == axesPage && axesTitlesList)
        syncRow = axesTitlesList->currentRow();
    else if (lastPage == axisBreakPage && axisBreakAxesList)
        syncRow = axisBreakAxesList->currentRow();

    if (syncRow >= 0) {
        if (pageWidget == axesPage && axesTitlesList)
            axesTitlesList->setCurrentRow(syncRow);
        else if (pageWidget == scalesPage && axesList)
            axesList->setCurrentRow(syncRow);
        else if (pageWidget == axisBreakPage && axisBreakAxesList)
            axisBreakAxesList->setCurrentRow(syncRow);
    }
    if (pageWidget == axisBreakPage)
        updateAxisBreakPage();
    lastPage = pageWidget;
}

int AxesDialog::exec()
{
    axesList->setCurrentRow(0);
    axesGridList->setCurrentRow(0);
    axesTitlesList->setCurrentRow(0);
    if (axisBreakAxesList)
        axisBreakAxesList->setCurrentRow(0);

    setModal(true);
    show();
    return 0;
}

void AxesDialog::updateMinorTicksList(int scaleType)
{
    if (!d_graph)
        return;

    updatePlot();

    boxMinorValue->clear();
    if (scaleType) // log scale
        boxMinorValue->addItems(QStringList() << "0"
                                              << "2"
                                              << "4"
                                              << "8");
    else
        boxMinorValue->addItems(QStringList() << "0"
                                              << "1"
                                              << "4"
                                              << "9"
                                              << "14"
                                              << "19");

    int a = Graph::mapToQwtAxis(axesList->currentRow());
    boxMinorValue->setEditText(QString::number(d_graph->plotWidget()->axisMaxMinor(a)));
}

void AxesDialog::showAxis(int axis, Graph::AxisType type, const QString &labelsColName, bool axisOn,
                          int majTicksType, int minTicksType, bool labelsOn, const QColor &c,
                          int format, int prec, int rotation, int baselineDist,
                          const QString &formula, const QColor &labelsColor)
{
    if (!d_graph)
        return;

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    Table *w = app ? app->table(labelsColName) : nullptr;
    if ((type == Graph::AxisType::Txt || type == Graph::AxisType::ColHeader) && !w)
        return;

    d_graph->showAxis(axis, type, labelsColName, w, axisOn, majTicksType, minTicksType, labelsOn, c,
                      format, prec, rotation, baselineDist, formula, labelsColor);
}

Graph::AxisType AxesDialog::currentSelectedAxisType()
{
    int index = boxAxisType->currentIndex();
    if (index < 0)
        return Graph::AxisType::Numeric;
    return static_cast<Graph::AxisType>(boxAxisType->itemData(index).toInt());
}
