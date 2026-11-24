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
#include "I18nJapanese.h"
#include "MultiLayer.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "TextDialog.h"
#include "Grid.h"
#include "Plot.h"
#include "MyParser.h"
#include "ColorButton.h"
#include "TextFormatButtons.h"
#include "PlotCurve.h"
#include "PlotStyleLimits.h"
#include "ScaleDraw.h"
#include "Table.h"
#include "core/column/Column.h"

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
#include <QEvent>
#include <QList>
#include <QListWidget>
#include <QSignalBlocker>
#include <QVector>
#include <QGroupBox>
#include <QRadioButton>
#include <QToolButton>
#include <QTextCursor>
#include <QScrollArea>
#include <QGuiApplication>
#include <QLocale>
#include <QPair>
#include <QScreen>
#include <QSet>
#include <cmath>
#include <limits>

#include <qwt_plot.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>

#ifndef M_PI
#define M_PI 3.141592653589793238462643
#endif

namespace {

void registerAxesDialogTranslations()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("Horizontal Grid (Left/Right Y)", "水平グリッド (左・右 Y軸)");
    reg("Vertical Grid (Bottom/Top X)", "垂直グリッド (下・上 X軸)");
    reg("0 (None)", "0 (なし)");
    reg("1 (2 subdivisions)", "1 (2分割)");
    reg("2 (3 subdivisions)", "2 (3分割)");
    reg("4 (5 subdivisions)", "4 (5分割)");
    reg("8 (9 subdivisions)", "8 (9分割)");
    reg("9 (10 subdivisions)", "9 (10分割)");
    reg("14 (15 subdivisions)", "14 (15分割)");
    reg("19 (20 subdivisions)", "19 (20分割)");
    reg("Apply Tick & Axis Style to All 4 Axes", "目盛り・軸スタイルを4辺すべてに適用");
    reg("Apply Tick && Axis Style to All 4 Axes", "目盛り・軸スタイルを4辺すべてに適用");
    reg("Insert Subscript (<sub>...</sub>)", "下付き文字を挿入 (<sub>...</sub>)");
    reg("Insert Superscript (<sup>...</sup>)", "上付き文字を挿入 (<sup>...</sup>)");
    reg("Insert Materials Science Symbol", "材料科学記号・単位を挿入");
    reg("Extra border drawn around the plot area. For a normal 4-sided box use the axis lines "
        "(Draw backbones / Frame Presets) instead, otherwise the frame is drawn twice.",
        "プロット領域の外側に重ねる追加の外枠です。通常の4辺の枠は軸線 (Draw backbones / "
        "Frame Presets) で描いてください。併用すると二重線になります。");
    reg("Fill Opacity", "塗りの不透明度");
    reg("Add Y=0 Line", "Y=0 の線を追加");
    reg("Add X=0 Line", "X=0 の線を追加");
    reg("Zero lines (use Reference Lines)", "ゼロ基準線 (参照線タブを使用)");
}

// 単位表記のコンボ項目(0 = Name (Unit), 1 = Name / Unit (ISIJ), 2 = Name [Unit])と
// Column::UnitFormat の相互変換。
int unitFormatToIndex(Column::UnitFormat fmt)
{
    switch (fmt) {
    case Column::UnitFormat::ISIJSlash:
        return 1;
    case Column::UnitFormat::Bracket:
        return 2;
    default:
        return 0;
    }
}

Column::UnitFormat unitFormatFromIndex(int idx)
{
    switch (idx) {
    case 1:
        return Column::UnitFormat::ISIJSlash;
    case 2:
        return Column::UnitFormat::Bracket;
    default:
        return Column::UnitFormat::Paren;
    }
}

// 各タブのページを内部 QScrollArea にする(911x472 などの小画面でも使えるように)。
// 戻り値のスクロール領域がタブのページ本体(currentWidget() との比較に使う)で、
// innerOut にレイアウトを載せる中身のウィジェットを返す。
QWidget *createScrollPage(QWidget *&innerOut)
{
    QScrollArea *area = new QScrollArea();
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area->setAutoFillBackground(false);
    area->viewport()->setAutoFillBackground(false);
    QWidget *inner = new QWidget();
    inner->setAutoFillBackground(false);
    area->setWidget(inner);
    innerOut = inner;
    return area;
}

QStringList minorTickItemsForScale(bool isLog10)
{
    if (isLog10) {
        return QStringList() << AxesDialog::tr("0 (None)")
                             << AxesDialog::tr("2 (3 subdivisions)")
                             << AxesDialog::tr("4 (5 subdivisions)")
                             << AxesDialog::tr("8 (9 subdivisions)");
    }
    return QStringList() << AxesDialog::tr("0 (None)")
                         << AxesDialog::tr("1 (2 subdivisions)")
                         << AxesDialog::tr("4 (5 subdivisions)")
                         << AxesDialog::tr("9 (10 subdivisions)")
                         << AxesDialog::tr("14 (15 subdivisions)")
                         << AxesDialog::tr("19 (20 subdivisions)");
}

int parseLeadingMinorInt(const QString &text, int fallback = 0)
{
    const QString token = text.trimmed().section(QLatin1Char(' '), 0, 0);
    bool ok = false;
    const int val = token.toInt(&ok);
    return ok ? qMax(0, val) : fallback;
}

void selectOrSetMinorTickCombo(QComboBox *combo, int count)
{
    if (!combo)
        return;
    const int safeCount = qMax(0, count);
    for (int i = 0; i < combo->count(); ++i) {
        if (parseLeadingMinorInt(combo->itemText(i), -1) == safeCount) {
            combo->setCurrentIndex(i);
            return;
        }
    }
    combo->setEditText(QString::number(safeCount));
}

QString formatLocaleScalar(double val, int prec = 12)
{
    QLocale loc = QLocale();
    loc.setNumberOptions(loc.numberOptions() | QLocale::OmitGroupSeparator);
    return loc.toString(val, 'g', prec);
}

bool parseLocaleOrCScalar(const QString &trimmed, double *outVal)
{
    if (trimmed.isEmpty())
        return false;
    const QLocale curLoc = QLocale();
    QLocale locNoGroup = curLoc;
    locNoGroup.setNumberOptions((curLoc.numberOptions() & ~QLocale::OmitGroupSeparator)
                                | QLocale::RejectGroupSeparator);
    bool ok = false;
    double val = locNoGroup.toDouble(trimmed, &ok);
    if (ok && std::isfinite(val)) {
        if (outVal)
            *outVal = val;
        return true;
    }
    QLocale cLoc = QLocale::c();
    cLoc.setNumberOptions(QLocale::RejectGroupSeparator);
    val = cLoc.toDouble(trimmed, &ok);
    if (ok && std::isfinite(val)) {
        if (outVal)
            *outVal = val;
        return true;
    }
    if (!(curLoc.numberOptions() & (QLocale::OmitGroupSeparator | QLocale::RejectGroupSeparator))) {
        val = curLoc.toDouble(trimmed, &ok);
        if (ok && std::isfinite(val)) {
            if (outVal)
                *outVal = val;
            return true;
        }
    }
    return false;
}

QString normalizeExprDecimalCommas(const QString &expr)
{
    if (QLocale().decimalPoint() != QLatin1Char(','))
        return expr;
    static const QSet<QString> kMultiArgFns = {
        QStringLiteral("avg"),  QStringLiteral("min"),       QStringLiteral("max"),
        QStringLiteral("sum"),  QStringLiteral("beta"),      QStringLiteral("bessel_jn"),
        QStringLiteral("bessel_yn")
    };
    QVector<QPair<int, int>> skipRanges;
    const int n = expr.size();
    for (int i = 0; i < n; ++i) {
        if (!expr[i].isLetter() && expr[i] != QLatin1Char('_'))
            continue;
        const int idStart = i;
        while (i < n && (expr[i].isLetterOrNumber() || expr[i] == QLatin1Char('_')))
            ++i;
        const QString fnName = expr.mid(idStart, i - idStart).toLower();
        int j = i;
        while (j < n && expr[j].isSpace())
            ++j;
        if (j < n && expr[j] == QLatin1Char('(') && kMultiArgFns.contains(fnName)) {
            int depth = 1;
            int k = j + 1;
            bool hasCommaSpace = false;
            bool hasTightComma = false;
            while (k < n && depth > 0) {
                if (expr[k] == QLatin1Char('('))
                    ++depth;
                else if (expr[k] == QLatin1Char(')'))
                    --depth;
                else if (depth == 1 && expr[k] == QLatin1Char(',')) {
                    if ((k + 1 < n && expr[k + 1].isSpace())
                        || (k > 0 && expr[k - 1].isSpace()))
                        hasCommaSpace = true;
                    else
                        hasTightComma = true;
                }
                ++k;
            }
            if (depth == 0 && hasTightComma && !hasCommaSpace)
                skipRanges.append(qMakePair(j, k));
        }
        --i;
    }

    QString out = expr;
    for (int i = 1; i + 1 < n; ++i) {
        if (out[i] != QLatin1Char(',') || !out[i - 1].isDigit() || !out[i + 1].isDigit())
            continue;
        bool inSkip = false;
        for (const auto &rng : skipRanges) {
            if (i > rng.first && i < rng.second) {
                inSkip = true;
                break;
            }
        }
        if (!inSkip)
            out[i] = QLatin1Char('.');
    }
    return out;
}

bool evalAxisScalarExpr(const QString &raw, double *out, QString *errMsg = nullptr)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        if (errMsg)
            *errMsg = QObject::tr("Expression is empty.");
        return false;
    }

    double val = 0.0;
    if (parseLocaleOrCScalar(trimmed, &val)) {
        if (out)
            *out = val;
        return true;
    }

    const QString normalized = normalizeExprDecimalCommas(trimmed);
    auto tryEval = [&](const QString &expr, double &res, QString *localErr) -> bool {
        try {
            MyParser parser;
            parser.SetExpr(expr);
            res = parser.Eval();
            if (!std::isfinite(res)) {
                if (localErr)
                    *localErr = QObject::tr("Expression evaluates to a non-finite value.");
                return false;
            }
            return true;
        } catch (mu::ParserError &e) {
            if (localErr)
                *localErr = QStringFromString(e.GetMsg());
            return false;
        }
    };

    QString primaryErr;
    if (tryEval(normalized, val, &primaryErr)) {
        if (out)
            *out = val;
        return true;
    }
    if (normalized != trimmed) {
        double rawVal = 0.0;
        if (tryEval(trimmed, rawVal, nullptr)) {
            if (out)
                *out = rawVal;
            return true;
        }
    }
    if (errMsg)
        *errMsg = primaryErr;
    return false;
}

double evalAxisScalarOrEmptyFallback(const QString &raw, double fallback, bool *okOut)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        if (okOut)
            *okOut = true;
        return fallback;
    }
    double val = fallback;
    const bool ok = evalAxisScalarExpr(trimmed, &val);
    if (okOut)
        *okOut = ok;
    return ok ? val : fallback;
}

} // namespace

AxesDialog::AxesDialog()
    : buttonApplyAllAxes(nullptr),
      axesList(nullptr),
      axesGridList(nullptr),
      axesTitlesList(nullptr),
      axisBreakAxesList(nullptr),
      btnAutoRange(nullptr),
      boxTitleFontSize(nullptr),
      boxAxesFontSize(nullptr),
      xBottomFont(QFont("Arial", 12, QFont::Normal, false)),
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
    registerAxesDialogTranslations();
    setWindowTitle(tr("General Plot Options"));
    setSizeGripEnabled(true);

    initScalesPage();
    initGridPage();
    initAxesPage();
    initFramePage();
    initReferenceLinesPage();
    initAxisBreakPage();
    updateSidebarListWidths();

    // 各タブは内部 QScrollArea を持つ(小画面対応)。テスト・外部コードが判別できるよう目印を付ける。
    setProperty("toriHasInternalScroll", true);

    // buttonApplyAllAxes は Axis タブ内(initAxesPage)で生成する。ここは OK/Apply/Cancel のみ。
    auto bottomButtons = new QHBoxLayout();
    bottomButtons->addStretch();

    buttonApply = new QPushButton();
    buttonApply->setText(tr("&Apply"));
    buttonApply->setMinimumWidth(84);
    bottomButtons->addWidget(buttonApply);

    buttonOk = new QPushButton();
    buttonOk->setText(tr("&OK"));
    buttonOk->setDefault(true);
    buttonOk->setMinimumWidth(84);
    bottomButtons->addWidget(buttonOk);

    buttonCancel = new QPushButton();
    buttonCancel->setText(tr("&Cancel"));
    buttonCancel->setMinimumWidth(84);
    bottomButtons->addWidget(buttonCancel);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(&generalDialog);
    mainLayout->addLayout(bottomButtons);

    lastPage = scalesPage;

    connect(buttonApplyAllAxes, SIGNAL(clicked()), this, SLOT(applyTickAndAxisStyleToAllAxes()));
    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(buttonApply, SIGNAL(clicked()), this, SLOT(updatePlot()));
    connect(&generalDialog, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));

    ModernUiTheme::upgradeAllWindowIcons(this);
}

void AxesDialog::updateSidebarListWidths()
{
    for (QListWidget *lw : { axesList, axesGridList, axesTitlesList, axisBreakAxesList }) {
        if (!lw)
            continue;
        QFontMetrics fm(lw->font());
        int maxTextWidth = 48;
        for (int i = 0; i < lw->count(); ++i) {
            if (!lw->item(i))
                continue;
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
            const int itemW = fm.width(lw->item(i)->text());
#else
            const int itemW = fm.horizontalAdvance(lw->item(i)->text());
#endif
            if (itemW > maxTextWidth)
                maxTextWidth = itemW;
        }
        const int needed = lw->iconSize().width() + maxTextWidth + 56;
        lw->setMinimumWidth(qMax(120, needed));
        lw->setMaximumWidth(qMax(160, needed + 24));
        lw->updateGeometry();
    }
}

void AxesDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (!event)
        return;
    if (event->type() == QEvent::LanguageChange || event->type() == QEvent::FontChange) {
        if (axesGridList && axesGridList->count() >= 2) {
            axesGridList->item(0)->setText(tr("Horizontal Grid (Left/Right Y)"));
            axesGridList->item(1)->setText(tr("Vertical Grid (Bottom/Top X)"));
        }
        for (QListWidget *lw : { axesList, axesTitlesList, axisBreakAxesList }) {
            if (lw && lw->count() >= 4) {
                lw->item(0)->setText(tr("Bottom"));
                lw->item(1)->setText(tr("Left"));
                lw->item(2)->setText(tr("Top"));
                lw->item(3)->setText(tr("Right"));
            }
        }
        if (buttonApplyAllAxes) {
            buttonApplyAllAxes->setText(tr("Apply Tick && Axis Style to All 4 Axes"));
            buttonApplyAllAxes->setToolTip(tr("Apply Tick & Axis Style to All 4 Axes"));
        }
        if (boxMinorValue && boxScaleType) {
            const int curMinor = parseLeadingMinorInt(boxMinorValue->currentText(), 0);
            QSignalBlocker b(boxMinorValue);
            boxMinorValue->clear();
            boxMinorValue->addItems(minorTickItemsForScale(boxScaleType->currentIndex() == 1));
            selectOrSetMinorTickCombo(boxMinorValue, curMinor);
        }
        updateSidebarListWidths();
    }
}

void AxesDialog::initScalesPage()
{
    QWidget *scalesInner = nullptr;
    scalesPage = createScrollPage(scalesInner);

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

    btnAutoRange = new QPushButton(tr("Auto Range"));
    middleLayout->addWidget(btnAutoRange, 4, 0, 1, 2);

    middleLayout->setRowStretch(5, 1);

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
    boxMinorValue->addItems(minorTickItemsForScale(false));
    rightLayout->addWidget(boxMinorValue, 2, 1);

    rightLayout->setRowStretch(3, 1);

    const QIcon scaleIcon = IconLoader::duotoneIcon(QStringLiteral("axis-scale"));

    axesList = new QListWidget();
    axesList->addItem(new QListWidgetItem(scaleIcon, tr("Bottom")));
    axesList->addItem(new QListWidgetItem(scaleIcon, tr("Left")));
    axesList->addItem(new QListWidgetItem(scaleIcon, tr("Top")));
    axesList->addItem(new QListWidgetItem(scaleIcon, tr("Right")));
    axesList->setIconSize(QSize(24, 24));
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

    QHBoxLayout *mainLayout = new QHBoxLayout(scalesInner);
    mainLayout->addWidget(axesList);
    mainLayout->addWidget(middleBox);
    mainLayout->addWidget(rightBox);

    generalDialog.addTab(scalesPage, tr("Scale"));

    connect(axesList, SIGNAL(currentRowChanged(int)), this, SLOT(updateScale()));
    connect(boxScaleType, SIGNAL(activated(int)), this, SLOT(updateMinorTicksList(int)));
    connect(btnAutoRange, SIGNAL(clicked()), this, SLOT(autoRangeCurrentAxis()));

    connect(btnStep, SIGNAL(toggled(bool)), boxStep, SLOT(setEnabled(bool)));
    connect(btnStep, SIGNAL(toggled(bool)), boxUnit, SLOT(setEnabled(bool)));
    connect(btnMajor, SIGNAL(toggled(bool)), boxMajorValue, SLOT(setEnabled(bool)));

    connect(boxStart, &QLineEdit::textChanged, this, [this]() { markScaleDirty(); });
    connect(boxEnd, &QLineEdit::textChanged, this, [this]() { markScaleDirty(); });
    connect(boxStep, &QLineEdit::textChanged, this, [this]() { markScaleDirty(); });
    connect(boxUnit, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int) { markScaleDirty(); });
    connect(btnStep, &QRadioButton::toggled, this, [this](bool) { markScaleDirty(); });
    connect(btnMajor, &QRadioButton::toggled, this, [this](bool) { markScaleDirty(); });
    connect(boxMajorValue, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int) { markScaleDirty(); });
    connect(boxMinorValue, &QComboBox::currentTextChanged, this,
            [this](const QString &) { markScaleDirty(); });
    connect(boxScaleType, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { markScaleDirty(); });
    connect(btnInvert, &QCheckBox::toggled, this, [this](bool) { markScaleDirty(); });
}

void AxesDialog::initGridPage()
{
    QWidget *gridInner = nullptr;
    gridPage = createScrollPage(gridInner);

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
    boxWidthMajor->setSuffix(QStringLiteral(" px"));
    boxWidthMajor->setValue(1);
    rightLayout->addWidget(boxWidthMajor, 3, 1);

    boxWidthMinor = new QSpinBox();
    boxWidthMinor->setRange(1, 20);
    boxWidthMinor->setSuffix(QStringLiteral(" px"));
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

    // X=0 / Y=0 のゼロ基準線は Reference Lines タブに一本化した。ここのチェックボックスは
    // 旧ファイルや Zero-Crossed プリセットでグリッドのゼロ線が有効なときだけ(消せるように)表示する。
    // 読み書き経路(showGridOptions / updateGrid / commitGridState の d_stagedGridX/YZero)は
    // 互換のためそのまま残す。表示の切り替えは updateLegacyZeroLineVisibility()。
    labelGridZeroLines = new QLabel(tr("Zero lines (use Reference Lines)"));
    rightLayout->addWidget(labelGridZeroLines, 5, 0);

    boxXLine = new QCheckBox();
    boxXLine->setText(tr("X=0"));
    boxXLine->setDisabled(true);
    rightLayout->addWidget(boxXLine, 5, 1);

    boxYLine = new QCheckBox();
    boxYLine->setText(tr("Y=0"));
    rightLayout->addWidget(boxYLine, 5, 2);

    labelGridZeroLines->hide();
    boxXLine->hide();
    boxYLine->hide();

    rightLayout->setRowStretch(6, 1);
    rightLayout->setColumnStretch(4, 1);

    const QIcon gridIcon = IconLoader::duotoneIcon(QStringLiteral("axis-grid"));

    axesGridList = new QListWidget();
    axesGridList->addItem(new QListWidgetItem(gridIcon, tr("Horizontal Grid (Left/Right Y)")));
    axesGridList->addItem(new QListWidgetItem(gridIcon, tr("Vertical Grid (Bottom/Top X)")));
    axesGridList->setIconSize(QSize(24, 24));
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

    QHBoxLayout *mainLayout2 = new QHBoxLayout(gridInner);
    mainLayout2->addWidget(axesGridList);
    mainLayout2->addWidget(rightBox);

    generalDialog.addTab(gridPage, tr("Grid"));

    // grid page slot connections
    connect(axesGridList, SIGNAL(currentRowChanged(int)), this, SLOT(showGridOptions(int)));

    connect(boxMajorGrid, SIGNAL(toggled(bool)), this, SLOT(majorGridEnabled(bool)));
    connect(boxMinorGrid, SIGNAL(toggled(bool)), this, SLOT(minorGridEnabled(bool)));
    connect(boxColorMajor, &ColorButton::changed, this, [this](const QColor &) { markGridDirty(); });
    connect(boxColorMinor, &ColorButton::changed, this, [this](const QColor &) { markGridDirty(); });
    connect(boxTypeMajor, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { markGridDirty(); });
    connect(boxTypeMinor, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { markGridDirty(); });
    connect(boxWidthMajor, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int) { markGridDirty(); });
    connect(boxWidthMinor, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int) { markGridDirty(); });
    connect(boxGridXAxis, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (!d_loadingUi) {
                    d_gridSharedDirty = true;
                    d_stagedGridXAxisIdx = boxGridXAxis->currentIndex();
                    markGridDirty();
                }
            });
    connect(boxGridYAxis, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (!d_loadingUi) {
                    d_gridSharedDirty = true;
                    d_stagedGridYAxisIdx = boxGridYAxis->currentIndex();
                    markGridDirty();
                }
            });
    connect(boxXLine, &QCheckBox::toggled, this, [this](bool checked) {
        if (!d_loadingUi) {
            d_gridSharedDirty = true;
            d_stagedGridXZero = checked;
            markGridDirty();
        }
    });
    connect(boxYLine, &QCheckBox::toggled, this, [this](bool checked) {
        if (!d_loadingUi) {
            d_gridSharedDirty = true;
            d_stagedGridYZero = checked;
            markGridDirty();
        }
    });
}

void AxesDialog::initAxesPage()
{
    // axes page
    const QIcon axisItemIcon = IconLoader::duotoneIcon(QStringLiteral("axis-ticks-in"));

    QWidget *axesInner = nullptr;
    axesPage = createScrollPage(axesInner);

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
    axesTitlesList->addItem(new QListWidgetItem(axisItemIcon, tr("Bottom")));
    axesTitlesList->addItem(new QListWidgetItem(axisItemIcon, tr("Left")));
    axesTitlesList->addItem(new QListWidgetItem(axisItemIcon, tr("Top")));
    axesTitlesList->addItem(new QListWidgetItem(axisItemIcon, tr("Right")));
    axesTitlesList->setIconSize(QSize(24, 24));
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
    topLayout->addWidget(labelBox, 1);

    QVBoxLayout *labelBoxLayout = new QVBoxLayout(labelBox);
    labelBoxLayout->setSpacing(2);

    boxTitle = new QTextEdit();
    boxTitle->setAcceptRichText(false);
    QFontMetrics metrics(this->font());
    boxTitle->setMaximumHeight(2 * metrics.height() + 12);
    boxTitle->setMinimumWidth(220);
    labelBoxLayout->addWidget(boxTitle);

    QHBoxLayout *hl = new QHBoxLayout();
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(4);
    buttonLabelFont = new QPushButton(tr("&Font"));
    hl->addWidget(buttonLabelFont);

    boxTitleFontSize = new PlotStyleLimits::FractionalPtSpinBox();
    boxTitleFontSize->setRange(PlotStyleLimits::kFontSizeMin, PlotStyleLimits::kFontSizeMax);
    boxTitleFontSize->setSingleStep(PlotStyleLimits::kFontSizeStep);
    boxTitleFontSize->setValue(12.0);
    boxTitleFontSize->setToolTip(tr("Axis Title Font Size"));
    hl->addWidget(boxTitleFontSize);
    hl->addStretch();

    labelBoxLayout->addLayout(hl);

    boxAutoAxisTitle = new QCheckBox(tr("Auto Title from Column"));
    boxAutoAxisTitle->setChecked(true);
    labelBoxLayout->addWidget(boxAutoAxisTitle);

    formatButtons = new TextFormatButtons(boxTitle);
    formatButtons->toggleCurveButton(false);
    labelBoxLayout->addWidget(formatButtons);

    QHBoxLayout *ufLayout = new QHBoxLayout();
    ufLayout->setMargin(0);
    ufLayout->setSpacing(4);
    ufLayout->addWidget(new QLabel(tr("Unit Format:")));
    boxAxisUnitFormat = new QComboBox();
    boxAxisUnitFormat->addItem(tr("Name (Unit)"));
    boxAxisUnitFormat->addItem(tr("Name / Unit (ISIJ)"));
    boxAxisUnitFormat->addItem(tr("Name [Unit]"));
    ufLayout->addWidget(boxAxisUnitFormat, 1);
    labelBoxLayout->addLayout(ufLayout);

    connect(boxTitle, SIGNAL(textChanged()), this, SLOT(onTitleTextEdited()));
    connect(boxAutoAxisTitle, SIGNAL(toggled(bool)), this, SLOT(onAutoAxisTitleToggled(bool)));
    connect(boxAxisUnitFormat, SIGNAL(currentIndexChanged(int)), this,
            SLOT(onAxisUnitFormatChanged(int)));
    connect(boxTitleFontSize, SIGNAL(valueChanged(double)), this,
            SLOT(onTitleFontSizeChanged(double)));

    QHBoxLayout *bottomLayout = new QHBoxLayout();

    QGroupBox *leftBox = new QGroupBox(QString());
    bottomLayout->addWidget(leftBox);
    QGridLayout *leftBoxLayout = new QGridLayout(leftBox);

    leftBoxLayout->addWidget(new QLabel(tr("Type")), 0, 0);

    leftBoxLayout->addWidget(boxAxisType, 0, 1);

    leftBoxLayout->addWidget(new QLabel(tr("Font")), 1, 0);

    QHBoxLayout *axesFontRowLayout = new QHBoxLayout();
    axesFontRowLayout->setContentsMargins(0, 0, 0, 0);
    axesFontRowLayout->setSpacing(4);
    btnAxesFont = new QPushButton();
    btnAxesFont->setText(tr("Axis &Font"));
    axesFontRowLayout->addWidget(btnAxesFont);
    boxAxesFontSize = new PlotStyleLimits::FractionalPtSpinBox();
    boxAxesFontSize->setRange(PlotStyleLimits::kFontSizeMin, PlotStyleLimits::kFontSizeMax);
    boxAxesFontSize->setSingleStep(PlotStyleLimits::kFontSizeStep);
    boxAxesFontSize->setValue(12.0);
    boxAxesFontSize->setToolTip(tr("Tick Labels Font Size"));
    axesFontRowLayout->addWidget(boxAxesFontSize);
    leftBoxLayout->addLayout(axesFontRowLayout, 1, 1);

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
    boxBaseline->setRange(-500, 1000);
    boxBaseline->setSuffix(QStringLiteral(" px"));
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
    boxAngle->setSuffix(QString::fromUtf8("\xC2\xB0"));
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

    // 「4辺すべてに適用」は Axis タブ専用の操作なので、全タブ共通のフッターからここへ移した。
    QHBoxLayout *applyAllRow = new QHBoxLayout();
    buttonApplyAllAxes = new QPushButton(tr("Apply Tick && Axis Style to All 4 Axes"));
    buttonApplyAllAxes->setToolTip(tr("Apply Tick & Axis Style to All 4 Axes"));
    applyAllRow->addWidget(buttonApplyAllAxes);
    applyAllRow->addStretch();

    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addLayout(topLayout);
    rightLayout->addLayout(bottomLayout);
    rightLayout->addLayout(applyAllRow);
    rightLayout->addStretch(1);

    QHBoxLayout *mainLayout3 = new QHBoxLayout(axesInner);
    mainLayout3->addWidget(axesTitlesList);
    mainLayout3->addLayout(rightLayout);

    generalDialog.addTab(axesPage, tr("Axis"));

    connect(buttonLabelFont, SIGNAL(clicked()), this, SLOT(customAxisLabelFont()));
    connect(axesTitlesList, SIGNAL(currentRowChanged(int)), this, SLOT(onAxesTitlesRowChanged(int)));

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
    connect(boxAxesFontSize, SIGNAL(valueChanged(double)), this,
            SLOT(onAxesFontSizeChanged(double)));
    connect(boxBaseline, SIGNAL(valueChanged(int)), this, SLOT(changeBaselineDist(int)));
    connect(boxAxisType, SIGNAL(activated(int)), this, SLOT(showAxisFormatOptions(int)));
    connect(boxPrecision, SIGNAL(valueChanged(int)), this, SLOT(setLabelsNumericFormat(int)));

    connect(boxColName, &QComboBox::currentTextChanged, this,
            [this](const QString &) { markAxisDirty(); });
    connect(boxTableName, &QComboBox::currentTextChanged, this,
            [this](const QString &) { markAxisDirty(); });
    connect(boxFormat, &QComboBox::currentTextChanged, this,
            [this](const QString &) { markAxisDirty(); });
    connect(boxAngle, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int) { markAxisDirty(); });
    connect(boxFormula, &QTextEdit::textChanged, this, [this]() { markAxisDirty(); });
    connect(boxDivideFactor, &QLineEdit::textChanged, this,
            [this](const QString &) { markAxisDirty(); });
    connect(boxTickPrefix, &QLineEdit::textChanged, this,
            [this](const QString &) { markAxisDirty(); });
    connect(boxTickSuffix, &QLineEdit::textChanged, this,
            [this](const QString &) { markAxisDirty(); });
}

void AxesDialog::initFramePage()
{
    QWidget *frameInner = nullptr;
    frame = createScrollPage(frameInner);

    boxFramed = new QGroupBox(tr("Canvas frame"));
    boxFramed->setCheckable(true);
    // 4辺の枠は軸線(Axis タブの Backbone / Frame Presets)で描くので、キャンバス枠はそれに重ねる
    // 追加の外枠という位置づけ。重複して二重線にならないよう案内する(保存値・読み書き経路は従来どおり)。
    boxFramed->setToolTip(tr("Extra border drawn around the plot area. For a normal 4-sided box "
                             "use the axis lines (Draw backbones / Frame Presets) instead, "
                             "otherwise the frame is drawn twice."));

    QGridLayout *boxFramedLayout = new QGridLayout(boxFramed);
    boxFramedLayout->addWidget(new QLabel(tr("Color")), 0, 0);
    boxFrameColor = new ColorButton(boxFramed);
    boxFramedLayout->addWidget(boxFrameColor, 0, 1);

    boxFramedLayout->addWidget(new QLabel(tr("Width")), 1, 0);
    boxFrameWidth = new QSpinBox();
    boxFrameWidth->setMinimum(1);
    boxFrameWidth->setSuffix(QStringLiteral(" px"));
    boxFramedLayout->addWidget(boxFrameWidth, 1, 1);

    boxFramedLayout->setRowStretch(2, 1);

    QGroupBox *boxAxes = new QGroupBox(tr("Axes"));
    QGridLayout *boxAxesLayout = new QGridLayout(boxAxes);
    boxBackbones = new QCheckBox();
    boxBackbones->setText(tr("Draw backbones"));
    boxAxesLayout->addWidget(boxBackbones, 0, 0);

    boxAxesLayout->addWidget(new QLabel(tr("Line Width")), 1, 0);
    // 軸線幅: PlotStyleLimits::kAxisLineWidth* に上限を合わせる。ただし Plot::axesLinewidth() /
    // Graph::setAxesLinewidth() / QwtScaleWidget::penWidth() / .sciprj の "AxesLineWidth" が
    // いずれも整数のため、このダイアログだけ 0.5 px 刻み(QDoubleSpinBox)にすると丸められて
    // 表示と適用がずれる。Graph/Plot 側が小数対応するまでは整数 px のまま(下限は 0.5 を切り上げた 1)。
    boxAxesLinewidth = new QSpinBox();
    boxAxesLinewidth->setRange(int(std::ceil(PlotStyleLimits::kAxisLineWidthMin)),
                               int(PlotStyleLimits::kAxisLineWidthMax));
    boxAxesLinewidth->setSuffix(QStringLiteral(" px"));
    boxAxesLayout->addWidget(boxAxesLinewidth, 1, 1);

    boxAxesLayout->addWidget(new QLabel(tr("Major ticks length")), 2, 0);
    boxMajorTicksLength = new QSpinBox();
    boxMajorTicksLength->setRange(0, 1000);
    boxMajorTicksLength->setSuffix(QStringLiteral(" px"));
    boxAxesLayout->addWidget(boxMajorTicksLength, 2, 1);

    boxAxesLayout->addWidget(new QLabel(tr("Minor ticks length")), 3, 0);
    boxMinorTicksLength = new QSpinBox();
    boxMinorTicksLength->setRange(0, 1000);
    boxMinorTicksLength->setSuffix(QStringLiteral(" px"));
    boxAxesLayout->addWidget(boxMinorTicksLength, 3, 1);

    boxAxesLayout->setRowStretch(4, 1);

    QGroupBox *boxPresets = new QGroupBox(tr("Frame Presets"));
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

    QHBoxLayout *mainLayout = new QHBoxLayout(frameInner);
    mainLayout->addWidget(boxFramed);
    mainLayout->addWidget(boxAxes);
    mainLayout->addWidget(boxPresets);

    generalDialog.addTab(frame, tr("General"));

    connect(boxFrameColor, SIGNAL(clicked()), this, SLOT(pickCanvasFrameColor()));
    connect(boxFrameColor, &ColorButton::changed, this,
            [this](const QColor &) { markFrameDirty(); });
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
    QWidget *refInner = nullptr;
    refLinesPage = createScrollPage(refInner);

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

    // ゼロ基準線(X=0 / Y=0)はここに一本化。Grid タブのチェックボックスの代わりに1回で追加できる。
    QHBoxLayout *zeroBtns = new QHBoxLayout();
    btnRefAddZeroY = new QPushButton(tr("Add Y=0 Line"));
    btnRefAddZeroX = new QPushButton(tr("Add X=0 Line"));
    zeroBtns->addWidget(btnRefAddZeroY);
    zeroBtns->addWidget(btnRefAddZeroX);
    listLayout->addLayout(zeroBtns);

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
    refWidthBox->setSuffix(QStringLiteral(" px"));
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

    // 塗りの不透明度は 0-100 %(PlotStyleLimits::kOpacity*)。色のアルファ値(0-255)へ変換して使う。
    gl->addWidget(new QLabel(tr("Fill Opacity")), 8, 0);
    refFillOpacityBox = new QSpinBox();
    refFillOpacityBox->setRange(PlotStyleLimits::kOpacityMinPercent,
                                PlotStyleLimits::kOpacityMaxPercent);
    refFillOpacityBox->setSuffix(QStringLiteral(" %"));
    refFillOpacityBox->setValue(qRound(32.0 * 100.0 / 255.0)); // 既定の塗り色 alpha 32
    gl->addWidget(refFillOpacityBox, 8, 1);
    gl->setRowStretch(9, 1);

    QHBoxLayout *mainLayout = new QHBoxLayout(refInner);
    mainLayout->addWidget(listGroup, 1);
    mainLayout->addWidget(propsGroup, 1);

    generalDialog.addTab(refLinesPage, tr("Reference Lines"));

    connect(refLinesList, SIGNAL(currentRowChanged(int)), this, SLOT(selectReferenceLine(int)));
    connect(refStatBox, SIGNAL(currentIndexChanged(int)), this, SLOT(refStatModeChanged(int)));
    connect(btnRefAdd, SIGNAL(clicked()), this, SLOT(addRefLineFromUi()));
    connect(btnRefUpdate, SIGNAL(clicked()), this, SLOT(updateSelectedRefLineFromUi()));
    connect(btnRefRemove, SIGNAL(clicked()), this, SLOT(removeSelectedRefLine()));
    connect(btnRefClear, SIGNAL(clicked()), this, SLOT(clearAllRefLines()));
    connect(btnRefAddZeroY, &QPushButton::clicked, this, [this]() { addZeroRefLine(0); });
    connect(btnRefAddZeroX, &QPushButton::clicked, this, [this]() { addZeroRefLine(1); });

    auto markRefDirty = [this]() {
        if (!d_loadingUi)
            syncSelectedRefLineFromUi();
    };
    connect(refOrientBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [markRefDirty](int) { markRefDirty(); });
    connect(refStatBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [markRefDirty](int) { markRefDirty(); });
    connect(refValueBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, [markRefDirty](double) { markRefDirty(); });
    connect(refColorBtn, &ColorButton::changed, this,
            [markRefDirty](const QColor &) { markRefDirty(); });
    connect(refStyleBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [markRefDirty](int) { markRefDirty(); });
    connect(refWidthBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, [markRefDirty](double) { markRefDirty(); });
    connect(refLabelEdit, &QLineEdit::textChanged, this,
            [markRefDirty](const QString &) { markRefDirty(); });
    connect(refFillBox, &QCheckBox::toggled, this, [markRefDirty](bool) { markRefDirty(); });
    connect(refFillColorBtn, &ColorButton::changed, this, [this, markRefDirty](const QColor &c) {
        if (refFillOpacityBox) {
            const int pct = qBound(PlotStyleLimits::kOpacityMinPercent,
                                   qRound(c.alpha() * 100.0 / 255.0),
                                   PlotStyleLimits::kOpacityMaxPercent);
            if (refFillOpacityBox->value() != pct) {
                QSignalBlocker blocker(refFillOpacityBox);
                refFillOpacityBox->setValue(pct);
            }
        }
        markRefDirty();
    });
    connect(refFillOpacityBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this, markRefDirty](int pct) {
                if (refFillColorBtn) {
                    QColor c = refFillColorBtn->color();
                    const int curPct = qBound(PlotStyleLimits::kOpacityMinPercent,
                                              qRound(c.alpha() * 100.0 / 255.0),
                                              PlotStyleLimits::kOpacityMaxPercent);
                    if (curPct != pct) {
                        c.setAlpha(qBound(0, qRound(pct * 255.0 / 100.0), 255));
                        QSignalBlocker blocker(refFillColorBtn);
                        refFillColorBtn->setColor(c);
                    }
                }
                markRefDirty();
            });
}

void AxesDialog::initAxisBreakPage()
{
    QWidget *breakInner = nullptr;
    axisBreakPage = createScrollPage(breakInner);

    const QIcon scaleIcon = IconLoader::duotoneIcon(QStringLiteral("axis-scale"));

    axisBreakAxesList = new QListWidget();
    axisBreakAxesList->addItem(new QListWidgetItem(scaleIcon, tr("Bottom")));
    axisBreakAxesList->addItem(new QListWidgetItem(scaleIcon, tr("Left")));
    axisBreakAxesList->addItem(new QListWidgetItem(scaleIcon, tr("Top")));
    axisBreakAxesList->addItem(new QListWidgetItem(scaleIcon, tr("Right")));
    axisBreakAxesList->setIconSize(QSize(24, 24));
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

    QHBoxLayout *mainLayout = new QHBoxLayout(breakInner);
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

    connect(boxEnableAxisBreak, &QCheckBox::toggled, this, [this](bool) { markBreakDirty(); });
    connect(boxBreakFrom, &QLineEdit::textChanged, this,
            [this](const QString &) { markBreakDirty(); });
    connect(boxBreakTo, &QLineEdit::textChanged, this,
            [this](const QString &) { markBreakDirty(); });
    connect(boxBreakPositionPct,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            [this](double) { markBreakDirty(); });
    connect(boxBreakStepBefore, &QLineEdit::textChanged, this,
            [this](const QString &) { markBreakDirty(); });
    connect(boxBreakStepAfter, &QLineEdit::textChanged, this,
            [this](const QString &) { markBreakDirty(); });
    connect(boxBreakTypeBefore, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { markBreakDirty(); });
    connect(boxBreakTypeAfter, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { markBreakDirty(); });

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
    boxMajorTicksLength->setMinimum(minLength);
    markFrameDirty();
}

void AxesDialog::changeMajorTicksLength(int majLength)
{
    boxMinorTicksLength->setMaximum(majLength);
    markFrameDirty();
}

void AxesDialog::drawAxesBackbones(bool)
{
    markFrameDirty();
}

void AxesDialog::changeAxesLinewidth(int)
{
    markFrameDirty();
}

void AxesDialog::drawFrame(bool)
{
    markFrameDirty();
}

void AxesDialog::updateFrame(int)
{
    markFrameDirty();
}

void AxesDialog::pickCanvasFrameColor()
{
    QColor c = QColorDialog::getColor(boxFrameColor->color(), this);
    if (!c.isValid() || c == boxFrameColor->color())
        return;

    boxFrameColor->setColor(c);
    markFrameDirty();
}

void AxesDialog::showAxisFormatOptions(int format)
{
    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;
    int axis = mapToQwtAxisId();

    QSignalBlocker blockFormat(boxFormat);
    QSignalBlocker blockCol(boxColName);
    QSignalBlocker blockTbl(boxTableName);
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
        if (d_graph && d_graph->plotWidget())
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
        if (axis >= 0 && axis < formatInfo.size())
            boxFormat->setCurrentIndex(formatInfo[axis].toInt());
    } break;

    case Graph::AxisType::Month: {
        int month = (QDate::currentDate()).month();
        label2->show();
        boxFormat->show();
        boxFormat->addItem(QLocale().monthName(month, QLocale::ShortFormat));
        boxFormat->addItem(QLocale().monthName(month, QLocale::LongFormat));
        boxFormat->addItem((QLocale().monthName(month, QLocale::ShortFormat)).left(1));
        if (axis >= 0 && axis < formatInfo.size())
            boxFormat->setCurrentIndex(formatInfo[axis].toInt());
    } break;

    case Graph::AxisType::Time: {
        label2->show();
        boxFormat->show();
        boxFormat->setEditable(true);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = formatInfo.value(axis).split(";", Qt::KeepEmptyParts);
#else
        QStringList lst = formatInfo.value(axis).split(";", QString::KeepEmptyParts);
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
        QStringList lst = formatInfo.value(axis).split(";", Qt::KeepEmptyParts);
#else
        QStringList lst = formatInfo.value(axis).split(";", QString::KeepEmptyParts);
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
        QStringList lst = formatInfo.value(axis).split(";", Qt::KeepEmptyParts);
#else
        QStringList lst = formatInfo.value(axis).split(";", QString::KeepEmptyParts);
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
        if (axis >= 0 && axis < formatInfo.size() && tablesList.contains(formatInfo[axis])) {
            QSignalBlocker blockTbl(boxTableName);
            const int idx = boxTableName->findText(formatInfo[axis]);
            if (idx >= 0)
                boxTableName->setCurrentIndex(idx);
        }
        boxTableName->show();
    } break;
    }

    d_loadingUi = prevLoading;
    if (!d_loadingUi)
        markAxisDirty();
}

void AxesDialog::insertColList(const QStringList &cols)
{
    QSignalBlocker blockCol(boxColName);
    boxColName->addItems(cols);
}

void AxesDialog::showAxis()
{
    bool ok = boxShowAxis->isChecked();
    boxTitle->setEnabled(ok);
    boxAxisColor->setEnabled(ok);
    boxAxisNumColor->setEnabled(ok);
    btnAxesFont->setEnabled(ok);
    if (boxAxesFontSize)
        boxAxesFontSize->setEnabled(ok);
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

    bool labels = (axis >= 0 && axis < tickLabelsOn.size() && tickLabelsOn[axis] == "1");

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

    bool userFormat = (boxFormat->currentIndex() != 0);
    boxPrecision->setEnabled(labels && ok && userFormat);

    markAxisDirty();
}

void AxesDialog::updateShowBox(int axis)
{
    if (tickLabelsOn.size() < 4)
        return;

    QSignalBlocker b1(boxShowAxis);
    QSignalBlocker b2(boxShowLabels);
    QSignalBlocker b3(boxAngle);
    QSignalBlocker b4(boxAxisBackbone);
    QSignalBlocker b5(boxDivideFactor);
    QSignalBlocker b6(boxTickPrefix);
    QSignalBlocker b7(boxTickSuffix);

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
        boxAxisBackbone->setChecked(d_graph->isAxisBackboneVisible(qwtAxis));
        boxDivideFactor->setText(formatLocaleScalar(d_graph->tickDivideFactor(qwtAxis), 12));
        boxTickPrefix->setText(d_graph->tickPrefix(qwtAxis));
        boxTickSuffix->setText(d_graph->tickSuffix(qwtAxis));
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
    if (boxAxesFontSize)
        boxAxesFontSize->setEnabled(ok);
    boxShowLabels->setEnabled(ok);
    boxMajorTicksType->setEnabled(ok);
    boxMinorTicksType->setEnabled(ok);
    boxAxisType->setEnabled(ok);
    boxBaseline->setEnabled(ok);
    labelBox->setEnabled(ok);
}

void AxesDialog::customAxisFont()
{
    if (!d_graphGuard || !d_graph)
        return;

    bool okF = false;
    const int row = axesTitlesList ? axesTitlesList->currentRow() : -1;
    QFont *targetFont = nullptr;
    switch (row) {
    case 0:
        targetFont = &xBottomFont;
        break;
    case 1:
        targetFont = &yLeftFont;
        break;
    case 2:
        targetFont = &xTopFont;
        break;
    case 3:
        targetFont = &yRightFont;
        break;
    default:
        break;
    }
    if (!targetFont)
        return;

    const QFont beforeFont =
            (row >= 0 && row < 4 && d_stagedAxes[row].axisFontDirty) ? d_stagedAxes[row].axisFont
                                                                     : *targetFont;
    QFont fnt = QFontDialog::getFont(&okF, beforeFont, this);
    if (okF && row >= 0 && row < 4) {
        if (fnt.family().trimmed().isEmpty())
            fnt.setFamily(QStringLiteral("Arial"));
        fnt = PlotStyleLimits::preserveFractionalPtAfterFontDialog(beforeFont, fnt);
        *targetFont = fnt;
        if (boxAxesFontSize) {
            QSignalBlocker sb(boxAxesFontSize);
            boxAxesFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(fnt, 12.0));
        }
        d_stagedAxes[row].axisFont = fnt;
        d_stagedAxes[row].axisFontDirty = true;
        markAxisDirty();
    }
}

void AxesDialog::accept()
{
    // exec() のイベントループを Accepted で終えるため QDialog::accept() を使う
    // (以前の close() は reject 経由になり、exec() の戻り値が常に Rejected になる)。
    if (updatePlot())
        QDialog::accept();
}

void AxesDialog::majorGridEnabled(bool on)
{
    boxTypeMajor->setEnabled(on);
    boxColorMajor->setEnabled(on);
    boxWidthMajor->setEnabled(on);
    markGridDirty();
}

void AxesDialog::minorGridEnabled(bool on)
{
    boxTypeMinor->setEnabled(on);
    boxColorMinor->setEnabled(on);
    boxWidthMinor->setEnabled(on);
    markGridDirty();
}

void AxesDialog::updateGrid()
{
    if (!d_graphGuard || !d_graph)
        return;

    if (axesGridList && axesGridList->currentRow() >= 0 && axesGridList->currentRow() < 2) {
        captureGridRowFromUi(axesGridList->currentRow());
        d_stagedGrids[axesGridList->currentRow()].dirty = true;
    }
    d_gridSharedDirty = true;
    d_stagedGridXAxisIdx = boxGridXAxis->currentIndex();
    d_stagedGridYAxisIdx = boxGridYAxis->currentIndex();
    d_stagedGridXZero = boxXLine->isChecked();
    d_stagedGridYZero = boxYLine->isChecked();

    commitGridState();
    d_graph->replot();
    d_graph->notifyChanges();
}

void AxesDialog::showGridOptions(int axis)
{
    if (!d_graphGuard || !d_graph || !d_graph->plotWidget())
        return;
    Grid *grd = (Grid *)d_graph->plotWidget()->grid();
    if (!grd)
        return;

    if (!d_loadingUi && d_prevGridRow >= 0 && d_prevGridRow < 2 && d_prevGridRow != axis
        && d_stagedGrids[d_prevGridRow].dirty) {
        captureGridRowFromUi(d_prevGridRow);
    }
    if (axis >= 0 && axis < 2)
        d_prevGridRow = axis;

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;

    QSignalBlocker bMaj(boxMajorGrid);
    QSignalBlocker bMin(boxMinorGrid);
    QSignalBlocker bTypeMaj(boxTypeMajor);
    QSignalBlocker bTypeMin(boxTypeMinor);
    QSignalBlocker bColMaj(boxColorMajor);
    QSignalBlocker bColMin(boxColorMinor);
    QSignalBlocker bWidMaj(boxWidthMajor);
    QSignalBlocker bWidMin(boxWidthMinor);
    QSignalBlocker bGridX(boxGridXAxis);
    QSignalBlocker bGridY(boxGridYAxis);
    QSignalBlocker bXLine(boxXLine);
    QSignalBlocker bYLine(boxYLine);

    if (axis == 1) {
        boxXLine->setEnabled(true);
        boxYLine->setDisabled(true);
        boxGridXAxis->setEnabled(true);
        boxGridYAxis->setDisabled(true);

        if (d_stagedGrids[1].dirty) {
            const StagedGridEdit &st = d_stagedGrids[1];
            boxMajorGrid->setChecked(st.majorOn);
            boxMinorGrid->setChecked(st.minorOn);
            boxTypeMajor->setCurrentIndex(st.typeMajor);
            boxColorMajor->setColor(st.colorMajor);
            boxWidthMajor->setValue(st.widthMajor);
            boxTypeMinor->setCurrentIndex(st.typeMinor);
            boxColorMinor->setColor(st.colorMinor);
            boxWidthMinor->setValue(st.widthMinor);
        } else {
            boxMajorGrid->setChecked(grd->xEnabled());
            boxMinorGrid->setChecked(grd->xMinEnabled());
            QPen majPenX = grd->majPenX();
            boxTypeMajor->setCurrentIndex(majPenX.style() - 1);
            boxColorMajor->setColor(majPenX.color());
            boxWidthMajor->setValue(majPenX.width());
            QPen minPenX = grd->minPenX();
            boxTypeMinor->setCurrentIndex(minPenX.style() - 1);
            boxColorMinor->setColor(minPenX.color());
            boxWidthMinor->setValue(minPenX.width());
        }
    } else if (axis == 0) {
        boxXLine->setDisabled(true);
        boxYLine->setEnabled(true);
        boxGridXAxis->setDisabled(true);
        boxGridYAxis->setEnabled(true);

        if (d_stagedGrids[0].dirty) {
            const StagedGridEdit &st = d_stagedGrids[0];
            boxMajorGrid->setChecked(st.majorOn);
            boxMinorGrid->setChecked(st.minorOn);
            boxTypeMajor->setCurrentIndex(st.typeMajor);
            boxColorMajor->setColor(st.colorMajor);
            boxWidthMajor->setValue(st.widthMajor);
            boxTypeMinor->setCurrentIndex(st.typeMinor);
            boxColorMinor->setColor(st.colorMinor);
            boxWidthMinor->setValue(st.widthMinor);
        } else {
            boxMajorGrid->setChecked(grd->yEnabled());
            boxMinorGrid->setChecked(grd->yMinEnabled());
            QPen majPenY = grd->majPenY();
            boxTypeMajor->setCurrentIndex(majPenY.style() - 1);
            boxColorMajor->setColor(majPenY.color());
            boxWidthMajor->setValue(majPenY.width());
            QPen minPenY = grd->minPenY();
            boxTypeMinor->setCurrentIndex(minPenY.style() - 1);
            boxColorMinor->setColor(minPenY.color());
            boxWidthMinor->setValue(minPenY.width());
        }
    }

    bool majorOn = boxMajorGrid->isChecked();
    boxTypeMajor->setEnabled(majorOn);
    boxColorMajor->setEnabled(majorOn);
    boxWidthMajor->setEnabled(majorOn);

    bool minorOn = boxMinorGrid->isChecked();
    boxTypeMinor->setEnabled(minorOn);
    boxColorMinor->setEnabled(minorOn);
    boxWidthMinor->setEnabled(minorOn);

    if (d_gridSharedDirty) {
        boxGridXAxis->setCurrentIndex(d_stagedGridXAxisIdx);
        boxGridYAxis->setCurrentIndex(d_stagedGridYAxisIdx);
        boxXLine->setChecked(d_stagedGridXZero);
        boxYLine->setChecked(d_stagedGridYZero);
    } else {
        boxGridXAxis->setCurrentIndex(grd->xAxis() - 2);
        boxGridYAxis->setCurrentIndex(grd->yAxis());
        boxXLine->setChecked(grd->xZeroLineEnabled());
        boxYLine->setChecked(grd->yZeroLineEnabled());
        d_stagedGridXAxisIdx = boxGridXAxis->currentIndex();
        d_stagedGridYAxisIdx = boxGridYAxis->currentIndex();
        d_stagedGridXZero = boxXLine->isChecked();
        d_stagedGridYZero = boxYLine->isChecked();
    }

    updateLegacyZeroLineVisibility();
    d_loadingUi = prevLoading;
}

void AxesDialog::updateLegacyZeroLineVisibility()
{
    if (!labelGridZeroLines || !boxXLine || !boxYLine)
        return;
    // ゼロ線が有効なときだけ旧チェックボックスを見せる(無効なら Reference Lines タブを使う)。
    const bool legacyOn = d_stagedGridXZero || d_stagedGridYZero || boxXLine->isChecked()
            || boxYLine->isChecked();
    labelGridZeroLines->setVisible(legacyOn);
    boxXLine->setVisible(legacyOn);
    boxYLine->setVisible(legacyOn);
}

void AxesDialog::stepEnabled()
{
    boxStep->setEnabled(btnStep->isChecked());
    boxUnit->setEnabled(btnStep->isChecked());
    boxMajorValue->setDisabled(btnStep->isChecked());
    btnMajor->setChecked(!btnStep->isChecked());
    markScaleDirty();
}

void AxesDialog::stepDisabled()
{
    boxStep->setDisabled(btnMajor->isChecked());
    boxUnit->setDisabled(btnMajor->isChecked());
    boxMajorValue->setEnabled(btnMajor->isChecked());
    btnStep->setChecked(!btnMajor->isChecked());
    markScaleDirty();
}

void AxesDialog::updateAxisColor(int)
{
    if (!d_graph)
        return;
    int a = mapToQwtAxisId();
    QSignalBlocker b1(boxAxisColor);
    QSignalBlocker b2(boxAxisNumColor);
    boxAxisColor->setColor(d_graph->axisColor(a));
    boxAxisNumColor->setColor(d_graph->axisNumbersColor(a));
}

void AxesDialog::changeBaselineDist(int baseline)
{
    int axis = mapToQwtAxisId();
    if (axis >= 0 && axis < axesBaseline.size())
        axesBaseline[axis] = baseline;
    markAxisDirty();
}

bool AxesDialog::updatePlot()
{
    if (!d_graphGuard || !d_graph)
        return false;
    QPointer<AxesDialog> self(this);

    if (d_scalesTabDirty && axesList && axesList->currentRow() >= 0 && axesList->currentRow() < 4
        && d_stagedScales[axesList->currentRow()].dirty) {
        captureScaleRowFromUi(axesList->currentRow());
    }
    if (d_gridTabDirty && axesGridList && axesGridList->currentRow() >= 0
        && axesGridList->currentRow() < 2 && d_stagedGrids[axesGridList->currentRow()].dirty) {
        captureGridRowFromUi(axesGridList->currentRow());
    }
    if (d_axesTabDirty && axesTitlesList && axesTitlesList->currentRow() >= 0
        && axesTitlesList->currentRow() < 4 && d_stagedAxes[axesTitlesList->currentRow()].dirty) {
        captureAxisRowFromUi(axesTitlesList->currentRow());
    }
    if (d_axisBreakTabDirty && axisBreakAxesList && axisBreakAxesList->currentRow() >= 0
        && axisBreakAxesList->currentRow() < 4
        && d_stagedBreaks[axisBreakAxesList->currentRow()].dirty) {
        captureBreakRowFromUi(axisBreakAxesList->currentRow());
    }

    const bool anyDirty = d_scalesTabDirty || d_gridTabDirty || d_axesTabDirty || d_frameTabDirty
            || d_refLinesTabDirty || d_axisBreakTabDirty;

    bool commitScales = d_scalesTabDirty;
    bool commitGrid = d_gridTabDirty;
    bool commitAxes = d_axesTabDirty;
    bool commitFrame = d_frameTabDirty;
    bool commitRefLines = d_refLinesTabDirty;
    bool commitBreak = d_axisBreakTabDirty;

    if (!anyDirty) {
        QWidget *cur = generalDialog.currentWidget();
        if (cur == scalesPage && axesList) {
            int r = qBound(0, axesList->currentRow(), 3);
            captureScaleRowFromUi(r);
            d_stagedScales[r].dirty = true;
            commitScales = true;
        } else if (cur == gridPage && axesGridList) {
            int r = qBound(0, axesGridList->currentRow(), 1);
            captureGridRowFromUi(r);
            d_stagedGrids[r].dirty = true;
            commitGrid = true;
        } else if (cur == axesPage && axesTitlesList) {
            int r = qBound(0, axesTitlesList->currentRow(), 3);
            captureAxisRowFromUi(r);
            d_stagedAxes[r].dirty = true;
            commitAxes = true;
        } else if (cur == frame) {
            commitFrame = true;
        } else if (cur == refLinesPage) {
            commitRefLines = true;
        } else if (cur == axisBreakPage && axisBreakAxesList) {
            int r = qBound(0, axisBreakAxesList->currentRow(), 3);
            captureBreakRowFromUi(r);
            d_stagedBreaks[r].dirty = true;
            commitBreak = true;
        }
    }

    // Pre-validate all dirty Scale, Axis Formula, and Axis Break inputs before mutating d_graph
    if (commitScales) {
        auto findSmallestPositiveOnAxis = [this](int qwtAxis, double fallbackEnd) -> double {
            const bool xAxis = (qwtAxis == QwtPlot::xBottom || qwtAxis == QwtPlot::xTop);
            double minPos = std::numeric_limits<double>::infinity();
            for (int ci = 0; ci < d_graph->curves(); ++ci) {
                QwtPlotCurve *c = d_graph->curve(ci);
                if (!c)
                    continue;
                if (xAxis ? (c->xAxis() != qwtAxis) : (c->yAxis() != qwtAxis))
                    continue;
                for (int i = 0; i < c->dataSize(); ++i) {
                    const double v = xAxis ? c->x(i) : c->y(i);
                    if (std::isfinite(v) && v > 0.0 && v < minPos)
                        minPos = v;
                }
            }
            if (!std::isfinite(minPos)) {
                for (int ci = 0; ci < d_graph->curves(); ++ci) {
                    QwtPlotCurve *c = d_graph->curve(ci);
                    if (!c)
                        continue;
                    for (int i = 0; i < c->dataSize(); ++i) {
                        const double v = xAxis ? c->x(i) : c->y(i);
                        if (std::isfinite(v) && v > 0.0 && v < minPos)
                            minPos = v;
                    }
                }
            }
            if (std::isfinite(minPos) && minPos > 0.0 && minPos < fallbackEnd)
                return minPos;
            return std::min(1e-3, 0.01 * fallbackEnd);
        };

        for (int r = 0; r < 4; ++r) {
            if (!d_stagedScales[r].dirty)
                continue;
            const StagedScaleEdit &st = d_stagedScales[r];
            QString from = st.start.toLower();
            QString to = st.end.toLower();
            QString step = st.step.toLower();
            const int a = Graph::mapToQwtAxis(r);
            const Graph::AxisType aType = d_stagedAxes[r].dirty ? d_stagedAxes[r].axisType
                                                                : axesType.value(a, Graph::AxisType::Numeric);
            if (aType == Graph::AxisType::Time || aType == Graph::AxisType::Date
                || aType == Graph::AxisType::DateTime) {
                QDateTime qdt = QDateTime::fromString(st.start.trimmed(), "yyyy-MM-dd hh:mm:ss");
                if (!qdt.isValid()) {
                    generalDialog.setCurrentWidget(scalesPage);
                    axesList->setCurrentRow(r);
                    QMessageBox::critical(
                            this, tr("Start limit error"),
                            tr("Please enter a valid date/time in yyyy-MM-dd hh:mm:ss format."));
                    if (!self)
                        return false;
                    boxStart->setFocus();
                    return false;
                }
                from = QString::number(qdt.toMSecsSinceEpoch() / 86400000. + 2440587.5, 'f', 16);
                qdt = QDateTime::fromString(st.end.trimmed(), "yyyy-MM-dd hh:mm:ss");
                if (!qdt.isValid()) {
                    generalDialog.setCurrentWidget(scalesPage);
                    axesList->setCurrentRow(r);
                    QMessageBox::critical(
                            this, tr("End limit error"),
                            tr("Please enter a valid date/time in yyyy-MM-dd hh:mm:ss format."));
                    if (!self)
                        return false;
                    boxEnd->setFocus();
                    return false;
                }
                to = QString::number(qdt.toMSecsSinceEpoch() / 86400000. + 2440587.5, 'f', 16);
            }
            double startVal = 0.0, endVal = 0.0;
            QString exprErr;
            if (!evalAxisScalarExpr(from, &startVal, &exprErr)) {
                generalDialog.setCurrentWidget(scalesPage);
                axesList->setCurrentRow(r);
                QMessageBox::critical(this, tr("Start limit error"), exprErr);
                if (!self)
                    return false;
                boxStart->setFocus();
                return false;
            }
            if (!evalAxisScalarExpr(to, &endVal, &exprErr)) {
                generalDialog.setCurrentWidget(scalesPage);
                axesList->setCurrentRow(r);
                QMessageBox::critical(this, tr("End limit error"), exprErr);
                if (!self)
                    return false;
                boxEnd->setFocus();
                return false;
            }
            if (startVal >= endVal) {
                generalDialog.setCurrentWidget(scalesPage);
                axesList->setCurrentRow(r);
                QMessageBox::critical(this, tr("Input error"),
                                      tr("Please enter limits that satisfy: Start < End!"));
                if (!self)
                    return false;
                boxEnd->setFocus();
                return false;
            }
            if (st.scaleType == 1) {
                if (endVal <= 0.0) {
                    generalDialog.setCurrentWidget(scalesPage);
                    axesList->setCurrentRow(r);
                    QMessageBox::critical(
                            this, tr("Scale input error"),
                            tr("Logarithmic scale requires positive limits (> 0)."));
                    if (!self)
                        return false;
                    boxEnd->setFocus();
                    return false;
                }
                if (startVal <= 0.0) {
                    const double clampedStart = findSmallestPositiveOnAxis(a, endVal);
                    bool acceptClamp = ApplicationWindow::isNonInteractiveContext(this);
                    if (!acceptClamp) {
                        acceptClamp = (QMessageBox::question(
                                               this, tr("Logarithmic Scale"),
                                               tr("Logarithmic scale requires a positive Start limit (> 0).\n"
                                                  "Clamp Start from %1 to the smallest positive value (%2)?")
                                                       .arg(QLocale().toString(startVal, 'g', 8),
                                                            QLocale().toString(clampedStart, 'g', 8)),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                                       == QMessageBox::Yes);
                    }
                    if (!self || !d_graphGuard || !d_graph)
                        return false;
                    if (!acceptClamp) {
                        generalDialog.setCurrentWidget(scalesPage);
                        axesList->setCurrentRow(r);
                        boxStart->setFocus();
                        return false;
                    }
                    const QString clampedStr = formatLocaleScalar(clampedStart, 12);
                    d_stagedScales[r].start = clampedStr;
                    if (axesList && axesList->currentRow() == r && boxStart) {
                        QSignalBlocker bStart(boxStart);
                        boxStart->setText(clampedStr);
                    }
                }
            }
            if (st.useStep) {
                double stp = 0.0;
                QString stepErr;
                if (!evalAxisScalarExpr(step, &stp, &stepErr)) {
                    generalDialog.setCurrentWidget(scalesPage);
                    axesList->setCurrentRow(r);
                    QMessageBox::critical(this, tr("Step input error"), stepErr);
                    if (!self)
                        return false;
                    boxStep->setFocus();
                    return false;
                }
                if (!std::isfinite(stp) || stp <= 0) {
                    generalDialog.setCurrentWidget(scalesPage);
                    axesList->setCurrentRow(r);
                    QMessageBox::critical(this, tr("Step input error"),
                                          tr("Please enter a positive step value!"));
                    if (!self)
                        return false;
                    boxStep->setFocus();
                    return false;
                }
            }
        }
    }

    if (commitAxes) {
        for (int r = 0; r < 4; ++r) {
            if (!d_stagedAxes[r].dirty)
                continue;
            const StagedAxisEdit &st = d_stagedAxes[r];
            const QString factorTxt = st.divideFactor.trimmed();
            if (!factorTxt.isEmpty()) {
                double v = 0.0;
                const bool ok = evalAxisScalarExpr(factorTxt, &v);
                if (!ok || !std::isfinite(v) || std::fabs(v) <= 1e-300) {
                    generalDialog.setCurrentWidget(axesPage);
                    if (axesTitlesList)
                        axesTitlesList->setCurrentRow(r);
                    QMessageBox::critical(
                            this, tr("Divide factor error"),
                            tr("Please enter a valid non-zero numeric value or expression for Divide by Factor."));
                    if (!self)
                        return false;
                    if (boxDivideFactor)
                        boxDivideFactor->setFocus();
                    return false;
                }
            }
            if (st.axisType == Graph::AxisType::Numeric && st.showFormula) {
                QString formula = st.formula.toLower();
                const QString normalizedFormula = normalizeExprDecimalCommas(formula);
                auto tryEvalTickFormula = [&](const QString &expr) {
                    double value = 1.0;
                    MyParser parser;
                    if (expr.contains(QLatin1Char('x')))
                        parser.DefineVar(_T("x"), &value);
                    else if (expr.contains(QLatin1Char('y')))
                        parser.DefineVar(_T("y"), &value);
                    parser.SetExpr(expr);
                    parser.Eval();
                };
                try {
                    try {
                        tryEvalTickFormula(normalizedFormula);
                    } catch (mu::ParserError &) {
                        if (normalizedFormula != formula)
                            tryEvalTickFormula(formula);
                        else
                            throw;
                    }
                } catch (mu::ParserError &e) {
                    generalDialog.setCurrentWidget(axesPage);
                    axesTitlesList->setCurrentRow(r);
                    QMessageBox::critical(this, tr("Formula input error"),
                                          QStringFromString(e.GetMsg()) + "\n"
                                                  + tr("Valid variables are 'x' for Top/Bottom "
                                                       "axes and 'y' for Left/Right axes!"));
                    if (!self)
                        return false;
                    boxFormula->setFocus();
                    return false;
                }
            }
        }
    }

    if (commitBreak) {
        for (int r = 0; r < 4; ++r) {
            if (!d_stagedBreaks[r].dirty || !d_stagedBreaks[r].enabled)
                continue;
            const StagedBreakEdit &st = d_stagedBreaks[r];
            bool okFrom = false, okTo = false, okStepB = false, okStepA = false;
            const double bf = evalAxisScalarOrEmptyFallback(st.breakFrom, 0.0, &okFrom);
            const double bt = evalAxisScalarOrEmptyFallback(st.breakTo, 0.0, &okTo);
            evalAxisScalarOrEmptyFallback(st.stepBefore, 0.0, &okStepB);
            evalAxisScalarOrEmptyFallback(st.stepAfter, 0.0, &okStepA);
            if (!okFrom || !okTo || !(bf < bt) || !okStepB || !okStepA) {
                generalDialog.setCurrentWidget(axisBreakPage);
                if (axisBreakAxesList)
                    axisBreakAxesList->setCurrentRow(r);
                QMessageBox::critical(this, tr("Error"),
                                      tr("Break From:") + " < " + tr("Break To:"));
                if (!self)
                    return false;
                boxBreakFrom->setFocus();
                return false;
            }
        }
    }

    if (!self || !d_graphGuard || !d_graph)
        return false;

    MultiLayer *ml = d_graph->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    const int committedFramePreset = d_stagedFramePreset;
    if (committedFramePreset >= 0)
        d_graph->applyFramePreset(static_cast<Graph::FramePreset>(committedFramePreset));

    if (commitFrame)
        commitFrameState();

    if (commitAxes) {
        if (d_unitFormatDirty) {
            d_graph->setAxisUnitFormat(unitFormatFromIndex(d_stagedUnitFormatIdx));
        }
        for (int r = 0; r < 4; ++r) {
            if (d_stagedAxes[r].dirty)
                commitAxisRow(r);
        }
        if (d_unitFormatDirty)
            d_graph->refreshAutoAxisTitlesFromColumns();
    }

    if (commitScales) {
        for (int r = 0; r < 4; ++r) {
            if (d_stagedScales[r].dirty)
                commitScaleRow(r);
        }
    }

    if (committedFramePreset == Graph::FrameBoxInwardTicks
        || committedFramePreset == Graph::FrameBoxPlainOpposite) {
        if (!d_stagedScales[2].dirty)
            d_graph->updateSecondaryAxis(QwtPlot::xTop);
        if (!d_stagedScales[3].dirty)
            d_graph->updateSecondaryAxis(QwtPlot::yRight);
    }

    if (commitGrid)
        commitGridState();

    if (commitRefLines) {
        if (d_stagedRefLines.isEmpty() && d_refControlsEdited) {
            d_stagedRefLines.append(buildRefLineSpecFromUi());
        } else if (refLinesList && refLinesList->currentRow() >= 0
                   && refLinesList->currentRow() < d_stagedRefLines.size()
                   && d_refControlsEdited) {
            const int rIdx = refLinesList->currentRow();
            const bool keepVis = d_stagedRefLines[rIdx].visible;
            d_stagedRefLines[rIdx] = buildRefLineSpecFromUi();
            d_stagedRefLines[rIdx].visible = keepVis;
        }
        d_graph->setReferenceLines(d_stagedRefLines);
        d_graph->updateStatReferenceLines();
        d_stagedRefLines = d_graph->referenceLines();
        refreshReferenceLinesList();
    }

    if (commitBreak) {
        for (int r = 0; r < 4; ++r) {
            if (d_stagedBreaks[r].dirty)
                commitBreakRow(r);
        }
    }

    clearAllDirtyState();
    d_graph->replot();
    d_graph->notifyChanges();

    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Axes Settings"), beforeSnap);

    return true;
}

void AxesDialog::setGraph(Graph *g)
{
    if (!g)
        return;

    d_graph = g;
    d_graphGuard = g;
    clearAllDirtyState();
    d_stagedRefLines = d_graph->referenceLines();

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    if (app) {
        tablesList = app->tableWindows();
        QSignalBlocker bTbl(boxTableName);
        QSignalBlocker bCol(boxColName);
        boxTableName->clear();
        boxTableName->addItems(tablesList);
        boxColName->clear();
        boxColName->addItems(app->columnsList());
    }
    d_loadingUi = prevLoading;

    refreshGraphState();
    refreshReferenceLinesList();
    clearAllDirtyState();
}

void AxesDialog::refreshGraphState()
{
    if (!d_graphGuard || !d_graph)
        return;

    Plot *p = d_graph->plotWidget();
    if (!p)
        return;

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;
    if (!d_refLinesTabDirty)
        d_stagedRefLines = d_graph->referenceLines();

    xAxisOn = p->axisEnabled(QwtPlot::xBottom);
    yAxisOn = p->axisEnabled(QwtPlot::yLeft);
    topAxisOn = p->axisEnabled(QwtPlot::xTop);
    rightAxisOn = p->axisEnabled(QwtPlot::yRight);

    xBottomFont = p->axisFont(QwtPlot::xBottom);
    if (xBottomFont.family().trimmed().isEmpty())
        xBottomFont.setFamily("Arial");
    yLeftFont = p->axisFont(QwtPlot::yLeft);
    if (yLeftFont.family().trimmed().isEmpty())
        yLeftFont.setFamily("Arial");
    xTopFont = p->axisFont(QwtPlot::xTop);
    if (xTopFont.family().trimmed().isEmpty())
        xTopFont.setFamily("Arial");
    yRightFont = p->axisFont(QwtPlot::yRight);
    if (yRightFont.family().trimmed().isEmpty())
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

    {
        QSignalBlocker b1(boxAxesLinewidth);
        QSignalBlocker b2(boxBackbones);
        QSignalBlocker b3(boxFramed);
        QSignalBlocker b4(boxFrameColor);
        QSignalBlocker b5(boxFrameWidth);
        QSignalBlocker b6(boxMinorTicksLength);
        QSignalBlocker b7(boxMajorTicksLength);

        // 共通上限(20 px)を超える既存値(旧ファイル由来)を黙って切り詰めないよう、上限だけ広げる。
        boxAxesLinewidth->setMaximum(qMax(int(PlotStyleLimits::kAxisLineWidthMax), p->axesLinewidth()));
        boxAxesLinewidth->setValue(p->axesLinewidth());
        boxBackbones->setChecked(d_graph->axesBackbones());

        boxFramed->setChecked(d_graph->framed());
        boxFrameColor->setColor(d_graph->canvasFrameColor());
        boxFrameWidth->setValue(d_graph->canvasFrameWidth());

        boxMinorTicksLength->setValue(p->minorTickLength());
        boxMajorTicksLength->setValue(p->majorTickLength());
    }

    if (axesTitlesList) {
        if (curAxisRow < 0 && axesTitlesList->count() > 0) {
            QSignalBlocker sbTitList(axesTitlesList);
            axesTitlesList->setCurrentRow(0);
            curAxisRow = 0;
        }
    }
    if (curAxisRow >= 0) {
        onAxesTitlesRowChanged(curAxisRow);
    } else if (boxAxesFontSize) {
        QSignalBlocker sb(boxAxesFontSize);
        boxAxesFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(xBottomFont, 12.0));
    }
    if (axesList) {
        if (axesList->currentRow() < 0 && axesList->count() > 0) {
            QSignalBlocker sbList(axesList);
            axesList->setCurrentRow(0);
        }
        if (axesList->currentRow() >= 0)
            updateScale();
    }
    showGridOptions(axesGridList->currentRow());
    if (axisBreakAxesList)
        updateAxisBreakPage(axisBreakAxesList->currentRow() >= 0 ? axisBreakAxesList->currentRow()
                                                                 : 0);

    d_loadingUi = prevLoading;
}

int AxesDialog::mapToQwtAxisId()
{
    int row = axesTitlesList ? axesTitlesList->currentRow() : 0;
    if (row < 0)
        row = 0;
    return Graph::mapToQwtAxis(row);
}

void AxesDialog::updateScale()
{
    if (!d_graphGuard || !d_graph || !d_graph->plotWidget())
        return;

    int axis = axesList ? axesList->currentRow() : 0;
    if (axis < 0 || axis >= 4)
        return;

    if (!d_loadingUi && d_prevScaleRow >= 0 && d_prevScaleRow < 4 && d_prevScaleRow != axis
        && d_stagedScales[d_prevScaleRow].dirty) {
        captureScaleRowFromUi(d_prevScaleRow);
    }
    d_prevScaleRow = axis;

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;

    QSignalBlocker bStart(boxStart);
    QSignalBlocker bEnd(boxEnd);
    QSignalBlocker bStep(boxStep);
    QSignalBlocker bUnit(boxUnit);
    QSignalBlocker bBtnStep(btnStep);
    QSignalBlocker bBtnMajor(btnMajor);
    QSignalBlocker bMajVal(boxMajorValue);
    QSignalBlocker bMinVal(boxMinorValue);
    QSignalBlocker bScType(boxScaleType);
    QSignalBlocker bInvert(btnInvert);

    boxStart->clear();
    boxEnd->clear();
    boxStep->clear();
    boxUnit->hide();
    boxUnit->clear();

    Plot *d_plot = d_graph->plotWidget();
    int a = Graph::mapToQwtAxis(axis);
    const Graph::AxisType aType = d_stagedAxes[axis].dirty
            ? d_stagedAxes[axis].axisType
            : axesType.value(a, Graph::AxisType::Numeric);

    if (aType == Graph::AxisType::Time) {
        boxUnit->show();
        boxUnit->addItem(tr("millisec."));
        boxUnit->addItem(tr("sec."));
        boxUnit->addItem(tr("min."));
        boxUnit->addItem(tr("hours"));
    } else if (aType == Graph::AxisType::Date) {
        boxUnit->show();
        boxUnit->addItem(tr("days"));
        boxUnit->addItem(tr("weeks"));
    } else if (aType == Graph::AxisType::DateTime) {
        boxUnit->show();
        boxUnit->addItem(tr("millisec."));
        boxUnit->addItem(tr("sec."));
        boxUnit->addItem(tr("min."));
        boxUnit->addItem(tr("hours"));
        boxUnit->addItem(tr("days"));
        boxUnit->addItem(tr("weeks"));
        boxUnit->setCurrentIndex(4); // days
    }

    if (d_stagedScales[axis].dirty) {
        const StagedScaleEdit &st = d_stagedScales[axis];
        boxStart->setText(st.start);
        boxEnd->setText(st.end);
        boxStep->setText(st.step);
        if (st.unitIndex >= 0 && st.unitIndex < boxUnit->count())
            boxUnit->setCurrentIndex(st.unitIndex);
        btnStep->setChecked(st.useStep);
        btnMajor->setChecked(!st.useStep);
        boxStep->setEnabled(st.useStep);
        boxUnit->setEnabled(st.useStep);
        boxMajorValue->setEnabled(!st.useStep);
        boxMajorValue->setValue(st.majorValue);
        boxScaleType->setCurrentIndex(st.scaleType);
        btnInvert->setChecked(st.inverted);
        boxMinorValue->clear();
        boxMinorValue->addItems(minorTickItemsForScale(st.scaleType == 1));
        selectOrSetMinorTickCombo(boxMinorValue, parseLeadingMinorInt(st.minorText, 0));
        d_loadingUi = prevLoading;
        return;
    }

    const QwtScaleDiv *scDiv = d_plot->axisScaleDiv(a);
    double astart = std::min(scDiv->lowerBound(), scDiv->upperBound());
    double aend = std::max(scDiv->lowerBound(), scDiv->upperBound());
    double astep = d_graph->axisStep(a);

    switch (aType) {
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
        boxStart->setText(formatLocaleScalar(astart, 12));
        boxEnd->setText(formatLocaleScalar(aend, 12));
    }
    }

    QwtValueList lst = scDiv->ticks(QwtScaleDiv::MajorTick);
    boxStep->setText(formatLocaleScalar(d_graph->axisStep(a), 12));
    boxMajorValue->setValue(lst.count());

    if (d_graph->axisStep(a) != 0.0) {
        btnStep->setChecked(true);
        boxStep->setEnabled(true);
        boxUnit->setEnabled(true);
        if (aType == Graph::AxisType::DateTime) {
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
            boxStep->setText(formatLocaleScalar(astep, 12));
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
    boxMinorValue->addItems(minorTickItemsForScale(isLog10));
    selectOrSetMinorTickCombo(boxMinorValue, d_plot->axisMaxMinor(a));
    d_loadingUi = prevLoading;
}

void AxesDialog::updateTitleBox(int axis)
{
    if (!d_graph)
        return;
    int axisId = Graph::mapToQwtAxis(axis);
    QSignalBlocker b1(boxTitle);
    QSignalBlocker b2(boxAutoAxisTitle);
    QSignalBlocker b3(boxAxisUnitFormat);
    const int fmtIdx = d_unitFormatDirty ? d_stagedUnitFormatIdx
                                         : unitFormatToIndex(d_graph->axisUnitFormat());
    const Column::UnitFormat effFmt = unitFormatFromIndex(fmtIdx);
    const bool isAuto = (axis >= 0 && axis < 4 && d_stagedAxes[axis].dirty)
            ? d_stagedAxes[axis].autoTitle
            : d_graph->isAxisTitleAuto(axisId);
    QString shownTitle = (axis >= 0 && axis < 4 && d_stagedAxes[axis].dirty)
            ? d_stagedAxes[axis].title
            : d_graph->axisTitle(axisId);
    if (isAuto && d_unitFormatDirty) {
        const QString preview = previewAutoAxisTitle(axisId, effFmt);
        if (!preview.isEmpty())
            shownTitle = preview;
    }
    boxTitle->setText(shownTitle);
    boxAutoAxisTitle->setChecked(isAuto);
    boxAxisUnitFormat->setCurrentIndex(fmtIdx);
    if (boxTitleFontSize) {
        QSignalBlocker b4(boxTitleFontSize);
        QFont tf = (axis >= 0 && axis < 4 && d_stagedAxes[axis].titleFontDirty)
                ? d_stagedAxes[axis].titleFont
                : d_graph->axisTitleFont(axisId);
        boxTitleFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(tf, 12.0));
    }
}

void AxesDialog::onTitleTextEdited()
{
    if (!d_graphGuard || !d_graph || d_loadingUi)
        return;
    const int row = axesTitlesList ? axesTitlesList->currentRow() : 0;
    const int axisId = mapToQwtAxisId();
    const QString baseTitle = (row >= 0 && row < 4 && d_stagedAxes[row].dirty)
            ? d_stagedAxes[row].title
            : d_graph->axisTitle(axisId);
    if (boxTitle->toPlainText() != baseTitle && boxAutoAxisTitle->isChecked()) {
        QSignalBlocker b(boxAutoAxisTitle);
        boxAutoAxisTitle->setChecked(false);
    }
    markAxisDirty();
}

void AxesDialog::onAutoAxisTitleToggled(bool checked)
{
    if (!d_graphGuard || !d_graph || d_loadingUi)
        return;
    markAxisDirty();
    if (checked) {
        const int axisId = mapToQwtAxisId();
        const int fmtIdx = d_unitFormatDirty
                ? d_stagedUnitFormatIdx
                : (boxAxisUnitFormat ? boxAxisUnitFormat->currentIndex()
                                     : unitFormatToIndex(d_graph->axisUnitFormat()));
        const Column::UnitFormat fmt = unitFormatFromIndex(fmtIdx);
        QString autoTxt = previewAutoAxisTitle(axisId, fmt);
        if (autoTxt.isEmpty())
            autoTxt = d_graph->axisTitle(axisId);
        QSignalBlocker b(boxTitle);
        boxTitle->setText(autoTxt);
        const int row = axesTitlesList ? axesTitlesList->currentRow() : -1;
        if (row >= 0 && row < 4) {
            d_stagedAxes[row].autoTitle = true;
            d_stagedAxes[row].title = autoTxt;
        }
    }
}

void AxesDialog::onAxisUnitFormatChanged(int index)
{
    if (!d_graphGuard || !d_graph || d_loadingUi)
        return;
    d_unitFormatDirty = true;
    d_stagedUnitFormatIdx = index;
    d_axesTabDirty = true;
    const Column::UnitFormat fmt = unitFormatFromIndex(index);

    const int curRow = axesTitlesList ? axesTitlesList->currentRow() : 0;
    if (curRow >= 0 && curRow < 4) {
        if (boxAutoAxisTitle->isChecked()) {
            const int axisId = Graph::mapToQwtAxis(curRow);
            const QString previewTitle = previewAutoAxisTitle(axisId, fmt);
            if (!previewTitle.isEmpty()) {
                QSignalBlocker b(boxTitle);
                boxTitle->setText(previewTitle);
            }
        }
        captureAxisRowFromUi(curRow);
        d_stagedAxes[curRow].unitDisplayMode = index;
        d_stagedAxes[curRow].dirty = true;
    }

    for (int r = 0; r < 4; ++r) {
        if (r == curRow)
            continue;
        const int axisId = Graph::mapToQwtAxis(r);
        const bool rowAuto = d_stagedAxes[r].dirty ? d_stagedAxes[r].autoTitle
                                                   : d_graph->isAxisTitleAuto(axisId);
        if (rowAuto) {
            ensureAxisRowStaged(r);
            d_stagedAxes[r].unitDisplayMode = index;
            const QString previewTitle = previewAutoAxisTitle(axisId, fmt);
            if (!previewTitle.isEmpty())
                d_stagedAxes[r].title = previewTitle;
        }
    }
}

void AxesDialog::pickAxisColor()
{
    QColor c = QColorDialog::getColor(boxAxisColor->color(), this);
    if (!c.isValid() || c == boxAxisColor->color())
        return;

    boxAxisColor->setColor(c);
    markAxisDirty();
}

void AxesDialog::pickAxisNumColor()
{
    QColor c = QColorDialog::getColor(boxAxisNumColor->color(), this);
    if (!c.isValid() || c == boxAxisNumColor->color())
        return;

    boxAxisNumColor->setColor(c);
    markAxisDirty();
}

void AxesDialog::setAxisType(int)
{
    if (!d_graph)
        return;
    int a = mapToQwtAxisId();
    auto type = d_graph->axesType()[a];

    QSignalBlocker b1(boxAxisType);
    boxAxisType->setCurrentIndex(boxAxisType->findData(static_cast<int>(type)));
    showAxisFormatOptions(boxAxisType->findData(static_cast<int>(type)));

    if (type == Graph::AxisType::Txt && a >= 0 && a < formatInfo.size()) {
        QSignalBlocker b2(boxColName);
        const int idx = boxColName->findText(formatInfo[a]);
        if (idx >= 0)
            boxColName->setCurrentIndex(idx);
    }
}

void AxesDialog::setBaselineDist(int)
{
    int a = mapToQwtAxisId();
    if (a >= 0 && a < axesBaseline.size()) {
        QSignalBlocker b(boxBaseline);
        boxBaseline->setValue(axesBaseline[a]);
    }
}

void AxesDialog::setTicksType(int)
{
    int a = mapToQwtAxisId();
    if (a >= 0 && a < majTicks.size() && a < minTicks.size()) {
        QSignalBlocker b1(boxMajorTicksType);
        QSignalBlocker b2(boxMinorTicksType);
        boxMajorTicksType->setCurrentIndex(majTicks[a]);
        boxMinorTicksType->setCurrentIndex(minTicks[a]);
    }
}

void AxesDialog::updateMajTicksType(int)
{
    int axis = mapToQwtAxisId();
    int type = boxMajorTicksType->currentIndex();
    if (axis >= 0 && axis < majTicks.size())
        majTicks[axis] = type;
    markAxisDirty();
}

void AxesDialog::updateMinTicksType(int)
{
    int axis = mapToQwtAxisId();
    int type = boxMinorTicksType->currentIndex();
    if (axis >= 0 && axis < minTicks.size())
        minTicks[axis] = type;
    markAxisDirty();
}

void AxesDialog::updateTickLabelsList(bool on)
{
    int axis = mapToQwtAxisId();
    if (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
        boxAngle->setEnabled(on);

    bool userFormat = (boxFormat->currentIndex() != 0);
    boxPrecision->setEnabled(on && userFormat);

    bool axisOk = boxShowAxis->isChecked();
    boxDivideFactor->setEnabled(on && axisOk);
    boxTickPrefix->setEnabled(on && axisOk);
    boxTickSuffix->setEnabled(on && axisOk);

    if (axis >= 0 && axis < tickLabelsOn.size())
        tickLabelsOn[axis] = QString::number(on);
    markAxisDirty();
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

void AxesDialog::setCurrentQwtAxis(int qwtAxis)
{
    // QwtPlot の軸ID(yLeft=0, yRight=1, xBottom=2, xTop=3)を各タブの行番号
    // (0 = Bottom, 1 = Left, 2 = Top, 3 = Right)に直す。setCurrentScale() は
    // QwtScaleDraw::Alignment を受け取る別仕様なので、軸IDをそのまま渡してはいけない。
    int row = -1;
    switch (qwtAxis) {
    case QwtPlot::xBottom:
        row = 0;
        break;
    case QwtPlot::yLeft:
        row = 1;
        break;
    case QwtPlot::xTop:
        row = 2;
        break;
    case QwtPlot::yRight:
        row = 3;
        break;
    default:
        break;
    }
    if (row < 0)
        return;
    if (axesList)
        axesList->setCurrentRow(row);
    if (axesTitlesList)
        axesTitlesList->setCurrentRow(row);
    if (axisBreakAxesList)
        axisBreakAxesList->setCurrentRow(row);
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
    auto type = currentSelectedAxisType();
    int format = boxFormat->currentIndex();
    if (type == Graph::AxisType::Numeric)
        boxPrecision->setEnabled(format != 0 && boxShowLabels->isChecked()
                                 && boxShowAxis->isChecked());
    markAxisDirty();
}

void AxesDialog::showAxisFormula(int axis)
{
    if (!d_graph)
        return;
    QStringList l = d_graph->getAxesFormulas();
    QString formula = l.value(axis);
    QSignalBlocker b1(boxShowFormula);
    QSignalBlocker b2(boxFormula);
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
        QSignalBlocker b1(boxDivideFactor);
        QSignalBlocker b2(boxTickPrefix);
        QSignalBlocker b3(boxTickSuffix);
        boxDivideFactor->setText(formatLocaleScalar(d_graph->tickDivideFactor(a), 12));
        boxTickPrefix->setText(d_graph->tickPrefix(a));
        boxTickSuffix->setText(d_graph->tickSuffix(a));
    }

    if (currentSelectedAxisType() != Graph::AxisType::Numeric || !d_graph || !d_graph->plotWidget())
        return;

    int format = d_graph->plotWidget()->axisLabelFormat(a);
    QSignalBlocker bFmt(boxFormat);
    QSignalBlocker bPrec(boxPrecision);
    boxFormat->setCurrentIndex(format);
    boxPrecision->setValue(d_graph->plotWidget()->axisLabelPrecision(a));

    boxPrecision->setEnabled(format != 0 && boxShowLabels->isChecked() && boxShowAxis->isChecked());
    showAxisFormula(a);
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
    markAxisDirty();
}

void AxesDialog::customAxisLabelFont()
{
    if (!d_graphGuard || !d_graph)
        return;

    const int row = axesTitlesList ? axesTitlesList->currentRow() : 0;
    const int axis = mapToQwtAxisId();
    bool okF = false;
    QFont oldFont = (row >= 0 && row < 4 && d_stagedAxes[row].titleFontDirty)
            ? d_stagedAxes[row].titleFont
            : d_graph->axisTitleFont(axis);
    if (oldFont.family().trimmed().isEmpty())
        oldFont.setFamily(QStringLiteral("Arial"));
    QFont fnt = QFontDialog::getFont(&okF, oldFont, this);
    if (okF) {
        if (fnt.family().trimmed().isEmpty())
            fnt.setFamily(QStringLiteral("Arial"));
        fnt = PlotStyleLimits::preserveFractionalPtAfterFontDialog(oldFont, fnt);
        if (boxTitleFontSize) {
            QSignalBlocker sb(boxTitleFontSize);
            boxTitleFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(fnt, 12.0));
        }
        if (row >= 0 && row < 4) {
            d_stagedAxes[row].titleFont = fnt;
            d_stagedAxes[row].titleFontDirty = true;
            markAxisDirty();
        }
    }
}

void AxesDialog::toggleAxisBackbone(bool)
{
    if (!d_graphGuard || !d_graph || d_loadingUi)
        return;
    markAxisDirty();
}

void AxesDialog::ensureAxisRowStaged(int row)
{
    if (!d_graphGuard || !d_graph || row < 0 || row >= 4)
        return;
    if (d_stagedAxes[row].dirty) {
        if (axesTitlesList && axesTitlesList->currentRow() == row)
            captureAxisRowFromUi(row);
        return;
    }

    StagedAxisEdit &st = d_stagedAxes[row];
    const int axis = Graph::mapToQwtAxis(row);
    Plot *p = d_graph->plotWidget();
    st.showAxis = p ? p->axisEnabled(axis) : true;
    st.showBackbone = d_graph->isAxisBackboneVisible(axis);
    st.title = d_graph->axisTitle(axis);
    st.autoTitle = d_graph->isAxisTitleAuto(axis);
    st.unitDisplayMode = d_unitFormatDirty ? d_stagedUnitFormatIdx
                                           : unitFormatToIndex(d_graph->axisUnitFormat());
    st.axisType = axesType.value(axis, Graph::AxisType::Numeric);
    st.axisColor = d_graph->axisColor(axis);
    st.axisNumColor = d_graph->axisNumbersColor(axis);
    st.majTicksType = majTicks.value(axis, 0);
    st.minTicksType = minTicks.value(axis, 0);
    st.baseline = axesBaseline.value(axis, 0);
    st.showLabels = (tickLabelsOn.value(axis) != QStringLiteral("0"));
    const QString fmtInfo = formatInfo.value(axis);
    if (st.axisType == Graph::AxisType::Txt) {
        st.colName = fmtInfo;
    } else if (st.axisType == Graph::AxisType::ColHeader) {
        st.tableName = fmtInfo;
    } else if (st.axisType == Graph::AxisType::Numeric) {
        st.formatIndex = p ? p->axisLabelFormat(axis) : 0;
        st.precision = p ? p->axisLabelPrecision(axis) : 6;
    } else if (st.axisType == Graph::AxisType::Day || st.axisType == Graph::AxisType::Month) {
        st.formatIndex = fmtInfo.toInt();
    } else if (st.axisType == Graph::AxisType::Time || st.axisType == Graph::AxisType::Date
               || st.axisType == Graph::AxisType::DateTime) {
        const QStringList parts = fmtInfo.split(QLatin1Char(';'));
        if (parts.size() >= 2)
            st.formatText = parts[1];
    }
    st.angle = d_graph->labelsRotation(axis);
    const QString formula = d_graph->getAxesFormulas().value(axis);
    st.showFormula = !formula.isEmpty();
    st.formula = formula;
    st.divideFactor = formatLocaleScalar(d_graph->tickDivideFactor(axis), 12);
    st.tickPrefix = d_graph->tickPrefix(axis);
    st.tickSuffix = d_graph->tickSuffix(axis);
    st.dirty = true;
}

QString AxesDialog::previewAutoAxisTitle(int qwtAxis, Column::UnitFormat fmt) const
{
    if (!d_graphGuard || !d_graph || qwtAxis < 0 || qwtAxis >= 4 || d_graph->isPiePlot()
        || d_graph->isTernaryPlot())
        return QString();

    QList<Column *> xBottomCols, xTopCols, yLeftCols, yRightCols;
    int detectedStyle = -1;

    for (int i = 0; i < d_graph->curves(); ++i) {
        QwtPlotCurve *pc = d_graph->curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        const int cType = d_graph->curveType(i);
        if (cType == Graph::ErrorBars)
            continue;
        if (detectedStyle < 0)
            detectedStyle = cType;

        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (!dc || !dc->table())
            continue;
        Table *t = dc->table();
        const int xIdx = t->colIndex(dc->xColumnName());
        const int yIdx = t->colIndex(dc->yColumnName());
        if (Column *xc = (xIdx >= 0 ? t->column(xIdx) : nullptr)) {
            QList<Column *> &dst = (pc->xAxis() == QwtPlot::xTop) ? xTopCols : xBottomCols;
            if (!dst.contains(xc))
                dst << xc;
        }
        if (Column *yc = (yIdx >= 0 ? t->column(yIdx) : nullptr)) {
            QList<Column *> &dst = (pc->yAxis() == QwtPlot::yRight) ? yRightCols : yLeftCols;
            if (!dst.contains(yc))
                dst << yc;
        }
    }

    auto cleanColAxisTitle = [fmt](Column *col) -> QString {
        if (!col)
            return QString();
        QString t = col->formattedAxisTitle(fmt).trimmed();
        if (t.isEmpty())
            t = col->name().trimmed();
        return t;
    };

    auto resolveMultiCol = [&](const QList<Column *> &cols) -> QString {
        if (cols.isEmpty())
            return QString();
        if (cols.size() == 1)
            return cleanColAxisTitle(cols.first());
        const QString firstFull = cleanColAxisTitle(cols.first());
        bool allSameFull = !firstFull.isEmpty();
        const QString firstUnits = cols.first()->units().trimmed();
        bool allSameNonEmptyUnits = !firstUnits.isEmpty();
        for (int i = 1; i < cols.size(); ++i) {
            if (cleanColAxisTitle(cols[i]) != firstFull)
                allSameFull = false;
            if (cols[i]->units().trimmed() != firstUnits)
                allSameNonEmptyUnits = false;
        }
        if (allSameFull || allSameNonEmptyUnits)
            return firstFull;
        return QString();
    };

    if (detectedStyle == Graph::Histogram)
        return (qwtAxis == QwtPlot::xBottom) ? resolveMultiCol(yLeftCols) : QString();
    if (detectedStyle == Graph::Box)
        return (qwtAxis == QwtPlot::yLeft) ? resolveMultiCol(yLeftCols) : QString();
    if (detectedStyle == Graph::HorizontalBars) {
        if (qwtAxis == QwtPlot::xBottom)
            return resolveMultiCol(yLeftCols);
        if (qwtAxis == QwtPlot::yLeft)
            return resolveMultiCol(xBottomCols);
        return QString();
    }

    switch (qwtAxis) {
    case QwtPlot::xBottom:
        return resolveMultiCol(xBottomCols);
    case QwtPlot::xTop:
        return resolveMultiCol(xTopCols);
    case QwtPlot::yLeft:
        return resolveMultiCol(yLeftCols);
    case QwtPlot::yRight:
        return resolveMultiCol(yRightCols);
    default:
        return QString();
    }
}

void AxesDialog::stageFramePreset(Graph::FramePreset preset)
{
    if (!d_graphGuard || !d_graph)
        return;

    bool hasRightCurve = false;
    bool hasTopCurve = false;
    for (int i = 0; i < d_graph->curves(); ++i) {
        QwtPlotItem *it = d_graph->plotItem(i);
        if (!it)
            continue;
        if (it->yAxis() == QwtPlot::yRight)
            hasRightCurve = true;
        if (it->xAxis() == QwtPlot::xTop)
            hasTopCurve = true;
    }

    for (int r = 0; r < 4; ++r)
        ensureAxisRowStaged(r);

    // Row mapping: 0 = xBottom, 1 = yLeft, 2 = xTop, 3 = yRight
    switch (preset) {
    case Graph::FrameBoxInwardTicks:
        for (int r = 0; r < 4; ++r) {
            d_stagedAxes[r].showAxis = true;
            d_stagedAxes[r].showBackbone = true;
            d_stagedAxes[r].majTicksType = ScaleDraw::In;
            d_stagedAxes[r].minTicksType = ScaleDraw::In;
        }
        d_stagedAxes[0].showLabels = true;
        d_stagedAxes[1].showLabels = true;
        d_stagedAxes[2].showLabels = hasTopCurve;
        d_stagedAxes[3].showLabels = hasRightCurve;
        break;
    case Graph::FrameBoxPlainOpposite:
        for (int r = 0; r < 4; ++r) {
            d_stagedAxes[r].showAxis = true;
            d_stagedAxes[r].showBackbone = true;
        }
        d_stagedAxes[0].showLabels = true;
        d_stagedAxes[1].showLabels = true;
        d_stagedAxes[2].showLabels = hasTopCurve;
        d_stagedAxes[3].showLabels = hasRightCurve;
        d_stagedAxes[0].majTicksType = ScaleDraw::In;
        d_stagedAxes[0].minTicksType = ScaleDraw::In;
        d_stagedAxes[1].majTicksType = ScaleDraw::In;
        d_stagedAxes[1].minTicksType = ScaleDraw::In;
        d_stagedAxes[2].majTicksType = hasTopCurve ? ScaleDraw::In : ScaleDraw::None;
        d_stagedAxes[2].minTicksType = hasTopCurve ? ScaleDraw::In : ScaleDraw::None;
        d_stagedAxes[3].majTicksType = hasRightCurve ? ScaleDraw::In : ScaleDraw::None;
        d_stagedAxes[3].minTicksType = hasRightCurve ? ScaleDraw::In : ScaleDraw::None;
        break;
    case Graph::FrameOpenL:
        d_stagedAxes[0].showAxis = true;
        d_stagedAxes[1].showAxis = true;
        d_stagedAxes[2].showAxis = hasTopCurve;
        d_stagedAxes[3].showAxis = hasRightCurve;
        d_stagedAxes[0].showBackbone = true;
        d_stagedAxes[1].showBackbone = true;
        d_stagedAxes[2].showBackbone = hasTopCurve;
        d_stagedAxes[3].showBackbone = hasRightCurve;
        d_stagedAxes[0].showLabels = true;
        d_stagedAxes[1].showLabels = true;
        d_stagedAxes[2].showLabels = hasTopCurve;
        d_stagedAxes[3].showLabels = hasRightCurve;
        d_stagedAxes[0].majTicksType = ScaleDraw::Out;
        d_stagedAxes[0].minTicksType = ScaleDraw::Out;
        d_stagedAxes[1].majTicksType = ScaleDraw::Out;
        d_stagedAxes[1].minTicksType = ScaleDraw::Out;
        d_stagedAxes[2].majTicksType = hasTopCurve ? ScaleDraw::Out : ScaleDraw::None;
        d_stagedAxes[2].minTicksType = hasTopCurve ? ScaleDraw::Out : ScaleDraw::None;
        d_stagedAxes[3].majTicksType = hasRightCurve ? ScaleDraw::Out : ScaleDraw::None;
        d_stagedAxes[3].minTicksType = hasRightCurve ? ScaleDraw::Out : ScaleDraw::None;
        break;
    case Graph::FrameZeroCrossed:
        d_stagedAxes[0].showAxis = true;
        d_stagedAxes[1].showAxis = true;
        d_stagedAxes[2].showAxis = false;
        d_stagedAxes[3].showAxis = false;
        for (int r = 0; r < 4; ++r)
            d_stagedAxes[r].showBackbone = false;
        d_stagedAxes[0].showLabels = true;
        d_stagedAxes[1].showLabels = true;
        d_stagedAxes[0].majTicksType = ScaleDraw::Out;
        d_stagedAxes[0].minTicksType = ScaleDraw::Out;
        d_stagedAxes[1].majTicksType = ScaleDraw::Out;
        d_stagedAxes[1].minTicksType = ScaleDraw::Out;
        break;
    }

    d_stagedFramePreset = static_cast<int>(preset);
    d_axesTabDirty = true;
    d_frameTabDirty = true;

    if (boxBackbones) {
        QSignalBlocker b(boxBackbones);
        boxBackbones->setChecked(preset != Graph::FrameZeroCrossed);
    }

    const bool zeroLines = (preset == Graph::FrameZeroCrossed);
    if (!d_gridSharedDirty) {
        d_stagedGridXAxisIdx = boxGridXAxis ? boxGridXAxis->currentIndex() : 0;
        d_stagedGridYAxisIdx = boxGridYAxis ? boxGridYAxis->currentIndex() : 0;
    }
    d_stagedGridXZero = zeroLines;
    d_stagedGridYZero = zeroLines;
    d_gridSharedDirty = true;
    d_gridTabDirty = true;
    if (boxXLine) {
        QSignalBlocker b(boxXLine);
        boxXLine->setChecked(zeroLines);
    }
    if (boxYLine) {
        QSignalBlocker b(boxYLine);
        boxYLine->setChecked(zeroLines);
    }
    updateLegacyZeroLineVisibility();

    const int curRow = (axesTitlesList && axesTitlesList->currentRow() >= 0)
            ? axesTitlesList->currentRow()
            : 0;
    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;
    onAxesTitlesRowChanged(curRow);
    d_loadingUi = prevLoading;
}

void AxesDialog::applyPresetBoxInward()
{
    stageFramePreset(Graph::FrameBoxInwardTicks);
}

void AxesDialog::applyPresetBoxPlain()
{
    stageFramePreset(Graph::FrameBoxPlainOpposite);
}

void AxesDialog::applyPresetOpenL()
{
    stageFramePreset(Graph::FrameOpenL);
}

void AxesDialog::applyPresetZeroCrossed()
{
    stageFramePreset(Graph::FrameZeroCrossed);
}

Graph::ReferenceLineSpec AxesDialog::buildRefLineSpecFromUi() const
{
    Graph::ReferenceLineSpec sp;
    sp.orientation = refOrientBox ? refOrientBox->currentData().toInt() : 0;
    sp.statMode = refStatBox ? refStatBox->currentData().toString() : QString();
    sp.value = refValueBox ? refValueBox->value() : 0.0;
    sp.color = refColorBtn ? refColorBtn->color() : QColor(30, 102, 184);
    sp.style = refStyleBox ? static_cast<Qt::PenStyle>(refStyleBox->currentData().toInt())
                           : Qt::DashLine;
    sp.width = refWidthBox ? refWidthBox->value() : 1.5;
    sp.label = refLabelEdit ? refLabelEdit->text().trimmed() : QString();
    sp.fillToAxis = refFillBox && refFillBox->isChecked();
    sp.fillColor = refFillColorBtn ? refFillColorBtn->color() : QColor(30, 102, 184, 32);
    if (refFillOpacityBox) {
        // 不透明度(%)を色のアルファ値に反映する。ボタンの色の不透明度と同じ%なら元のアルファを
        // 触らない(32 -> 13 % -> 33 のような丸め誤差で値が動かないように)。
        const int colorPct = qRound(sp.fillColor.alpha() * 100.0 / 255.0);
        if (refFillOpacityBox->value() != colorPct)
            sp.fillColor.setAlpha(qBound(0, qRound(refFillOpacityBox->value() * 255.0 / 100.0), 255));
    } else if (sp.fillToAxis && sp.fillColor.alpha() == 255) {
        sp.fillColor.setAlpha(36);
    }
    return sp;
}

QString AxesDialog::formatRefLineListLabel(const Graph::ReferenceLineSpec &sp) const
{
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
    return sp.label.isEmpty() ? (orientTag + modeText)
                              : (orientTag + modeText + " - " + sp.label);
}

void AxesDialog::syncSelectedRefLineFromUi()
{
    if (d_loadingUi)
        return;
    d_refLinesTabDirty = true;
    d_refControlsEdited = true;
    if (!refLinesList)
        return;
    const int row = refLinesList->currentRow();
    if (row >= 0 && row < d_stagedRefLines.size()) {
        const bool keepVis = d_stagedRefLines[row].visible;
        d_stagedRefLines[row] = buildRefLineSpecFromUi();
        d_stagedRefLines[row].visible = keepVis;
        if (QListWidgetItem *it = refLinesList->item(row))
            it->setText(formatRefLineListLabel(d_stagedRefLines[row]));
    }
}

void AxesDialog::refreshReferenceLinesList()
{
    if (!refLinesList)
        return;
    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;
    int prevRow = refLinesList->currentRow();
    refLinesList->blockSignals(true);
    refLinesList->clear();
    for (int i = 0; i < d_stagedRefLines.size(); ++i)
        refLinesList->addItem(formatRefLineListLabel(d_stagedRefLines[i]));
    refLinesList->blockSignals(false);
    if (refLinesList->count() > 0) {
        int sel = qBound(0, prevRow >= 0 ? prevRow : 0, refLinesList->count() - 1);
        QSignalBlocker b(refLinesList);
        refLinesList->setCurrentRow(sel);
        selectReferenceLine(sel);
    }
    d_loadingUi = prevLoading;
}

void AxesDialog::selectReferenceLine(int row)
{
    if (row < 0 || row >= d_stagedRefLines.size())
        return;

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;
    QSignalBlocker b1(refOrientBox);
    QSignalBlocker b2(refStatBox);
    QSignalBlocker b3(refValueBox);
    QSignalBlocker b4(refColorBtn);
    QSignalBlocker b5(refStyleBox);
    QSignalBlocker b6(refWidthBox);
    QSignalBlocker b7(refLabelEdit);
    QSignalBlocker b8(refFillBox);
    QSignalBlocker b9(refFillColorBtn);
    QSignalBlocker b10(refFillOpacityBox);

    const Graph::ReferenceLineSpec &sp = d_stagedRefLines[row];
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
    refFillOpacityBox->setValue(qBound(PlotStyleLimits::kOpacityMinPercent,
                                       qRound(sp.fillColor.alpha() * 100.0 / 255.0),
                                       PlotStyleLimits::kOpacityMaxPercent));
    d_loadingUi = prevLoading;
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
    if (!d_graphGuard || !d_graph)
        return;
    d_stagedRefLines.append(buildRefLineSpecFromUi());
    d_refLinesTabDirty = true;
    d_refControlsEdited = false;
    refreshReferenceLinesList();
    if (refLinesList && refLinesList->count() > 0) {
        QSignalBlocker b(refLinesList);
        const int lastRow = refLinesList->count() - 1;
        refLinesList->setCurrentRow(lastRow);
        selectReferenceLine(lastRow);
    }
}

void AxesDialog::addZeroRefLine(int orientation)
{
    if (!d_graphGuard || !d_graph)
        return;
    // 現在の見た目(色・線種・太さ)を引き継ぎ、定数値 0 の水平(Y=0)/垂直(X=0)線として追加する。
    Graph::ReferenceLineSpec sp = buildRefLineSpecFromUi();
    sp.orientation = (orientation == 1) ? 1 : 0;
    sp.statMode = QString();
    sp.value = 0.0;
    sp.label = QString();
    sp.fillToAxis = false;
    sp.visible = true;
    d_stagedRefLines.append(sp);
    d_refLinesTabDirty = true;
    d_refControlsEdited = false;
    refreshReferenceLinesList();
    if (refLinesList && refLinesList->count() > 0) {
        QSignalBlocker b(refLinesList);
        const int lastRow = refLinesList->count() - 1;
        refLinesList->setCurrentRow(lastRow);
        selectReferenceLine(lastRow);
    }
}

void AxesDialog::updateSelectedRefLineFromUi()
{
    if (!d_graphGuard || !d_graph || !refLinesList)
        return;
    int row = refLinesList->currentRow();
    if (row >= 0 && row < d_stagedRefLines.size()) {
        const bool keepVis = d_stagedRefLines[row].visible;
        d_stagedRefLines[row] = buildRefLineSpecFromUi();
        d_stagedRefLines[row].visible = keepVis;
    } else if (d_stagedRefLines.isEmpty()) {
        d_stagedRefLines.append(buildRefLineSpecFromUi());
        row = 0;
    } else {
        return;
    }
    d_refLinesTabDirty = true;
    d_refControlsEdited = false;
    refreshReferenceLinesList();
    if (row >= 0 && row < refLinesList->count()) {
        QSignalBlocker b(refLinesList);
        refLinesList->setCurrentRow(row);
        selectReferenceLine(row);
    }
}

void AxesDialog::removeSelectedRefLine()
{
    if (!d_graphGuard || !d_graph || !refLinesList)
        return;
    int row = refLinesList->currentRow();
    if (row < 0 || row >= d_stagedRefLines.size())
        return;
    d_stagedRefLines.removeAt(row);
    d_refLinesTabDirty = true;
    d_refControlsEdited = false;
    refreshReferenceLinesList();
}

void AxesDialog::clearAllRefLines()
{
    if (!d_graphGuard || !d_graph)
        return;
    d_stagedRefLines.clear();
    d_refLinesTabDirty = true;
    d_refControlsEdited = false;
    refreshReferenceLinesList();
}

void AxesDialog::updateAxisBreakPage(int row)
{
    if (!d_graphGuard || !d_graph || !axisBreakAxesList)
        return;
    if (row < 0)
        row = axisBreakAxesList->currentRow();
    if (row < 0 || row >= 4)
        row = 0;

    if (!d_loadingUi && d_prevBreakRow >= 0 && d_prevBreakRow < 4 && d_prevBreakRow != row
        && d_stagedBreaks[d_prevBreakRow].dirty) {
        captureBreakRowFromUi(d_prevBreakRow);
    }
    d_prevBreakRow = row;

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;

    QSignalBlocker b1(boxEnableAxisBreak);
    QSignalBlocker b2(boxBreakFrom);
    QSignalBlocker b3(boxBreakTo);
    QSignalBlocker b4(boxBreakPositionPct);
    QSignalBlocker b5(boxBreakStepBefore);
    QSignalBlocker b6(boxBreakStepAfter);
    QSignalBlocker b7(boxBreakTypeBefore);
    QSignalBlocker b8(boxBreakTypeAfter);

    if (d_stagedBreaks[row].dirty) {
        const StagedBreakEdit &st = d_stagedBreaks[row];
        boxEnableAxisBreak->setChecked(st.enabled);
        boxBreakFrom->setText(st.breakFrom);
        boxBreakTo->setText(st.breakTo);
        boxBreakPositionPct->setValue(qBound(10.0, st.positionPct, 90.0));
        boxBreakStepBefore->setText(st.stepBefore);
        boxBreakStepAfter->setText(st.stepAfter);
        boxBreakTypeBefore->setCurrentIndex(st.typeBefore);
        boxBreakTypeAfter->setCurrentIndex(st.typeAfter);
        boxBreakFrom->setEnabled(st.enabled);
        boxBreakTo->setEnabled(st.enabled);
        boxBreakPositionPct->setEnabled(st.enabled);
        boxBreakStepBefore->setEnabled(st.enabled);
        boxBreakStepAfter->setEnabled(st.enabled);
        boxBreakTypeBefore->setEnabled(st.enabled);
        boxBreakTypeAfter->setEnabled(st.enabled);
        d_loadingUi = prevLoading;
        return;
    }

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
    boxBreakFrom->setText(formatLocaleScalar(spec.enabled ? spec.breakFrom : defFrom, 10));
    boxBreakTo->setText(formatLocaleScalar(spec.enabled ? spec.breakTo : defTo, 10));
    boxBreakPositionPct->setValue(qBound(10.0, spec.positionPct * 100.0, 90.0));
    boxBreakStepBefore->setText(formatLocaleScalar(spec.stepBefore, 10));
    boxBreakStepAfter->setText(formatLocaleScalar(spec.stepAfter, 10));
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
    d_loadingUi = prevLoading;
}

void AxesDialog::applyTickLabelTransformFromUi()
{
    if (!d_graphGuard || !d_graph)
        return;
    const int axis = mapToQwtAxisId();
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    double factor = 1.0;
    const QString factorTxt = boxDivideFactor ? boxDivideFactor->text().trimmed() : QString();
    if (!factorTxt.isEmpty()) {
        double v = 0.0;
        if (evalAxisScalarExpr(factorTxt, &v) && std::isfinite(v) && std::fabs(v) > 1e-300)
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
    // 事前に選ばれた軸/タブ(setCurrentScale / setCurrentQwtAxis / showAxesPage など)は維持する。
    // 行が未選択(-1)のリストだけ先頭(Bottom / Horizontal Grid)にそろえる。
    for (QListWidget *lw : { axesList, axesGridList, axesTitlesList, axisBreakAxesList }) {
        if (lw && lw->currentRow() < 0 && lw->count() > 0)
            lw->setCurrentRow(0);
    }

    // 小さい画面でも初期サイズが画面からはみ出さないようにする(各タブは内部スクロールで補う)。
    if (!isVisible()) {
        if (QScreen *scr = QGuiApplication::primaryScreen()) {
            const QSize maxSize = scr->availableGeometry().size() * 9 / 10;
            resize(sizeHint().boundedTo(maxSize));
        }
    }

    // 標準のモーダル exec()。以前は show() だけ呼んで 0 を返す疑似非ブロッキングだった。
    return QDialog::exec();
}

void AxesDialog::updateMinorTicksList(int scaleType)
{
    if (!d_graphGuard || !d_graph)
        return;

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;
    QSignalBlocker b(boxMinorValue);
    const int prevMinor = parseLeadingMinorInt(boxMinorValue->currentText(), -1);
    boxMinorValue->clear();
    boxMinorValue->addItems(minorTickItemsForScale(scaleType != 0));

    int targetMinor = prevMinor;
    bool matchedInNewList = false;
    if (prevMinor >= 0) {
        for (int i = 0; i < boxMinorValue->count(); ++i) {
            if (parseLeadingMinorInt(boxMinorValue->itemText(i), -1) == prevMinor) {
                matchedInNewList = true;
                break;
            }
        }
    }
    if (!matchedInNewList) {
        if (scaleType != 0) {
            targetMinor = 8;
        } else if (d_graph->plotWidget() && axesList && axesList->currentRow() >= 0) {
            const int a = Graph::mapToQwtAxis(axesList->currentRow());
            targetMinor = d_graph->plotWidget()->axisMaxMinor(a);
        } else {
            targetMinor = 4;
        }
    }
    selectOrSetMinorTickCombo(boxMinorValue, targetMinor);
    d_loadingUi = prevLoading;
    markScaleDirty();
}

void AxesDialog::showAxis(int axis, Graph::AxisType type, const QString &labelsColName, bool axisOn,
                          int majTicksType, int minTicksType, bool labelsOn, const QColor &c,
                          int format, int prec, int rotation, int baselineDist,
                          const QString &formula, const QColor &labelsColor)
{
    if (!d_graphGuard || !d_graph)
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

void AxesDialog::autoRangeCurrentAxis()
{
    if (!d_graphGuard || !d_graph)
        return;
    int row = axesList ? axesList->currentRow() : 0;
    if (row < 0 || row >= 4)
        row = 0;

    if (axesTitlesList && axesTitlesList->currentRow() != row) {
        if (d_prevAxisRow >= 0 && d_prevAxisRow < 4 && d_stagedAxes[d_prevAxisRow].dirty)
            captureAxisRowFromUi(d_prevAxisRow);
        QSignalBlocker b(axesTitlesList);
        axesTitlesList->setCurrentRow(row);
        onAxesTitlesRowChanged(row);
    }

    MultiLayer *ml = d_graph->multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    const int qwtAxis = mapToQwtAxisId();
    d_graph->setAutoScaleAxis(qwtAxis);
    d_stagedScales[row] = StagedScaleEdit();
    bool anyScaleStillDirty = false;
    for (int i = 0; i < 4; ++i)
        anyScaleStillDirty = anyScaleStillDirty || d_stagedScales[i].dirty;
    d_scalesTabDirty = anyScaleStillDirty;

    updateScale();

    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Auto Range"), beforeSnap);
}

void AxesDialog::onTitleFontSizeChanged(double pt)
{
    if (!d_graphGuard || !d_graph || d_loadingUi || pt <= 0.0)
        return;
    const int row = axesTitlesList ? axesTitlesList->currentRow() : 0;
    if (row < 0 || row >= 4)
        return;
    const int axis = mapToQwtAxisId();
    QFont tf = d_stagedAxes[row].titleFontDirty ? d_stagedAxes[row].titleFont
                                                : d_graph->axisTitleFont(axis);
    if (tf.family().trimmed().isEmpty())
        tf.setFamily(QStringLiteral("Arial"));
    tf.setPointSizeF(pt);
    d_stagedAxes[row].titleFont = tf;
    d_stagedAxes[row].titleFontDirty = true;
    markAxisDirty();
}

void AxesDialog::onAxesFontSizeChanged(double pt)
{
    if (!d_graphGuard || !d_graph || d_loadingUi || pt <= 0.0)
        return;
    const int row = axesTitlesList ? axesTitlesList->currentRow() : 0;
    if (row < 0 || row >= 4)
        return;
    QFont *targetFont = nullptr;
    switch (row) {
    case 0:
        targetFont = &xBottomFont;
        break;
    case 1:
        targetFont = &yLeftFont;
        break;
    case 2:
        targetFont = &xTopFont;
        break;
    case 3:
        targetFont = &yRightFont;
        break;
    }
    if (!targetFont)
        return;
    if (targetFont->family().trimmed().isEmpty())
        targetFont->setFamily(QStringLiteral("Arial"));
    targetFont->setPointSizeF(pt);
    d_stagedAxes[row].axisFont = *targetFont;
    d_stagedAxes[row].axisFontDirty = true;
    markAxisDirty();
}

void AxesDialog::onAxesTitlesRowChanged(int row)
{
    if (!d_graphGuard || !d_graph || row < 0 || row >= 4)
        return;

    if (!d_loadingUi && d_prevAxisRow >= 0 && d_prevAxisRow < 4 && d_prevAxisRow != row
        && d_stagedAxes[d_prevAxisRow].dirty) {
        captureAxisRowFromUi(d_prevAxisRow);
    }
    d_prevAxisRow = row;

    const bool prevLoading = d_loadingUi;
    d_loadingUi = true;

    updateShowBox(row);
    updateAxisColor(row);
    updateTitleBox(row);
    setTicksType(row);
    setAxisType(row);
    setBaselineDist(row);
    updateLabelsFormat(row);

    QFont rowAxisFont = xBottomFont;
    if (row == 1)
        rowAxisFont = yLeftFont;
    else if (row == 2)
        rowAxisFont = xTopFont;
    else if (row == 3)
        rowAxisFont = yRightFont;
    if (d_stagedAxes[row].axisFontDirty)
        rowAxisFont = d_stagedAxes[row].axisFont;
    if (boxAxesFontSize) {
        QSignalBlocker b(boxAxesFontSize);
        boxAxesFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(rowAxisFont, 12.0));
    }

    if (d_stagedAxes[row].dirty) {
        const StagedAxisEdit &st = d_stagedAxes[row];
        QSignalBlocker bShow(boxShowAxis);
        QSignalBlocker bBack(boxAxisBackbone);
        QSignalBlocker bTitle(boxTitle);
        QSignalBlocker bAuto(boxAutoAxisTitle);
        QSignalBlocker bType(boxAxisType);
        QSignalBlocker bCol(boxAxisColor);
        QSignalBlocker bNumCol(boxAxisNumColor);
        QSignalBlocker bMaj(boxMajorTicksType);
        QSignalBlocker bMin(boxMinorTicksType);
        QSignalBlocker bBase(boxBaseline);
        QSignalBlocker bLbls(boxShowLabels);
        QSignalBlocker bColN(boxColName);
        QSignalBlocker bTblN(boxTableName);
        QSignalBlocker bFmt(boxFormat);
        QSignalBlocker bPrec(boxPrecision);
        QSignalBlocker bAng(boxAngle);
        QSignalBlocker bShowF(boxShowFormula);
        QSignalBlocker bForm(boxFormula);
        QSignalBlocker bDiv(boxDivideFactor);
        QSignalBlocker bPfx(boxTickPrefix);
        QSignalBlocker bSfx(boxTickSuffix);

        boxShowAxis->setChecked(st.showAxis);
        boxAxisBackbone->setChecked(st.showBackbone);
        boxTitle->setText(st.title);
        boxAutoAxisTitle->setChecked(st.autoTitle);
        if (st.titleFontDirty && boxTitleFontSize) {
            QSignalBlocker bTfs(boxTitleFontSize);
            boxTitleFontSize->setValue(
                    PlotStyleLimits::effectiveFontPointSizeF(st.titleFont, 12.0));
        }

        const int typeIdx = boxAxisType->findData(static_cast<int>(st.axisType));
        if (typeIdx >= 0) {
            boxAxisType->setCurrentIndex(typeIdx);
            showAxisFormatOptions(typeIdx);
        }
        boxAxisColor->setColor(st.axisColor);
        boxAxisNumColor->setColor(st.axisNumColor);
        boxMajorTicksType->setCurrentIndex(st.majTicksType);
        boxMinorTicksType->setCurrentIndex(st.minTicksType);
        boxBaseline->setValue(st.baseline);
        boxShowLabels->setChecked(st.showLabels);
        if (!st.colName.isEmpty())
            boxColName->setCurrentText(st.colName);
        if (!st.tableName.isEmpty())
            boxTableName->setCurrentText(st.tableName);
        if (st.formatIndex >= 0 && st.formatIndex < boxFormat->count())
            boxFormat->setCurrentIndex(st.formatIndex);
        if (boxFormat->isEditable() && !st.formatText.isEmpty())
            boxFormat->setEditText(st.formatText);
        boxPrecision->setValue(st.precision);
        boxAngle->setValue(st.angle);
        boxShowFormula->setChecked(st.showFormula);
        if (st.showFormula) {
            boxFormula->show();
            boxFormula->setText(st.formula);
        } else {
            boxFormula->clear();
            boxFormula->hide();
        }
        boxDivideFactor->setText(st.divideFactor);
        boxTickPrefix->setText(st.tickPrefix);
        boxTickSuffix->setText(st.tickSuffix);

        const bool ok = st.showAxis;
        const bool labels = st.showLabels;
        boxTitle->setEnabled(ok);
        boxAxisColor->setEnabled(ok);
        boxAxisNumColor->setEnabled(ok);
        btnAxesFont->setEnabled(ok);
        if (boxAxesFontSize)
            boxAxesFontSize->setEnabled(ok);
        boxShowLabels->setEnabled(ok);
        boxMajorTicksType->setEnabled(ok);
        boxMinorTicksType->setEnabled(ok);
        boxAxisType->setEnabled(ok);
        boxBaseline->setEnabled(ok);
        labelBox->setEnabled(ok);
        boxFormat->setEnabled(labels && ok);
        boxColName->setEnabled(labels && ok);
        boxShowFormula->setEnabled(labels && ok);
        boxFormula->setEnabled(labels && ok);
        boxDivideFactor->setEnabled(labels && ok);
        boxTickPrefix->setEnabled(labels && ok);
        boxTickSuffix->setEnabled(labels && ok);
        const int qwtAxis = Graph::mapToQwtAxis(row);
        boxAngle->setEnabled((qwtAxis == QwtPlot::xBottom || qwtAxis == QwtPlot::xTop) && labels
                             && ok);
        boxPrecision->setEnabled(labels && ok && (boxFormat->currentIndex() != 0));
    }

    d_loadingUi = prevLoading;
}

void AxesDialog::clearAllDirtyState()
{
    d_scalesTabDirty = false;
    d_gridTabDirty = false;
    d_axesTabDirty = false;
    d_frameTabDirty = false;
    d_refLinesTabDirty = false;
    d_refControlsEdited = false;
    d_axisBreakTabDirty = false;
    d_gridSharedDirty = false;
    d_unitFormatDirty = false;
    d_stagedFramePreset = -1;
    for (int i = 0; i < 4; ++i) {
        d_stagedScales[i] = StagedScaleEdit();
        d_stagedAxes[i] = StagedAxisEdit();
        d_stagedBreaks[i] = StagedBreakEdit();
    }
    for (int i = 0; i < 2; ++i)
        d_stagedGrids[i] = StagedGridEdit();
}

void AxesDialog::captureScaleRowFromUi(int row)
{
    if (row < 0 || row >= 4)
        return;
    StagedScaleEdit &st = d_stagedScales[row];
    st.start = boxStart->text();
    st.end = boxEnd->text();
    st.step = boxStep->text();
    st.unitIndex = boxUnit->currentIndex();
    st.useStep = btnStep->isChecked();
    st.majorValue = boxMajorValue->value();
    st.minorText = QString::number(parseLeadingMinorInt(boxMinorValue->currentText(), 0));
    st.scaleType = boxScaleType->currentIndex();
    st.inverted = btnInvert->isChecked();
}

void AxesDialog::captureGridRowFromUi(int row)
{
    if (row < 0 || row >= 2)
        return;
    StagedGridEdit &st = d_stagedGrids[row];
    st.majorOn = boxMajorGrid->isChecked();
    st.minorOn = boxMinorGrid->isChecked();
    st.colorMajor = boxColorMajor->color();
    st.colorMinor = boxColorMinor->color();
    st.typeMajor = boxTypeMajor->currentIndex();
    st.typeMinor = boxTypeMinor->currentIndex();
    st.widthMajor = boxWidthMajor->value();
    st.widthMinor = boxWidthMinor->value();
}

void AxesDialog::captureAxisRowFromUi(int row)
{
    if (row < 0 || row >= 4)
        return;
    StagedAxisEdit &st = d_stagedAxes[row];
    st.showAxis = boxShowAxis->isChecked();
    st.showBackbone = boxAxisBackbone->isChecked();
    st.title = boxTitle->toPlainText();
    st.autoTitle = boxAutoAxisTitle->isChecked();
    st.unitDisplayMode = boxAxisUnitFormat ? boxAxisUnitFormat->currentIndex() : 0;
    st.axisType = currentSelectedAxisType();
    st.axisColor = boxAxisColor->color();
    st.axisNumColor = boxAxisNumColor->color();
    st.majTicksType = boxMajorTicksType->currentIndex();
    st.minTicksType = boxMinorTicksType->currentIndex();
    st.baseline = boxBaseline->value();
    st.showLabels = boxShowLabels->isChecked();
    st.colName = boxColName->currentText();
    st.tableName = boxTableName->currentText();
    st.formatIndex = boxFormat->currentIndex();
    st.formatText = boxFormat->currentText();
    st.precision = boxPrecision->value();
    st.angle = boxAngle->value();
    st.showFormula = boxShowFormula->isChecked();
    st.formula = boxFormula->toPlainText();
    st.divideFactor = boxDivideFactor->text();
    st.tickPrefix = boxTickPrefix->text();
    st.tickSuffix = boxTickSuffix->text();
}

void AxesDialog::captureBreakRowFromUi(int row)
{
    if (row < 0 || row >= 4)
        return;
    StagedBreakEdit &st = d_stagedBreaks[row];
    st.enabled = boxEnableAxisBreak->isChecked();
    st.breakFrom = boxBreakFrom->text();
    st.breakTo = boxBreakTo->text();
    st.positionPct = boxBreakPositionPct->value();
    st.stepBefore = boxBreakStepBefore->text();
    st.stepAfter = boxBreakStepAfter->text();
    st.typeBefore = boxBreakTypeBefore->currentIndex();
    st.typeAfter = boxBreakTypeAfter->currentIndex();
}

void AxesDialog::markScaleDirty()
{
    if (d_loadingUi || !axesList)
        return;
    const int row = axesList->currentRow();
    if (row < 0 || row >= 4)
        return;
    d_scalesTabDirty = true;
    d_stagedScales[row].dirty = true;
    captureScaleRowFromUi(row);
}

void AxesDialog::markGridDirty()
{
    if (d_loadingUi || !axesGridList)
        return;
    const int row = axesGridList->currentRow();
    if (row < 0 || row >= 2)
        return;
    d_gridTabDirty = true;
    d_stagedGrids[row].dirty = true;
    captureGridRowFromUi(row);
}

void AxesDialog::markAxisDirty()
{
    if (d_loadingUi || !axesTitlesList)
        return;
    const int row = axesTitlesList->currentRow();
    if (row < 0 || row >= 4)
        return;
    d_axesTabDirty = true;
    d_stagedAxes[row].dirty = true;
    captureAxisRowFromUi(row);
}

void AxesDialog::markFrameDirty()
{
    if (d_loadingUi)
        return;
    d_frameTabDirty = true;
}

void AxesDialog::markBreakDirty()
{
    if (d_loadingUi || !axisBreakAxesList)
        return;
    const int row = axisBreakAxesList->currentRow();
    if (row < 0 || row >= 4)
        return;
    d_axisBreakTabDirty = true;
    d_stagedBreaks[row].dirty = true;
    captureBreakRowFromUi(row);
}

bool AxesDialog::commitScaleRow(int row)
{
    if (!d_graphGuard || !d_graph || row < 0 || row >= 4)
        return false;
    const StagedScaleEdit &st = d_stagedScales[row];
    QString from = st.start.toLower();
    QString to = st.end.toLower();
    QString step = st.step.toLower();
    const int a = Graph::mapToQwtAxis(row);
    const Graph::AxisType aType = axesType.value(a, Graph::AxisType::Numeric);
    switch (aType) {
    case Graph::AxisType::Time:
    case Graph::AxisType::Date:
    case Graph::AxisType::DateTime: {
        QDateTime qdt = QDateTime::fromString(st.start.trimmed(), "yyyy-MM-dd hh:mm:ss");
        if (!qdt.isValid())
            return false;
        from = QString::number(qdt.toMSecsSinceEpoch() / 86400000. + 2440587.5, 'f', 16);
        qdt = QDateTime::fromString(st.end.trimmed(), "yyyy-MM-dd hh:mm:ss");
        if (!qdt.isValid())
            return false;
        to = QString::number(qdt.toMSecsSinceEpoch() / 86400000. + 2440587.5, 'f', 16);
        break;
    }
    default:
        break;
    }

    double start = 0.0, end = 0.0, stp = 0.0;
    if (!evalAxisScalarExpr(from, &start) || !evalAxisScalarExpr(to, &end))
        return false;
    if (st.useStep && !evalAxisScalarExpr(step, &stp))
        return false;
    if (!std::isfinite(start) || !std::isfinite(end) || start >= end)
        return false;
    if (st.scaleType == 1 && (start <= 0.0 || end <= 0.0))
        return false;

    if (st.useStep) {
        if (stp <= 0)
            return false;
        if (aType == Graph::AxisType::Time) {
            switch (st.unitIndex) {
            case 1:
                stp *= 1e3;
                break;
            case 2:
                stp *= 6e4;
                break;
            case 3:
                stp *= 36e5;
                break;
            default:
                break;
            }
        } else if (aType == Graph::AxisType::Date) {
            if (st.unitIndex == 1)
                stp *= 7;
        } else if (aType == Graph::AxisType::DateTime) {
            switch (st.unitIndex) {
            case 0:
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
            case 5:
                stp *= 7;
                break;
            default:
                break;
            }
        }
    }

    const Graph::AxisBreakSpec prevBrk = d_graph->axisBreak(a);
    d_graph->setScale(a, start, end, stp, st.majorValue, parseLeadingMinorInt(st.minorText, 0),
                      st.scaleType, st.inverted);
    d_graph->setAxisUserLocked(a, true);
    if (prevBrk.enabled) {
        const double lo = std::min(start, end);
        const double hi = std::max(start, end);
        if (lo < prevBrk.breakFrom && prevBrk.breakFrom < prevBrk.breakTo && prevBrk.breakTo < hi)
            d_graph->setAxisBreak(a, prevBrk);
    }
    return true;
}

void AxesDialog::commitGridState()
{
    if (!d_graphGuard || !d_graph || !d_graph->plotWidget())
        return;
    Grid *grid = (Grid *)d_graph->plotWidget()->grid();
    if (!grid)
        return;

    if (d_stagedGrids[1].dirty) {
        const StagedGridEdit &st = d_stagedGrids[1];
        grid->enableX(st.majorOn);
        grid->enableXMin(st.minorOn);
        grid->setMajPenX(QPen(st.colorMajor, st.widthMajor, Graph::getPenStyle(st.typeMajor)));
        grid->setMinPenX(QPen(st.colorMinor, st.widthMinor, Graph::getPenStyle(st.typeMinor)));
    }
    if (d_stagedGrids[0].dirty) {
        const StagedGridEdit &st = d_stagedGrids[0];
        grid->enableY(st.majorOn);
        grid->enableYMin(st.minorOn);
        grid->setMajPenY(QPen(st.colorMajor, st.widthMajor, Graph::getPenStyle(st.typeMajor)));
        grid->setMinPenY(QPen(st.colorMinor, st.widthMinor, Graph::getPenStyle(st.typeMinor)));
    }
    if (d_gridSharedDirty) {
        grid->enableZeroLineX(d_stagedGridXZero);
        grid->enableZeroLineY(d_stagedGridYZero);
        grid->setAxis(d_stagedGridXAxisIdx + 2, d_stagedGridYAxisIdx);
    }
}

bool AxesDialog::commitAxisRow(int row)
{
    if (!d_graphGuard || !d_graph || row < 0 || row >= 4)
        return false;
    const StagedAxisEdit &st = d_stagedAxes[row];
    const int axis = Graph::mapToQwtAxis(row);
    const Graph::AxisType format = st.axisType;
    if (axis >= 0 && axis < axesType.size())
        axesType[axis] = format;
    if (axis >= 0 && axis < axesBaseline.size())
        axesBaseline[axis] = st.baseline;
    if (axis >= 0 && axis < majTicks.size())
        majTicks[axis] = st.majTicksType;
    if (axis >= 0 && axis < minTicks.size())
        minTicks[axis] = st.minTicksType;
    if (axis >= 0 && axis < tickLabelsOn.size())
        tickLabelsOn[axis] = st.showLabels ? QStringLiteral("1") : QStringLiteral("0");

    if (row == 0)
        xAxisOn = st.showAxis;
    else if (row == 1)
        yAxisOn = st.showAxis;
    else if (row == 2)
        topAxisOn = st.showAxis;
    else if (row == 3)
        rightAxisOn = st.showAxis;

    if (axis >= 0 && axis < formatInfo.size()) {
        if (format == Graph::AxisType::Time || format == Graph::AxisType::Date
            || format == Graph::AxisType::DateTime) {
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
                lst << st.formatText;
            } else {
                lst[1] = st.formatText;
            }
            formatInfo[axis] = lst.join(";");
        } else if (format == Graph::AxisType::Day || format == Graph::AxisType::Month) {
            formatInfo[axis] = QString::number(st.formatIndex);
        } else if (format == Graph::AxisType::ColHeader) {
            formatInfo[axis] = st.tableName;
        } else if (format == Graph::AxisType::Txt) {
            formatInfo[axis] = st.colName;
        }
    }

    d_graph->setAxisTitleAuto(axis, st.autoTitle);
    if (st.autoTitle) {
        d_graph->refreshAutoAxisTitlesFromColumns();
        if (axesTitlesList && axesTitlesList->currentRow() == row) {
            QSignalBlocker b(boxTitle);
            boxTitle->setText(d_graph->axisTitle(axis));
        }
    } else if (d_graph->axisTitle(axis) != st.title) {
        d_graph->setAxisTitle(axis, st.title);
        d_graph->setAxisTitleAuto(axis, false);
    }

    if (axis == QwtPlot::xBottom)
        xBottomLabelsRotation = st.angle;
    else if (axis == QwtPlot::xTop)
        xTopLabelsRotation = st.angle;

    QString formula = st.showFormula ? st.formula : QString();
    showAxis(axis, format, formatInfo.value(axis), st.showAxis, st.majTicksType, st.minTicksType,
             st.showLabels, st.axisColor, st.formatIndex, st.precision, st.angle, st.baseline,
             formula, st.axisNumColor);
    d_graph->setAxisBackboneVisible(axis, st.showBackbone);

    if (st.axisFontDirty) {
        QFont af = st.axisFont;
        if (af.family().trimmed().isEmpty())
            af.setFamily(QStringLiteral("Arial"));
        if (row == 0)
            xBottomFont = af;
        else if (row == 1)
            yLeftFont = af;
        else if (row == 2)
            xTopFont = af;
        else if (row == 3)
            yRightFont = af;
        d_graph->setAxisFont(axis, af);
    }
    if (st.titleFontDirty) {
        QFont tf = st.titleFont;
        if (tf.family().trimmed().isEmpty())
            tf.setFamily(QStringLiteral("Arial"));
        d_graph->setAxisTitleFont(axis, tf);
    }

    double factor = 1.0;
    const QString factorTxt = st.divideFactor.trimmed();
    if (!factorTxt.isEmpty()) {
        double v = 0.0;
        if (evalAxisScalarExpr(factorTxt, &v) && std::isfinite(v) && std::fabs(v) > 1e-300)
            factor = v;
    }
    if (std::fabs(d_graph->tickDivideFactor(axis) - factor) > 1e-15
        || d_graph->tickPrefix(axis) != st.tickPrefix
        || d_graph->tickSuffix(axis) != st.tickSuffix) {
        d_graph->setTickLabelTransform(axis, factor, st.tickPrefix, st.tickSuffix);
    }
    return true;
}

void AxesDialog::commitFrameState()
{
    if (!d_graphGuard || !d_graph)
        return;
    d_graph->setAxesLinewidth(boxAxesLinewidth->value());
    d_graph->changeTicksLength(boxMinorTicksLength->value(), boxMajorTicksLength->value());
    d_graph->drawCanvasFrame(boxFramed->isChecked(), boxFrameWidth->value(),
                             boxFrameColor->color());
    if (boxBackbones->isChecked() != d_graph->axesBackbones())
        d_graph->drawAxesBackbones(boxBackbones->isChecked());
}

bool AxesDialog::commitBreakRow(int row)
{
    if (!d_graphGuard || !d_graph || row < 0 || row >= 4)
        return false;
    const StagedBreakEdit &st = d_stagedBreaks[row];
    const int qwtAxis = Graph::mapToQwtAxis(row);
    if (!st.enabled) {
        d_graph->clearAxisBreak(qwtAxis);
        return true;
    }

    bool okFrom = false, okTo = false, okStepB = false, okStepA = false;
    Graph::AxisBreakSpec spec;
    spec.enabled = true;
    spec.breakFrom = evalAxisScalarOrEmptyFallback(st.breakFrom, 0.0, &okFrom);
    spec.breakTo = evalAxisScalarOrEmptyFallback(st.breakTo, 0.0, &okTo);
    spec.positionPct = qBound(0.10, st.positionPct / 100.0, 0.90);
    spec.stepBefore = qMax(0.0, evalAxisScalarOrEmptyFallback(st.stepBefore, 0.0, &okStepB));
    spec.stepAfter = qMax(0.0, evalAxisScalarOrEmptyFallback(st.stepAfter, 0.0, &okStepA));
    spec.log10Before = (st.typeBefore == 1);
    spec.log10After = (st.typeAfter == 1);
    if (!okFrom || !okTo || !(spec.breakFrom < spec.breakTo) || !okStepB || !okStepA)
        return false;
    d_graph->setAxisBreak(qwtAxis, spec);
    return true;
}

void AxesDialog::applyTickAndAxisStyleToAllAxes()
{
    if (!d_graphGuard || !d_graph)
        return;

    int srcRow = 0;
    if (generalDialog.currentWidget() == axesPage && axesTitlesList
        && axesTitlesList->currentRow() >= 0 && axesTitlesList->currentRow() < 4) {
        srcRow = axesTitlesList->currentRow();
    } else if (generalDialog.currentWidget() == scalesPage && axesList
               && axesList->currentRow() >= 0 && axesList->currentRow() < 4) {
        srcRow = axesList->currentRow();
    } else if (axesTitlesList && axesTitlesList->currentRow() >= 0
               && axesTitlesList->currentRow() < 4) {
        srcRow = axesTitlesList->currentRow();
    }

    for (int r = 0; r < 4; ++r)
        ensureAxisRowStaged(r);

    if (axesTitlesList && axesTitlesList->currentRow() == srcRow)
        captureAxisRowFromUi(srcRow);

    const StagedAxisEdit &src = d_stagedAxes[srcRow];
    const int srcAxis = Graph::mapToQwtAxis(srcRow);

    QFont srcTickFont = xBottomFont;
    if (srcRow == 1)
        srcTickFont = yLeftFont;
    else if (srcRow == 2)
        srcTickFont = xTopFont;
    else if (srcRow == 3)
        srcTickFont = yRightFont;
    if (src.axisFontDirty)
        srcTickFont = src.axisFont;
    if (srcTickFont.family().trimmed().isEmpty())
        srcTickFont.setFamily(QStringLiteral("Arial"));
    srcTickFont.setPointSizeF(PlotStyleLimits::effectiveFontPointSizeF(srcTickFont, 12.0));

    QFont srcTitleFont = src.titleFontDirty ? src.titleFont : d_graph->axisTitleFont(srcAxis);
    if (srcTitleFont.family().trimmed().isEmpty())
        srcTitleFont.setFamily(QStringLiteral("Arial"));
    srcTitleFont.setPointSizeF(PlotStyleLimits::effectiveFontPointSizeF(srcTitleFont, 12.0));

    // Stage the current axis's tick & font styles onto all 4 axes without mutating cached member
    // fonts, forcing hidden axes visible, or applying to the graph until the user clicks Apply/OK.
    for (int r = 0; r < 4; ++r) {
        StagedAxisEdit &dst = d_stagedAxes[r];
        dst.majTicksType = src.majTicksType;
        dst.minTicksType = src.minTicksType;
        dst.axisColor = src.axisColor;
        dst.axisNumColor = src.axisNumColor;
        dst.baseline = src.baseline;
        dst.axisFont = srcTickFont;
        dst.axisFontDirty = true;
        dst.titleFont = srcTitleFont;
        dst.titleFontDirty = true;
        dst.dirty = true;
    }

    d_axesTabDirty = true;
}
