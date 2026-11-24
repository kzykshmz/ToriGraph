/***************************************************************************
    File                 : OpenProjectDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Knut Franke, Ion Vasilief
    Email (use @ for *)  : knut.franke*gmx.de, ion_vasilief*yahoo.fr
    Description          : Dialog for opening project files.

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

#include "OpenProjectDialog.h"
#include "ApplicationWindow.h"
#include "I18nJapanese.h"

#include <QCloseEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTextCodec>

#include <mutex>

namespace {
void registerOpenDialogTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Auto-detect", "自動判定 (Auto)");
        reg("Excel & CSV / Scientific Data", "Excel・CSV / 科学データ");
        reg("ToriGraph projects & data", "ToriGraph プロジェクトとデータ");
    });
}
} // namespace

bool OpenProjectDialog::isOriginProjectOrWindowFile(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QLatin1String("opj") || ext == QLatin1String("opju")
            || ext == QLatin1String("ogm") || ext == QLatin1String("ogw")
            || ext == QLatin1String("ogg") || ext == QLatin1String("org");
}

OpenProjectDialog::OpenProjectDialog(QWidget *parent, bool extended, Qt::WindowFlags flags)
    : ExtensibleFileDialog(parent, extended, flags)
{
    registerOpenDialogTranslations();
    setWindowTitle(tr("Open Project"));
    setFileMode(ExistingFile);
    QStringList filters;
    filters << tr("ToriGraph projects & data")
                    + " (*.sciprj *.sciprj.gz *.xlsx *.xlsm *.csv *.tsv *.txt *.dat *.asc *.tab "
                      "*.ras *.xy *.xye *.uxd *.chi *.mpt *.dta"
#ifdef ORIGIN_IMPORT
                      " *.opj *.OPJ *.opju *.OPJU"
#else
                      " *.opju *.OPJU"
#endif
                      ")"
            << tr("ToriGraph project") + " (*.sciprj)"
            << tr("Compressed ToriGraph project") + " (*.sciprj.gz)"
            << tr("Excel & CSV / Scientific Data")
                    + " (*.xlsx *.xlsm *.csv *.tsv *.txt *.dat *.asc *.tab *.ras *.xy *.xye "
                      "*.uxd *.chi *.mpt *.dta)"
            << tr("QtiPlot project") + " (*.qti)"
            << tr("Compressed QtiPlot project") + " (*.qti.gz)"
#ifdef ORIGIN_IMPORT
            << tr("Origin project") + " (*.opj *.OPJ *.opju *.OPJU)"
            << tr("Origin matrix") + " (*.ogm *.OGM)"
            << tr("Origin worksheet") + " (*.ogw *.OGW)" << tr("Origin graph") + " (*.ogg *.OGG)"
            << tr("Origin 3.5 project") + " (*.org *.ORG)"
#else
            << tr("Origin Unicode project") + " (*.opju *.OPJU)"
#endif
            << tr("Backup files") + " (*.sciprj~)"
            //<< tr("Python Source") + " (*.py *.PY)"
            << tr("All files") + " (*)";
    setNameFilters(filters);

    d_advanced_options = new QWidget();
    QVBoxLayout *advanced_layout = new QVBoxLayout();
    d_advanced_options->setLayout(advanced_layout);

    // Open As options
    QHBoxLayout *openAs_layout = new QHBoxLayout();
    advanced_layout->addLayout(openAs_layout);
    openAs_layout->addWidget(new QLabel(tr("Open As")));
    d_open_mode = new QComboBox();
    openAs_layout->addWidget(d_open_mode);
    // Important: Keep this is sync with enum OpenMode.
    d_open_mode->addItem(tr("New Project Window"));
    d_open_mode->addItem(tr("New Folder"));

    // Origin codepage options
    QHBoxLayout *codec_layout = new QHBoxLayout();
    codec_layout->addWidget(new QLabel(tr("Codepage")));
    d_open_codec = new QComboBox();
    d_open_codec->addItem(tr("Auto-detect"), 0);
    QSet<int> addedMibs;
    const char *const kPreferred[] = { "UTF-8",  "Shift-JIS", "Windows-1252", "ISO-8859-1",
                                       "GBK",    "EUC-KR",    "Big5",         "UTF-16" };
    for (const char *name : kPreferred) {
        if (QTextCodec *tc = QTextCodec::codecForName(name)) {
            const int mib = tc->mibEnum();
            if (mib != 0 && !addedMibs.contains(mib)) {
                d_open_codec->addItem(QString::fromLocal8Bit(tc->name()), mib);
                addedMibs.insert(mib);
            }
        }
    }
    if (!addedMibs.isEmpty())
        d_open_codec->insertSeparator(d_open_codec->count());
    for (const int id : QTextCodec::availableMibs()) {
        if (id == 0 || addedMibs.contains(id))
            continue;
        if (QTextCodec *tc = QTextCodec::codecForMib(id)) {
            d_open_codec->addItem(QString::fromLocal8Bit(tc->name()), id);
            addedMibs.insert(id);
        }
    }
    codec_layout->addWidget(d_open_codec);
    d_open_codec->setEnabled(false);
    advanced_layout->addLayout(codec_layout);

    setExtensionWidget(d_advanced_options);

#if QT_VERSION >= 0x040300
    connect(this, SIGNAL(filterSelected(const QString &)), this,
            SLOT(updateAdvancedOptions(const QString &)));
#else
    QList<QComboBox *> combo_boxes = findChildren<QComboBox *>();
    if (combo_boxes.size() >= 2)
        connect(combo_boxes[1], SIGNAL(currentIndexChanged(const QString &)), this,
                SLOT(updateAdvancedOptions(const QString &)));
#endif
    connect(this, &QFileDialog::currentChanged, this, [this](const QString &path) {
        d_current_file = path;
        updateAdvancedOptions(selectedNameFilter());
    });
    updateAdvancedOptions(selectedNameFilter());
}

void OpenProjectDialog::updateAdvancedOptions(const QString &filter)
{
    const QString selExt =
            d_current_file.isEmpty() ? QString() : QFileInfo(d_current_file).suffix().toLower();
    const bool selectedIsDataSheet = (selExt == QLatin1String("xlsx") || selExt == QLatin1String("xlsm")
                                      || selExt == QLatin1String("ogm")
                                      || selExt == QLatin1String("ogw")
                                      || selExt == QLatin1String("csv")
                                      || selExt == QLatin1String("tsv")
                                      || selExt == QLatin1String("txt")
                                      || selExt == QLatin1String("dat")
                                      || selExt == QLatin1String("asc")
                                      || selExt == QLatin1String("tab")
                                      || selExt == QLatin1String("ras")
                                      || selExt == QLatin1String("xy")
                                      || selExt == QLatin1String("xye")
                                      || selExt == QLatin1String("uxd")
                                      || selExt == QLatin1String("chi")
                                      || selExt == QLatin1String("mpt")
                                      || selExt == QLatin1String("dta"));
    if (selectedIsDataSheet || filter.contains("*.ogm") || filter.contains("*.ogw")
        || (filter.contains("*.xlsx") && !filter.contains("*.sciprj"))) {
        d_open_mode->setCurrentIndex(0);
        d_open_mode->setEnabled(false);
    } else {
        d_open_mode->setEnabled(true);
    }

    const bool explicitOriginFilter = !filter.contains("*.sciprj")
            && (filter.contains("*.ogm") || filter.contains("*.ogw") || filter.contains("*.opj")
                || filter.contains("*.opju") || filter.contains("*.ogg")
                || filter.contains("*.org"));
    const bool enableCodec = d_current_file.isEmpty() ? explicitOriginFilter
                                                      : isOriginProjectOrWindowFile(d_current_file);
    d_open_codec->setEnabled(enableCodec);
}

void OpenProjectDialog::closeEvent(QCloseEvent *e)
{
    if (isExtendable()) {
        ApplicationWindow *app = (ApplicationWindow *)this->parent();
        if (app)
            app->d_extended_open_dialog = this->isExtended();
    }

    e->accept();
}

QString OpenProjectDialog::codec() const
{
    const QStringList sel = selectedFiles();
    const QString chosen = !sel.isEmpty() ? sel.first() : d_current_file;
    if (!chosen.isEmpty() && !isOriginProjectOrWindowFile(chosen))
        return d_initial_codec;
    if (chosen.isEmpty() && !d_open_codec->isEnabled())
        return d_initial_codec;

    const int mib = d_open_codec->currentData().toInt();
    if (mib == 0)
        return QString();
    if (QTextCodec *tc = QTextCodec::codecForMib(mib))
        return QString::fromLocal8Bit(tc->name());
    return QString();
}

bool OpenProjectDialog::setCodec(const QString &codec)
{
    d_initial_codec = codec;
    if (codec.isEmpty()) {
        d_open_codec->setCurrentIndex(0);
        return true;
    }
    if (QTextCodec *target = QTextCodec::codecForName(codec.toLocal8Bit())) {
        const int ind = d_open_codec->findData(target->mibEnum());
        if (ind >= 0) {
            d_open_codec->setCurrentIndex(ind);
            return true;
        }
    }
    for (const int id : QTextCodec::availableMibs()) {
        if (QTextCodec *tc = QTextCodec::codecForMib(id)) {
            if (codec.compare(QString::fromLocal8Bit(tc->name()), Qt::CaseInsensitive) == 0) {
                const int ind = d_open_codec->findData(id);
                if (ind >= 0) {
                    d_open_codec->setCurrentIndex(ind);
                    return true;
                }
            }
        }
    }
    return false;
}
