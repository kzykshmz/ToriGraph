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
#include <QPointer>
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
    void updateSidebarListWidths();
    void changeEvent(QEvent *event) override;

    QPushButton *buttonApplyAllAxes;
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
    //! Legacy grid zero-line controls: shown only while a grid zero line is enabled (the zero
    //! lines are now created in the Reference Lines tab).
    QLabel *labelGridZeroLines = nullptr;
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
    QSpinBox *refFillOpacityBox = nullptr;
    QPushButton *btnRefAdd, *btnRefUpdate, *btnRefRemove, *btnRefClear;
    QPushButton *btnRefAddZeroY = nullptr, *btnRefAddZeroX = nullptr;
    QWidget *axisBreakPage;
    QListWidget *axisBreakAxesList;
    QCheckBox *boxEnableAxisBreak;
    QLineEdit *boxBreakFrom, *boxBreakTo, *boxBreakStepBefore, *boxBreakStepAfter;
    QDoubleSpinBox *boxBreakPositionPct;
    QComboBox *boxBreakTypeBefore, *boxBreakTypeAfter;
    QPushButton *btnAutoRange;
    QDoubleSpinBox *boxTitleFontSize;
    QDoubleSpinBox *boxAxesFontSize;

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
    //! Select an axis by QwtScaleDraw::Alignment (BottomScale / LeftScale / ...); a plain 0-3 is
    //! taken as the tab row (0 = Bottom, 1 = Left, 2 = Top, 3 = Right).
    void setCurrentScale(int axisPos);
    //! Select an axis by QwtPlot axis id (QwtPlot::xBottom / yLeft / xTop / yRight) in every tab
    //! that has an axis list. Does not change the visible tab.
    void setCurrentQwtAxis(int qwtAxis);

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
    //! Append a constant X=0 (orientation 1) or Y=0 (orientation 0) reference line.
    void addZeroRefLine(int orientation);
    void updateSelectedRefLineFromUi();
    void removeSelectedRefLine();
    void clearAllRefLines();
    void updateAxisBreakPage(int row = -1);
    void applyTickLabelTransformFromUi();
    void onTitleTextEdited();
    void onAutoAxisTitleToggled(bool checked);
    void onAxisUnitFormatChanged(int index);
    void autoRangeCurrentAxis();
    void onTitleFontSizeChanged(double pt);
    void onAxesFontSizeChanged(double pt);
    void onAxesTitlesRowChanged(int row);
    void applyTickAndAxisStyleToAllAxes();

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

    struct StagedScaleEdit
    {
        bool dirty = false;
        QString start;
        QString end;
        QString step;
        int unitIndex = 0;
        bool useStep = true;
        int majorValue = 5;
        QString minorText;
        int scaleType = 0;
        bool inverted = false;
    };

    struct StagedGridEdit
    {
        bool dirty = false;
        bool majorOn = true;
        bool minorOn = false;
        QColor colorMajor;
        QColor colorMinor;
        int typeMajor = 0;
        int typeMinor = 0;
        int widthMajor = 1;
        int widthMinor = 1;
    };

    struct StagedAxisEdit
    {
        bool dirty = false;
        bool showAxis = true;
        bool showBackbone = true;
        QString title;
        bool autoTitle = true;
        int unitDisplayMode = 0;
        QFont titleFont;
        bool titleFontDirty = false;
        QFont axisFont;
        bool axisFontDirty = false;
        Graph::AxisType axisType = Graph::AxisType::Numeric;
        QColor axisColor;
        QColor axisNumColor;
        int majTicksType = 0;
        int minTicksType = 0;
        int baseline = 0;
        bool showLabels = true;
        QString colName;
        QString tableName;
        int formatIndex = 0;
        QString formatText;
        int precision = 0;
        int angle = 0;
        bool showFormula = false;
        QString formula;
        QString divideFactor = QStringLiteral("1");
        QString tickPrefix;
        QString tickSuffix;
    };

    struct StagedBreakEdit
    {
        bool dirty = false;
        bool enabled = false;
        QString breakFrom;
        QString breakTo;
        double positionPct = 50.0;
        QString stepBefore;
        QString stepAfter;
        int typeBefore = 0;
        int typeAfter = 0;
    };

    void clearAllDirtyState();
    void captureScaleRowFromUi(int row);
    void captureGridRowFromUi(int row);
    void captureAxisRowFromUi(int row);
    void captureBreakRowFromUi(int row);
    void markScaleDirty();
    void markGridDirty();
    void markAxisDirty();
    void markFrameDirty();
    void markBreakDirty();
    bool commitScaleRow(int row);
    void commitGridState();
    bool commitAxisRow(int row);
    void commitFrameState();
    bool commitBreakRow(int row);
    void ensureAxisRowStaged(int row);
    void stageFramePreset(Graph::FramePreset preset);
    QString previewAutoAxisTitle(int qwtAxis, Column::UnitFormat fmt) const;
    Graph::ReferenceLineSpec buildRefLineSpecFromUi() const;
    QString formatRefLineListLabel(const Graph::ReferenceLineSpec &sp) const;
    void syncSelectedRefLineFromUi();
    void updateLegacyZeroLineVisibility();

    QStringList tickLabelsOn, formatInfo;
    QStringList tablesList;
    QList<int> majTicks, minTicks,  axesBaseline;
    QList<Graph::AxisType> axesType;
    QFont xBottomFont, yLeftFont, xTopFont, yRightFont;
    bool xAxisOn, yAxisOn, topAxisOn, rightAxisOn;
    int xBottomLabelsRotation, xTopLabelsRotation;

    QPointer<Graph> d_graph;
    //! Last selected tab
    QWidget *lastPage;

    bool d_loadingUi = false;
    int d_prevScaleRow = -1;
    int d_prevGridRow = -1;
    int d_prevAxisRow = -1;
    int d_prevBreakRow = -1;
    bool d_scalesTabDirty = false;
    bool d_gridTabDirty = false;
    bool d_axesTabDirty = false;
    bool d_frameTabDirty = false;
    bool d_refLinesTabDirty = false;
    bool d_axisBreakTabDirty = false;
    bool d_gridSharedDirty = false;
    int d_stagedGridXAxisIdx = 0;
    int d_stagedGridYAxisIdx = 0;
    bool d_stagedGridXZero = false;
    bool d_stagedGridYZero = false;
    bool d_unitFormatDirty = false;
    int d_stagedUnitFormatIdx = 0;
    int d_stagedFramePreset = -1;
    bool d_refControlsEdited = false;
    QVector<Graph::ReferenceLineSpec> d_stagedRefLines;
    StagedScaleEdit d_stagedScales[4];
    StagedGridEdit d_stagedGrids[2];
    StagedAxisEdit d_stagedAxes[4];
    StagedBreakEdit d_stagedBreaks[4];
};

#endif
