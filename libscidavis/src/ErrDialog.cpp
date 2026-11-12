/***************************************************************************
    File                 : ErrDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Add error bars dialog

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
#include "ErrDialog.h"
#include "Table.h"
#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "Graph.h"
#include "PlotCurve.h"
#include "future/table/future_Table.h"
#include "core/column/Column.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QList>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QButtonGroup>
#include <QMessageBox>
#include <QPointer>
#include <QSignalBlocker>
#include <QWidget>
#include <cmath>

ErrDialog::ErrDialog(QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl), srcTables(nullptr)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizeGripEnabled(true);

    QVBoxLayout *vbox1 = new QVBoxLayout();
    vbox1->setSpacing(5);

    QHBoxLayout *hbox1 = new QHBoxLayout();
    vbox1->addLayout(hbox1);

    textLabel1 = new QLabel();
    hbox1->addWidget(textLabel1);

    nameLabel = new QComboBox();
    hbox1->addWidget(nameLabel);

    groupBox1 = new QGroupBox(QString(tr("Source of errors")));
    QGridLayout *gridLayout = new QGridLayout(groupBox1);
    vbox1->addWidget(groupBox1);

    buttonGroup1 = new QButtonGroup(this);
    buttonGroup1->setExclusive(true);

    columnBox = new QRadioButton();
    columnBox->setChecked(true);
    buttonGroup1->addButton(columnBox);
    gridLayout->addWidget(columnBox, 0, 0);

    colNamesBox = new QComboBox();
    tableNamesBox = new QComboBox();

    QHBoxLayout *comboBoxes = new QHBoxLayout();
    comboBoxes->addWidget(tableNamesBox);
    comboBoxes->addWidget(colNamesBox);

    gridLayout->addLayout(comboBoxes, 0, 1);

    adjacentColBox = new QRadioButton();
    buttonGroup1->addButton(adjacentColBox);
    gridLayout->addWidget(adjacentColBox, 1, 0, 1, 2);

    percentBox = new QRadioButton();
    buttonGroup1->addButton(percentBox);
    gridLayout->addWidget(percentBox, 2, 0);

    valueBox = new QLineEdit();
    valueBox->setAlignment(Qt::AlignHCenter);
    valueBox->setEnabled(false);
    gridLayout->addWidget(valueBox, 2, 1);

    standardBox = new QRadioButton();
    buttonGroup1->addButton(standardBox);
    gridLayout->addWidget(standardBox, 3, 0, 1, 2);

    standardErrorBox = new QRadioButton();
    buttonGroup1->addButton(standardErrorBox);
    gridLayout->addWidget(standardErrorBox, 4, 0, 1, 2);

    groupBox3 = new QGroupBox(QString());
    vbox1->addWidget(groupBox3);
    QHBoxLayout *hbox2 = new QHBoxLayout(groupBox3);

    buttonGroup2 = new QButtonGroup(this);
    buttonGroup2->setExclusive(true);

    xErrBox = new QRadioButton();
    buttonGroup2->addButton(xErrBox);
    hbox2->addWidget(xErrBox);

    yErrBox = new QRadioButton();
    buttonGroup2->addButton(yErrBox);
    hbox2->addWidget(yErrBox);
    yErrBox->setChecked(true);

    QVBoxLayout *vbox2 = new QVBoxLayout();
    buttonAdd = new QPushButton();
    buttonAdd->setDefault(true);
    vbox2->addWidget(buttonAdd);

    buttonCancel = new QPushButton();
    vbox2->addWidget(buttonCancel);

    vbox2->addStretch(1);

    QHBoxLayout *hlayout1 = new QHBoxLayout(this);
    hlayout1->addLayout(vbox1);
    hlayout1->addLayout(vbox2);

    languageChange();

    // signals and slots connections
    connect(buttonAdd, SIGNAL(clicked()), this, SLOT(add()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(percentBox, SIGNAL(toggled(bool)), valueBox, SLOT(setEnabled(bool)));
    connect(columnBox, SIGNAL(toggled(bool)), tableNamesBox, SLOT(setEnabled(bool)));
    connect(columnBox, SIGNAL(toggled(bool)), colNamesBox, SLOT(setEnabled(bool)));
    connect(tableNamesBox, SIGNAL(activated(int)), this, SLOT(selectSrcTable(int)));
    connect(nameLabel, SIGNAL(currentIndexChanged(int)), this, SLOT(onCurveOrDirectionChanged()));
    connect(xErrBox, SIGNAL(toggled(bool)), this, SLOT(onCurveOrDirectionChanged()));
    connect(yErrBox, SIGNAL(toggled(bool)), this, SLOT(onCurveOrDirectionChanged()));
}

int ErrDialog::findMatchingTableIndex(const QString &curveName, QString *outShortColName) const
{
    if (outShortColName)
        outShortColName->clear();
    if (curveName.isEmpty() || curveName.contains(QLatin1Char('=')))
        return -1;

    int bestExactIdx = -1;
    int bestExactLen = -1;
    int bestPrefixIdx = -1;
    int bestPrefixLen = -1;

    const int count = tableNamesBox ? tableNamesBox->count() : 0;
    for (int i = 0; i < count; ++i) {
        QString tName = tableNamesBox->itemText(i);
        if (tName.isEmpty() && srcTables && i < srcTables->size() && srcTables->at(i))
            tName = srcTables->at(i)->name();
        if (tName.isEmpty())
            continue;
        const QString prefix = tName + QLatin1Char('_');
        if (!curveName.startsWith(prefix))
            continue;

        const QString shortCol = curveName.mid(prefix.length());
        const int len = tName.length();
        if (len > bestPrefixLen) {
            bestPrefixLen = len;
            bestPrefixIdx = i;
        }
        if (srcTables && i < srcTables->size()) {
            Table *t = qobject_cast<Table *>(srcTables->at(i));
            if (t && !shortCol.isEmpty() && t->colIndex(shortCol) >= 0 && len > bestExactLen) {
                bestExactLen = len;
                bestExactIdx = i;
            }
        }
    }

    const int chosen = (bestExactIdx >= 0) ? bestExactIdx : bestPrefixIdx;
    if (chosen >= 0 && outShortColName) {
        QString tName = tableNamesBox->itemText(chosen);
        if (tName.isEmpty() && srcTables && chosen < srcTables->size() && srcTables->at(chosen))
            tName = srcTables->at(chosen)->name();
        *outShortColName = curveName.mid(tName.length() + 1);
    }
    return chosen;
}

void ErrDialog::preselectBestErrorColumn(int tabnr)
{
    if (!srcTables || tabnr < 0 || tabnr >= srcTables->size() || colNamesBox->count() <= 0)
        return;

    Table *t = qobject_cast<Table *>(srcTables->at(tabnr));
    if (!t || t->numCols() <= 0)
        return;

    QString shortColName;
    const int matchedTab = findMatchingTableIndex(nameLabel->currentText(), &shortColName);
    int yColIdx = -1;
    if (matchedTab == tabnr && !shortColName.isEmpty())
        yColIdx = t->colIndex(shortColName);
    if (yColIdx < 0)
        yColIdx = t->colIndex(nameLabel->currentText());

    int baseColIdx = yColIdx;
    if (xErrBox->isChecked() && yColIdx >= 0) {
        const int xIdx = t->colX(yColIdx);
        if (xIdx >= 0)
            baseColIdx = xIdx;
    }

    const SciDAVis::PlotDesignation targetErrDesig =
            xErrBox->isChecked() ? SciDAVis::xErr : SciDAVis::yErr;
    const int numCols = t->numCols();
    int bestCol = -1;

    // 1. Prefer adjacent column right next to base column if it is designated as error column
    if (baseColIdx >= 0 && baseColIdx + 1 < numCols
        && t->colPlotDesignation(baseColIdx + 1) == targetErrDesig) {
        bestCol = baseColIdx + 1;
    }

    // 2. Otherwise look for nearest explicit [yEr]/[xEr] column (preferring columns to the right of baseColIdx)
    if (bestCol < 0) {
        int bestRightDist = numCols + 1;
        int bestAnyDist = numCols + 1;
        int bestRightCol = -1;
        int bestAnyCol = -1;
        const int anchor = (baseColIdx >= 0) ? (baseColIdx + 1) : 0;
        for (int c = 0; c < numCols; ++c) {
            if (c == baseColIdx)
                continue;
            if (t->colPlotDesignation(c) == targetErrDesig) {
                const int dist = qAbs(c - anchor);
                if (baseColIdx >= 0 && c > baseColIdx && dist < bestRightDist) {
                    bestRightDist = dist;
                    bestRightCol = c;
                }
                if (dist < bestAnyDist) {
                    bestAnyDist = dist;
                    bestAnyCol = c;
                }
            }
        }
        bestCol = (bestRightCol >= 0) ? bestRightCol : bestAnyCol;
    }

    // 3. Fallback to adjacent column (yColIndex + 1) instead of column 0
    if (bestCol < 0 && baseColIdx >= 0 && baseColIdx + 1 < numCols)
        bestCol = baseColIdx + 1;

    if (bestCol >= 0 && bestCol < colNamesBox->count()) {
        QSignalBlocker blocker(colNamesBox);
        colNamesBox->setCurrentIndex(bestCol);
    }
}

void ErrDialog::onCurveOrDirectionChanged()
{
    if (!srcTables || srcTables->isEmpty())
        return;

    const int matched = findMatchingTableIndex(nameLabel->currentText());
    if (matched >= 0 && matched != tableNamesBox->currentIndex()) {
        {
            QSignalBlocker blocker(tableNamesBox);
            tableNamesBox->setCurrentIndex(matched);
        }
        selectSrcTable(matched);
    } else {
        preselectBestErrorColumn(tableNamesBox->currentIndex());
    }
}

void ErrDialog::setCurveNames(const QStringList &names)
{
    {
        QSignalBlocker blocker(nameLabel);
        nameLabel->clear();
        nameLabel->addItems(names);
    }
    onCurveOrDirectionChanged();
}

void ErrDialog::setSrcTables(QList<MyWidget *> *tables)
{
    srcTables = tables;
    {
        QSignalBlocker blocker(tableNamesBox);
        tableNamesBox->clear();
        if (srcTables) {
            for (QList<MyWidget *>::const_iterator i = srcTables->begin(); i != srcTables->end();
                 ++i) {
                if (!*i)
                    continue;
                const QString tName = (*i)->name().isEmpty() ? (*i)->objectName() : (*i)->name();
                tableNamesBox->addItem(tName);
            }
        }
        const int matched = findMatchingTableIndex(nameLabel->currentText());
        if (matched >= 0)
            tableNamesBox->setCurrentIndex(matched);
        else if (tableNamesBox->count() > 0)
            tableNamesBox->setCurrentIndex(0);
    }
    selectSrcTable(tableNamesBox->currentIndex());
}

void ErrDialog::selectSrcTable(int tabnr)
{
    {
        QSignalBlocker blocker(colNamesBox);
        colNamesBox->clear();
        if (srcTables && tabnr >= 0 && tabnr < srcTables->size()) {
            if (Table *t = qobject_cast<Table *>(srcTables->at(tabnr)))
                colNamesBox->addItems(t->colNames());
        }
    }
    preselectBestErrorColumn(tabnr);
}

void ErrDialog::add()
{
    const QString curveName = nameLabel->currentText();
    if (curveName.isEmpty())
        return;

    const int direction = xErrBox->isChecked() ? 0 : 1;

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    QPointer<MultiLayer> mlGuard =
            app ? qobject_cast<MultiLayer *>(app->d_workspace.activeSubWindow()) : nullptr;

    if (columnBox->isChecked()) {
        emit options(curveName,
                     tableNamesBox->currentText() + QLatin1Char('_') + colNamesBox->currentText(),
                     direction);
    } else if (adjacentColBox->isChecked()) {
        QString shortColName;
        int tabIdx = findMatchingTableIndex(curveName, &shortColName);
        if (tabIdx < 0)
            tabIdx = tableNamesBox->currentIndex();
        Table *t = (srcTables && tabIdx >= 0 && tabIdx < srcTables->size())
                ? qobject_cast<Table *>(srcTables->at(tabIdx))
                : nullptr;
        if (!t) {
            QMessageBox::critical(
                    this, tr("Error"),
                    tr("This feature is not available for user defined function curves!"));
            return;
        }
        int baseColIdx = !shortColName.isEmpty() ? t->colIndex(shortColName) : -1;
        if (baseColIdx < 0)
            baseColIdx = t->colIndex(curveName);
        if (direction == 0 && baseColIdx >= 0) {
            const int xIdx = t->colX(baseColIdx);
            if (xIdx >= 0)
                baseColIdx = xIdx;
        }
        const int adjIdx = (baseColIdx >= 0) ? (baseColIdx + 1) : -1;
        if (adjIdx < 0 || adjIdx >= t->numCols()) {
            QMessageBox::warning(
                    this, tr("Error"),
                    tr("No adjacent column exists to the right of the selected curve column."));
            return;
        }
        emit options(curveName, t->name() + QLatin1Char('_') + t->colLabel(adjIdx), direction);
    } else if (standardErrorBox->isChecked()) {
        QString shortColName;
        int tabIdx = findMatchingTableIndex(curveName, &shortColName);
        if (tabIdx < 0)
            tabIdx = tableNamesBox->currentIndex();
        Table *t = (srcTables && tabIdx >= 0 && tabIdx < srcTables->size())
                ? qobject_cast<Table *>(srcTables->at(tabIdx))
                : nullptr;
        if (!t || !t->d_future_table) {
            QMessageBox::critical(
                    this, tr("Error"),
                    tr("This feature is not available for user defined function curves!"));
            return;
        }
        int yColIdx = !shortColName.isEmpty() ? t->colIndex(shortColName) : -1;
        if (yColIdx < 0)
            yColIdx = t->colIndex(curveName);

        Column *dataCol = nullptr;
        if (direction == 0) {
            if (mlGuard && mlGuard->activeGraph()) {
                if (DataCurve *mc =
                            dynamic_cast<DataCurve *>(mlGuard->activeGraph()->curve(curveName))) {
                    const int xIdx = t->colIndex(mc->xColumnName());
                    if (xIdx >= 0)
                        dataCol = t->column(xIdx);
                }
            }
            if (!dataCol && yColIdx >= 0) {
                const int xIdx = t->colX(yColIdx);
                if (xIdx >= 0)
                    dataCol = t->column(xIdx);
            }
        } else if (yColIdx >= 0) {
            dataCol = t->column(yColIdx);
        }

        if (!dataCol || dataCol->dataType() != SciDAVis::TypeDouble) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("You can only define error bars for numeric columns."));
            return;
        }

        const int rows = dataCol->rowCount();
        int validCount = 0;
        double mean = 0.0;
        double m2 = 0.0;
        for (int r = 0; r < rows; ++r) {
            if (dataCol->isInvalid(r))
                continue;
            const double v = dataCol->valueAt(r);
            if (!std::isfinite(v))
                continue;
            ++validCount;
            const double delta = v - mean;
            mean += delta / double(validCount);
            const double delta2 = v - mean;
            m2 += delta * delta2;
        }
        const double seVal = (validCount >= 2)
                ? std::sqrt(m2 / (double(validCount) * double(validCount - 1)))
                : 0.0;

        const QString errColShort = t->d_future_table->uniqueNameFor(QStringLiteral("SE_1"));
        Column *errors = new Column(errColShort, SciDAVis::ColumnMode::Numeric);
        errors->setPlotDesignation(direction == 0 ? SciDAVis::xErr : SciDAVis::yErr);
        const int totalRows = t->numRows();
        for (int r = 0; r < totalRows; ++r) {
            if (r < rows && !dataCol->isInvalid(r) && std::isfinite(dataCol->valueAt(r)))
                errors->setValueAt(r, seVal);
        }
        t->d_future_table->addChild(errors);
        if (tableNamesBox->currentIndex() == tabIdx)
            selectSrcTable(tabIdx);

        emit options(curveName, t->name() + QLatin1Char('_') + errors->name(), direction);
    } else {
        const int type = percentBox->isChecked() ? 0 : 1;
        emit options(curveName, type, valueBox->text(), direction);
    }
}

ErrDialog::~ErrDialog() { }

void ErrDialog::languageChange()
{
    setWindowTitle(tr("Error Bars"));
    xErrBox->setText(tr("&X Error Bars"));
    buttonAdd->setText(tr("&Add"));
    textLabel1->setText(tr("Add Error Bars to"));
    groupBox1->setTitle(tr("Source of errors"));
    adjacentColBox->setText(tr("Adjacent Column (Next to Y)"));
    percentBox->setText(tr("Percent of data (%)"));
    valueBox->setText(tr("5"));
    standardBox->setText(tr("Standard Deviation of Data"));
    standardErrorBox->setText(tr("Standard Error of Mean (SE)"));
    yErrBox->setText(tr("&Y Error Bars"));
    buttonCancel->setText(tr("&Close"));
    columnBox->setText(tr("Existing column"));
}
