/***************************************************************************
    File                 : AxesDialog.h
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
#ifndef AXESDIALOG_H
#define AXESDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QList>
#include <QVector>
#include <QTextEdit>
#include <QTabWidget>
#include "SciDAVisObject.h"
#include "Graph.h"

class QListWidget;
class QListWidgetItem;
class QCheckBox;
class QGroupBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QDoubleSpinBox;
class QTabWidget;
class QWidget;
class QStringList;
class ColorButton;
class TextFormatButtons;

//! General plot options dialog
/**
 * Remark: Don't use this dialog as a non modal dialog!
 */
class AxesDialog : public QDialog // SciDAVisObject<QDialog>
{
    Q_OBJECT

public:
    //! Constructor
    /**
     * \param parent parent widget
     * \param fl window flags
     */
    AxesDialog();
    //! Destructor
    ~AxesDialog() {};

    void setGraph(Graph *g);

    Graph::AxisType currentSelectedAxisType();

protected:
    //! generate UI for the axes page
    void initAxesPage();
    //! generate UI for the scales page
    void initScalesPage();
    //! generate UI for the grid page
    void initGridPage();
    //! generate UI for the general page
    void initFramePage();
    //! generate UI for the reference lines page
    void initReferenceLinesPage();
    //! generate UI for the axis break page
    void initAxisBreakPage();
    void refreshGraphState();

    QPushButton *buttonApply;
    QPushButton *buttonOk;
    QPushButton *buttonCancel;
    QTabWidget generalDialog;
    QWidget *scalesPage;
    QLineEdit *boxEnd;
    QLineEdit *boxStart;
    QComboBox *boxScaleType;
    QComboBox *boxMinorValue;
    QLineEdit *boxStep;
    QCheckBox *btnInvert;
    QSpinBox *boxMajorValue;
    QRadioButton *btnStep, *btnMajor;
    QListWidget *axesList;
    QWidget *gridPage;
    QCheckBox *boxMajorGrid;
    QCheckBox *boxMinorGrid;
    QComboBox *boxTypeMajor;
    ColorButton *boxColorMinor;
    ColorButton *boxColorMajor;
    ColorButton *boxCanvasColor;
    QSpinBox *boxWidthMajor;
    QComboBox *boxTypeMinor;
    QSpinBox *boxWidthMinor;
    QCheckBox *boxXLine;
    QCheckBox *boxYLine;
    QListWidget *axesGridList;
    QWidget *axesPage, *frame;
    QListWidget *axesTitlesList;
    QGroupBox *boxShowLabels;
    QCheckBox *boxShowAxis;

    QTextEdit *boxFormula, *boxTitle;
    QSpinBox *boxFrameWidth, *boxPrecision, *boxAngle, *boxBaseline, *boxAxesLinewidth;
    QPushButton *btnAxesFont;
    QCheckBox *boxBackbones, *boxShowFormula;
    ColorButton *boxAxisColor;
    QComboBox *boxMajorTicksType, *boxMinorTicksType, *boxFormat, *boxAxisType, *boxColName;
    QGroupBox *boxFramed;
    QLabel *label1, *label2, *label3, *boxScaleTypeLabel, *minorBoxLabel, *labelTable;
    QLabel *labelDivideFactor, *labelTickPrefix, *labelTickSuffix;
    QLineEdit *boxDivideFactor, *boxTickPrefix, *boxTickSuffix;
    QSpinBox *boxMajorTicksLength, *boxMinorTicksLength, *boxBorderWidth;
    QComboBox *boxUnit, *boxTableName, *boxGridXAxis, *boxGridYAxis;
    ColorButton *boxFrameColor, *boxAxisNumColor;
    QGroupBox *labelBox;
    QPushButton *buttonLabelFont;
    TextFormatButtons *formatButtons;
    QCheckBox *boxAutoAxisTitle;
    QComboBox *boxAxisUnitFormat;
    QCheckBox *boxAxisBackbone;
    QPushButton *btnPresetBoxInward, *btnPresetBoxPlain, *btnPresetOpenL, *btnPresetZeroCrossed;
    QWidget *refLinesPage;
    QListWidget *refLinesList;
    QComboBox *refOrientBox, *refStatBox, *refStyleBox;
    QDoubleSpinBox *refValueBox, *refWidthBox;
    ColorButton *refColorBtn, *refFillColorBtn;
    QLineEdit *refLabelEdit;
    QCheckBox *refFillBox;
    QPushButton *btnRefAdd, *btnRefUpdate, *btnRefRemove, *btnRefClear;
    QWidget *axisBreakPage;
    QListWidget *axisBreakAxesList;
    QCheckBox *boxEnableAxisBreak;
    QLineEdit *boxBreakFrom, *boxBreakTo, *boxBreakStepBefore, *boxBreakStepAfter;
    QDoubleSpinBox *boxBreakPositionPct;
    QComboBox *boxBreakTypeBefore, *boxBreakTypeAfter;

public slots:
    void setAxisType(int axis);
    void updateTitleBox(int axis);
    bool updatePlot();
    void updateScale();
    void stepEnabled();
    void stepDisabled();
    void majorGridEnabled(bool on);
    void minorGridEnabled(bool on);
    void showGridOptions(int axis);
    void accept();
    void customAxisFont();
    void showAxis();
    void updateShowBox(int axis);
    void drawFrame(bool framed);

    void pickAxisColor();
    void pickAxisNumColor();
    void updateAxisColor(int);
    int mapToQwtAxisId();
    void updateTickLabelsList(bool);
    void updateMinorTicksList(int scaleType);
    void setTicksType(int);
    void setCurrentScale(int axisPos);

    void updateMajTicksType(int);
    void updateMinTicksType(int);
    void updateGrid();
    void updateFrame(int);
    void setLabelsNumericFormat(int);
    void updateLabelsFormat(int);
    void insertColList(const QStringList &cols);
    void showAxisFormatOptions(int format);
    void setBaselineDist(int);
    void changeBaselineDist(int baseline);
    void changeMinorTicksLength(int minLength);
    void changeMajorTicksLength(int majLength);
    void pickCanvasFrameColor();
    void changeAxesLinewidth(int);
    void drawAxesBackbones(bool);
    void showGeneralPage();
    void showAxesPage();
    void showGridPage();
    void showFormulaBox();
    void showAxisFormula(int axis);

    void customAxisLabelFont();
    void toggleAxisBackbone(bool checked);
    void applyPresetBoxInward();
    void applyPresetBoxPlain();
    void applyPresetOpenL();
    void applyPresetZeroCrossed();
    void refreshReferenceLinesList();
    void selectReferenceLine(int row);
    void refStatModeChanged(int index);
    void addRefLineFromUi();
    void updateSelectedRefLineFromUi();
    void removeSelectedRefLine();
    void clearAllRefLines();
    void updateAxisBreakPage(int row = -1);
    void applyTickLabelTransformFromUi();
    void onTitleTextEdited();
    void onAutoAxisTitleToggled(bool checked);
    void onAxisUnitFormatChanged(int index);

    //! Shows the dialog as a modal dialog
    /**
     * Show the dialog as a modal dialog and do
     * some initialization.
     */
    int exec();

private slots:
    void pageChanged(int);

protected:
    void showAxis(int, Graph::AxisType, const QString &, bool, int, int, bool, const QColor &, int, int, int,
                  int, const QString &, const QColor &);

    QStringList tickLabelsOn, formatInfo;
    QStringList tablesList;
    QList<int> majTicks, minTicks,  axesBaseline;
    QList<Graph::AxisType> axesType;
    QFont xBottomFont, yLeftFont, xTopFont, yRightFont;
    bool xAxisOn, yAxisOn, topAxisOn, rightAxisOn;
    int xBottomLabelsRotation, xTopLabelsRotation;

    Graph *d_graph;
    //! Last selected tab
    QWidget *lastPage;
};

#endif
