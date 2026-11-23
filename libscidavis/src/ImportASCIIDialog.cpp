/***************************************************************************
    File                 : ImportASCIIDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006,2007 by Ion Vasilief,
                           Tilman Benkert, Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net,
                           knut.franke*gmx.de
    Description          : Import ASCII file(s) dialog

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

#include "ImportASCIIDialog.h"
#include "ApplicationWindow.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QRegExp>
#include <QMessageBox>
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QBuffer>

#include "I18nJapanese.h"
#include "table/AsciiTableImportFilter.h"
#include "table/future_Table.h"

#include <memory>
#include <mutex>

namespace {
void registerImportDialogTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Import Data File(s)", "データファイルを読み込む");
        reg("Preview (first lines):", "プレビュー(先頭の数行):");
        reg("Detected: %1", "自動判定: %1");
        reg("Auto-detect", "自動判定 (Auto)");
        reg("Text encoding:", "文字コード:");
        reg("Unit row:", "単位行:");
        reg("Auto", "自動");
        reg("2nd line holds units", "2行目を単位として扱う");
        reg("No unit row", "単位行なし");
        reg("Preview could not be built for this file.", "このファイルのプレビューを作成できませんでした。");
        reg("Select a file to see a preview.",
            "ファイルを選ぶと先頭の数行と自動判定の結果を表示します。");
    });
}
} // namespace

ImportASCIIDialog::ImportASCIIDialog(bool import_mode_enabled, QWidget *parent, bool extended,
                                     Qt::WindowFlags flags)
    : ExtensibleFileDialog(parent, extended, flags)
{
    registerImportDialogTranslations();
    setWindowTitle(tr("Import Data File(s)"));

    QStringList filters;
    filters << tr("All files") + " (*)";
    filters << tr("Comma Separated Values") + " (*.CSV *.csv *.TSV *.tsv)";
    filters << tr("Text files") + " (*.TXT *.txt)";
    filters << tr("Data files")
                    + " (*.DAT *.dat *.RAS *.ras *.xy *.xye *.asc *.chi *.uxd *.mpt *.dta *.tab)";
    setNameFilters(filters);

    setFileMode(QFileDialog::ExistingFiles);

    initAdvancedOptions();
    d_import_mode->setEnabled(import_mode_enabled);
    setExtensionWidget(d_advanced_options);

    // The preview and the result of the automatic detection are what a first-time user needs
    // most, so they stay visible below the file list even while the advanced options (which
    // are folded away by default) are hidden.
    {
        QWidget *previewBox = new QWidget(this);
        previewBox->setObjectName(QStringLiteral("ImportPreviewBox"));
        QVBoxLayout *previewLayout = new QVBoxLayout(previewBox);
        previewLayout->setContentsMargins(0, 4, 0, 0);
        previewLayout->addWidget(new QLabel(tr("Preview (first lines):"), previewBox));
        QPlainTextEdit *preview = new QPlainTextEdit(previewBox);
        preview->setObjectName(QStringLiteral("ImportPreview"));
        preview->setReadOnly(true);
        preview->setLineWrapMode(QPlainTextEdit::NoWrap);
        preview->setMaximumHeight(110);
        previewLayout->addWidget(preview);
        QLabel *summary = new QLabel(tr("Select a file to see a preview."), previewBox);
        summary->setObjectName(QStringLiteral("ImportDetectionSummary"));
        summary->setWordWrap(true);
        previewLayout->addWidget(summary);
        if (QGridLayout *grid = qobject_cast<QGridLayout *>(layout()))
            grid->addWidget(previewBox, grid->rowCount(), 0, 1, grid->columnCount());
        else if (layout())
            layout()->addWidget(previewBox);
    }

    // get remembered option values
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent);
    if (app) {
        d_strip_spaces->setChecked(app->strip_spaces);
        d_simplify_spaces->setChecked(app->simplify_spaces);
        d_ignored_lines->setValue(app->ignoredLines);
        d_rename_columns->setChecked(app->renameColumns);
        // "\t" is the factory default of the old dialog: treat it (and an empty value) as
        // "not chosen" so that the automatic detection stays the default.
        if (app->columnSeparator.isEmpty() || app->columnSeparator == QLatin1String("\t"))
            setColumnSeparator(QString());
        else
            setColumnSeparator(app->columnSeparator);

        if (app->d_ASCII_import_locale.name() == QLocale::c().name())
            boxDecimalSeparator->setCurrentIndex(1);
        else if (app->d_ASCII_import_locale.name() == QLocale(QLocale::German).name())
            boxDecimalSeparator->setCurrentIndex(2);
        else if (app->d_ASCII_import_locale.name() == QLocale(QLocale::French).name())
            boxDecimalSeparator->setCurrentIndex(3);
        boxDecimalSeparator->setEnabled(app->d_convert_to_numeric);
        d_convert_to_numeric->setChecked(app->d_convert_to_numeric);
    } else {
        setColumnSeparator(QString());
    }

    connect(d_import_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(updateImportMode(int)));

    // The extension decides nothing any more: the separator stays on "Auto-detect" unless the
    // user picks one, and the filter recognises .csv/.tsv/.ras/instrument files by content.
    // Preview of the first lines and what the import makes of them with the CURRENT options.
    connect(this, &QFileDialog::currentChanged, this, [this](const QString &path) {
        d_preview_path = path;
        refreshPreview();
    });
    // Any option change refreshes the preview (so it is never out of sync with the dialog)
    auto refresh = [this]() { refreshPreview(); };
    connect(d_column_separator, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            refresh);
    connect(d_column_separator, &QComboBox::editTextChanged, this, refresh);
    connect(d_ignored_lines, QOverload<int>::of(&QSpinBox::valueChanged), this, refresh);
    connect(d_rename_columns, &QCheckBox::toggled, this, refresh);
    connect(d_strip_spaces, &QCheckBox::toggled, this, refresh);
    connect(d_simplify_spaces, &QCheckBox::toggled, this, refresh);
    connect(d_convert_to_numeric, &QCheckBox::toggled, this, refresh);
    connect(boxDecimalSeparator, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            refresh);
    connect(d_encoding, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
    connect(d_units_row, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
}

ImportASCIIDialog::~ImportASCIIDialog()
{
    // The encoding / unit-row choice only applies while this dialog drives the import
    AsciiTableImportFilter::setDialogImportDefaults(QString(), -1);
}

void ImportASCIIDialog::done(int r)
{
    if (r == QDialog::Accepted)
        AsciiTableImportFilter::setDialogImportDefaults(encodingHint(), unitsRowHint());
    ExtensibleFileDialog::done(r);
}

QString ImportASCIIDialog::encodingHint() const
{
    const QString v = d_encoding->currentData().toString();
    return v;
}

int ImportASCIIDialog::unitsRowHint() const
{
    return d_units_row->currentData().toInt();
}

namespace {
// Drop an incomplete multi-byte sequence at the end of a block that was cut at a fixed size, so
// the preview does not end in a replacement character (UTF-8) or a broken surrogate (UTF-16).
void trimIncompleteTail(QByteArray &b, bool truncated)
{
    if (!truncated || b.isEmpty())
        return;
    const auto u = [&b](int i) { return static_cast<unsigned char>(b.at(i)); };
    const bool utf16le = b.size() >= 2 && u(0) == 0xFF && u(1) == 0xFE;
    const bool utf16be = b.size() >= 2 && u(0) == 0xFE && u(1) == 0xFF;
    bool zeroPattern = false;
    if (!utf16le && !utf16be && b.size() >= 4) {
        int zeros = 0;
        const int n = qMin(b.size(), 64);
        for (int i = 0; i < n; ++i)
            zeros += (u(i) == 0);
        zeroPattern = zeros * 4 >= n; // BOM-less UTF-16 of mostly ASCII text
    }
    if (utf16le || utf16be || zeroPattern) {
        if (b.size() % 2)
            b.chop(1);
        const bool be = utf16be || (zeroPattern && u(0) == 0);
        if (b.size() >= 2) {
            const int hi = be ? u(b.size() - 2) : u(b.size() - 1);
            if (hi >= 0xD8 && hi <= 0xDB) // dangling high surrogate
                b.chop(2);
        }
        return;
    }
    // UTF-8 (a Shift-JIS lead byte at the very end is dropped by the line cut below)
    int i = b.size() - 1;
    int back = 0;
    while (i >= 0 && back < 3 && (u(i) & 0xC0) == 0x80) {
        --i;
        ++back;
    }
    if (i >= 0 && u(i) >= 0xC0) {
        const int need = (u(i) >= 0xF0) ? 4 : (u(i) >= 0xE0) ? 3 : 2;
        if (b.size() - i < need)
            b.truncate(i);
    }
}
} // namespace

void ImportASCIIDialog::refreshPreview()
{
    QLabel *summary = findChild<QLabel *>(QStringLiteral("ImportDetectionSummary"));
    QPlainTextEdit *preview = findChild<QPlainTextEdit *>(QStringLiteral("ImportPreview"));
    if (!summary || !preview)
        return;
    const QString path = d_preview_path;
    QFile f(path);
    if (path.isEmpty() || !QFileInfo(path).isFile() || !f.open(QIODevice::ReadOnly)) {
        summary->setText(tr("Select a file to see a preview."));
        preview->clear();
        return;
    }
    const qint64 fileSize = f.size();
    QByteArray head = f.read(64 * 1024);
    f.close();
    const bool truncated = fileSize > head.size();
    trimIncompleteTail(head, truncated);
    if (truncated) {
        // cut at the last complete line (LF, CR LF or CR only) so a half-read record never
        // shows up as a bogus last row
        const int lastBreak = qMax(head.lastIndexOf('\n'), head.lastIndexOf('\r'));
        if (lastBreak > 0)
            head.truncate(lastBreak + 1);
    }

    QString text = AsciiTableImportFilter::decodeLabTextAuto(head, encodingHint());
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    preview->setPlainText(text.section(QLatin1Char('\n'), 0, 7));

    AsciiTableImportFilter filter;
    filter.set_ignored_lines(ignoredLines());
    filter.set_separator(columnSeparator());
    filter.set_first_row_names_columns(renameColumns());
    filter.set_trim_whitespace(stripSpaces());
    filter.set_simplify_whitespace(simplifySpaces());
    filter.set_convert_to_numeric(convertToNumeric());
    filter.set_numeric_locale(decimalSeparators());
    filter.set_encoding(encodingHint());
    filter.set_units_row(unitsRowHint());
    QBuffer buf(&head);
    buf.open(QIODevice::ReadOnly);
    std::unique_ptr<AbstractAspect> probe(filter.importAspect(buf));
    summary->setText(probe ? tr("Detected: %1")
                                     .arg(AsciiTableImportFilter::describeImportReport(
                                             AsciiTableImportFilter::lastImportReport()))
                           : tr("Preview could not be built for this file."));
}
void ImportASCIIDialog::initAdvancedOptions()
{
    d_advanced_options = new QGroupBox();
    QVBoxLayout *main_layout = new QVBoxLayout(d_advanced_options);
    QGridLayout *advanced_layout = new QGridLayout();
    main_layout->addLayout(advanced_layout);

    advanced_layout->addWidget(new QLabel(tr("Import each file as: ")), 0, 0);
    d_import_mode = new QComboBox();
    // Important: Keep this in sync with the ImportMode enum.
    d_import_mode->addItem(tr("New Table"));
    d_import_mode->addItem(tr("New Columns"));
    d_import_mode->addItem(tr("New Rows"));
    d_import_mode->addItem(tr("Overwrite Current Table"));
    advanced_layout->addWidget(d_import_mode, 0, 1);

    QLabel *label_column_separator = new QLabel(tr("Separator:"));
    advanced_layout->addWidget(label_column_separator, 1, 0);
    d_column_separator = new QComboBox();
    // Index 0 = automatic detection (separator + decimal marks from the file content)
    d_column_separator->addItem(tr("Auto-detect"));
    d_column_separator->addItem(tr("TAB"));
    d_column_separator->addItem(tr("SPACE"));
    d_column_separator->addItem(";" + tr("TAB"));
    d_column_separator->addItem("," + tr("TAB"));
    d_column_separator->addItem(";" + tr("SPACE"));
    d_column_separator->addItem("," + tr("SPACE"));
    d_column_separator->addItem(";");
    d_column_separator->addItem(",");
    d_column_separator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d_column_separator->setEditable(true);
    advanced_layout->addWidget(d_column_separator, 1, 1);
    // context-sensitive help
    QString help_column_separator =
            tr("The column separator can be customized. \nThe following special codes can be "
               "used:\n\\t for a TAB character \n\\s for a SPACE");
    help_column_separator +=
            "\n" + tr("The separator must not contain the following characters: \n0-9eE.+-");
    d_column_separator->setWhatsThis(help_column_separator);
    label_column_separator->setToolTip(help_column_separator);
    d_column_separator->setToolTip(help_column_separator);
    label_column_separator->setWhatsThis(help_column_separator);

    QLabel *label_ignore_lines = new QLabel(tr("Ignore first"));
    advanced_layout->addWidget(label_ignore_lines, 2, 0);
    d_ignored_lines = new QSpinBox();
    d_ignored_lines->setRange(0, 10000);
    d_ignored_lines->setSuffix(" " + tr("lines"));
    d_ignored_lines->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    advanced_layout->addWidget(d_ignored_lines, 2, 1);

    d_rename_columns = new QCheckBox(tr("Use first row to &name columns"));
    advanced_layout->addWidget(d_rename_columns, 0, 2, 1, 2);

    d_strip_spaces = new QCheckBox(tr("&Remove white spaces from line ends"));
    advanced_layout->addWidget(d_strip_spaces, 1, 2, 1, 2);
    // context-sensitive help
    QString help_strip_spaces =
            tr("By checking this option all white spaces will be \nremoved from the beginning and "
               "the end of \nthe lines in the ASCII file.",
               "when translating this check the what's this functions and tool tips to place the "
               "'\\n's correctly");
    help_strip_spaces += "\n\n"
            + tr("Warning: checking this option leads to column \noverlaping if the columns in the "
                 "ASCII file don't \nhave the same number of rows.");
    help_strip_spaces += "\n"
            + tr("To avoid this problem you should precisely \ndefine the column separator using "
                 "TAB and \nSPACE characters.",
                 "when translating this check the what's this functions and tool tips to place the "
                 "'\\n's correctly");
    d_strip_spaces->setWhatsThis(help_strip_spaces);
    d_strip_spaces->setToolTip(help_strip_spaces);

    d_simplify_spaces = new QCheckBox(tr("&Simplify white spaces"));
    advanced_layout->addWidget(d_simplify_spaces, 2, 2, 1, 2);
    // context-sensitive help
    QString help_simplify_spaces =
            tr("By checking this option all white spaces will be \nremoved from the beginning and "
               "the end of the \nlines and each sequence of internal \nwhitespaces (including the "
               "TAB character) will \nbe replaced with a single space.",
               "when translating this check the what's this functions and tool tips to place the "
               "'\\n's correctly");
    help_simplify_spaces += "\n\n"
            + tr("Warning: checking this option leads to column \noverlaping if the columns in the "
                 "ASCII file don't \nhave the same number of rows.",
                 "when translating this check the what's this functions and tool tips to place the "
                 "'\\n's correctly");
    help_simplify_spaces += "\n"
            + tr("To avoid this problem you should precisely \ndefine the column separator using "
                 "TAB and \nSPACE characters.",
                 "when translating this check the what's this functions and tool tips to place the "
                 "'\\n's correctly");
    d_simplify_spaces->setWhatsThis(help_simplify_spaces);
    d_simplify_spaces->setToolTip(help_simplify_spaces);

    advanced_layout->addWidget(new QLabel(tr("Decimal Separators")), 3, 0);
    boxDecimalSeparator = new QComboBox();
    boxDecimalSeparator->addItem(tr("default") + " (" + QLocale().toString(1000.0, 'f', 1) + ")");
    boxDecimalSeparator->addItem(QLocale::c().toString(1000.0, 'f', 1));
    boxDecimalSeparator->addItem(QLocale(QLocale::German).toString(1000.0, 'f', 1));
    boxDecimalSeparator->addItem(QLocale(QLocale::French).toString(1000.0, 'f', 1));
    advanced_layout->addWidget(boxDecimalSeparator, 3, 1);

    d_convert_to_numeric = new QCheckBox(tr("&Numeric data"));
    connect(d_convert_to_numeric, SIGNAL(toggled(bool)), boxDecimalSeparator,
            SLOT(setEnabled(bool)));
    advanced_layout->addWidget(d_convert_to_numeric, 3, 2, 1, 2);

    advanced_layout->addWidget(new QLabel(tr("Text encoding:")), 4, 0);
    d_encoding = new QComboBox();
    d_encoding->addItem(tr("Auto"), QString());
    d_encoding->addItem(QStringLiteral("UTF-8"), QStringLiteral("UTF-8"));
    d_encoding->addItem(QStringLiteral("Shift-JIS"), QStringLiteral("Shift-JIS"));
    d_encoding->addItem(QStringLiteral("UTF-16"), QStringLiteral("UTF-16"));
    d_encoding->addItem(QStringLiteral("Latin-1"), QStringLiteral("Latin-1"));
    advanced_layout->addWidget(d_encoding, 4, 1);

    advanced_layout->addWidget(new QLabel(tr("Unit row:")), 4, 2);
    d_units_row = new QComboBox();
    d_units_row->addItem(tr("Auto"), -1);
    d_units_row->addItem(tr("2nd line holds units"), 1);
    d_units_row->addItem(tr("No unit row"), 0);
    advanced_layout->addWidget(d_units_row, 4, 3);

    QHBoxLayout *meta_options_layout = new QHBoxLayout();
    d_remember_options = new QCheckBox(tr("Re&member the above options"));
    meta_options_layout->addWidget(d_remember_options);
    d_help_button = new QPushButton(tr("&Help"));
    connect(d_help_button, SIGNAL(clicked()), this, SLOT(displayHelp()));
    meta_options_layout->addStretch();
    meta_options_layout->addWidget(d_help_button);
    main_layout->addLayout(meta_options_layout);
}

void ImportASCIIDialog::setColumnSeparator(const QString &sep)
{
    // index 0 is "Auto-detect"; the fixed entries follow in this order
    static const char *const fixed[] = { "\t", " ", ";\t", ",\t", "; ", ", ", ";", "," };
    if (sep.isEmpty() || sep.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0) {
        d_column_separator->setCurrentIndex(0);
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if (sep == QLatin1String(fixed[i])) {
            d_column_separator->setCurrentIndex(i + 1);
            return;
        }
    }
    QString separator = sep;
    d_column_separator->setEditText(separator.replace(" ", "\\s").replace("\t", "\\t"));
}

const QString ImportASCIIDialog::columnSeparator() const
{
    // "Auto-detect" -> empty string: the import filter then detects the separator itself
    if (d_column_separator->currentIndex() == 0
        && d_column_separator->currentText() == d_column_separator->itemText(0))
        return QString();

    QString sep = d_column_separator->currentText();

    // A TAB separator stays a TAB, also with "Simplify white spaces" (the filter simplifies
    // inside each field, so empty TAB-separated cells keep their column).
    sep.replace(tr("TAB"), "\t", Qt::CaseInsensitive);
    sep.replace(tr("SPACE"), " ", Qt::CaseInsensitive);
    sep.replace("\\s", " ");
    sep.replace("\\t", "\t");

    return sep;
}
void ImportASCIIDialog::displayHelp()
{
    QString s = tr("The column separator can be customized. The following special codes can be "
                   "used:\n\\t for a TAB character \n\\s for a SPACE");
    s += "\n" + tr("The separator must not contain the following characters: 0-9eE.+-") + "\n\n";
    s += tr("Remove white spaces from line ends") + ":\n";
    s += tr("By checking this option all white spaces will be removed from the beginning and the "
            "end of the lines in the ASCII file.")
            + "\n\n";
    s += tr("Simplify white spaces") + ":\n";
    s += tr("By checking this option all white spaces will be removed from the beginning and the "
            "end of the lines and each sequence of internal whitespaces (including the TAB "
            "character) will be replaced with a single space.");

    s += "\n\n"
            + tr("Warning: using these two last options leads to column overlaping if the columns "
                 "in the ASCII file don't have the same number of rows.");
    s += "\n"
            + tr("To avoid this problem you should precisely define the column separator using TAB "
                 "and SPACE characters.");

    QMessageBox::about(this, tr("Help"), s);
}

void ImportASCIIDialog::updateImportMode(int mode)
{
    if (mode == Overwrite)
        setFileMode(QFileDialog::ExistingFile);
    else
        setFileMode(QFileDialog::ExistingFiles);
}

void ImportASCIIDialog::closeEvent(QCloseEvent *e)
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (app) {
        app->d_extended_import_ASCII_dialog = this->isExtended();
        app->d_ASCII_file_filter = this->selectedNameFilter();
    }

    e->accept();
}

QLocale ImportASCIIDialog::decimalSeparators()
{
    QLocale locale;
    switch (boxDecimalSeparator->currentIndex()) {
    case 0:
        locale = QLocale::system();
        break;
    case 1:
        locale = QLocale::c();
        break;
    case 2:
        locale = QLocale(QLocale::German);
        break;
    case 3:
        locale = QLocale(QLocale::French);
        break;
    }
    return locale;
}
