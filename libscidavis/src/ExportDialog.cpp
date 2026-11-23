/***************************************************************************
    File                 : ExportDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Export ASCII dialog

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
#include "ExportDialog.h"

#include <QLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>

#include "ApplicationWindow.h"
#include "I18nJapanese.h"

#include <mutex>

namespace {
void registerExportDialogTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Write Long Name and &Units rows (instead of short names)",
            "ロングネームと単位の行を書き出す(短い名前の代わりに)(&U)");
        reg("Add a UTF-8 &BOM (Excel then shows Japanese text correctly)",
            "UTF-8 の BOM を付ける(Excel で日本語が化けない)(&B)");
    });
}
} // namespace

ExportDialog::ExportDialog(QWidget *parent, Qt::WindowFlags fl) : QDialog(parent, fl)
{
    setWindowTitle(tr("Export ASCII"));
    setSizeGripEnabled(true);

    QGridLayout *gl1 = new QGridLayout();
    gl1->addWidget(new QLabel(tr("Table")), 0, 0);
    boxTable = new QComboBox();
    boxTable->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    gl1->addWidget(boxTable, 0, 1);

    boxAllTables = new QCheckBox(tr("&All"));
    boxAllTables->setChecked(false);
    gl1->addWidget(boxAllTables, 0, 2);

    QLabel *sepText = new QLabel(tr("Separator"));
    gl1->addWidget(sepText, 1, 0);

    boxSeparator = new QComboBox();
    boxSeparator->addItem(tr("TAB"));
    boxSeparator->addItem(tr("SPACE"));
    boxSeparator->addItem(";" + tr("TAB"));
    boxSeparator->addItem("," + tr("TAB"));
    boxSeparator->addItem(";" + tr("SPACE"));
    boxSeparator->addItem("," + tr("SPACE"));
    boxSeparator->addItem(";");
    boxSeparator->addItem(",");
    boxSeparator->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    boxSeparator->setEditable(true);
    gl1->addWidget(boxSeparator, 1, 1);

    QString help = tr("The column separator can be customized. The following special codes can be "
                      "used:\n\\t for a TAB character \n\\s for a SPACE");
    help += "\n" + tr("The separator must not contain the following characters: 0-9eE.+-");

    boxSeparator->setWhatsThis(help);
    sepText->setWhatsThis(help);
    boxSeparator->setToolTip(help);
    sepText->setToolTip(help);

    registerExportDialogTranslations();
    auto &settings = ApplicationWindow::getSettings();
    boxNames = new QCheckBox(tr("Include Column &Names"));
    boxNames->setChecked(settings.value("/General/ExportASCII/IncludeNames", true).toBool());

    boxSelection = new QCheckBox(tr("Export &Selection"));
    boxSelection->setChecked(settings.value("/General/ExportASCII/ExportSelection", false).toBool());

    if (settings.contains("/General/ExportASCII/Separator"))
        setColumnSeparator(settings.value("/General/ExportASCII/Separator").toString());

    QCheckBox *boxLongNameUnits =
            new QCheckBox(tr("Write Long Name and &Units rows (instead of short names)"));
    boxLongNameUnits->setObjectName(QStringLiteral("ExportLongNameUnits"));
    boxLongNameUnits->setChecked(
            settings.value("/General/ExportASCII/LongNameUnits", true).toBool());
    QCheckBox *boxBom =
            new QCheckBox(tr("Add a UTF-8 &BOM (Excel then shows Japanese text correctly)"));
    boxBom->setObjectName(QStringLiteral("ExportUtf8Bom"));
    boxBom->setChecked(settings.value("/General/ExportASCII/UTF8BOM", true).toBool());

    QVBoxLayout *vl1 = new QVBoxLayout();
    vl1->addLayout(gl1);
    vl1->addWidget(boxNames);
    vl1->addWidget(boxSelection);
    vl1->addWidget(boxLongNameUnits);
    vl1->addWidget(boxBom);

    QHBoxLayout *hbox3 = new QHBoxLayout();
    buttonOk = new QPushButton(tr("&OK"));
    buttonOk->setDefault(true);
    hbox3->addWidget(buttonOk);
    buttonCancel = new QPushButton(tr("&Cancel"));
    hbox3->addWidget(buttonCancel);
    buttonHelp = new QPushButton(tr("&Help"));
    hbox3->addWidget(buttonHelp);
    hbox3->addStretch();

    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->addLayout(vl1);
    vl->addStretch();
    vl->addLayout(hbox3);

    // signals and slots connections
    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(buttonHelp, SIGNAL(clicked()), this, SLOT(help()));
    connect(boxAllTables, SIGNAL(toggled(bool)), this, SLOT(enableTableName(bool)));
}

void ExportDialog::help()
{
    QString s = tr("The column separator can be customized. The following special codes can be "
                   "used:\n\\t for a TAB character \n\\s for a SPACE");
    s += "\n" + tr("The separator must not contain the following characters: 0-9eE.+-");
    QMessageBox::about(0, tr("Help"), s);
}

void ExportDialog::setTableNames(const QStringList &names)
{
    boxTable->addItems(names);
}

void ExportDialog::setActiveTableName(const QString &name)
{
    boxTable->setCurrentIndex(boxTable->findText(name));
}

void ExportDialog::enableTableName(bool ok)
{
    boxTable->setEnabled(!ok);
}

void ExportDialog::accept()
{
    QString sep = boxSeparator->currentText();
    sep.replace(tr("TAB"), "\t", Qt::CaseInsensitive);
    sep.replace(tr("SPACE"), " ");
    sep.replace("\\s", " ");
    sep.replace("\\t", "\t");

    if (sep.contains(QRegExp("[0-9.eE+-]"))) {
        QMessageBox::warning(
                0, tr("Import options error"),
                tr("The separator must not contain the following characters: 0-9eE.+-"));
        return;
    }

    auto &settings = ApplicationWindow::getSettings();
    settings.setValue("/General/ExportASCII/Separator", sep);
    settings.setValue("/General/ExportASCII/IncludeNames", boxNames->isChecked());
    settings.setValue("/General/ExportASCII/ExportSelection", boxSelection->isChecked());
    if (QCheckBox *cb = findChild<QCheckBox *>(QStringLiteral("ExportLongNameUnits")))
        settings.setValue("/General/ExportASCII/LongNameUnits", cb->isChecked());
    if (QCheckBox *cb = findChild<QCheckBox *>(QStringLiteral("ExportUtf8Bom")))
        settings.setValue("/General/ExportASCII/UTF8BOM", cb->isChecked());

    hide();
    if (boxAllTables->isChecked())
        emit exportAllTables(sep, boxNames->isChecked(), boxSelection->isChecked());
    else
        emit exportTable(boxTable->currentText(), sep, boxNames->isChecked(),
                         boxSelection->isChecked());
    close();
}

void ExportDialog::setColumnSeparator(const QString &sep)
{
    QString effective = sep;
    if (effective.isEmpty()) {
        auto &settings = ApplicationWindow::getSettings();
        effective = settings.value("/General/ExportASCII/Separator", QStringLiteral("\t")).toString();
        if (effective.isEmpty())
            effective = QStringLiteral("\t");
    }
    if (effective == "\t")
        boxSeparator->setCurrentIndex(0);
    else if (effective == " ")
        boxSeparator->setCurrentIndex(1);
    else if (effective == ";\t")
        boxSeparator->setCurrentIndex(2);
    else if (effective == ",\t")
        boxSeparator->setCurrentIndex(3);
    else if (effective == "; ")
        boxSeparator->setCurrentIndex(4);
    else if (effective == ", ")
        boxSeparator->setCurrentIndex(5);
    else if (effective == ";")
        boxSeparator->setCurrentIndex(6);
    else if (effective == ",")
        boxSeparator->setCurrentIndex(7);
    else {
        QString separator = effective;
        boxSeparator->setEditText(separator.replace(" ", "\\s").replace("\t", "\\t"));
    }
}

ExportDialog::~ExportDialog() { }
