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
#include "Graph.h"
#include "I18nJapanese.h"

#include <QLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QLocale>
#include <QValidator>
#include <QFontDialog>
#include <QFont>
#include <QMessageBox>
#include <QPointer>
#include <QSignalBlocker>
#include <QStatusBar>
#include <cmath>

namespace {

//! E7: integer pixel spin box that can show/enter its value in millimetres. The stored value
//! (value()/setValue()) stays in screen pixels (1 px = 1/96 in at 100 % zoom), so all callers
//! keep working; only the display changes.
class PxMmSpinBox : public QSpinBox
{
public:
    explicit PxMmSpinBox(QWidget *parent = nullptr) : QSpinBox(parent) { }
    void setMmMode(bool on)
    {
        if (d_mm == on)
            return;
        d_mm = on;
        setSuffix(on ? QStringLiteral(" mm") : d_px_suffix);
        setSingleStep(on ? 4 : d_px_step); // 4 px ~ 1.06 mm
        lineEdit()->setText(textFromValue(value()));
    }
    bool mmMode() const { return d_mm; }
    void setPixelSuffixAndStep(const QString &suffix, int step)
    {
        d_px_suffix = suffix;
        d_px_step = step;
        if (!d_mm) {
            setSuffix(suffix);
            setSingleStep(step);
        }
    }
    static double pxToMm(int px) { return double(px) * 25.4 / 96.0; }
    static int mmToPx(double mm) { return int(std::lround(mm * 96.0 / 25.4)); }

protected:
    QString textFromValue(int v) const override
    {
        if (!d_mm)
            return QSpinBox::textFromValue(v);
        return QLocale().toString(pxToMm(v), 'f', 1);
    }
    int valueFromText(const QString &text) const override
    {
        if (!d_mm)
            return QSpinBox::valueFromText(text);
        QString t = text;
        t.remove(suffix());
        bool ok = false;
        double mm = QLocale().toDouble(t.trimmed(), &ok);
        if (!ok)
            mm = t.trimmed().toDouble(&ok);
        return ok ? mmToPx(mm) : value();
    }
    QValidator::State validate(QString &input, int &pos) const override
    {
        if (!d_mm)
            return QSpinBox::validate(input, pos);
        Q_UNUSED(pos);
        QString t = input;
        t.remove(suffix());
        t = t.trimmed();
        if (t.isEmpty())
            return QValidator::Intermediate;
        bool ok = false;
        QLocale().toDouble(t, &ok);
        if (!ok)
            t.toDouble(&ok);
        return ok ? QValidator::Acceptable : QValidator::Intermediate;
    }

private:
    bool d_mm = false;
    QString d_px_suffix;
    int d_px_step = 5;
};

void registerLayerDialogTranslations()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("Collapse Inner Gaps (0-gap Stack/Grid)", "パネル間の隙間をゼロにする (0-gap スタック/グリッド)");
    reg("Delete Non-Empty Layers?", "曲線を含むレイヤーを削除しますか？");
    reg("%1 of the %2 layer(s) to be removed contain %3 plotted item(s).\n"
        "Are you sure you want to delete these layers and their curves?",
        "削除対象の %2 レイヤーのうち %1 レイヤーに合計 %3 個のプロットが含まれています。\n"
        "これらのレイヤーと曲線を削除してもよろしいですか？");
    reg("Units", "単位");
    // B5-2: twin-axis overlay vs. independent grid cell
    reg("Overlay (Twin Axes) / Axis Link / Layer Geometry",
        "重ね合わせ (ツイン軸) / 軸リンク / レイヤー位置");
    reg("Overlay On (Parent Layer)", "重ねる先 (親レイヤー)");
    reg("None (own grid cell)", "なし (独立したグリッドセル)");
    reg("X Axis Link to Parent", "X軸の親レイヤーとのリンク");
    reg("Y Axis Link to Parent", "Y軸の親レイヤーとのリンク");
    reg("Parent Layer = None: the layer keeps its own cell in the grid above (independent "
        "layer with its own axes).\n"
        "Parent Layer = Layer N: this layer is drawn on top of layer N as a twin-axis overlay and "
        "takes no grid cell; the X/Y link modes then tie its axis scales to layer N.",
        "親レイヤー = なし: 上のグリッド内に独立したセルを持ちます (軸も独立)。\n"
        "親レイヤー = レイヤー N: レイヤー N の上にツイン軸として重ねて描画し、グリッドのセルは"
        "占有しません。X/Y リンクモードで軸の範囲をレイヤー N に連動させます。");
    reg("Choose the layer on which this layer is overlaid as twin axes. \"None\" keeps it as an "
        "independent layer in its own grid cell.",
        "このレイヤーをツイン軸として重ねる先のレイヤーを選びます。「なし」の場合は独立した"
        "グリッドセルのレイヤーのままです。");
    reg("Axis scale link to the parent layer (available only when an overlay parent is selected).",
        "親レイヤーへの軸範囲リンクです (重ねる先を選んだときだけ有効)。");
    reg("Layers are placed row by row. The grid may have more cells than layers (for example "
        "2 x 2 for 3 layers); the unused cells stay empty.",
        "レイヤーは行ごとに順に配置されます。レイヤー数よりセルが多くてもかまいません "
        "(例: 3 枚を 2 x 2 に配置。空いたセルはそのまま空白になります)。");
    reg("The grid (%1 x %2 = %3 cells) is too small for the %4 layer(s) that occupy a grid cell!",
        "グリッド (%1 x %2 = %3 セル) が、セルを占有する %4 枚のレイヤーに対して小さすぎます。");
    reg("Grid input error", "グリッド入力エラー");
    reg("Arrange the layers automatically (grid and canvas size are chosen for you).",
        "レイヤーを自動配置します (グリッドとキャンバスサイズは自動で決まります)。");
}

} // namespace

LayerDialog::LayerDialog(QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl), multi_layer(nullptr)
{
    registerLayerDialogTranslations();
    setWindowTitle(tr("Arrange Layers"));

    QGroupBox *gb1 = new QGroupBox(tr("Layers"));
    QGridLayout *gl1 = new QGridLayout(gb1);
    gl1->addWidget(new QLabel(tr("Number")), 0, 0);
    layersBox = new QSpinBox();
    layersBox->setRange(0, 100);
    gl1->addWidget(layersBox, 0, 1);

    fitBox = new QCheckBox(tr("Automatic &layout"));
    fitBox->setChecked(false);
    fitBox->setToolTip(
            tr("Arrange the layers automatically (grid and canvas size are chosen for you)."));
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

    QGroupBox *gb2 = new QGroupBox(tr("Alignment"));
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
    GroupGrid->setToolTip(
            tr("Layers are placed row by row. The grid may have more cells than layers (for "
               "example 2 x 2 for 3 layers); the unused cells stay empty."));
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
    boxCanvasWidth = new PxMmSpinBox();
    boxCanvasWidth->setRange(0, 10000);
    static_cast<PxMmSpinBox *>(boxCanvasWidth)->setPixelSuffixAndStep(tr(" pixels"), 50);
    gl5->addWidget(boxCanvasWidth, 0, 1);
    gl5->addWidget(new QLabel(tr("Height")), 1, 0);
    boxCanvasHeight = new PxMmSpinBox();
    boxCanvasHeight->setRange(0, 10000);
    static_cast<PxMmSpinBox *>(boxCanvasHeight)->setPixelSuffixAndStep(tr(" pixels"), 50);
    gl5->addWidget(boxCanvasHeight, 1, 1);

    QGroupBox *gb4 = new QGroupBox(tr("Spacing"));
    QGridLayout *gl4 = new QGridLayout(gb4);
    gl4->addWidget(new QLabel(tr("Columns gap")), 0, 0);
    boxColsGap = new PxMmSpinBox();
    boxColsGap->setRange(0, 1000);
    static_cast<PxMmSpinBox *>(boxColsGap)->setPixelSuffixAndStep(tr(" pixels"), 5);
    gl4->addWidget(boxColsGap, 0, 1);
    gl4->addWidget(new QLabel(tr("Rows gap")), 1, 0);
    boxRowsGap = new PxMmSpinBox();
    boxRowsGap->setRange(0, 1000);
    static_cast<PxMmSpinBox *>(boxRowsGap)->setPixelSuffixAndStep(tr(" pixels"), 5);
    gl4->addWidget(boxRowsGap, 1, 1);
    gl4->addWidget(new QLabel(tr("Left margin")), 2, 0);
    boxLeftSpace = new PxMmSpinBox();
    boxLeftSpace->setRange(0, 1000);
    static_cast<PxMmSpinBox *>(boxLeftSpace)->setPixelSuffixAndStep(tr(" pixels"), 5);
    gl4->addWidget(boxLeftSpace, 2, 1);
    gl4->addWidget(new QLabel(tr("Right margin")), 3, 0);
    boxRightSpace = new PxMmSpinBox();
    boxRightSpace->setRange(0, 1000);
    static_cast<PxMmSpinBox *>(boxRightSpace)->setPixelSuffixAndStep(tr(" pixels"), 5);
    gl4->addWidget(boxRightSpace, 3, 1);
    gl4->addWidget(new QLabel(tr("Top margin")), 4, 0);
    boxTopSpace = new PxMmSpinBox();
    boxTopSpace->setRange(0, 1000);
    static_cast<PxMmSpinBox *>(boxTopSpace)->setPixelSuffixAndStep(tr(" pixels"), 5);
    gl4->addWidget(boxTopSpace, 4, 1);
    gl4->addWidget(new QLabel(tr("Bottom margin")), 5, 0);
    boxBottomSpace = new PxMmSpinBox();
    boxBottomSpace->setRange(0, 1000);
    static_cast<PxMmSpinBox *>(boxBottomSpace)->setPixelSuffixAndStep(tr(" pixels"), 5);
    gl4->addWidget(boxBottomSpace, 5, 1);

    // E7: margins, gaps and canvas size can be shown/entered in mm.
    QComboBox *spacingUnitBox = new QComboBox(gb4);
    spacingUnitBox->setObjectName(QStringLiteral("layerSpacingUnitCombo"));
    spacingUnitBox->addItem(tr("pixels"), 0);
    spacingUnitBox->addItem(QStringLiteral("mm"), 1);
    gl4->addWidget(new QLabel(tr("Units")), 7, 0);
    gl4->addWidget(spacingUnitBox, 7, 1);
    connect(spacingUnitBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, spacingUnitBox](int) {
                const bool mm = spacingUnitBox->currentData().toInt() == 1;
                for (QSpinBox *sb : { boxColsGap, boxRowsGap, boxLeftSpace, boxRightSpace,
                                      boxTopSpace, boxBottomSpace, boxCanvasWidth,
                                      boxCanvasHeight }) {
                    if (auto *pm = dynamic_cast<PxMmSpinBox *>(sb))
                        pm->setMmMode(mm);
                }
            });

    QCheckBox *collapseGapsBox =
            new QCheckBox(tr("Collapse Inner Gaps (0-gap Stack/Grid)"), gb4);
    collapseGapsBox->setObjectName(QStringLiteral("collapseInnerGapsCheck"));
    collapseGapsBox->setProperty("prevColsGap", 5);
    collapseGapsBox->setProperty("prevRowsGap", 5);
    gl4->addWidget(collapseGapsBox, 6, 0, 1, 2);

    connect(collapseGapsBox, &QCheckBox::toggled, this, [this, collapseGapsBox](bool checked) {
        if (checked) {
            if (boxColsGap->value() > 0)
                collapseGapsBox->setProperty("prevColsGap", boxColsGap->value());
            if (boxRowsGap->value() > 0)
                collapseGapsBox->setProperty("prevRowsGap", boxRowsGap->value());
            QSignalBlocker bc(boxColsGap);
            QSignalBlocker br(boxRowsGap);
            boxColsGap->setValue(0);
            boxRowsGap->setValue(0);
        } else {
            const int prevC = qMax(5, collapseGapsBox->property("prevColsGap").toInt());
            const int prevR = qMax(5, collapseGapsBox->property("prevRowsGap").toInt());
            QSignalBlocker bc(boxColsGap);
            QSignalBlocker br(boxRowsGap);
            boxColsGap->setValue(prevC);
            boxRowsGap->setValue(prevR);
        }
    });
    auto syncCollapseCheck = [this, collapseGapsBox]() {
        QSignalBlocker b(collapseGapsBox);
        collapseGapsBox->setChecked(boxColsGap->value() == 0 && boxRowsGap->value() == 0);
    };
    connect(boxColsGap, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [syncCollapseCheck](int) { syncCollapseCheck(); });
    connect(boxRowsGap, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [syncCollapseCheck](int) { syncCollapseCheck(); });

    QVBoxLayout *vbox1 = new QVBoxLayout();
    vbox1->addWidget(GroupGrid);
    vbox1->addWidget(GroupCanvasSize);

    buttonApply = new QPushButton(tr("&Apply"));
    buttonApply->setAutoDefault(false);
    buttonOk = new QPushButton(tr("&OK"));
    buttonOk->setDefault(true);
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

    GroupLinkAxes = new QGroupBox(tr("Overlay (Twin Axes) / Axis Link / Layer Geometry"));
    QGridLayout *glLink = new QGridLayout(GroupLinkAxes);
    glLink->addWidget(new QLabel(tr("Layer")), 0, 0);
    targetLayerBox = new QComboBox();
    glLink->addWidget(targetLayerBox, 0, 1);
    const QString parentTip =
            tr("Choose the layer on which this layer is overlaid as twin axes. \"None\" keeps it "
               "as an independent layer in its own grid cell.");
    QLabel *parentLabel = new QLabel(tr("Overlay On (Parent Layer)"));
    parentLabel->setToolTip(parentTip);
    glLink->addWidget(parentLabel, 0, 2);
    parentLayerBox = new QComboBox();
    parentLayerBox->setToolTip(parentTip);
    glLink->addWidget(parentLayerBox, 0, 3);

    const QString linkTip = tr("Axis scale link to the parent layer (available only when an "
                               "overlay parent is selected).");
    QLabel *xLinkLabel = new QLabel(tr("X Axis Link to Parent"));
    xLinkLabel->setToolTip(linkTip);
    glLink->addWidget(xLinkLabel, 1, 0);
    xLinkModeBox = new QComboBox();
    xLinkModeBox->addItem(tr("None"), 0);
    xLinkModeBox->addItem(tr("Straight (1:1)"), 1);
    xLinkModeBox->addItem(tr("Custom Formula"), 2);
    glLink->addWidget(xLinkModeBox, 1, 1);
    glLink->addWidget(new QLabel(tr("X Link Formula")), 1, 2);
    xFormulaEdit = new QLineEdit(QString("x"));
    xFormulaEdit->setPlaceholderText(QString("1000/(x+273.15)"));
    glLink->addWidget(xFormulaEdit, 1, 3);

    QLabel *yLinkLabel = new QLabel(tr("Y Axis Link to Parent"));
    yLinkLabel->setToolTip(linkTip);
    glLink->addWidget(yLinkLabel, 2, 0);
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

    // P0/B5-1: remember whether the user really edited the geometry fields.
    for (QSpinBox *sb : { layerGeoXBox, layerGeoYBox, layerGeoWBox, layerGeoHBox }) {
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
            if (!loadingGeo)
                userEditedGeo = true;
        });
    }

    // B5-2: explain overlay (twin axes) vs. independent grid cell.
    QLabel *overlayHint = new QLabel(
            tr("Parent Layer = None: the layer keeps its own cell in the grid above (independent "
               "layer with its own axes).\n"
               "Parent Layer = Layer N: this layer is drawn on top of layer N as a twin-axis "
               "overlay and takes no grid cell; the X/Y link modes then tie its axis scales to "
               "layer N."));
    overlayHint->setWordWrap(true);
    glLink->addWidget(overlayHint, 4, 0, 1, 4);

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
    userEditedGeo = false;

    layersBox->setValue(g->layers());
    boxX->setValue(g->getCols());
    boxY->setValue(g->getRows());
    boxColsGap->setValue(g->colsSpacing());
    boxRowsGap->setValue(g->rowsSpacing());
    if (auto *collapseGapsBox =
                findChild<QCheckBox *>(QStringLiteral("collapseInnerGapsCheck"))) {
        QSignalBlocker b(collapseGapsBox);
        if (g->colsSpacing() > 0)
            collapseGapsBox->setProperty("prevColsGap", g->colsSpacing());
        if (g->rowsSpacing() > 0)
            collapseGapsBox->setProperty("prevRowsGap", g->rowsSpacing());
        collapseGapsBox->setChecked(g->colsSpacing() == 0 && g->rowsSpacing() == 0);
    }
    boxLeftSpace->setValue(g->leftMargin());
    boxRightSpace->setValue(g->rightMargin());
    boxTopSpace->setValue(g->topMargin());
    boxBottomSpace->setValue(g->bottomMargin());
    boxCanvasWidth->setValue(g->layerCanvasSize().width());
    boxCanvasHeight->setValue(g->layerCanvasSize().height());
    alignHorBox->setCurrentIndex(g->horizontalAlignement());
    alignVertBox->setCurrentIndex(g->verticalAlignement());

    // B1-7: MultiLayer does not persist "Automatic layout" / "Layer Canvas Size" (fixed size)
    // flags, so the only faithful state when the dialog opens is "off" for both: the grid and
    // canvas boxes above show MultiLayer's current values and stay editable. Set the check
    // states explicitly (and sync the enabled state of the groups) so that re-using the
    // dialog for another MultiLayer never keeps the previous plot's choice.
    fitBox->setChecked(false);
    GroupCanvasSize->setChecked(false);
    enableLayoutOptions(false);

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
    parentLayerBox->addItem(tr("None (own grid cell)"), 0);

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

    // Load the layer's current geometry. This is not a user edit (P0/B5-1).
    loadLayerGeometryFields(g->geometry());
    onLinkModeChanged();
}

void LayerDialog::loadLayerGeometryFields(const QRect &geo)
{
    loadingGeo = true;
    layerGeoXBox->setValue(geo.x());
    layerGeoYBox->setValue(geo.y());
    layerGeoWBox->setValue(geo.width());
    layerGeoHBox->setValue(geo.height());
    loadingGeo = false;
    userEditedGeo = false;
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

int LayerDialog::gridCellLayerCount(int layerCount) const
{
    if (!multi_layer || layerCount <= 0)
        return qMax(0, layerCount);
    const int existing = multi_layer->layers();
    const int keepCount = qMin(layerCount, existing);
    const int targetNum = targetLayerBox ? targetLayerBox->currentData().toInt() : 0;
    const int parentNum = parentLayerBox ? parentLayerBox->currentData().toInt() : 0;
    Graph *pendingTarget =
            (targetNum >= 1 && targetNum <= keepCount) ? multi_layer->layer(targetNum) : nullptr;
    Graph *pendingParent = (pendingTarget && parentNum >= 1 && parentNum <= keepCount
                            && parentNum != targetNum)
            ? multi_layer->layer(parentNum)
            : nullptr;
    int n = 0;
    for (int i = 1; i <= keepCount; ++i) {
        Graph *lg = multi_layer->layer(i);
        if (!lg)
            continue;
        Graph *effParent = (lg == pendingTarget) ? pendingParent : lg->linkedParentLayer();
        // If a surviving layer's linked parent is among the layers being removed, it reverts to
        // its own grid cell once setLayersNumber(layerCount) deletes that parent.
        if (effParent) {
            bool parentSurvives = false;
            for (int j = 1; j <= keepCount; ++j) {
                if (multi_layer->layer(j) == effParent) {
                    parentSurvives = true;
                    break;
                }
            }
            if (!parentSurvives)
                effParent = nullptr;
        }
        if (effParent || multi_layer->isInsetLayer(lg))
            continue;
        ++n;
    }
    // layers that will be created by this Apply are ordinary grid layers
    if (layerCount > existing)
        n += layerCount - existing;
    return n;
}

void LayerDialog::update()
{
    setProperty("lastUpdateAborted", false);
    if (!multi_layer)
        return;

    int graphs = layersBox->value();
    int cols = boxX->value();
    int rows = boxY->value();

    // B5-10: only layers that occupy a grid cell count (overlay/inset layers have none).
    // A grid is valid when rows * cols >= that number, so e.g. 3 layers may use 2 x 2; the
    // unused trailing cell(s) simply stay empty (MultiLayer fills cells row by row).
    const int cellLayers = gridCellLayerCount(graphs);

    if (graphs > 0 && cellLayers > 0 && !fitBox->isChecked()) {
        if (cols > cellLayers) {
            QMessageBox::warning(this, tr("Columns input error"),
                                 tr("The number of columns you've entered is greater than the "
                                    "number of graphs (%1)!")
                                         .arg(cellLayers));
            boxX->setFocus();
            setProperty("lastUpdateAborted", true);
            return;
        }

        if (rows > cellLayers) {
            QMessageBox::warning(this, tr("Rows input error"),
                                 tr("The number of rows you've entered is greater than the number "
                                    "of graphs (%1)!")
                                         .arg(cellLayers));
            boxY->setFocus();
            setProperty("lastUpdateAborted", true);
            return;
        }

        if (rows * cols < cellLayers) {
            QMessageBox::warning(this, tr("Grid input error"),
                                 tr("The grid (%1 x %2 = %3 cells) is too small for the %4 "
                                    "layer(s) that occupy a grid cell!")
                                         .arg(cols)
                                         .arg(rows)
                                         .arg(rows * cols)
                                         .arg(cellLayers));
            boxX->setFocus();
            setProperty("lastUpdateAborted", true);
            return;
        }
    }

    int old_graphs = multi_layer->layers();
    int dn = old_graphs - graphs;
    if (dn > 0) {
        int nonEmptyLayers = 0;
        int totalItemsToDelete = 0;
        for (int i = graphs + 1; i <= old_graphs; ++i) {
            if (Graph *lg = multi_layer->layer(i)) {
                const int itemCnt = qMax(lg->curves(), lg->plotItemsList().size());
                if (itemCnt > 0) {
                    ++nonEmptyLayers;
                    totalItemsToDelete += itemCnt;
                }
            }
        }
        if (nonEmptyLayers > 0 && !ApplicationWindow::isNonInteractiveContext(this)) {
            const QMessageBox::StandardButton ans = QMessageBox::question(
                    this, tr("Delete Non-Empty Layers?"),
                    tr("%1 of the %2 layer(s) to be removed contain %3 plotted item(s).\n"
                       "Are you sure you want to delete these layers and their curves?")
                            .arg(nonEmptyLayers)
                            .arg(dn)
                            .arg(totalItemsToDelete),
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (ans != QMessageBox::Yes) {
                QSignalBlocker b(layersBox);
                layersBox->setValue(old_graphs);
                setProperty("lastUpdateAborted", true);
                return;
            }
        }
    }

    QPointer<MultiLayer> mlGuard = multi_layer;
    const QString beforeSnap = mlGuard ? mlGuard->captureUndoSnapshot() : QString();

    multi_layer->setLayersNumber(graphs);

    if (!graphs) {
        refreshLayerSelectors();
        if (mlGuard && !beforeSnap.isEmpty())
            mlGuard->pushUndoSnapshot(tr("Arrange Layers"), beforeSnap);
        return;
    }

    if (dn < 0) { // Customize new layers with user default settings
        ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
        if (app) {
            for (int i = old_graphs + 1; i <= graphs; i++)
                app->setPreferences(multi_layer->layer(i));
        }
    }

    // 配置の適用は利用者が求めた配置なので、収まらない格子にはページを広げてよい（ステータスバーで
    // 知らせる）。setKeepAspectRatio / setTargetAspectRatio も途中で並べ直すため、許可の範囲は
    // 設定の反映から最後の arrangeLayers までとし、余白・間隔・重ね合わせ・行列数を先に入れて
    // 途中の並べ直しも最終の格子で行う（許可のない途中の並べ直しで 50 px に潰れると目盛りが
    // 減り、最後の配置が「収まった」と判定されてしまう）。キャンバス寸法を固定したとき
    // （GroupCanvasSize）はページでなくウィンドウを合わせるので広げない。
    multi_layer->takeLayoutPageGrowth();
    const int targetNum = targetLayerBox->currentData().toInt();
    {
        LayoutPageGrowthScope growthScope(GroupCanvasSize->isChecked() ? nullptr : multi_layer);

        multi_layer->setAlignement(alignHorBox->currentIndex(), alignVertBox->currentIndex());

        multi_layer->setMargins(boxLeftSpace->value(), boxRightSpace->value(),
                                boxTopSpace->value(), boxBottomSpace->value());

        multi_layer->setSpacing(boxRowsGap->value(), boxColsGap->value());

        // Apply the overlay/link settings of the target layer BEFORE arranging: a layer that gets
        // an overlay parent no longer occupies a grid cell, and arrangeLayers() has to know that.
        if (Graph *targetG = multi_layer->layer(targetNum)) {
            const int parentNum = parentLayerBox->currentData().toInt();
            Graph *parentG = (parentNum > 0 && parentNum != targetNum)
                    ? multi_layer->layer(parentNum)
                    : nullptr;
            targetG->setLinkedParentLayer(parentG, xLinkModeBox->currentData().toInt(),
                                          xFormulaEdit->text(), yLinkModeBox->currentData().toInt(),
                                          yFormulaEdit->text());
        }

        if (!fitBox->isChecked()) {
            multi_layer->setCols(cols);
            multi_layer->setRows(rows);
        } else { // the grid arrangeLayers(true, ...) will choose, already for the steps below
            int bestRows = multi_layer->getRows();
            int bestCols = multi_layer->getCols();
            multi_layer->findBestLayout(bestRows, bestCols);
            multi_layer->setRows(bestRows);
            multi_layer->setCols(bestCols);
        }

        // 新しい比率を先に入れる: setKeepAspectRatio(true) は今の比率で並べ直すので、古い比率で
        // あふれた格子のためにページが広がり、新しい比率で収まっても広いページが残ってしまう。
        const bool newTargetFirst = !GroupCanvasSize->isChecked() && keepAspectBox->isChecked()
                && customAspectBox->value() > 0.05;
        if (newTargetFirst)
            multi_layer->setTargetAspectRatio(customAspectBox->value());
        multi_layer->setKeepAspectRatio(keepAspectBox->isChecked());
        if (GroupCanvasSize->isChecked()) {
            multi_layer->setLayerCanvasSize(boxCanvasWidth->value(), boxCanvasHeight->value());
            if (boxCanvasWidth->value() > 0 && boxCanvasHeight->value() > 0) {
                QSignalBlocker blocker(customAspectBox);
                customAspectBox->setValue((double)boxCanvasWidth->value()
                                          / (double)boxCanvasHeight->value());
            }
        }

        multi_layer->arrangeLayers(fitBox->isChecked(), GroupCanvasSize->isChecked());
    }
    const QSizeF grownFrom = multi_layer->takeLayoutPageGrowth();

    if (!GroupCanvasSize->isChecked()) { // show new layer canvas size
        QSignalBlocker bW(boxCanvasWidth);
        QSignalBlocker bH(boxCanvasHeight);
        boxCanvasWidth->setValue(multi_layer->layerCanvasSize().width());
        boxCanvasHeight->setValue(multi_layer->layerCanvasSize().height());
    }

    if (fitBox->isChecked()) { // show new grid settings
        QSignalBlocker bX(boxX);
        QSignalBlocker bY(boxY);
        boxX->setValue(multi_layer->getCols());
        boxY->setValue(multi_layer->getRows());
    }

    // P0/B5-1: the geometry spin boxes were loaded BEFORE arrangeLayers() moved the layers, so
    // writing them back unconditionally would put the active layer back on its old position.
    // Apply them only when the user actually edited them; otherwise the arranged geometry wins
    // and refreshLayerSelectors() below reloads the fields from the layer.
    if (userEditedGeo) {
        if (Graph *targetG = multi_layer->layer(targetNum)) {
            const QRect desiredGeo(layerGeoXBox->value(), layerGeoYBox->value(),
                                   layerGeoWBox->value(), layerGeoHBox->value());
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
        userEditedGeo = false;
    }
    refreshLayerSelectors();

    if (mlGuard && !beforeSnap.isEmpty())
        mlGuard->pushUndoSnapshot(tr("Arrange Layers"), beforeSnap);

    if (mlGuard && grownFrom.isValid()) {
        if (auto *app = qobject_cast<ApplicationWindow *>(this->parent()))
            if (QStatusBar *sb = app->statusBar())
                sb->showMessage(mlGuard->layoutPageGrowthMessage(grownFrom), 10000);
    }
}

void LayerDialog::accept()
{
    update();
    if (!property("lastUpdateAborted").toBool())
        QDialog::accept();
}

void LayerDialog::done(int r)
{
    QDialog::done(r);
    if (testAttribute(Qt::WA_DeleteOnClose))
        deleteLater();
}
