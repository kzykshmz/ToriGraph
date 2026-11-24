/***************************************************************************
    File                 : FitDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Fit Wizard

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
#include "FitDialog.h"
#include "MyParser.h"
#include "ApplicationWindow.h"
#include "ColorButton.h"
#include "Fit.h"
#include "MultiPeakFit.h"
#include "ExponentialFit.h"
#include "PolynomialFit.h"
#include "PluginFit.h"
#include "NonLinearFit.h"
#include "SigmoidalFit.h"
#include "Matrix.h"
#include <muParserError.h>

#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QWidget>
#include <QMessageBox>
#include <QComboBox>
#include <QWidgetList>
#include <QRadioButton>
#include <QFileDialog>
#include <QGroupBox>
#include <QLibrary>
#include <QLocale>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QApplication>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QPair>
#include <QPointer>
#include <QVector>
#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QStringListModel>
#include <mutex>
#include "I18nJapanese.h"
#include <qwt_plot_curve.h>
#include <stdio.h>
#include <cmath>
#include <limits>
#include <vector>

namespace {

void registerFitDialogJapaneseStrings()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Search models:", "モデルを検索:");
        reg("Type part of a name or description (e.g. gauss, voigt, arrhenius)",
            "名前や説明の一部を入力(例: gauss, voigt, arrhenius)");
        reg("Recently used:", "最近使ったモデル:");
        reg("Models you fitted recently (up to 8)", "最近フィットに使ったモデル (最大 8 件)");
        reg("Choose a recent model", "最近使ったモデルから選ぶ");
        reg("(none yet)", "(まだありません)");
        reg("The fit did not converge.", "フィットが収束していません。");
        reg("The curve and parameters shown are the last iterate. Check the initial values, "
            "increase the number of iterations, or fix some parameters, and fit again.",
            "表示中の曲線とパラメータは最後の反復値です。初期値を見直す、反復回数を増やす、"
            "一部のパラメータを固定する、などをしてからフィットし直してください。");
    });
}

// C4-4: VoigtApprox has exactly the same formula and parameters as PseudoVoigt1, so the
// dialog lists only PseudoVoigt1 and maps the duplicate name onto it.
bool isRedundantBuiltInModel(const QString &name)
{
    return name.compare(QStringLiteral("VoigtApprox"), Qt::CaseInsensitive) == 0;
}

QString canonicalDialogModelName(const QString &name)
{
    return isRedundantBuiltInModel(name) ? QStringLiteral("PseudoVoigt1") : name;
}

class FitPreviewCurveItem : public QwtPlotCurve
{
public:
    explicit FitPreviewCurveItem(QwtPlotCurve **slotRef, const QString &title)
        : QwtPlotCurve(title), d_slot_ref(slotRef)
    {
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    ~FitPreviewCurveItem() override
    {
        if (d_slot_ref && *d_slot_ref == this)
            *d_slot_ref = nullptr;
    }

private:
    QwtPlotCurve **d_slot_ref = nullptr;
};

class ModelSearchReturnFilter : public QObject
{
public:
    explicit ModelSearchReturnFilter(QLineEdit *edit) : QObject(edit), d_edit(edit) { }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == d_edit && event && event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                if (d_edit)
                    QMetaObject::invokeMethod(d_edit, "returnPressed", Qt::DirectConnection);
                ke->accept();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QLineEdit *d_edit = nullptr;
};

QString formatLocaleScalar(double val, char fmt = 'g', int prec = 15)
{
    QLocale loc = QLocale();
    loc.setNumberOptions(loc.numberOptions() | QLocale::OmitGroupSeparator);
    return loc.toString(val, fmt, prec);
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

bool evalScalarExpr(const QString &raw, double *out, QString *errMsg = nullptr)
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
            int nRes = 0;
            const double *vals = parser.Eval(nRes);
            if (nRes != 1 || !vals) {
                if (localErr)
                    *localErr = QObject::tr("Multiple comma-separated expressions are not allowed.");
                return false;
            }
            res = vals[0];
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

} // namespace

FitDialog::FitDialog(QWidget *parent, Qt::WindowFlags fl) : QDialog(parent, fl)
{
    setWindowTitle(tr("Fit Wizard"));
    setSizeGripEnabled(true);
    setProperty("toriHasInternalScroll", true);
    setMinimumSize(620, 340);
    resize(780, 520);

    d_user_function_names = QStringList();
    d_user_functions = QStringList();
    d_user_function_params = QStringList();

    d_fitter = 0;
    d_graph = 0;
    d_src_table = 0;

    tw = new QStackedWidget();

    initEditPage();
    initFitPage();
    initAdvancedPage();

    QVBoxLayout *vl = new QVBoxLayout();
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(4);
    vl->addWidget(tw);
    setLayout(vl);

    setBuiltInFunctionNames();
    setBuiltInFunctions();

    // C4-4: open on a model category (Peak Functions) instead of the parser function
    // list ("Basic"), so the first thing a user sees is something that can be fitted.
    categoryBox->setCurrentRow(4);
    funcBox->setCurrentRow(0);

    loadPlugins();
}

FitDialog::~FitDialog()
{
    clearPreviewCurve();
    if (d_fitter) {
        if (d_accept_depth > 0) {
            // FitDialog::accept() currently has d_fitter active on the call stack (e.g.
            // inside Filter::setDataCurve or Fit::fit modal QMessageBox); detach ownership
            // so the unwinding accept() guard deletes it once all Fit frames have returned.
            d_fitter = nullptr;
        } else {
            delete d_fitter;
            d_fitter = nullptr;
        }
    }
}

void FitDialog::initFitPage()
{
    QGridLayout *gl1 = new QGridLayout();
    gl1->addWidget(new QLabel(tr("Curve")), 0, 0);
    boxCurve = new QComboBox();
    gl1->addWidget(boxCurve, 0, 1);
    gl1->addWidget(new QLabel(tr("Function")), 1, 0);
    lblFunction = new QLabel();
    gl1->addWidget(lblFunction, 1, 1);
    boxFunction = new QTextEdit();
    boxFunction->setReadOnly(true);
    QPalette palette = boxFunction->palette();
    palette.setColor(QPalette::Active, QPalette::Base, Qt::lightGray);
    boxFunction->setPalette(palette);
    boxFunction->setMaximumHeight(42);
    gl1->addWidget(boxFunction, 2, 1);
    gl1->addWidget(new QLabel(tr("Initial guesses")), 3, 0);

    boxParams = new QTableWidget();
    boxParams->setMinimumHeight(88);
    boxParams->setColumnCount(5);
#if QT_VERSION >= 0x050000
    boxParams->horizontalHeader()->setSectionsClickable(false);
    boxParams->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    boxParams->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    boxParams->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    boxParams->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    boxParams->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    boxParams->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    boxParams->horizontalHeader()->setClickable(false);
    boxParams->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    boxParams->horizontalHeader()->setResizeMode(1, QHeaderView::Stretch);
    boxParams->horizontalHeader()->setResizeMode(2, QHeaderView::ResizeToContents);
    boxParams->horizontalHeader()->setResizeMode(3, QHeaderView::Stretch);
    boxParams->horizontalHeader()->setResizeMode(4, QHeaderView::Stretch);
    boxParams->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif
    QStringList header = QStringList() << tr("Parameter") << tr("Value") << tr("Fixed")
                                       << tr("Lower") << tr("Upper");
    boxParams->setHorizontalHeaderLabels(header);
    boxParams->verticalHeader()->hide();
    gl1->addWidget(boxParams, 3, 1);
    gl1->setRowStretch(3, 1);

    gl1->addWidget(new QLabel(tr("Algorithm")), 4, 0);
    boxAlgorithm = new QComboBox();
    boxAlgorithm->addItem(tr("Scaled Levenberg-Marquardt"));
    boxAlgorithm->addItem(tr("Unscaled Levenberg-Marquardt"));
    boxAlgorithm->addItem(tr("Nelder-Mead Simplex"));
    gl1->addWidget(boxAlgorithm, 4, 1);

    gl1->addWidget(new QLabel(tr("Color")), 5, 0);
    btnColor = new ColorButton();
    btnColor->setColor(QColor(Qt::red));
    gl1->addWidget(btnColor, 5, 1);

    gl1->addWidget(new QLabel(tr("Linear Constraints")), 6, 0);
    boxLinearConstraints = new QLineEdit();
    boxLinearConstraints->setPlaceholderText(tr("e.g. w1=w2; A2=0.5*A1"));
    boxLinearConstraints->setToolTip(
            tr("Optional semicolon-separated linear equality constraints (e.g. w1=w2; A2=0.5*A1)"));
    gl1->addWidget(boxLinearConstraints, 6, 1);

    QGroupBox *gb1 = new QGroupBox();
    gb1->setLayout(gl1);

    QGridLayout *gl2 = new QGridLayout();
    gl2->addWidget(new QLabel(tr("From x=")), 0, 0);
    boxFrom = new QLineEdit();
    gl2->addWidget(boxFrom, 0, 1);
    gl2->addWidget(new QLabel(tr("To x=")), 1, 0);
    boxTo = new QLineEdit();
    gl2->addWidget(boxTo, 1, 1);
    QGroupBox *gb2 = new QGroupBox();
    gb2->setLayout(gl2);

    QGridLayout *gl3 = new QGridLayout();
    gl3->addWidget(new QLabel(tr("Iterations")), 0, 0);
    boxPoints = new QSpinBox();
    boxPoints->setRange(10, 10000);
    boxPoints->setSingleStep(50);
    boxPoints->setValue(1000);
    gl3->addWidget(boxPoints, 0, 1);
    gl3->addWidget(new QLabel(tr("Tolerance")), 1, 0);
    boxTolerance = new QLineEdit("1e-6");
    gl3->addWidget(boxTolerance, 1, 1);
    QGroupBox *gb3 = new QGroupBox();
    gb3->setLayout(gl3);

    QHBoxLayout *hbox1 = new QHBoxLayout();
    hbox1->addWidget(gb2);
    hbox1->addWidget(gb3);

    QHBoxLayout *hbox2 = new QHBoxLayout();
    hbox2->addWidget(new QLabel(tr("Weighting / Y Error")));
    boxYErrorSource = new QComboBox();
    boxYErrorSource->addItem(tr("No Weighting (Errors Unknown)"));
    boxYErrorSource->addItem(tr("Instrumental (Associated Y Error)"));
    boxYErrorSource->addItem(tr("Statistical (Poisson 1/|Y|)"));
    boxYErrorSource->addItem(tr("Instrumental (Arbitrary Column)"));
    boxYErrorSource->addItem(tr("Direct Weighting (w = col)"));
    boxYErrorSource->addItem(tr("Proportional (w = 1/Y^2)"));
    boxYErrorSource->addItem(tr("Variance (w = 1/s^2 col)"));
    hbox2->addWidget(boxYErrorSource);
    QGroupBox *gb4 = new QGroupBox();
    gb4->setLayout(hbox2);

    tableNamesBox = new QComboBox();
    tableNamesBox->setEnabled(false);
    hbox2->addWidget(tableNamesBox);
    colNamesBox = new QComboBox();
    colNamesBox->setEnabled(false);
    hbox2->addWidget(colNamesBox);

    QHBoxLayout *hbox3 = new QHBoxLayout();
    hbox3->setSpacing(6);
    buttonEdit = new QPushButton(tr("<< &Edit function"));
    hbox3->addWidget(buttonEdit);
    btnDeleteFitCurves = new QPushButton(tr("&Delete Fit Curves"));
    hbox3->addWidget(btnDeleteFitCurves);
    buttonOk = new QPushButton(tr("&Fit"));
    buttonOk->setDefault(true);
    hbox3->addWidget(buttonOk);
    btnBootstrapCI = new QPushButton(tr("&Bootstrap CI..."));
    hbox3->addWidget(btnBootstrapCI);
    buttonCancel1 = new QPushButton(tr("&Close"));
    hbox3->addWidget(buttonCancel1);
    buttonAdvanced = new QPushButton(tr("Custom &Output >>"));
    hbox3->addWidget(buttonAdvanced);
    hbox3->addStretch();

    QHBoxLayout *hboxBands = new QHBoxLayout();
    chkLivePreview = new QCheckBox(tr("Live Preview"));
    chkLivePreview->setToolTip(
            tr("Render a live dashed initial-guess curve on the graph as parameters change"));
    confBandBox = new QCheckBox(tr("95% Confidence Band"));
    predBandBox = new QCheckBox(tr("95% Prediction Band"));
    summaryOnGraphBox = new QCheckBox(tr("Summary Table on Graph"));
    hboxBands->addWidget(chkLivePreview);
    hboxBands->addWidget(confBandBox);
    hboxBands->addWidget(predBandBox);
    hboxBands->addWidget(summaryOnGraphBox);
    hboxBands->addStretch();
    QGroupBox *gbBands = new QGroupBox(tr("Bands && Diagnostics"));
    gbBands->setLayout(hboxBands);

    QWidget *fitBody = new QWidget();
    QVBoxLayout *fitBodyLayout = new QVBoxLayout(fitBody);
    fitBodyLayout->setContentsMargins(0, 0, 4, 0);
    fitBodyLayout->setSpacing(6);
    fitBodyLayout->addWidget(gb1, 1);
    fitBodyLayout->addLayout(hbox1);
    fitBodyLayout->addWidget(gb4);
    fitBodyLayout->addWidget(gbBands);

    QScrollArea *fitScroll = new QScrollArea();
    fitScroll->setFrameShape(QFrame::NoFrame);
    fitScroll->setWidgetResizable(true);
    fitScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    fitScroll->setMinimumHeight(160);
    fitScroll->setWidget(fitBody);

    QVBoxLayout *vbox1 = new QVBoxLayout();
    vbox1->setContentsMargins(2, 2, 2, 2);
    vbox1->setSpacing(6);
    vbox1->addWidget(fitScroll, 1);
    vbox1->addLayout(hbox3);

    fitPage = new QWidget();
    fitPage->setLayout(vbox1);
    tw->addWidget(fitPage);

    connect(boxCurve, SIGNAL(activated(const QString &)), this,
            SLOT(activateCurve(const QString &)));
    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(btnBootstrapCI, SIGNAL(clicked()), this, SLOT(showBootstrapDialog()));
    connect(buttonCancel1, SIGNAL(clicked()), this, SLOT(close()));
    connect(buttonEdit, SIGNAL(clicked()), this, SLOT(showEditPage()));
    connect(btnDeleteFitCurves, SIGNAL(clicked()), this, SLOT(deleteFitCurves()));
    connect(boxYErrorSource, SIGNAL(activated(int)), this, SLOT(yErrorSourceChanged(int)));
    connect(buttonAdvanced, SIGNAL(clicked()), this, SLOT(showAdvancedPage()));
    connect(tableNamesBox, SIGNAL(activated(int)), this, SLOT(selectSrcTable(int)));
    connect(chkLivePreview, SIGNAL(toggled(bool)), this, SLOT(updateLivePreview()));
    connect(boxParams, SIGNAL(cellChanged(int, int)), this, SLOT(updateLivePreview()));
    connect(boxFrom, SIGNAL(editingFinished()), this, SLOT(updateLivePreview()));
    connect(boxTo, SIGNAL(editingFinished()), this, SLOT(updateLivePreview()));
    connect(boxLinearConstraints, SIGNAL(textChanged(const QString &)), this,
            SLOT(updateLivePreview()));

    setFocusProxy(boxFunction);
}

void FitDialog::initEditPage()
{
    QGridLayout *gl1 = new QGridLayout();
    gl1->addWidget(new QLabel(tr("Category")), 0, 0);
    gl1->addWidget(new QLabel(tr("Function")), 0, 1);
    gl1->addWidget(new QLabel(tr("Expression")), 0, 2);

    registerFitDialogJapaneseStrings();
    // Model search: type part of a model name or description and jump to it in its category.
    QHBoxLayout *searchRow = new QHBoxLayout();
    searchRow->addWidget(new QLabel(tr("Search models:")));
    modelSearchBox = new QLineEdit();
    modelSearchBox->setObjectName(QStringLiteral("fitModelSearchBox"));
    modelSearchBox->setPlaceholderText(
            tr("Type part of a name or description (e.g. gauss, voigt, arrhenius)"));
    modelSearchBox->setClearButtonEnabled(true);
    {
        QStringList entries;
        for (const QString &mName : NonLinearFit::builtInModelNames()) {
            if (isRedundantBuiltInModel(mName))
                continue;
            const QString desc = NonLinearFit::builtInModelDescription(mName);
            const QString cat = NonLinearFit::builtInModelCategory(mName);
            QString entry = mName;
            if (!desc.isEmpty())
                entry += QStringLiteral(" \u2014 ") + tr(desc.toUtf8().constData());
            if (!cat.isEmpty())
                entry += QStringLiteral(" [") + tr(cat.toUtf8().constData()) + QStringLiteral("]");
            entries << entry;
        }
        QCompleter *completer = new QCompleter(entries, modelSearchBox);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setMaxVisibleItems(15);
        modelSearchBox->setCompleter(completer);
        connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this,
                &FitDialog::jumpToModel);
    }
    modelSearchBox->installEventFilter(new ModelSearchReturnFilter(modelSearchBox));
    connect(modelSearchBox, &QLineEdit::returnPressed, this, [this]() {
        if (!modelSearchBox || modelSearchBox->text().trimmed().isEmpty())
            return;
        QCompleter *c = modelSearchBox->completer();
        if (c && c->popup() && c->popup()->isVisible())
            c->popup()->hide();
        if (c && c->completionCount() > 0) {
            c->setCurrentRow(0);
            jumpToModel(c->currentCompletion());
        } else {
            jumpToModel(modelSearchBox->text());
        }
    });
    searchRow->addWidget(modelSearchBox, 1);

    // F11: the models fitted most recently, next to the search box (stored in QSettings)
    searchRow->addSpacing(8);
    searchRow->addWidget(new QLabel(tr("Recently used:")));
    recentModelBox = new QComboBox();
    recentModelBox->setObjectName(QStringLiteral("fitRecentModelsBox"));
    recentModelBox->setToolTip(tr("Models you fitted recently (up to 8)"));
    recentModelBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    recentModelBox->setMinimumContentsLength(14);
    connect(recentModelBox, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
        if (idx <= 0 || !recentModelBox)
            return;
        const QString name = recentModelBox->itemData(idx).toString();
        recentModelBox->setCurrentIndex(0);
        selectRecentModel(name);
    });
    searchRow->addWidget(recentModelBox);
    refreshRecentModelsBox();

    categoryBox = new QListWidget();
    categoryBox->addItem(tr("User defined"));
    categoryBox->addItem(tr("Built-in"));
    categoryBox->addItem(tr("Basic"));
    categoryBox->addItem(tr("Plugins"));
    categoryBox->addItem(tr("Peak Functions"));
    categoryBox->addItem(tr("Exponential"));
    categoryBox->addItem(tr("Sigmoidal / Growth"));
    categoryBox->addItem(tr("Materials / Metallurgy / Physics"));
    categoryBox->addItem(tr("Waveform / Periodic"));
    categoryBox->addItem(tr("Power / Logarithmic / Rational / Polynomial"));
    categoryBox->setMinimumHeight(105);

    gl1->addWidget(categoryBox, 1, 0);
    funcBox = new QListWidget();
    funcBox->setMinimumHeight(105);
    gl1->addWidget(funcBox, 1, 1);
    explainBox = new QTextEdit();
    explainBox->setReadOnly(true);
    explainBox->setMinimumHeight(105);
    gl1->addWidget(explainBox, 1, 2);
    gl1->setRowStretch(1, 1);

    boxUseBuiltIn = new QCheckBox();
    boxUseBuiltIn->setText(tr("Fit with &built-in function"));
    boxUseBuiltIn->hide();

    QHBoxLayout *hbox1 = new QHBoxLayout();
    hbox1->addWidget(boxUseBuiltIn);
    hbox1->addStretch();

    polynomOrderLabel = new QLabel(tr("Polynomial Order"));
    polynomOrderLabel->hide();
    hbox1->addWidget(polynomOrderLabel);

    polynomOrderBox = new QSpinBox();
    polynomOrderBox->setMinimum(1);
    polynomOrderBox->setValue(2);
    polynomOrderBox->hide();
    connect(polynomOrderBox, SIGNAL(valueChanged(int)), this, SLOT(showExpression(int)));
    hbox1->addWidget(polynomOrderBox);

    buttonPlugins = new QPushButton(tr("&Choose plugins folder..."));
    hbox1->addWidget(buttonPlugins);
    buttonPlugins->hide();

    buttonClearUsrList = new QPushButton(tr("Clear user &list"));
    hbox1->addWidget(buttonClearUsrList);
    buttonClearUsrList->hide();

    QGridLayout *gl2 = new QGridLayout();
    gl2->addWidget(new QLabel(tr("Name")), 0, 0);
    boxName = new QLineEdit(tr("user1"));
    gl2->addWidget(boxName, 0, 1);
    btnAddFunc = new QPushButton(tr("&Save"));
    gl2->addWidget(btnAddFunc, 0, 2);
    gl2->addWidget(new QLabel(tr("Parameters")), 1, 0);
    boxParam = new QLineEdit("a, b");
    gl2->addWidget(boxParam, 1, 1);
    btnDelFunc = new QPushButton(tr("&Remove"));
    gl2->addWidget(btnDelFunc, 1, 2);

    QGroupBox *gb = new QGroupBox();
    gb->setLayout(gl2);

    editBox = new QTextEdit();
    editBox->setAcceptRichText(false);
    editBox->setMinimumHeight(64);
    editBox->setFocus();

    QVBoxLayout *vbox1 = new QVBoxLayout();
    btnAddTxt = new QPushButton(tr("Add &expression"));
    vbox1->addWidget(btnAddTxt);
    btnAddName = new QPushButton(tr("Add &name"));
    vbox1->addWidget(btnAddName);
    buttonClear = new QPushButton(tr("Rese&t"));
    vbox1->addWidget(buttonClear);
    vbox1->addStretch();

    QHBoxLayout *hbox2 = new QHBoxLayout();
    hbox2->addWidget(editBox, 1);
    hbox2->addLayout(vbox1);

    QWidget *editBody = new QWidget();
    QVBoxLayout *editBodyLayout = new QVBoxLayout(editBody);
    editBodyLayout->setContentsMargins(0, 0, 4, 0);
    editBodyLayout->setSpacing(6);
    editBodyLayout->addLayout(searchRow);
    editBodyLayout->addLayout(gl1, 1);
    editBodyLayout->addLayout(hbox1);
    editBodyLayout->addWidget(gb);
    editBodyLayout->addLayout(hbox2);

    QScrollArea *editScroll = new QScrollArea();
    editScroll->setFrameShape(QFrame::NoFrame);
    editScroll->setWidgetResizable(true);
    editScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editScroll->setMinimumHeight(160);
    editScroll->setWidget(editBody);

    QHBoxLayout *editBottomBar = new QHBoxLayout();
    editBottomBar->setSpacing(8);
    editBottomBar->addStretch();
    btnContinue = new QPushButton(tr("&Fit >>"));
    btnContinue->setAutoDefault(false);
    btnContinue->setDefault(true);
    editBottomBar->addWidget(btnContinue);
    buttonCancel2 = new QPushButton(tr("&Close"));
    editBottomBar->addWidget(buttonCancel2);

    QVBoxLayout *vbox2 = new QVBoxLayout();
    vbox2->setContentsMargins(2, 2, 2, 2);
    vbox2->setSpacing(6);
    vbox2->addWidget(editScroll, 1);
    vbox2->addLayout(editBottomBar);

    editPage = new QWidget();
    editPage->setLayout(vbox2);
    tw->addWidget(editPage);

    connect(buttonPlugins, SIGNAL(clicked()), this, SLOT(choosePluginsFolder()));
    connect(buttonClear, SIGNAL(clicked()), this, SLOT(resetFunction()));
    connect(buttonClearUsrList, SIGNAL(clicked()), this, SLOT(clearUserFunctions()));
    connect(categoryBox, SIGNAL(currentRowChanged(int)), this, SLOT(showFunctionsList(int)));
    connect(funcBox, SIGNAL(currentRowChanged(int)), this, SLOT(showExpression(int)));
    connect(boxUseBuiltIn, SIGNAL(toggled(bool)), this, SLOT(setFunction(bool)));
    connect(btnAddName, SIGNAL(clicked()), this, SLOT(pasteFunctionName()));
    connect(btnAddTxt, SIGNAL(clicked()), this, SLOT(pasteExpression()));
    connect(btnContinue, SIGNAL(clicked()), this, SLOT(showFitPage()));
    connect(btnAddFunc, SIGNAL(clicked()), this, SLOT(saveUserFunction()));
    connect(btnDelFunc, SIGNAL(clicked()), this, SLOT(removeUserFunction()));
    connect(buttonCancel2, SIGNAL(clicked()), this, SLOT(close()));
}

void FitDialog::initAdvancedPage()
{
    ApplicationWindow *app = (ApplicationWindow *)this->parent();

    generatePointsBtn = new QRadioButton(tr("&Uniform X Function"));
    generatePointsBtn->setChecked(app->generateUniformFitPoints);
    connect(generatePointsBtn, SIGNAL(clicked()), this, SLOT(enableApplyChanges()));

    QGridLayout *gl1 = new QGridLayout();
    gl1->addWidget(generatePointsBtn, 0, 0);

    lblPoints = new QLabel(tr("Points"));

    generatePointsBox = new QSpinBox();
    generatePointsBox->setRange(0, 1000000);
    generatePointsBox->setSingleStep(10);
    generatePointsBox->setValue(app->fitPoints);
    connect(generatePointsBox, SIGNAL(valueChanged(int)), this, SLOT(enableApplyChanges(int)));
    showPointsBox(!app->generateUniformFitPoints);

    QHBoxLayout *hb = new QHBoxLayout();
    hb->addStretch();
    hb->addWidget(lblPoints);
    hb->addWidget(generatePointsBox);
    gl1->addLayout(hb, 0, 1);

    samePointsBtn = new QRadioButton(tr("Same X as Fitting &Data"));
    gl1->addWidget(samePointsBtn, 1, 0);
    samePointsBtn->setChecked(!app->generateUniformFitPoints);
    connect(samePointsBtn, SIGNAL(clicked()), this, SLOT(enableApplyChanges()));

    QGroupBox *gb1 = new QGroupBox(tr("Generated Fit Curve"));
    gb1->setLayout(gl1);

    QGridLayout *gl2 = new QGridLayout();
    gl2->addWidget(new QLabel(tr("Significant Digits")), 0, 1);
    boxPrecision = new QSpinBox();
    boxPrecision->setRange(1, 15);
    boxPrecision->setValue(qBound(1, app->fit_output_precision, 15));
    connect(boxPrecision, SIGNAL(valueChanged(int)), this, SLOT(enableApplyChanges(int)));
    gl2->addWidget(boxPrecision, 0, 2);
    btnParamTable = new QPushButton(tr("Parameters &Table"));
    gl2->addWidget(btnParamTable, 1, 0);
    gl2->addWidget(new QLabel(tr("Name: ")), 1, 1);
    paramTableName = new QLineEdit(tr("Parameters"));
    gl2->addWidget(paramTableName, 1, 2);
    btnCovMatrix = new QPushButton(tr("Covariance &Matrix"));
    gl2->addWidget(btnCovMatrix, 2, 0);
    gl2->addWidget(new QLabel(tr("Name: ")), 2, 1);
    covMatrixName = new QLineEdit(tr("CovMatrix"));
    gl2->addWidget(covMatrixName, 2, 2);
    btnResidualPlot = new QPushButton(tr("&Residual Plot"));
    btnResidualPlot->setToolTip(
            tr("Create residual table (with Leverage & Cook's Distance) and residual plot window"));
    gl2->addWidget(btnResidualPlot, 3, 0, 1, 3);

    scaleErrorsBox = new QCheckBox(tr("Scale Errors with sqrt(Chi^2/doF)"));
    scaleErrorsBox->setChecked(app->fit_scale_errors);
    connect(scaleErrorsBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyChanges(int)));

    QGroupBox *gb2 = new QGroupBox(tr("Parameters Output"));
    gb2->setLayout(gl2);

    logBox = new QCheckBox(tr("&Write Parameters to Result Log"));
    logBox->setChecked(app->writeFitResultsToLog);
    connect(logBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyChanges(int)));

    plotLabelBox = new QCheckBox(tr("&Paste Parameters to Plot"));
    plotLabelBox->setChecked(app->pasteFitResultsToPlot);
    connect(plotLabelBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyChanges(int)));

    QHBoxLayout *hbox1 = new QHBoxLayout();

    btnBack = new QPushButton(tr("<< &Fit"));
    connect(btnBack, SIGNAL(clicked()), this, SLOT(showFitPage()));
    connect(btnBack, SIGNAL(clicked()), this, SLOT(applyChanges()));
    hbox1->addWidget(btnBack);

    btnApply = new QPushButton(tr("&Apply"));
    btnApply->setEnabled(false);
    connect(btnApply, SIGNAL(clicked()), this, SLOT(applyChanges()));
    hbox1->addWidget(btnApply);

    buttonCancel3 = new QPushButton(tr("&Close"));
    hbox1->addWidget(buttonCancel3);
    hbox1->addStretch();

    QWidget *advBody = new QWidget();
    QVBoxLayout *advBodyLayout = new QVBoxLayout(advBody);
    advBodyLayout->setContentsMargins(0, 0, 4, 0);
    advBodyLayout->setSpacing(6);
    advBodyLayout->addWidget(gb1);
    advBodyLayout->addWidget(gb2);
    advBodyLayout->addWidget(scaleErrorsBox);
    advBodyLayout->addWidget(logBox);
    advBodyLayout->addWidget(plotLabelBox);
    advBodyLayout->addStretch();

    QScrollArea *advScroll = new QScrollArea();
    advScroll->setFrameShape(QFrame::NoFrame);
    advScroll->setWidgetResizable(true);
    advScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    advScroll->setMinimumHeight(160);
    advScroll->setWidget(advBody);

    QVBoxLayout *vbox1 = new QVBoxLayout();
    vbox1->setContentsMargins(2, 2, 2, 2);
    vbox1->setSpacing(6);
    vbox1->addWidget(advScroll, 1);
    vbox1->addLayout(hbox1);

    advancedPage = new QWidget();
    advancedPage->setLayout(vbox1);
    tw->addWidget(advancedPage);

    connect(btnParamTable, SIGNAL(clicked()), this, SLOT(showParametersTable()));
    connect(btnCovMatrix, SIGNAL(clicked()), this, SLOT(showCovarianceMatrix()));
    connect(btnResidualPlot, SIGNAL(clicked()), this, SLOT(showResidualPlot()));
    connect(samePointsBtn, SIGNAL(toggled(bool)), this, SLOT(showPointsBox(bool)));
    connect(generatePointsBtn, SIGNAL(toggled(bool)), this, SLOT(showPointsBox(bool)));
    connect(buttonCancel3, SIGNAL(clicked()), this, SLOT(close()));
}

void FitDialog::applyChanges()
{
    ApplicationWindow *app = (ApplicationWindow *)this->parent();
    app->fit_output_precision = boxPrecision->value();
    app->pasteFitResultsToPlot = plotLabelBox->isChecked();
    app->writeFitResultsToLog = logBox->isChecked();
    app->fitPoints = generatePointsBox->value();
    app->generateUniformFitPoints = generatePointsBtn->isChecked();
    app->fit_scale_errors = scaleErrorsBox->isChecked();
    app->saveSettings();
    btnApply->setEnabled(false);
}

void FitDialog::showParametersTable()
{
    QString tableName = paramTableName->text();
    if (tableName.isEmpty()) {
        QMessageBox::critical(parentWidget(), tr("Error"),
                              tr("Please enter a valid name for the parameters table."));
        return;
    }

    if (!d_fitter) {
        QMessageBox::critical(parentWidget(), tr("Error"),
                              tr("Please perform a fit first and try again."));
        return;
    }

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (!app)
        return;
    tableName = app->generateUniqueName(tableName, false);
    d_fitter->parametersTable(tableName);
}

void FitDialog::showCovarianceMatrix()
{
    QString matrixName = covMatrixName->text();
    if (matrixName.isEmpty()) {
        QMessageBox::critical(parentWidget(), tr("Error"),
                              tr("Please enter a valid name for the covariance matrix."));
        return;
    }

    if (!d_fitter) {
        QMessageBox::critical(parentWidget(), tr("Error"),
                              tr("Please perform a fit first and try again."));
        return;
    }

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (!app)
        return;
    matrixName = app->generateUniqueName(matrixName, false);
    d_fitter->covarianceMatrix(matrixName);
}

void FitDialog::showResidualPlot()
{
    if (!d_fitter) {
        QMessageBox::critical(parentWidget(), tr("Error"),
                              tr("Please perform a fit first and try again."));
        return;
    }
    d_fitter->createResidualPlotWindow();
}

void FitDialog::showBootstrapDialog()
{
    QPointer<FitDialog> self(this);
    if (!d_fitter) {
        accept();
        if (!self || !d_fitter)
            return;
    }

    QPointer<Fit> fitterGuard(d_fitter);
    QDialog dlg(parentWidget());
    dlg.setWindowTitle(tr("Bootstrap / Monte Carlo Confidence Intervals"));
    dlg.setMinimumWidth(380);
    QFormLayout *fl = new QFormLayout(&dlg);

    QSpinBox *samplesSpin = new QSpinBox(&dlg);
    samplesSpin->setRange(20, 5000);
    samplesSpin->setSingleStep(50);
    samplesSpin->setValue(300);

    QDoubleSpinBox *confSpin = new QDoubleSpinBox(&dlg);
    confSpin->setRange(50.0, 99.9);
    confSpin->setDecimals(1);
    confSpin->setSingleStep(1.0);
    confSpin->setSuffix(QStringLiteral("%"));
    confSpin->setValue(95.0);

    QComboBox *modeCombo = new QComboBox(&dlg);
    modeCombo->addItem(tr("Residual Resampling (Homoscedastic / Weighted)"));
    modeCombo->addItem(tr("Wild Bootstrap (Heteroscedastic Rademacher)"));

    fl->addRow(tr("Bootstrap Replicates:"), samplesSpin);
    fl->addRow(tr("Confidence Level:"), confSpin);
    fl->addRow(tr("Resampling Mode:"), modeCombo);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    fl->addRow(btns);

    if (dlg.exec() != QDialog::Accepted || !self || !fitterGuard || !d_fitter)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    Fit::BootstrapResult boot = d_fitter->runBootstrapCI(
            samplesSpin->value(), confSpin->value() / 100.0, modeCombo->currentIndex());
    QApplication::restoreOverrideCursor();

    if (!self)
        return;

    if (!boot.valid) {
        QMessageBox::warning(parentWidget(), tr("Bootstrap Confidence Intervals"),
                             tr("Bootstrap resampling failed to converge on enough replicates."));
        return;
    }

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    const int prec = qMax(1, boxPrecision ? boxPrecision->value() : 6);
    if (app && app->writeFitResultsToLog) {
        QString msg = QString("[%1 - %2 (%3 %4, %5% CI)]\n")
                              .arg(tr("Bootstrap CI"), boxCurve->currentText(),
                                   QString::number(boot.nSuccess), tr("replicates"),
                                   QLocale().toString(confSpin->value(), 'f', 1));
        for (int i = 0; i < boxParams->rowCount() && i < boot.meanParams.size(); ++i) {
            QString pName = boxParams->item(i, 0) ? boxParams->item(i, 0)->text()
                                                  : QString("p%1").arg(i + 1);
            msg += QString("  %1: %2 = %3, %4 = %5, CI = [%6, %7]\n")
                           .arg(pName, tr("Mean"), QLocale().toString(boot.meanParams[i], 'g', prec),
                                tr("SE"), QLocale().toString(boot.sdParams[i], 'g', prec),
                                QLocale().toString(boot.ciLow[i], 'g', prec),
                                QLocale().toString(boot.ciHigh[i], 'g', prec));
        }
        app->updateLog(msg);
    }

    // Always display bootstrap intervals directly to the user even if Results Log output is off.
    if (!ApplicationWindow::isNonInteractiveContext(this)) {
        QDialog resDlg(parentWidget());
        resDlg.setWindowTitle(tr("Bootstrap Confidence Intervals"));
        resDlg.setMinimumSize(460, 260);
        QVBoxLayout *rvl = new QVBoxLayout(&resDlg);
        QLabel *hdr = new QLabel(
                tr("Curve: %1 | Successful replicates: %2 | Confidence level: %3%")
                        .arg(boxCurve->currentText(), QString::number(boot.nSuccess),
                             QLocale().toString(confSpin->value(), 'f', 1)),
                &resDlg);
        rvl->addWidget(hdr);

        const int nRows = qMin(boxParams->rowCount(), int(boot.meanParams.size()));
        QTableWidget *tbl = new QTableWidget(nRows, 5, &resDlg);
        tbl->setHorizontalHeaderLabels(
                { tr("Parameter"), tr("Mean"), tr("SE"), tr("CI Low"), tr("CI High") });
        tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (int i = 0; i < nRows; ++i) {
            const QString pName =
                    boxParams->item(i, 0) ? boxParams->item(i, 0)->text() : QString("p%1").arg(i + 1);
            tbl->setItem(i, 0, new QTableWidgetItem(pName));
            tbl->setItem(i, 1, new QTableWidgetItem(QLocale().toString(boot.meanParams[i], 'g', prec)));
            tbl->setItem(i, 2, new QTableWidgetItem(QLocale().toString(boot.sdParams[i], 'g', prec)));
            tbl->setItem(i, 3, new QTableWidgetItem(QLocale().toString(boot.ciLow[i], 'g', prec)));
            tbl->setItem(i, 4, new QTableWidgetItem(QLocale().toString(boot.ciHigh[i], 'g', prec)));
        }
        rvl->addWidget(tbl, 1);

        QDialogButtonBox *okBox = new QDialogButtonBox(QDialogButtonBox::Ok, &resDlg);
        connect(okBox, &QDialogButtonBox::accepted, &resDlg, &QDialog::accept);
        rvl->addWidget(okBox);
        resDlg.exec();
        if (!self)
            return;
    }
}

void FitDialog::clearPreviewCurve()
{
    if (d_preview_curve) {
        d_preview_curve->detach();
        delete d_preview_curve;
        d_preview_curve = nullptr;
        if (d_graph && d_graph->plotWidget())
            d_graph->replot();
    }
}

void FitDialog::updateLivePreview()
{
    if (!chkLivePreview || !chkLivePreview->isChecked() || !d_graph || !d_graph->plotWidget()
        || !boxCurve || boxCurve->currentText().isEmpty()) {
        clearPreviewCurve();
        return;
    }

    const QString curveName = boxCurve->currentText();
    QwtPlotCurve *targetCurve = d_graph->curve(curveName);
    if (!targetCurve) {
        clearPreviewCurve();
        return;
    }

    const int rows = boxParams ? boxParams->rowCount() : 0;
    if (rows <= 0) {
        clearPreviewCurve();
        return;
    }

    QStringList paramNames;
    std::vector<double> initVals(static_cast<size_t>(rows), 1.0);
    for (int i = 0; i < rows; ++i) {
        if (!boxParams->item(i, 0) || !boxParams->item(i, 1)) {
            clearPreviewCurve();
            return;
        }
        paramNames << boxParams->item(i, 0)->text();
        const QString valTxt = boxParams->item(i, 1)->text().trimmed();
        if (valTxt.isEmpty()) {
            clearPreviewCurve();
            return;
        }
        double v = 0.0;
        if (!evalScalarExpr(valTxt, &v)) {
            clearPreviewCurve();
            return;
        }
        initVals[static_cast<size_t>(i)] = v;
    }

    QString formula = expandUserFunctions(boxFunction->toPlainText(), d_user_function_names,
                                          d_user_functions);
    for (int i = 0; i < d_built_in_function_names.count() && i < d_built_in_functions.count(); ++i) {
        const QString &biName = d_built_in_function_names[i];
        if (biName.isEmpty())
            continue;
        QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(biName)
                              + QStringLiteral("\\b"));
        if (re.match(formula).hasMatch()) {
            formula.prepend(QString("%1=%2\n").arg(biName, d_built_in_functions[i]));
        }
    }
    if (formula.trimmed().isEmpty()) {
        clearPreviewCurve();
        return;
    }

    ApplicationWindow *app = dynamic_cast<ApplicationWindow *>(this->parent());
    if (!app)
        return;
    QPointer<FitDialog> self(this);

    NonLinearFit previewFit(app, d_graph);
    previewFit.setSilent(true);
    previewFit.setParametersList(paramNames);
    previewFit.setFormula(formula);

    double xStart = 0.0, xEnd = 0.0;
    const bool okFrom = boxFrom && evalScalarExpr(boxFrom->text(), &xStart);
    const bool okTo = boxTo && evalScalarExpr(boxTo->text(), &xEnd);

    const bool dataOk = (okFrom && okTo && xStart < xEnd)
                                ? previewFit.setDataFromCurve(curveName, xStart, xEnd)
                                : previewFit.setDataFromCurve(curveName);
    if (!self || !d_graph)
        return;
    if (!dataOk) {
        clearPreviewCurve();
        return;
    }

    for (int i = 0; i < rows; ++i) {
        previewFit.setInitialGuess(i, initVals[static_cast<size_t>(i)]);
        QCheckBox *cb = qobject_cast<QCheckBox *>(boxParams->cellWidget(i, 2));
        if (cb && cb->isChecked())
            previewFit.setParameterFixed(i, true);
    }
    if (boxLinearConstraints && !boxLinearConstraints->text().trimmed().isEmpty())
        previewFit.setLinearConstraintExpr(boxLinearConstraints->text().trimmed());

    std::vector<double> X, Y;
    const bool previewOk = previewFit.evaluatePreviewCurve(X, Y, 200);
    if (!self || !d_graph)
        return;
    if (!previewOk || X.size() < 2) {
        clearPreviewCurve();
        return;
    }

    if (!d_preview_curve) {
        d_preview_curve = new FitPreviewCurveItem(&d_preview_curve, tr("InitialGuessPreview"));
        d_preview_curve->attach(d_graph->plotWidget());
    }
    d_preview_curve->setAxis(targetCurve->xAxis(), targetCurve->yAxis());
    QColor previewCol = btnColor ? btnColor->color() : QColor(30, 100, 220);
    previewCol.setAlpha(200);
    d_preview_curve->setPen(QPen(previewCol, 2, Qt::DashLine));
    d_preview_curve->setData(X.data(), Y.data(), static_cast<int>(X.size()));
    d_graph->replot();
}

void FitDialog::showPointsBox(bool)
{
    if (generatePointsBtn->isChecked()) {
        lblPoints->show();
        generatePointsBox->show();
    } else {
        lblPoints->hide();
        generatePointsBox->hide();
    }
}

void FitDialog::setGraph(Graph *g)
{
    if (!g)
        return;

    if (d_graph)
        disconnect(d_graph, nullptr, this, nullptr);

    d_graph = g;
    boxCurve->clear();
    boxCurve->addItems(d_graph->analysableCurvesList());

    QString selectedCurve = g->selectedCurveTitle();
    if (!selectedCurve.isEmpty()) {
        int index = boxCurve->findText(selectedCurve);
        boxCurve->setCurrentIndex(index);
    }
    activateCurve(boxCurve->currentText());

    if (!d_graph)
        return;
    connect(d_graph, SIGNAL(closedGraph()), this, SLOT(close()));
    connect(d_graph, SIGNAL(dataRangeChanged()), this, SLOT(changeDataRange()));
}

void FitDialog::activateCurve(const QString &curveName)
{
    if (!d_graph)
        return;
    QwtPlotCurve *c = d_graph->curve(curveName);
    if (!c)
        return;

    double start, end;
    d_graph->range(d_graph->curveIndex(curveName), start, end);
    boxFrom->setText(formatLocaleScalar(qMin(start, end), 'g', 15));
    boxTo->setText(formatLocaleScalar(qMax(start, end), 'g', 15));
    updateLivePreview();
}

void FitDialog::saveUserFunction()
{
    QPointer<FitDialog> self(this);
    QPointer<QWidget> msgParent(parentWidget());
    if (editBox->toPlainText().isEmpty()) {
        QMessageBox::critical(msgParent.data(), tr("Input function error"),
                              tr("Please enter a valid function!"));
        if (!self)
            return;
        editBox->setFocus();
        return;
    } else if (boxName->text().isEmpty()) {
        QMessageBox::critical(msgParent.data(), tr("Input function error"),
                              tr("Please enter a function name!"));
        if (!self)
            return;
        boxName->setFocus();
        return;
    } else if (boxParam->text().remove(QRegExp("[,;\\s]")).isEmpty()) {
        QMessageBox::critical(msgParent.data(), tr("Input function error"),
                              tr("Please enter at least one parameter name!"));
        if (!self)
            return;
        boxParam->setFocus();
        return;
    }

    if (d_built_in_function_names.contains(boxName->text())) {
        QMessageBox::critical(msgParent.data(), tr("Error: function name"),
                              "<p><b>" + boxName->text() + "</b>"
                                      + tr(" is a built-in function name"
                                           "<p>You must choose another name for your function!"));
        if (!self)
            return;
        editBox->setFocus();
        return;
    }
    QRegularExpression selfRe(QStringLiteral("\\b") + QRegularExpression::escape(boxName->text())
                              + QStringLiteral("\\b"));
    if (selfRe.match(editBox->toPlainText()).hasMatch()) {
        QMessageBox::critical(msgParent.data(), tr("Input function error"),
                              tr("You can't define functions recursively!"));
        if (!self)
            return;
        editBox->setFocus();
        return;
    }

    QString name = boxName->text();
    QString f = name + "(x, " + boxParam->text() + ")=" + editBox->toPlainText().remove("\n");

    if (d_user_function_names.contains(name)) {
        int index = d_user_function_names.indexOf(name);
        d_user_functions[index] = f;
        d_user_function_params[index] = boxParam->text();

        if (funcBox->currentItem() && funcBox->currentItem()->text() == name)
            showExpression(index);
    } else {
        d_user_function_names << name;
        d_user_functions << f;
        d_user_function_params << boxParam->text();

        if (categoryBox->currentRow() == 0) {
            funcBox->addItem(name);
            funcBox->setCurrentRow(funcBox->count() - 1);
        }

        if (d_user_function_names.count() > 0 && !boxUseBuiltIn->isEnabled()
            && categoryBox->currentRow() == 0)
            boxUseBuiltIn->setEnabled(true);
    }
    buttonClearUsrList->setEnabled(true);
    emit saveFunctionsList(d_user_functions);
}

void FitDialog::removeUserFunction()
{
    if (!funcBox->currentItem())
        return;
    QString name = funcBox->currentItem()->text();
    if (d_user_function_names.contains(name)) {
        explainBox->setText(QString());

        // C4-#9: remove by position. removeAll() removed by value, so a parameter string
        // shared with another function (the default "a, b") vanished for both and the three
        // parallel lists went out of step.
        const int index = d_user_function_names.indexOf(name);
        if (index >= 0 && index < d_user_function_names.size())
            d_user_function_names.removeAt(index);
        if (index >= 0 && index < d_user_functions.size())
            d_user_functions.removeAt(index);
        if (index >= 0 && index < d_user_function_params.size())
            d_user_function_params.removeAt(index);

        funcBox->clear();
        funcBox->addItems(d_user_function_names);
        funcBox->setCurrentRow(0);

        if (!d_user_function_names.count()) {
            boxUseBuiltIn->setEnabled(false);
            buttonClearUsrList->setEnabled(false);
        }

        emit saveFunctionsList(d_user_functions);
    }
}

void FitDialog::showFitPage()
{
    boxParams->blockSignals(true);
    int param_table_rows = boxParams->rowCount();

    QString par = boxParam->text().simplified();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList paramList = par.split(QRegExp("[,;]+[\\s]*"), Qt::SkipEmptyParts);
#else
    QStringList paramList = par.split(QRegExp("[,;]+[\\s]*"), QString::SkipEmptyParts);
#endif
    int parameters = paramList.count();
    boxParams->setRowCount(parameters);
    boxParams->showColumn(2);
    boxParams->showColumn(3);
    boxParams->showColumn(4);

    const int minVisRows = qBound(2, parameters, 4);
    const int hdrH = qMax(22, boxParams->horizontalHeader()->height());
    boxParams->setMinimumHeight(4 + (minVisRows + 1) * hdrH);

    for (int i = param_table_rows; i < paramList.count(); i++) {
        QTableWidgetItem *it = new QTableWidgetItem(paramList[i]);
        it->setFlags(Qt::ItemFlags(Qt::NoItemFlags));
        it->setBackground(QBrush(Qt::lightGray));
        it->setForeground(QBrush(Qt::darkRed));
        QFont font = it->font();
        font.setBold(true);
        it->setFont(font);
        boxParams->setItem(i, 0, it);

        it = new QTableWidgetItem(formatLocaleScalar(1.0, 'f', boxPrecision->value()));
        it->setTextAlignment(Qt::AlignRight);
        boxParams->setItem(i, 1, it);

        QTableWidgetItem *itFix = new QTableWidgetItem();
        itFix->setFlags(Qt::ItemFlags(Qt::NoItemFlags));
        itFix->setBackground(QBrush(Qt::lightGray));
        boxParams->setItem(i, 2, itFix);
        boxParams->setCellWidget(i, 2, new QCheckBox());

        QTableWidgetItem *itLo = new QTableWidgetItem(QString());
        itLo->setTextAlignment(Qt::AlignRight);
        itLo->setToolTip(tr("Optional lower bound (leave blank for -Inf)"));
        boxParams->setItem(i, 3, itLo);

        QTableWidgetItem *itHi = new QTableWidgetItem(QString());
        itHi->setTextAlignment(Qt::AlignRight);
        itHi->setToolTip(tr("Optional upper bound (leave blank for +Inf)"));
        boxParams->setItem(i, 4, itHi);
    }
    for (int i = 0; i < paramList.count(); i++) {
        boxParams->item(i, 0)->setText(paramList[i]);
        if (!boxParams->cellWidget(i, 2)) {
            QTableWidgetItem *itFix = new QTableWidgetItem();
            itFix->setFlags(Qt::ItemFlags(Qt::NoItemFlags));
            itFix->setBackground(QBrush(Qt::lightGray));
            boxParams->setItem(i, 2, itFix);
            boxParams->setCellWidget(i, 2, new QCheckBox());
        }
        if (!boxParams->item(i, 3)) {
            QTableWidgetItem *itLo = new QTableWidgetItem(QString());
            itLo->setTextAlignment(Qt::AlignRight);
            boxParams->setItem(i, 3, itLo);
        }
        if (!boxParams->item(i, 4)) {
            QTableWidgetItem *itHi = new QTableWidgetItem(QString());
            itHi->setTextAlignment(Qt::AlignRight);
            boxParams->setItem(i, 4, itHi);
        }
    }

    QPointer<FitDialog> self(this);
    // Auto-populate smart initial parameter guesses from the active curve
    if (d_graph && boxCurve && !boxCurve->currentText().isEmpty() && !paramList.isEmpty()) {
        ApplicationWindow *app = (ApplicationWindow *)this->parent();
        QString fnName = boxName->text().trimmed();
        if ((categoryBox->currentRow() == 1 || categoryBox->currentRow() >= 4)
            && funcBox->currentItem())
            fnName = funcBox->currentItem()->text().trimmed();

        NonLinearFit tmpFit(app, d_graph);
        tmpFit.setSilent(true);
        tmpFit.setParametersList(paramList);
        tmpFit.setFormula(editBox->toPlainText().simplified());
        // C4-4: guess from the same X range the fit will use (From/To boxes), not the
        // whole curve, so a peak picked inside a sub-range gets sensible start values.
        double xStart = 0.0, xEnd = 0.0;
        const bool okFrom = boxFrom && evalScalarExpr(boxFrom->text(), &xStart);
        const bool okTo = boxTo && evalScalarExpr(boxTo->text(), &xEnd);
        const bool rangeOk = okFrom && okTo && xStart < xEnd;
        const bool guessDataOk = rangeOk
                ? tmpFit.setDataFromCurve(boxCurve->currentText(), xStart, xEnd)
                : tmpFit.setDataFromCurve(boxCurve->currentText());
        if (!self)
            return;
        if (guessDataOk) {
            tmpFit.guessBuiltInParameters(fnName);
            for (int i = 0; i < paramList.count() && i < tmpFit.numParameters(); ++i) {
                QCheckBox *cb = qobject_cast<QCheckBox *>(boxParams->cellWidget(i, 2));
                if (cb && cb->isChecked())
                    continue;
                double val = tmpFit.initialGuess(i);
                if (std::isfinite(val)) {
                    boxParams->item(i, 1)->setText(
                            formatLocaleScalar(val, 'g', boxPrecision->value()));
                }
            }
        }
    }
    if (!self)
        return;

    boxFunction->setText(editBox->toPlainText().simplified());
    lblFunction->setText(boxName->text() + " (x, " + par + ")");
    boxParams->blockSignals(false);

    tw->setCurrentWidget(fitPage);
    updateLivePreview();
}

void FitDialog::showEditPage()
{
    clearPreviewCurve();
    tw->setCurrentWidget(editPage);
}

void FitDialog::showAdvancedPage()
{
    tw->setCurrentWidget(advancedPage);
}

void FitDialog::setFunction(bool ok)
{
    editBox->setEnabled(!ok);
    boxParam->setEnabled(!ok);
    boxName->setEnabled(!ok);
    btnAddFunc->setEnabled(!ok);
    btnAddName->setEnabled(!ok);
    btnAddTxt->setEnabled(!ok);
    buttonClear->setEnabled(!ok);

    // funcBox has no current item while a category list is being rebuilt (the built-in
    // categories check boxUseBuiltIn before selecting a row), so guard the dereference.
    if (ok && funcBox->currentItem()) {
        boxName->setText(funcBox->currentItem()->text());
        editBox->setText(explainBox->toPlainText());

        if (categoryBox->currentRow() == 0 && funcBox->currentRow() >= 0
            && funcBox->currentRow() < d_user_function_params.size())
            boxParam->setText(d_user_function_params[funcBox->currentRow()]);
        else if (categoryBox->currentRow() == 1 || categoryBox->currentRow() >= 4) {
            QString fn = funcBox->currentItem() ? funcBox->currentItem()->text() : QString();
            QStringList lst;
            if ((fn == "Gauss" || fn == "Lorentz" || fn == "PseudoVoigt" || fn == "PearsonVII"
                 || fn == "Voigt" || fn == "DoniachSunjic")
                && polynomOrderBox->isVisible()) {
                lst = MultiPeakFit::generateParameterList(polynomOrderBox->value());
            } else if (fn == "GaussAmp" && polynomOrderBox->isVisible()
                       && polynomOrderBox->value() > 1) {
                lst = MultiPeakFit::generateParameterList(polynomOrderBox->value());
            } else if (fn == "Polynomial") {
                lst = PolynomialFit::generateParameterList(polynomOrderBox->value());
            } else {
                lst = NonLinearFit::builtInModelParameters(fn);
                if (lst.isEmpty())
                    lst << "A" << "xc" << "w" << "y0";
            }
            boxParam->setText(lst.join(", "));
        } else if (categoryBox->currentRow() == 3 && d_plugin_params.size() > 0)
            boxParam->setText(d_plugin_params[funcBox->currentRow()]);
    }
}

void FitDialog::clearUserFunctions()
{
    d_user_functions.clear();
    d_user_function_names.clear();
    d_user_function_params.clear();
    if (categoryBox->currentRow() == 0) {
        funcBox->clear();
        explainBox->clear();
        boxUseBuiltIn->setEnabled(false);
        buttonClearUsrList->setEnabled(false);
    }
    emit clearFunctionsList();
}

void FitDialog::insertFunctionsList(const QStringList &list)
{
    addUserFunctions(list);
}

void FitDialog::addUserFunctions(const QStringList &list)
{
    if (!list.count()) {
        boxUseBuiltIn->setEnabled(false);
        return;
    }

    for (int i = 0; i < list.count(); i++) {
        QString s = list.at(i).simplified();
        if (!s.isEmpty()) {
            d_user_functions << s;

            int pos1 = s.indexOf("(", 0);
            d_user_function_names << s.left(pos1);

            int pos2 = s.indexOf(")", pos1);
            d_user_function_params << s.mid(pos1 + 4, pos2 - pos1 - 4);
        }
    }
}

void FitDialog::showFunctionsList(int category)
{
    boxUseBuiltIn->setChecked(false);
    boxUseBuiltIn->setEnabled(false);
    boxUseBuiltIn->hide();
    buttonPlugins->hide();
    buttonClearUsrList->hide();
    buttonClearUsrList->setEnabled(false);
    btnDelFunc->setEnabled(false);
    funcBox->blockSignals(true);
    funcBox->clear();
    explainBox->clear();
    polynomOrderLabel->hide();
    polynomOrderBox->hide();

    switch (category) {
    case 0:
        if (d_user_function_names.count() > 0) {
            showUserFunctions();
            buttonClearUsrList->show();
            boxUseBuiltIn->setEnabled(true);
            buttonClearUsrList->setEnabled(true);
        }

        boxUseBuiltIn->setText(tr("Fit with selected &user function"));
        boxUseBuiltIn->show();
        buttonClearUsrList->show();
        btnDelFunc->setEnabled(true);
        break;

    case 1:
        // C4-4: selecting a model applies it right away (as in the model categories);
        // unticking the box is still possible to edit the expression by hand.
        boxUseBuiltIn->setText(tr("Fit using &built-in function"));
        boxUseBuiltIn->show();
        boxUseBuiltIn->setEnabled(true);
        for (const QString &fn : d_built_in_function_names) {
            if (!isRedundantBuiltInModel(fn))
                funcBox->addItem(fn);
        }
        boxUseBuiltIn->setChecked(true);
        break;

    case 2:
        showParseFunctions();
        break;

    case 3:
        buttonPlugins->show();
        boxUseBuiltIn->setText(tr("Fit using &plugin function"));
        boxUseBuiltIn->show();
        if (d_plugin_function_names.size() > 0) {
            funcBox->addItems(d_plugin_function_names);
            boxUseBuiltIn->setEnabled(true);
        }
        break;

    default: {
        static const char *const kCats[] = {
            "Peak Functions",
            "Exponential",
            "Sigmoidal / Growth",
            "Materials / Metallurgy / Physics",
            "Waveform / Periodic",
            "Power / Logarithmic / Rational / Polynomial"
        };
        int catIdx = category - 4;
        if (catIdx >= 0 && catIdx < 6) {
            QString targetCat = QString::fromLatin1(kCats[catIdx]);
            for (const QString &mName : NonLinearFit::builtInModelNames()) {
                if (isRedundantBuiltInModel(mName))
                    continue;
                if (NonLinearFit::builtInModelCategory(mName) == targetCat)
                    funcBox->addItem(mName);
            }
            boxUseBuiltIn->setText(tr("Fit using &built-in function"));
            boxUseBuiltIn->show();
            boxUseBuiltIn->setEnabled(true);
            boxUseBuiltIn->setChecked(true);
        }
        break;
    }
    }
    funcBox->blockSignals(false);
    funcBox->setCurrentRow(0);
}

bool FitDialog::jumpToModel(const QString &entry)
{
    QString name = entry.section(QStringLiteral(" \u2014 "), 0, 0).trimmed();
    name = name.section(QStringLiteral(" ["), 0, 0).trimmed();
    if (name.isEmpty())
        return false;
    // Case-insensitive match against the built-in Origin model names
    QString canonical;
    for (const QString &mName : NonLinearFit::builtInModelNames()) {
        if (mName.compare(name, Qt::CaseInsensitive) == 0) {
            canonical = mName;
            break;
        }
    }
    if (canonical.isEmpty())
        return false;
    canonical = canonicalDialogModelName(canonical);

    static const char *const kCats[] = { "Peak Functions",
                                         "Exponential",
                                         "Sigmoidal / Growth",
                                         "Materials / Metallurgy / Physics",
                                         "Waveform / Periodic",
                                         "Power / Logarithmic / Rational / Polynomial" };
    const QString cat = NonLinearFit::builtInModelCategory(canonical);
    int row = -1;
    for (int k = 0; k < 6; ++k) {
        if (cat == QString::fromLatin1(kCats[k])) {
            row = 4 + k;
            break;
        }
    }
    if (row < 0)
        return false;
    if (categoryBox->currentRow() != row)
        categoryBox->setCurrentRow(row);
    const QList<QListWidgetItem *> hits = funcBox->findItems(canonical, Qt::MatchExactly);
    if (hits.isEmpty())
        return false;
    funcBox->setCurrentItem(hits.first());
    if (modelSearchBox) {
        modelSearchBox->blockSignals(true);
        modelSearchBox->clear();
        modelSearchBox->blockSignals(false);
    }
    funcBox->setFocus();
    return true;
}

QStringList FitDialog::recentModels()
{
    QStringList list = ApplicationWindow::getSettings()
                               .value(QStringLiteral("/Fit/RecentModels"))
                               .toStringList();
    list.removeAll(QString());
    while (list.size() > kMaxRecentModels)
        list.removeLast();
    return list;
}

void FitDialog::recordRecentModel(const QString &modelName)
{
    const QString name = modelName.trimmed();
    if (name.isEmpty())
        return;
    QStringList list = recentModels();
    list.removeAll(name);
    list.prepend(name);
    while (list.size() > kMaxRecentModels)
        list.removeLast();
    ApplicationWindow::getSettings().setValue(QStringLiteral("/Fit/RecentModels"), list);
}

void FitDialog::clearRecentModels()
{
    ApplicationWindow::getSettings().remove(QStringLiteral("/Fit/RecentModels"));
}

void FitDialog::refreshRecentModelsBox()
{
    if (!recentModelBox)
        return;
    registerFitDialogJapaneseStrings();
    const QStringList list = recentModels();
    QSignalBlocker blocker(recentModelBox);
    recentModelBox->clear();
    if (list.isEmpty()) {
        recentModelBox->addItem(tr("(none yet)"));
        recentModelBox->setEnabled(false);
        return;
    }
    recentModelBox->addItem(tr("Choose a recent model"));
    for (const QString &name : list)
        recentModelBox->addItem(name, name);
    recentModelBox->setCurrentIndex(0);
    recentModelBox->setEnabled(true);
}

bool FitDialog::selectRecentModel(const QString &modelName)
{
    const QString name = modelName.trimmed();
    if (name.isEmpty())
        return false;
    if (jumpToModel(name))
        return true;
    // legacy built-in functions (Boltzmann, ExpDecay1, ...) live in the "Built-in" category
    if (!d_built_in_function_names.contains(name))
        return false;
    if (categoryBox->currentRow() != 1)
        categoryBox->setCurrentRow(1);
    const QList<QListWidgetItem *> hits = funcBox->findItems(name, Qt::MatchExactly);
    if (hits.isEmpty())
        return false;
    funcBox->setCurrentItem(hits.first());
    boxUseBuiltIn->setChecked(true);
    funcBox->setFocus();
    return true;
}

void FitDialog::choosePluginsFolder()
{
    QPointer<FitDialog> self(this);
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (!app)
        return;
    QPointer<QWidget> dlgParent(parentWidget());
    QString dir = QFileDialog::getExistingDirectory(dlgParent.data(), tr("Choose the plugins folder"),
                                                    QDir::currentPath(), QFileDialog::ShowDirsOnly);
    if (!self || dir.isEmpty())
        return;
    {
        d_plugin_files_list.clear();
        d_plugin_function_names.clear();
        d_plugin_functions.clear();
        d_plugin_params.clear();
        funcBox->clear();
        explainBox->clear();

        app->fitPluginsPath = dir;
        loadPlugins();
        if (d_plugin_function_names.size() > 0) {
            funcBox->addItems(d_plugin_function_names);
            if (!boxUseBuiltIn->isEnabled())
                boxUseBuiltIn->setEnabled(true);

            funcBox->setCurrentRow(0);
        } else
            boxUseBuiltIn->setEnabled(false);
    }
}

void FitDialog::loadPlugins()
{
    typedef char *(*fitFunc)();

    ApplicationWindow *app = (ApplicationWindow *)this->parent();
    QString path = app->fitPluginsPath + "/";
    QDir dir(path);
    QStringList lst = dir.entryList(QDir::Files | QDir::NoSymLinks);

    for (int i = 0; i < lst.count(); i++) {
        QLibrary lib(path + lst[i]);

        fitFunc name = (fitFunc)lib.resolve("name");
        fitFunc function = (fitFunc)lib.resolve("function");
        fitFunc params = (fitFunc)lib.resolve("parameters");

        if (name && function && params) {
            d_plugin_function_names << QString(name());
            d_plugin_functions << QString(function());
            d_plugin_params << QString(params());
            d_plugin_files_list << lib.fileName();
        }
    }
}

void FitDialog::showUserFunctions()
{
    funcBox->addItems(d_user_function_names);
}

void FitDialog::setBuiltInFunctionNames()
{
    d_built_in_function_names << "Boltzmann"
                              << "ExpGrowth"
                              << "ExpDecay1"
                              << "ExpDecay2"
                              << "ExpDecay3"
                              << "GaussAmp"
                              << "Gauss"
                              << "Lorentz"
                              << "Polynomial"
                              << "PseudoVoigt"
                              << "PearsonVII"
                              << "VoigtApprox"
                              << "LogNormal";
    for (const QString &mName : NonLinearFit::builtInModelNames()) {
        if (!d_built_in_function_names.contains(mName))
            d_built_in_function_names << mName;
    }
}

void FitDialog::setBuiltInFunctions()
{
    d_built_in_functions.clear();
    for (const QString &fn : d_built_in_function_names) {
        if (fn == "Polynomial")
            d_built_in_functions << "a0+a1*x+a2*x^2";
        else if (fn == "PseudoVoigt")
            d_built_in_functions << "y0+A*(0.5*2/PI*w/(4*(x-xc)^2+w^2)+0.5*sqrt(4*ln(2)/PI)/w*exp(-4*ln(2)*((x-xc)/w)^2))";
        else if (fn == "PearsonVII")
            d_built_in_functions << "y0+A*(1+(sqrt(2)-1)*(2*(x-xc)/w)^2)^(-2)";
        else
            d_built_in_functions << NonLinearFit::builtInModelFormula(fn);
    }
}

void FitDialog::showParseFunctions()
{
    funcBox->addItems(MyParser::functionsList());
}

void FitDialog::showExpression(int function)
{
    if (function < 0)
        return;

    if (categoryBox->currentRow() == 2) {
        explainBox->setText(MyParser::explainFunction(function));
    } else if (categoryBox->currentRow() == 1 || categoryBox->currentRow() >= 4) {
        QString fn = funcBox->currentItem() ? funcBox->currentItem()->text() : QString();
        polynomOrderLabel->hide();
        polynomOrderBox->hide();

        if (fn == "GaussAmp") {
            polynomOrderLabel->setText(tr("Peaks"));
            polynomOrderLabel->show();
            polynomOrderBox->show();
            if (polynomOrderBox->value() > 1)
                explainBox->setText(MultiPeakFit::generateFormula(polynomOrderBox->value(),
                                                                  MultiPeakFit::GaussAmp));
            else
                explainBox->setText(NonLinearFit::builtInModelFormula("GaussAmp"));
        } else if (fn == "Gauss") {
            polynomOrderLabel->setText(tr("Peaks"));
            polynomOrderLabel->show();
            polynomOrderBox->show();
            explainBox->setText(
                    MultiPeakFit::generateFormula(polynomOrderBox->value(), MultiPeakFit::Gauss));
        } else if (fn == "Lorentz") {
            polynomOrderLabel->setText(tr("Peaks"));
            polynomOrderLabel->show();
            polynomOrderBox->show();
            explainBox->setText(
                    MultiPeakFit::generateFormula(polynomOrderBox->value(), MultiPeakFit::Lorentz));
        } else if (fn == "Polynomial") {
            polynomOrderLabel->setText(tr("Polynomial Order"));
            polynomOrderLabel->show();
            polynomOrderBox->show();
            explainBox->setText(PolynomialFit::generateFormula(polynomOrderBox->value()));
        } else if (fn == "PseudoVoigt") {
            polynomOrderLabel->setText(tr("Peaks"));
            polynomOrderLabel->show();
            polynomOrderBox->show();
            explainBox->setText(MultiPeakFit::generateFormula(polynomOrderBox->value(),
                                                              MultiPeakFit::PseudoVoigt));
        } else if (fn == "PearsonVII" && categoryBox->currentRow() == 1) {
            polynomOrderLabel->setText(tr("Peaks"));
            polynomOrderLabel->show();
            polynomOrderBox->show();
            explainBox->setText(MultiPeakFit::generateFormula(polynomOrderBox->value(),
                                                              MultiPeakFit::PearsonVII));
        } else {
            polynomOrderBox->setValue(1);
            QString expr = NonLinearFit::builtInModelFormula(fn);
            const int fnIdx = d_built_in_function_names.indexOf(fn);
            if (expr.isEmpty() && fnIdx >= 0 && fnIdx < d_built_in_functions.size())
                expr = d_built_in_functions[fnIdx];
            explainBox->setText(expr);
            QString desc = NonLinearFit::builtInModelDescription(fn);
            if (!desc.isEmpty())
                explainBox->setToolTip(desc);
        }
        setFunction(boxUseBuiltIn->isChecked());
    } else if (categoryBox->currentRow() == 0) {
        if (d_user_functions.size() > function) {
            const int eqPos = d_user_functions[function].indexOf('=');
            explainBox->setText(eqPos >= 0 ? d_user_functions[function].mid(eqPos + 1)
                                           : d_user_functions[function]);
        } else
            explainBox->clear();
        setFunction(boxUseBuiltIn->isChecked());
    } else if (categoryBox->currentRow() == 3) {
        if (d_plugin_functions.size() > 0) {
            explainBox->setText(d_plugin_functions[function]);
            setFunction(boxUseBuiltIn->isChecked());
        } else
            explainBox->clear();
    }
}

void FitDialog::pasteExpression()
{
    QString f = explainBox->toPlainText();
    if (categoryBox->currentRow() == 2) { // basic parser function
        f = f.left(f.indexOf("(", 0) + 1);
        if (editBox->textCursor().hasSelection()) {
            QString markedText = editBox->textCursor().selectedText();
            editBox->insertPlainText(f + markedText + ")");
        } else
            editBox->insertPlainText(f + ")");
    } else
        editBox->insertPlainText(f);

    editBox->setFocus();
}

void FitDialog::pasteFunctionName()
{
    if (!funcBox->currentItem())
        return;
    editBox->insertPlainText(funcBox->currentItem()->text());
    editBox->setFocus();
}

void FitDialog::accept()
{
    if (!d_graph || d_accept_depth > 0)
        return;
    QPointer<FitDialog> self(this);
    struct AcceptDepthGuard {
        QPointer<FitDialog> dlg;
        QPointer<Fit> *fitterSlot = nullptr;
        explicit AcceptDepthGuard(FitDialog *d, QPointer<Fit> *slot) : dlg(d), fitterSlot(slot)
        {
            if (dlg) {
                if (dlg->d_accept_depth == 0 && fitterSlot)
                    *fitterSlot = dlg->d_fitter;
                ++dlg->d_accept_depth;
            }
        }
        ~AcceptDepthGuard()
        {
            if (dlg) {
                --dlg->d_accept_depth;
            } else if (fitterSlot && *fitterSlot) {
                delete fitterSlot->data();
                *fitterSlot = nullptr;
            }
        }
    };
    QPointer<Fit> activeFitter;
    AcceptDepthGuard acceptGuard(this, &activeFitter);
    QPointer<QWidget> msgParent(parentWidget());

    QString curve = boxCurve->currentText();
    QStringList curvesList = d_graph->analysableCurvesList();
    if (curve.isEmpty() || (!curvesList.contains(curve) && !d_graph->curvesList().contains(curve))) {
        QMessageBox::critical(
                msgParent.data(), tr("Warning"),
                tr("The curve <b> %1 </b> doesn't exist anymore! Operation aborted!").arg(curve));
        if (!self)
            return;
        boxCurve->clear();
        boxCurve->addItems(curvesList);
        return;
    }

    if (!validInitialValues() || !self || !d_graph)
        return;

    double start = 0.0, end = 0.0, eps = 0.0;
    QString errMsg;
    if (!evalScalarExpr(boxFrom->text().toLower(), &start, &errMsg)) {
        QMessageBox::critical(msgParent.data(), tr("Start limit error"), errMsg);
        if (!self)
            return;
        boxFrom->setFocus();
        return;
    }

    if (!evalScalarExpr(boxTo->text().toLower(), &end, &errMsg)) {
        QMessageBox::critical(msgParent.data(), tr("End limit error"), errMsg);
        if (!self)
            return;
        boxTo->setFocus();
        return;
    }

    if (start >= end) {
        QMessageBox::critical(msgParent.data(), tr("Input error"),
                              tr("Please enter x limits that satisfy: from < end!"));
        if (!self)
            return;
        boxTo->setFocus();
        return;
    }

    if (!evalScalarExpr(boxTolerance->text().toLower(), &eps, &errMsg)) {
        QMessageBox::critical(msgParent.data(), tr("Tolerance input error"), errMsg);
        if (!self)
            return;
        boxTolerance->setFocus();
        return;
    }

    if (eps <= 0.0 || eps >= 1.0) {
        QMessageBox::critical(msgParent.data(), tr("Tolerance input error"),
                              tr("The tolerance value must be positive and less than 1!"));
        if (!self)
            return;
        boxTolerance->setFocus();
        return;
    }

    const int rows = boxParams->rowCount();
    if (rows <= 0)
        return;

    QStringList parameters;
    std::vector<double> paramsInit(static_cast<size_t>(rows), 0.0);
    std::vector<double> loBounds(static_cast<size_t>(rows), -std::numeric_limits<double>::infinity());
    std::vector<double> hiBounds(static_cast<size_t>(rows), std::numeric_limits<double>::infinity());
    std::vector<bool> hasBounds(static_cast<size_t>(rows), false);

    QString formula = expandUserFunctions(boxFunction->toPlainText(), d_user_function_names,
                                          d_user_functions);

    // define variables for builtin functions used in formula
    for (int i = 0; i < d_built_in_function_names.count() && i < d_built_in_functions.count(); i++) {
        const QString &biName = d_built_in_function_names[i];
        if (biName.isEmpty())
            continue;
        QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(biName)
                              + QStringLiteral("\\b"));
        if (re.match(formula).hasMatch())
            formula.prepend(QString("%1=%2\n").arg(biName, d_built_in_functions[i]));
    }

    for (int i = 0; i < rows; i++) {
        QTableWidgetItem *nameItem = boxParams->item(i, 0);
        QTableWidgetItem *valItem = boxParams->item(i, 1);
        if (!nameItem || !valItem)
            return;

        double v = 0.0;
        if (!evalScalarExpr(valItem->text(), &v, &errMsg)) {
            QMessageBox::critical(msgParent.data(), tr("Initial guess error"), errMsg);
            if (!self)
                return;
            boxParams->setCurrentCell(i, 1);
            return;
        }
        paramsInit[static_cast<size_t>(i)] = v;
        parameters << nameItem->text();

        const QString loTxt = boxParams->item(i, 3) ? boxParams->item(i, 3)->text().trimmed() : QString();
        const QString hiTxt = boxParams->item(i, 4) ? boxParams->item(i, 4)->text().trimmed() : QString();
        if (!loTxt.isEmpty() || !hiTxt.isEmpty()) {
            double loVal = -std::numeric_limits<double>::infinity();
            double hiVal = std::numeric_limits<double>::infinity();
            if (!loTxt.isEmpty() && !evalScalarExpr(loTxt, &loVal, &errMsg)) {
                QMessageBox::critical(
                        msgParent.data(), tr("Bound input error"),
                        tr("Invalid lower bound for parameter '%1': %2").arg(nameItem->text(), errMsg));
                if (!self)
                    return;
                boxParams->setCurrentCell(i, 3);
                return;
            }
            if (!hiTxt.isEmpty() && !evalScalarExpr(hiTxt, &hiVal, &errMsg)) {
                QMessageBox::critical(
                        msgParent.data(), tr("Bound input error"),
                        tr("Invalid upper bound for parameter '%1': %2").arg(nameItem->text(), errMsg));
                if (!self)
                    return;
                boxParams->setCurrentCell(i, 4);
                return;
            }
            if (loVal > hiVal) {
                QMessageBox::critical(
                        msgParent.data(), tr("Bound input error"),
                        tr("Lower bound must be <= upper bound for parameter '%1'.")
                                .arg(nameItem->text()));
                if (!self)
                    return;
                boxParams->setCurrentCell(i, 3);
                return;
            }
            loBounds[static_cast<size_t>(i)] = loVal;
            hiBounds[static_cast<size_t>(i)] = hiVal;
            hasBounds[static_cast<size_t>(i)] = true;
        }
    }

    const int catRow = categoryBox ? categoryBox->currentRow() : -1;
    const bool useBuiltInCat = boxUseBuiltIn->isChecked() && (catRow == 1 || catRow >= 4);
    const bool usePluginCat = boxUseBuiltIn->isChecked() && catRow == 3;
    QString builtInFuncName;
    if (useBuiltInCat) {
        if (!funcBox->currentItem()) {
            QMessageBox::warning(msgParent.data(), tr("Input error"),
                                 tr("Please select a fit function."));
            return;
        }
        builtInFuncName = funcBox->currentItem()->text();
        auto checkTauNonZero = [&](int idx) -> bool {
            if (idx < rows && std::abs(paramsInit[static_cast<size_t>(idx)]) < 1e-300) {
                QMessageBox::critical(
                        msgParent.data(), tr("Initial guess error"),
                        tr("Initial time constant '%1' cannot be zero.")
                                .arg(parameters.value(idx)));
                if (self)
                    boxParams->setCurrentCell(idx, 1);
                return false;
            }
            return true;
        };
        if ((builtInFuncName == "ExpDecay1" || builtInFuncName == "ExpGrowth")
            && !checkTauNonZero(1))
            return;
        if (builtInFuncName == "ExpDecay2" && (!checkTauNonZero(1) || !checkTauNonZero(3)))
            return;
        if (builtInFuncName == "ExpDecay3"
            && (!checkTauNonZero(1) || !checkTauNonZero(3) || !checkTauNonZero(5)))
            return;
    } else if (usePluginCat) {
        const int pluginRow = funcBox ? funcBox->currentRow() : -1;
        if (pluginRow < 0 || pluginRow >= d_plugin_files_list.size()) {
            QMessageBox::warning(msgParent.data(), tr("Input error"),
                                 tr("Please select a plugin fit function."));
            return;
        }
    }

    if (!self || !d_graph)
        return;

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());

    if (d_fitter) {
        delete d_fitter;
        d_fitter = 0;
        activeFitter = nullptr;
    }

    if (useBuiltInCat) {
        fitBuiltInFunction(builtInFuncName, paramsInit.data());
        activeFitter = d_fitter;
    } else if (usePluginCat) {
        d_fitter = new PluginFit(app, d_graph);
        activeFitter = d_fitter;
        const bool pluginLoaded =
                ((PluginFit *)d_fitter)->load(d_plugin_files_list[funcBox->currentRow()]);
        if (!self)
            return;
        if (!pluginLoaded) {
            delete d_fitter;
            d_fitter = 0;
            activeFitter = nullptr;
            return;
        }
        d_fitter->setInitialGuesses(paramsInit.data());
    } else {
        d_fitter = new NonLinearFit(app, d_graph);
        activeFitter = d_fitter;
        ((NonLinearFit *)d_fitter)->setParametersList(parameters);
        ((NonLinearFit *)d_fitter)->setFormula(formula);
        d_fitter->setInitialGuesses(paramsInit.data());
    }

    if (d_fitter) {
        for (int i = 0; i < rows && i < d_fitter->numParameters(); ++i) {
            if (d_fitter->objectName() == tr("Polynomial") || d_fitter->objectName() == tr("Linear"))
                d_fitter->setInitialGuess(i, paramsInit[static_cast<size_t>(i)]);
            QCheckBox *cb = qobject_cast<QCheckBox *>(boxParams->cellWidget(i, 2));
            if (cb && cb->isChecked())
                d_fitter->setParameterFixed(i, true);
            if (hasBounds[static_cast<size_t>(i)])
                d_fitter->setParameterBounds(i, loBounds[static_cast<size_t>(i)],
                                             hiBounds[static_cast<size_t>(i)]);
        }
    }

    const bool dataOk = d_fitter && d_fitter->setDataFromCurve(curve, start, end);
    if (!self)
        return;
    if (!dataOk) {
        delete d_fitter;
        d_fitter = 0;
        activeFitter = nullptr;
        return;
    }

    const int weightIdx = boxYErrorSource->currentIndex();
    const QString colFullName = tableNamesBox->currentText() + "_" + colNamesBox->currentText();
    bool weightOk = false;
    if (weightIdx >= 0 && weightIdx <= 3) {
        weightOk = d_fitter->setYErrorSource((Fit::ErrorSource)weightIdx, colFullName);
    } else if (weightIdx == 4) {
        weightOk = d_fitter->setWeightingMethod(Fit::WeightDirect, colFullName);
    } else if (weightIdx == 5) {
        weightOk = d_fitter->setWeightingMethod(Fit::WeightProportional);
    } else if (weightIdx == 6) {
        weightOk = d_fitter->setWeightingMethod(Fit::WeightVariance, colFullName);
    }
    if (!self)
        return;
    if (!weightOk) {
        if (weightIdx == 4 || weightIdx == 6) {
            QMessageBox::critical(msgParent.data(), tr("Weighting Error"),
                                  tr("Could not read valid weight/variance values from column %1.")
                                          .arg(colFullName));
        }
        if (!self)
            return;
        delete d_fitter;
        d_fitter = 0;
        activeFitter = nullptr;
        return;
    }

    if (boxLinearConstraints && !boxLinearConstraints->text().trimmed().isEmpty()) {
        if (!d_fitter->setLinearConstraintExpr(boxLinearConstraints->text().trimmed())) {
            QMessageBox::critical(
                    msgParent.data(), tr("Linear Constraint Error"),
                    tr("Invalid or inconsistent linear parameter constraint expression:\n%1")
                            .arg(boxLinearConstraints->text().trimmed()));
            if (!self)
                return;
            delete d_fitter;
            d_fitter = 0;
            activeFitter = nullptr;
            return;
        }
    }

    if (chkLivePreview && chkLivePreview->isChecked()) {
        chkLivePreview->blockSignals(true);
        chkLivePreview->setChecked(false);
        chkLivePreview->blockSignals(false);
    }
    clearPreviewCurve();

    if (!self)
        return;
    if (!d_graph) {
        delete d_fitter;
        d_fitter = 0;
        activeFitter = nullptr;
        return;
    }

    d_fitter->setTolerance(eps);
    d_fitter->setAlgorithm((Fit::Algorithm)boxAlgorithm->currentIndex());
    d_fitter->setColor(btnColor->color());
    d_fitter->generateFunction(generatePointsBtn->isChecked(), generatePointsBox->value());
    d_fitter->setMaximumIterations(boxPoints->value());
    d_fitter->scaleErrors(scaleErrorsBox->isChecked());
    d_fitter->enableConfidenceBand(confBandBox && confBandBox->isChecked(), 0.95);
    d_fitter->enablePredictionBand(predBandBox && predBandBox->isChecked(), 0.95);

    if (d_fitter->objectName() == tr("MultiPeak") && ((MultiPeakFit *)d_fitter)->peaks() > 1) {
        ((MultiPeakFit *)d_fitter)->enablePeakCurves(app ? app->generatePeakCurves : false);
        if (app)
            ((MultiPeakFit *)d_fitter)->setPeakCurvesColor(app->peakCurvesColor);
    }

    d_fitter->fit();
    if (!self || !d_fitter)
        return;
    // F11: remember the built-in model for the "Recently used" list
    if (boxUseBuiltIn->isChecked()
        && (categoryBox->currentRow() == 1 || categoryBox->currentRow() >= 4)
        && funcBox->currentItem()) {
        recordRecentModel(funcBox->currentItem()->text());
        refreshRecentModelsBox();
    }
    if (summaryOnGraphBox && summaryOnGraphBox->isChecked() && d_graph)
        d_fitter->insertFitSummaryBoxOnGraph();
    if (!d_fitter->converged() && (!app || !app->isHeadlessMode())
        && !(qApp && qApp->property("torigraph_noninteractive").toBool())) {
        registerFitDialogJapaneseStrings();
        QMessageBox::warning(msgParent.data(), tr("Fit Warning"),
                             tr("The fit did not converge.") + "\n" + d_fitter->convergenceMessage()
                                     + "\n\n"
                                     + tr("The curve and parameters shown are the last iterate. "
                                          "Check the initial values, increase the number of "
                                          "iterations, or fix some parameters, and fit again."));
        if (!self || !d_fitter)
            return;
    }

    auto res = d_fitter->results();
    boxParams->blockSignals(true);
    for (int i = 0; i < rows && size_t(i) < res.size(); i++) {
        if (boxParams->item(i, 1))
            boxParams->item(i, 1)->setText(
                    formatLocaleScalar(res[size_t(i)], 'g', boxPrecision->value()));
    }
    boxParams->blockSignals(false);
}

void FitDialog::fitBuiltInFunction(const QString &function, double *initVal)
{
    ApplicationWindow *app = (ApplicationWindow *)this->parent();
    auto safeInv = [](double v, double fallback) -> double {
        return (std::isfinite(v) && std::abs(v) > 1e-300) ? (1.0 / v) : fallback;
    };
    if (function == "ExpDecay1") {
        initVal[1] = safeInv(initVal[1], 1.0);
        d_fitter = new ExponentialFit(app, d_graph);
    } else if (function == "ExpGrowth") {
        initVal[1] = -safeInv(initVal[1], 1.0);
        d_fitter = new ExponentialFit(app, d_graph, true);
    } else if (function == "ExpDecay2") {
        initVal[1] = safeInv(initVal[1], 1.0);
        initVal[3] = safeInv(initVal[3], 1.0);
        d_fitter = new TwoExpFit(app, d_graph);
    } else if (function == "ExpDecay3") {
        initVal[1] = safeInv(initVal[1], 1.0);
        initVal[3] = safeInv(initVal[3], 1.0);
        initVal[5] = safeInv(initVal[5], 1.0);
        d_fitter = new ThreeExpFit(app, d_graph);
    } else if (function == "Boltzmann")
        d_fitter = new SigmoidalFit(app, d_graph);
    else if (function == "GaussAmp") {
        if (polynomOrderBox->value() > 1)
            d_fitter = new MultiPeakFit(app, d_graph, MultiPeakFit::GaussAmp,
                                        polynomOrderBox->value());
        else
            d_fitter = new GaussAmpFit(app, d_graph);
    } else if (function == "Gauss")
        d_fitter = new MultiPeakFit(app, d_graph, MultiPeakFit::Gauss, polynomOrderBox->value());
    else if (function == "Lorentz")
        d_fitter = new MultiPeakFit(app, d_graph, MultiPeakFit::Lorentz, polynomOrderBox->value());
    else if (function == "PseudoVoigt")
        d_fitter = new MultiPeakFit(app, d_graph, MultiPeakFit::PseudoVoigt,
                                    polynomOrderBox->value());
    else if (function == "PearsonVII" && categoryBox->currentRow() == 1)
        d_fitter = new MultiPeakFit(app, d_graph, MultiPeakFit::PearsonVII,
                                    polynomOrderBox->value());
    else if (function == "Polynomial")
        d_fitter = new PolynomialFit(app, d_graph, polynomOrderBox->value());
    else {
        NonLinearFit *nlf = new NonLinearFit(app, d_graph);
        if (!nlf->setBuiltInModel(function)) {
            nlf->setParametersList(QStringList() << "A" << "xc" << "w" << "y0");
            nlf->setFormula(boxFunction->toPlainText());
        }
        d_fitter = nlf;
    }

    if (function != "Polynomial" && d_fitter)
        d_fitter->setInitialGuesses(initVal);
}

QString FitDialog::expandUserFunctions(const QString &rawFormula, const QStringList &names,
                                       const QStringList &defs)
{
    if (rawFormula.trimmed().isEmpty())
        return rawFormula;
    const int n = qMin(names.size(), defs.size());
    if (n <= 0)
        return rawFormula;

    auto extractRhs = [](const QString &def) -> QString {
        for (int idx = 0; idx < def.size(); ++idx) {
            if (def[idx] != QLatin1Char('='))
                continue;
            const QChar prev = (idx > 0) ? def[idx - 1] : QChar();
            const QChar next = (idx + 1 < def.size()) ? def[idx + 1] : QChar();
            if (prev == QLatin1Char('<') || prev == QLatin1Char('>') || prev == QLatin1Char('!')
                || prev == QLatin1Char('=') || next == QLatin1Char('='))
                continue;
            return def.mid(idx + 1).trimmed();
        }
        return def.trimmed();
    };

    struct HelperFunc {
        QString name;
        QString rhs;
        QRegularExpression wordRe;
    };

    QVector<HelperFunc> helpers;
    helpers.reserve(n);
    QSet<QString> seenNames;
    for (int i = 0; i < n; ++i) {
        const QString name = names[i].trimmed();
        if (name.isEmpty() || seenNames.contains(name))
            continue;
        const QString rhs = extractRhs(defs[i]);
        if (rhs.isEmpty())
            continue;
        seenNames.insert(name);
        HelperFunc hf;
        hf.name = name;
        hf.rhs = rhs;
        hf.wordRe = QRegularExpression(QStringLiteral("\\b") + QRegularExpression::escape(name)
                                       + QStringLiteral("\\b"));
        helpers.push_back(std::move(hf));
    }

    const int m = helpers.size();
    if (m <= 0)
        return rawFormula;

    // Post-order DFS emits dependencies before dependents even on diamond references,
    // while state[u] == 1 (visiting) breaks cycles in O(1).
    const int maxDepth = qMax(16, m);
    QVector<int> state(m, 0); // 0 = unvisited, 1 = visiting, 2 = visited
    QString prefixDefs;

    auto visit = [&](auto &self, int u, int depth) -> void {
        if (state[u] != 0)
            return;
        state[u] = 1;
        if (depth < maxDepth) {
            const QString &uRhs = helpers[u].rhs;
            for (int v = 0; v < m; ++v) {
                if (v == u || state[v] != 0)
                    continue;
                if (helpers[v].wordRe.match(uRhs).hasMatch())
                    self(self, v, depth + 1);
            }
        }
        state[u] = 2;
        prefixDefs.append(QString("%1=%2\n").arg(helpers[u].name, helpers[u].rhs));
    };

    for (int i = 0; i < m; ++i) {
        if (state[i] == 0 && helpers[i].wordRe.match(rawFormula).hasMatch())
            visit(visit, i, 0);
    }

    if (prefixDefs.isEmpty())
        return rawFormula;
    return prefixDefs + rawFormula;
}

bool FitDialog::containsUserFunctionName(const QString &function)
{
    for (const QString &fn : d_user_function_names) {
        const QString trimmed = fn.trimmed();
        if (trimmed.isEmpty())
            continue;
        QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(trimmed)
                              + QStringLiteral("\\b"));
        if (re.match(function).hasMatch())
            return true;
    }

    return false;
}

bool FitDialog::validInitialValues()
{
    QPointer<FitDialog> self(this);
    QPointer<QWidget> msgParent(parentWidget());
    for (int i = 0; i < boxParams->rowCount(); i++) {
        QTableWidgetItem *valItem = boxParams->item(i, 1);
        if (!valItem || valItem->text().trimmed().isEmpty()) {
            QMessageBox::critical(msgParent.data(), tr("Input error"),
                                  tr("Please enter initial guesses for your parameters!"));
            if (self)
                boxParams->setCurrentCell(i, 1);
            return false;
        }

        double val = 0.0;
        QString errMsg;
        if (!evalScalarExpr(valItem->text(), &val, &errMsg)) {
            QMessageBox::critical(msgParent.data(), tr("Initial guess error"), errMsg);
            if (self)
                boxParams->setCurrentCell(i, 1);
            return false;
        }
    }
    return true;
}

void FitDialog::changeDataRange()
{
    if (!d_graph)
        return;
    double start = d_graph->selectedXStartValue();
    double end = d_graph->selectedXEndValue();
    boxFrom->setText(formatLocaleScalar(qMin(start, end), 'g', 15));
    boxTo->setText(formatLocaleScalar(qMax(start, end), 'g', 15));
}

void FitDialog::setSrcTables(QList<MyWidget *> *tables)
{
    tableNamesBox->clear();
    if (!tables || tables->isEmpty()) {
        tableNamesBox->addItem(tr("No data tables"));
        colNamesBox->clear();
        colNamesBox->addItem(tr("No data tables"));
        return;
    }

    d_src_table = tables;
    foreach (QWidget *i, *d_src_table)
        tableNamesBox->addItem(i->objectName());

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QString curvePrefix =
            boxCurve->currentText().split("_", Qt::SkipEmptyParts).value(0);
#else
    const QString curvePrefix =
            boxCurve->currentText().split("_", QString::SkipEmptyParts).value(0);
#endif
    if (!curvePrefix.isEmpty())
        tableNamesBox->setCurrentIndex(tableNamesBox->findText(curvePrefix));
    selectSrcTable(tableNamesBox->currentIndex());
}

void FitDialog::selectSrcTable(int tabnr)
{
    colNamesBox->clear();

    if (tabnr >= 0 && tabnr < d_src_table->count()) {
        Table *t = (Table *)d_src_table->at(tabnr);
        if (t)
            colNamesBox->addItems(t->colNames());
    }
}

void FitDialog::yErrorSourceChanged(int index)
{
    if (index == Fit::CustomErrors || index == 4 || index == 6) {
        tableNamesBox->setEnabled(true);
        colNamesBox->setEnabled(true);
    } else {
        tableNamesBox->setEnabled(false);
        colNamesBox->setEnabled(false);
    }
}

void FitDialog::closeEvent(QCloseEvent *e)
{
    clearPreviewCurve();
    if (d_fitter && plotLabelBox && plotLabelBox->isChecked() && d_graph)
        d_fitter->showLegend();

    e->accept();
}

void FitDialog::reject()
{
    // QDialog::reject() only hides the dialog, so Esc skipped closeEvent(): the preview
    // curve stayed and "Paste Parameters to Plot" never showed the legend. Route it
    // through close() like the Close buttons.
    if (isVisible())
        close();
    else
        QDialog::reject();
}

void FitDialog::enableApplyChanges(int)
{
    btnApply->setEnabled(true);
}

void FitDialog::deleteFitCurves()
{
    if (!d_graph)
        return;
    d_graph->deleteFitCurves();
    boxCurve->clear();
    boxCurve->addItems(d_graph->analysableCurvesList());
}

void FitDialog::resetFunction()
{
    boxName->clear();
    boxParam->clear();
    editBox->clear();
}
