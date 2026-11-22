/***************************************************************************
    File                 : PlotDialog.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Custom curves dialog

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
#ifndef PLOTDIALOG_H
#define PLOTDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QSet>
#include <QTreeWidgetItem>
#include "MultiLayer.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTabWidget;
class QWidget;
class QStringList;
class QGroupBox;
class QDoubleSpinBox;
class QRadioButton;
class QTreeWidget;

class LayerItem;
class CurveTreeItem;
class PatternBox;
class ColorButton;
class PenWidget;
class MultiLayer;
class SymbolBox;
class ColorMapEditor;
class QwtPlotItem;

//! Custom plot/curves dialog
class PlotDialog : public QDialog
{
    Q_OBJECT

public:
    PlotDialog(bool showExtended, QWidget *parent = 0, Qt::WindowFlags fl = Qt::Widget);
    void initFonts(const QFont &titlefont, const QFont &axesfont, const QFont &numbersfont,
                   const QFont &legendfont);
    void insertColumnsList(const QStringList &names) { columnNames = names; };
    void setMultiLayer(MultiLayer *ml);

public slots:
    void showAll(bool all);
    void togglePage(bool all) { showAll(all); }
    void selectCurve(int index);

private slots:
    void showStatistics();
    void customVectorsPage(bool angleMag);
    void updateEndPointColumns(const QString &text);

    void fillBoxSymbols();
    void fillSymbols();
    bool acceptParams();
    void showWorksheet();
    void quit();

    int setPlotType(CurveTreeItem *item);
    void changePlotType(int plotType);
    void setActiveCurve(CurveTreeItem *item);

    void insertTabs(int plot_type);
    void updateTabWindow(QTreeWidgetItem *currentItem, QTreeWidgetItem *previousItem);
    void showAreaColor(bool show);

    void removeSelectedCurve();
    void moveSelectedCurveUp();
    void moveSelectedCurveDown();
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onHollowSymbolToggled(bool checked);

    /******* error bars options **************/
    void pickErrorBarsColor(QColor);
    void changeErrorBarsType();
    void changeErrorBarsPlus();
    void changeErrorBarsMinus();
    void changeErrorBarsThrough();

    void setAutomaticBinning();
    bool validInput();
    void showPlotAssociations(QTreeWidgetItem *item, int);

    void setPiePenStyle(const Qt::PenStyle &style);

    // box plots
    void setBoxType(int index);
    void setBoxRangeType(int index);
    void setWhiskersRange(int index);

    // spectrograms
    void showDefaultContourLinesBox(bool show);
    void showColorMapEditor(bool show);
    void onColorMapPresetChanged(int index);
    void onReversePaletteToggled(bool checked);
    void onCustomZRangeToggled(bool checked);

protected slots:
    void setActiveLayer(LayerItem *item);
    void updateTreeWidgetItem(QTreeWidgetItem *item);
    void updateBackgroundTransparency(int alpha);
    void updateCanvasTransparency(int alpha);
    void updateAntialiasing(bool on);
    void updateBorder(int width);
    void pickBackgroundColor(QColor);
    void pickCanvasColor(QColor);
    void pickBorderColor(QColor);
    void changeMargin(int);
    void setTitlesFont();
    void setAxesLabelsFont();
    void setAxesNumbersFont();
    void setLegendsFont();
    void editCurve();
    void openColorPaletteEditor();
    void fitPageToLayersFromDialog();
    void fitLayersToPageFromDialog();
    void openPageSetupFromDialog();
    void syncPageTabFromMultiLayer();

private:
    void closeEvent(QCloseEvent *e);

    void clearTabWidget();
    void initPageTab();
    void initAxesPage();
    void initLinePage();
    void initSymbolsPage();
    void initHistogramPage();
    void initErrorsPage();
    void initSpacingPage();
    void initVectPage();
    void initBoxPage();
    void initPercentilePage();
    void initSpectrogramPage();
    void initLayerPage();
    void initFontsPage();
    void initPiePage();
    void initPrintPage();
    void contextMenuEvent(QContextMenuEvent *e);
    void connectTabDirtySignals(QWidget *tabPage);
    void markTabDirty(QWidget *tabPage);
    bool commitParamsForItem(QTreeWidgetItem *targetItem, bool explicitAccept);
    void moveSelectedCurveBy(int delta);
    void shiftCurveBy(int delta) { moveSelectedCurveBy(delta); }
    void updateCurveMoveButtons();

    QFont titleFont, legendFont, axesFont, numbersFont;

    MultiLayer *d_ml;
    QPointer<MultiLayer> d_mlGuard;
    bool d_loadingUi = false;
    QSet<QWidget *> d_dirtyTabs;
    QWidget *d_leftTreePanel = nullptr;
    QSplitter *d_mainSplitter = nullptr;
    QPushButton *btnMoveCurveUp = nullptr;
    QPushButton *btnMoveCurveDown = nullptr;
    QCheckBox *boxHollowSymbol = nullptr;
    QStringList columnNames;

    QPushButton *btnTitle, *btnAxesLabels, *btnAxesNumbers, *btnLegend;
    ColorMapEditor *colorMapEditor;
    QWidget *curvePlotTypeBox, *layerPage, *piePage, *fontsPage, *printPage;
    QTreeWidget *listBox;
    QCheckBox *boxAntialiasing, *boxAll, *boxScaleLayers, *boxPrintCrops;
    ColorButton *boxBorderColor, *boxBackgroundColor, *boxCanvasColor;
    QSpinBox *boxBackgroundTransparency, *boxCanvasTransparency, *boxBorderWidth, *boxMargin;
    QSpinBox *boxRadius, *boxPieLineWidth;
    ColorButton *boxFirstColor, *boxPieLineColor;
    PatternBox *boxPiePattern;
    QComboBox *boxPieLineStyle;

    QPushButton *buttonApply, *btnWorksheet;
    QPushButton *buttonOk, *btnMore;
    QPushButton *buttonCancel;
    QComboBox *boxPlotType;
    QWidget *linePage;
    QComboBox *boxConnect;
    PenWidget *penWidget;
    QDoubleSpinBox *boxPenWidth;
    QSpinBox *boxLineOpacity, *boxAreaOpacity, *boxSymbolOpacity;
    ColorButton *boxAreaColor;
    QWidget *symbolPage;
    QSpinBox *boxSymbolSize;
    ColorButton *boxSymbolColor, *boxFillColor;
    SymbolBox *boxSymbolStyle;
    PatternBox *boxPattern;
    QTabWidget *privateTabWidget;
    QWidget *errorsPage, *spectrogramPage;
    QGroupBox *fillGroupBox;
    QCheckBox *plusBox;
    QCheckBox *minusBox;
    QCheckBox *xBox;
    ColorButton *colorBox, *levelsColorBox;
    ColorButton *vectColorBox;
    QComboBox *widthBox;
    QComboBox *capBox;
    QCheckBox *throughBox;
    QLabel *labelPosition, *labelXEnd, *labelYEnd;
    QGroupBox *GroupBoxH;
    QWidget *histogramPage, *spacingPage;
    QLineEdit *binSizeBox, *histogramBeginBox, *histogramEndBox;
    QCheckBox *automaticBox;
    QPushButton *buttonStatistics, *btnEditCurve, *btnColorPalette;
    QSpinBox *gapBox, *offsetBox, *boxWidth;
    QWidget *vectPage, *boxPage, *percentilePage, *axesPage;
    QComboBox *xEndBox, *yEndBox, *boxType, *boxWhiskersType, *boxWhiskersRange, *boxRange;
    QSpinBox *headAngleBox, *headLengthBox, *vectWidthBox, *boxPercSize;
    QDoubleSpinBox *boxEdgeWidth;
    QCheckBox *filledHeadBox;
    QSpinBox *boxCoef, *boxWhiskersCoef;
    QCheckBox *boxFillSymbols, *boxFillSymbol;
    QCheckBox *boxViolinMode, *boxShowDataPoints;
    ColorButton *boxPercFillColor, *boxEdgeColor;
    QLabel *whiskerCoeffLabel, *whiskerRangeLabel, *boxCoeffLabel;
    QLabel *boxRangeLabel, *whiskerCntLabel, *boxCntLabel;
    QGroupBox *GroupBoxVectEnd;
    QComboBox *vectPosBox, *boxXAxis, *boxYAxis, *colorScaleBox, *boxContourStyle;
    QSpinBox *levelsBox, *contourWidthBox, *colorScaleWidthBox;
    QGroupBox *levelsGroupBox, *axisScaleBox, *imageGroupBox;
    QGroupBox *defaultPenBox;
    QRadioButton *defaultScaleBox, *grayScaleBox, *customScaleBox, *defaultContourBox,
            *autoContourBox;
    QComboBox *boxColorMapPreset;
    QCheckBox *boxReversePalette, *boxCustomZRange;
    QDoubleSpinBox *boxZMin, *boxZMax;
    QLineEdit *editColorBarTitle;

    SymbolBox *boxMaxStyle, *boxMinStyle, *boxMeanStyle, *box99Style, *box1Style;
    QDoubleSpinBox *whiskerCnt, *boxCnt;

    // OriginPro Page & Layer hierarchy controls
    QWidget *pageTab;
    QDoubleSpinBox *boxPageWidth, *boxPageHeight;
    QComboBox *boxPageUnit;
    QSpinBox *boxPageDpi, *boxPageZoom;
    QCheckBox *boxPageView;
    ColorButton *boxPageBgColor;
    QPushButton *btnFitPageToLayers, *btnFitLayersToPage, *btnOpenPageSetup;

    QSpinBox *boxLayerX, *boxLayerY, *boxLayerW, *boxLayerH;
    QCheckBox *boxKeepAspect;
    QDoubleSpinBox *boxAspectRatio;
    QComboBox *boxFramePreset;
    QComboBox *boxLinkParentLayer, *boxLinkXMode, *boxLinkYMode;
    QLineEdit *editLinkXFormula, *editLinkYFormula;
};

/*****************************************************************************
 *
 * Class LayerItem
 *
 *****************************************************************************/
//! LayerItem tree widget item class
class LayerItem : public QTreeWidgetItem
{
public:
    enum { LayerTreeItem = 1001 };
    LayerItem(Graph *g, QTreeWidgetItem *parent, const QString &s);

    Graph *graph() const { return d_graphGuard ? d_graphGuard.data() : nullptr; };
    void setActive(bool select);
    void rebuildCurvesList();

protected:
    void insertCurvesList();
    Graph *d_graph;
    QPointer<Graph> d_graphGuard;
};

/*****************************************************************************
 *
 * Class CurveTreeItem
 *
 *****************************************************************************/
//! CurveTreeItem tree widget item class
class CurveTreeItem : public QTreeWidgetItem
{
public:
    enum { PlotCurveTreeItem = 1002 };
    CurveTreeItem(const QwtPlotItem *curve, LayerItem *parent, const QString &s);

    Graph *graph() const
    {
        const LayerItem *li = dynamic_cast<const LayerItem *>(parent());
        return li ? li->graph() : nullptr;
    };
    bool isCurveAlive() const;
    void setActive(bool on);

    const QwtPlotItem *plotItem() const;
    const QwtPlotItem *curve() const { return plotItem(); };
    void updatePlotItemPointer(const QwtPlotItem *curve) { d_curve = curve; }
    int plotItemType() const;
    int plotItemIndex() const;
    int plotItemStyle() const;

protected:
    const QwtPlotItem *d_curve;
};

#endif
