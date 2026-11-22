/***************************************************************************
    File                 : ImageExportDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006,2007 by Ion Vasilief,
                           Tilman Benkert, Knut Franke
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net,
                           knut.franke*gmx.de
    Description          : QFileDialog extended with options for image export

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
#include "ImageExportDialog.h"
#include "ApplicationWindow.h"
#include "MultiLayer.h"
#include "I18nJapanese.h"

#include <QStackedWidget>
#include <QImageWriter>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QPrinter>
#include <QLabel>
#include <QComboBox>
#include <QCloseEvent>
#include <QMdiSubWindow>
#include <cmath>

namespace {
void registerImageExportDialogTranslationsOnce()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("PNG: slides, Word and web (raster, lossless).",
        "PNG: スライド・Word・Web 用 (画像、劣化なし)。");
    reg("TIFF: raster figures for journal submission (LZW, lossless).",
        "TIFF: 論文投稿用の画像 (LZW 圧縮、劣化なし)。");
    reg("PDF: vector, for papers and LaTeX; lines and text stay sharp at any size.",
        "PDF: ベクター。論文・LaTeX 用。拡大しても線と文字がにじみません。");
    reg("SVG: vector, for editing in Inkscape or Illustrator.",
        "SVG: ベクター。Inkscape や Illustrator で編集する用。");
    reg("EMF: vector for Word and PowerPoint on Windows.",
        "EMF: Windows の Word・PowerPoint 用のベクター。");
    reg("Size: %1 mm x %2 mm, %3 dpi -> %4 x %5 px",
        "寸法: 幅 %1 mm × 高さ %2 mm、%3 dpi → %4 × %5 px");
    reg("Size: %1 mm x %2 mm (vector, no pixel size)",
        "寸法: 幅 %1 mm × 高さ %2 mm (ベクター形式なので画素数はありません)");
    reg("Each graph is exported at its own page size.", "各グラフは自分のページ寸法で出力します。");
    reg("No physical page size is set (pixel page at 96 dpi). Apply a preset to fix the size in mm.",
        "物理寸法が未設定です (96 dpi のピクセルページ)。mm で寸法を決めるにはプリセットを適用してください。");
    reg("Export DPI:", "出力解像度 (DPI):");
}

//! The graph the dialog is about (active MDI window), if it is a 2D graph.
MultiLayer *activeGraphWindow(QObject *parentObj)
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parentObj);
    if (!app)
        return nullptr;
    return qobject_cast<MultiLayer *>(app->d_workspace.activeSubWindow());
}

//! Page size of \a ml in millimetres; returns false for an empty graph.
bool graphPageSizeMm(MultiLayer *ml, double *wMm, double *hMm, bool *physical)
{
    if (!ml)
        return false;
    const MultiLayer::PageUnit unit = ml->pageUnit();
    double w = ml->pageWidthPhysical();
    double h = ml->pageHeightPhysical();
    *physical = (unit != MultiLayer::UnitPx);
    switch (unit) {
    case MultiLayer::UnitMm:
        break;
    case MultiLayer::UnitCm:
        w *= 10.0;
        h *= 10.0;
        break;
    case MultiLayer::UnitInch:
        w *= 25.4;
        h *= 25.4;
        break;
    case MultiLayer::UnitPx:
    default: {
        QWidget *cv = ml->canvasWidget();
        const double z = qMax(0.25, ml->pageZoom());
        if (cv && cv->width() > 10 && cv->height() > 10) {
            w = double(cv->width()) / z;
            h = double(cv->height()) / z;
        }
        w = w * 25.4 / 96.0;
        h = h * 25.4 / 96.0;
        break;
    }
    }
    if (!(w > 0.5) || !(h > 0.5))
        return false;
    *wMm = w;
    *hMm = h;
    return true;
}
} // namespace

QStringList ImageExportDialog::supportedFilters()
{
    // B9: only the formats a lab member actually needs; Qt's long list (ICO, XBM, ...) and
    // EPS/PS (which Qt 5 cannot write; it silently produced a PDF) are gone.
    QStringList filters;
    filters << QStringLiteral("*.png") << QStringLiteral("*.tif") << QStringLiteral("*.pdf")
            << QStringLiteral("*.svg");
#ifdef Q_OS_WIN
    filters << QStringLiteral("*.emf");
#endif
    return filters;
}

QString ImageExportDialog::normalizedFilter(const QString &filter)
{
    const QString f = filter.trimmed().toLower();
    const QStringList all = supportedFilters();
    for (const QString &cand : all) {
        const QString ext = cand.mid(1); // ".png"
        if (f == cand || f == ext || f.endsWith(ext) || f.contains(cand))
            return cand;
    }
    if (f.contains(QLatin1String(".tiff")))
        return QStringLiteral("*.tif");
    return QStringLiteral("*.png");
}

QString ImageExportDialog::formatDescription(const QString &filter)
{
    registerImageExportDialogTranslationsOnce();
    const QString f = normalizedFilter(filter);
    if (f == QLatin1String("*.tif"))
        return tr("TIFF: raster figures for journal submission (LZW, lossless).");
    if (f == QLatin1String("*.pdf"))
        return tr("PDF: vector, for papers and LaTeX; lines and text stay sharp at any size.");
    if (f == QLatin1String("*.svg"))
        return tr("SVG: vector, for editing in Inkscape or Illustrator.");
    if (f == QLatin1String("*.emf"))
        return tr("EMF: vector for Word and PowerPoint on Windows.");
    return tr("PNG: slides, Word and web (raster, lossless).");
}

QString ImageExportDialog::sizeSummary(double wMm, double hMm, int dpi, bool raster)
{
    registerImageExportDialogTranslationsOnce();
    const QString wS = QString::number(wMm, 'f', 1);
    const QString hS = QString::number(hMm, 'f', 1);
    if (!raster)
        return tr("Size: %1 mm x %2 mm (vector, no pixel size)")
                .arg(wS, hS);
    const int pxW = qMax(1, int(std::lround(wMm / 25.4 * double(dpi))));
    const int pxH = qMax(1, int(std::lround(hMm / 25.4 * double(dpi))));
    return tr("Size: %1 mm x %2 mm, %3 dpi -> %4 x %5 px")
            .arg(wS, hS)
            .arg(dpi)
            .arg(pxW)
            .arg(pxH);
}

ImageExportDialog::ImageExportDialog(QWidget *parent, bool vector_options, bool extended,
                                     Qt::WindowFlags flags)
    : ExtensibleFileDialog(parent, extended, flags)
{
    registerImageExportDialogTranslationsOnce();
    setWindowTitle(tr("Choose a filename to save under"));
    setAcceptMode(QFileDialog::AcceptSave);

    setNameFilters(supportedFilters());
    setFileMode(QFileDialog::AnyFile);

    initAdvancedOptions();
    d_vector_options->setEnabled(vector_options);
    setExtensionWidget(d_advanced_options);

    // B9: always-visible line with the purpose of the format and the resulting size.
    d_info_label = new QLabel(this);
    d_info_label->setWordWrap(true);
    d_info_label->setObjectName(QStringLiteral("imageExportInfoLabel"));
    if (QGridLayout *mainLayout = qobject_cast<QGridLayout *>(layout()))
        mainLayout->addWidget(d_info_label, mainLayout->rowCount(), 0, 1,
                              mainLayout->columnCount());
    else if (layout())
        layout()->addWidget(d_info_label);
    connect(d_resolution, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { updateInfoLabel(); });

#if QT_VERSION >= 0x040300
    connect(this, SIGNAL(filterSelected(const QString &)), this,
            SLOT(updateAdvancedOptions(const QString &)));
#else
    QList<QComboBox *> combo_boxes = findChildren<QComboBox *>();
    if (combo_boxes.size() >= 2)
        connect(combo_boxes[1], SIGNAL(currentIndexChanged(const QString &)), this,
                SLOT(updateAdvancedOptions(const QString &)));
#endif
    updateAdvancedOptions(selectedNameFilter());
}

void ImageExportDialog::initAdvancedOptions()
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    // B2: start from the graph's page DPI when it has a physical page (e.g. 600 dpi for the
    // ISIJ preset); otherwise from the last used value (default 600).
    int initDpi = (app && app->d_export_resolution > 0) ? app->d_export_resolution : 600;
    if (MultiLayer *ml = activeGraphWindow(app)) {
        if (ml->pageUnit() != MultiLayer::UnitPx && ml->pageDpi() > 0)
            initDpi = ml->pageDpi();
    }
    const bool initColor = app ? app->d_export_color : true;
    const int initVecSize = app ? app->d_export_vector_size : int(QPageSize::Custom);
    const int initOrient = app ? app->d_export_orientation : int(QPageLayout::Portrait);
    const bool initKeepAspect = app ? app->d_keep_plot_aspect : true;
    const int initQuality = app ? app->d_export_quality : 100;
    const bool initTransparency = app ? app->d_export_transparency : false;

    d_advanced_options = new QStackedWidget();

    d_vector_options = new QGroupBox();
    QGridLayout *vector_layout = new QGridLayout(d_vector_options);
    d_advanced_options->addWidget(d_vector_options);

    vector_layout->addWidget(new QLabel(tr("Resolution (DPI)")), 1, 0);
    d_resolution = new QSpinBox();
    d_resolution->setRange(72, 2400);
    d_resolution->setSingleStep(50);
    d_resolution->setValue(qBound(72, initDpi, 2400));
    vector_layout->addWidget(d_resolution, 1, 1);
    d_resolution->setEnabled(true);

    d_color = new QCheckBox();
    d_color->setText(tr("Export in &color"));
    d_color->setChecked(initColor);
    vector_layout->addWidget(d_color, 2, 0, 1, 2);

    d_standard_page = new QCheckBox();
    d_standard_page->setText(tr("Export to &standard page size"));
    vector_layout->addWidget(d_standard_page, 3, 0, 1, 2);

    d_box_page_size = new QComboBox();
    d_box_page_size->addItem(tr("A0 - 841 x 1189 mm"), QVariant(QPageSize::A0));
    d_box_page_size->addItem(tr("A1 - 594 x 841 mm"), QVariant(QPageSize::A1));
    d_box_page_size->addItem(tr("A2 - 420 x 594 mm"), QVariant(QPageSize::A2));
    d_box_page_size->addItem(tr("A3 - 297 x 420 mm"), QVariant(QPageSize::A3));
    d_box_page_size->addItem(tr("A4 - 210 x 297 mm"), QVariant(QPageSize::A4));
    d_box_page_size->addItem(tr("A5 - 148 x 210 mm"), QVariant(QPageSize::A5));
    d_box_page_size->addItem(tr("A6 - 105 x 148 mm"), QVariant(QPageSize::A6));
    d_box_page_size->addItem(tr("A7 - 74 x 105 mm"), QVariant(QPageSize::A7));
    d_box_page_size->addItem(tr("A8 - 52 x 74 mm"), QVariant(QPageSize::A8));
    d_box_page_size->addItem(tr("A9 - 37 x 52 mm"), QVariant(QPageSize::A9));
    d_box_page_size->addItem(tr("B0 - 1030 x 1456 mm"), QVariant(QPageSize::B0));
    d_box_page_size->addItem(tr("B1 - 728 x 1030 mm"), QVariant(QPageSize::B1));
    d_box_page_size->addItem(tr("B2 - 515 x 728 mm"), QVariant(QPageSize::B2));
    d_box_page_size->addItem(tr("B3 - 364 x 515 mm"), QVariant(QPageSize::B3));
    d_box_page_size->addItem(tr("B4 - 257 x 364 mm"), QVariant(QPageSize::B4));
    d_box_page_size->addItem(tr("B5 - 182 x 257 mm"), QVariant(QPageSize::B5));
    d_box_page_size->addItem(tr("B6 - 128 x 182 mm"), QVariant(QPageSize::B6));
    d_box_page_size->addItem(tr("B7 - 91 x 128 mm"), QVariant(QPageSize::B7));
    d_box_page_size->addItem(tr("B8 - 64 x 91 mm"), QVariant(QPageSize::B8));
    d_box_page_size->addItem(tr("B9 - 45 x 64 mm"), QVariant(QPageSize::B9));
    d_box_page_size->addItem(tr("B10 - 32 x 45 mm"), QVariant(QPageSize::B10));
    d_box_page_size->addItem(tr("C5E - 163 x 226 mm"), QVariant(QPageSize::C5E));
    d_box_page_size->addItem(tr("Comm10E - 105 x 241 mm"), QVariant(QPageSize::Comm10E));
    d_box_page_size->addItem(tr("DLE - 110 x 220 mm"), QVariant(QPageSize::DLE));
    d_box_page_size->addItem(tr("Executive - 191 x 254 mm"), QVariant(QPageSize::Executive));
    d_box_page_size->addItem(tr("Folio 210 x 330 mm"), QVariant(QPageSize::Folio));
    d_box_page_size->addItem(tr("Ledger 432 x 279 mm"), QVariant(QPageSize::Ledger));
    d_box_page_size->addItem(tr("Legal 216 x 356 mm"), QVariant(QPageSize::Legal));
    d_box_page_size->addItem(tr("Letter 216 x 279 mm"), QVariant(QPageSize::Letter));
    d_box_page_size->addItem(tr("Tabloid 279 x 432 mm"), QVariant(QPageSize::Tabloid));

    vector_layout->addWidget(new QLabel(tr("Page size")), 4, 0);
    setPageSize(QPageSize((QPageSize::PageSizeId)initVecSize));
    d_standard_page->setChecked(initVecSize != QPageSize::Custom);
    d_box_page_size->setEnabled(initVecSize != QPageSize::Custom);
    vector_layout->addWidget(d_box_page_size, 4, 1, 1, 2);

    vector_layout->addWidget(new QLabel(tr("Orientation")), 5, 0);
    d_box_page_orientation = new QComboBox();
    d_box_page_orientation->addItem(tr("Portrait", "page orientation"),
                                    QVariant(QPageLayout::Portrait));
    d_box_page_orientation->addItem(tr("Landscape", "page orientation"),
                                    QVariant(QPageLayout::Landscape));
    d_box_page_orientation->setCurrentIndex(initOrient);
    d_box_page_orientation->setEnabled(initVecSize != QPrinter::Custom);
    vector_layout->addWidget(d_box_page_orientation, 5, 1, 1, 2);

    connect(d_standard_page, SIGNAL(toggled(bool)), d_box_page_size, SLOT(setEnabled(bool)));
    connect(d_standard_page, SIGNAL(toggled(bool)), d_box_page_orientation, SLOT(setEnabled(bool)));

    d_keep_aspect = new QCheckBox();
    d_keep_aspect->setText(tr("&Keep aspect ratio"));
    d_keep_aspect->setChecked(initKeepAspect);
    vector_layout->addWidget(d_keep_aspect, 6, 0, 1, 2);

    d_raster_options = new QGroupBox();
    QGridLayout *raster_layout = new QGridLayout(d_raster_options);
    d_advanced_options->addWidget(d_raster_options);

    raster_layout->addWidget(new QLabel(tr("Image quality")), 1, 0);
    d_quality = new QSpinBox();
    d_quality->setRange(1, 100);
    d_quality->setValue(qBound(1, initQuality, 100));
    raster_layout->addWidget(d_quality, 1, 1);

    raster_layout->addWidget(new QLabel(tr("Export DPI:")), 2, 0);
    d_raster_resolution = new QSpinBox();
    d_raster_resolution->setRange(72, 2400);
    d_raster_resolution->setSingleStep(50);
    d_raster_resolution->setValue(qBound(72, initDpi, 2400));
    raster_layout->addWidget(d_raster_resolution, 2, 1);

    d_transparency = new QCheckBox(tr("Transparency"));
    d_transparency->setChecked(initTransparency);
    raster_layout->addWidget(d_transparency, 3, 0, 1, 2);

    connect(d_resolution, QOverload<int>::of(&QSpinBox::valueChanged),
            d_raster_resolution, &QSpinBox::setValue);
    connect(d_raster_resolution, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { updateInfoLabel(); });
    connect(d_raster_resolution, QOverload<int>::of(&QSpinBox::valueChanged),
            d_resolution, &QSpinBox::setValue);
}

int ImageExportDialog::resolution() const
{
    if (d_advanced_options && d_advanced_options->currentIndex() == 1 && d_raster_resolution)
        return d_raster_resolution->value();
    return d_resolution ? d_resolution->value() : 300;
}

void ImageExportDialog::setResolution(int dpi)
{
    const int clamped = qBound(72, dpi, 2400);
    if (d_resolution)
        d_resolution->setValue(clamped);
    if (d_raster_resolution)
        d_raster_resolution->setValue(clamped);
}

void ImageExportDialog::setTransparency(bool on)
{
    if (d_transparency)
        d_transparency->setChecked(on);
}

void ImageExportDialog::updateInfoLabel()
{
    if (!d_info_label)
        return;
    const QString filter = normalizedFilter(selectedNameFilter());
    const bool raster = (filter == QLatin1String("*.png") || filter == QLatin1String("*.tif"));
    QString text = formatDescription(filter);
    if (fileMode() == QFileDialog::Directory) {
        text += QStringLiteral("\n") + tr("Each graph is exported at its own page size.");
    } else {
        double wMm = 0.0, hMm = 0.0;
        bool physical = false;
        if (graphPageSizeMm(activeGraphWindow(parent()), &wMm, &hMm, &physical)) {
            text += QStringLiteral("\n") + sizeSummary(wMm, hMm, resolution(), raster);
            if (!physical)
                text += QStringLiteral("\n")
                        + tr("No physical page size is set (pixel page at 96 dpi). Apply a "
                             "preset to fix the size in mm.");
        }
    }
    d_info_label->setText(text);
}

QString ImageExportDialog::infoText() const
{
    return d_info_label ? d_info_label->text() : QString();
}

void ImageExportDialog::showEvent(QShowEvent *e)
{
    updateInfoLabel();
    ExtensibleFileDialog::showEvent(e);
}

void ImageExportDialog::updateAdvancedOptions(const QString &filter)
{
    updateInfoLabel();
    if (filter.contains("*.svg", Qt::CaseInsensitive)
        || filter.contains("*.emf", Qt::CaseInsensitive)) {
        d_extension_toggle->setChecked(false);
        d_extension_toggle->setEnabled(false);
        return;
    }
    d_extension_toggle->setEnabled(true);
    if (filter.contains("*.pdf", Qt::CaseInsensitive))
        d_advanced_options->setCurrentIndex(0);
    else {
        d_advanced_options->setCurrentIndex(1);
        const bool supportsAlpha = filter.contains("*.png", Qt::CaseInsensitive)
                || filter.contains("*.tif", Qt::CaseInsensitive)
                || filter.contains("*.webp", Qt::CaseInsensitive)
                || filter.contains("*.ico", Qt::CaseInsensitive);
        if (d_transparency)
            d_transparency->setEnabled(supportsAlpha);
    }
}

void ImageExportDialog::saveOptionsToApp()
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(this->parent());
    if (app) {
        app->d_extended_export_dialog = this->isExtended();
        app->d_image_export_filter = this->selectedNameFilter();
        app->d_export_quality = d_quality->value();
        app->d_export_resolution = resolution();
        app->d_export_transparency = transparency();
        app->d_export_color = d_color->isChecked();
        app->d_export_vector_size = (int)pageSize().id();
        app->d_keep_plot_aspect = d_keep_aspect->isChecked();
        app->d_export_orientation = d_box_page_orientation->currentIndex();
    }
}

void ImageExportDialog::done(int r)
{
    if (r == QDialog::Accepted)
        saveOptionsToApp();
    ExtensibleFileDialog::done(r);
}

void ImageExportDialog::closeEvent(QCloseEvent *e)
{
    saveOptionsToApp();
    e->accept();
}

QPageSize ImageExportDialog::pageSize() const
{
    if (!d_standard_page->isChecked())
        return QPageSize(QPageSize::Custom);

    QPageSize size;
    switch (d_box_page_size->currentIndex()) {
    case -1:
        size = QPageSize(QPageSize::A4);
        break;
    default:
        size = QPageSize(
                (QPageSize::PageSizeId)d_box_page_size->itemData(d_box_page_size->currentIndex())
                        .toInt());
        break;
    }
    return size;
}

QPageLayout::Orientation ImageExportDialog::pageOrientation() const
{
    QPageLayout::Orientation orientation;
    switch (d_box_page_orientation->currentIndex()) {
    case -1:
        orientation = QPageLayout::Portrait;
        break;
    default:
        orientation = (QPageLayout::Orientation)d_box_page_orientation
                              ->itemData(d_box_page_orientation->currentIndex())
                              .toInt();
        break;
    }
    return orientation;
}

void ImageExportDialog::setPageSize(QPageSize size)
{
    if (size == QPageSize(QPageSize::Custom))
        return;

    int index = d_box_page_size->findData(QVariant(size.id()));
    if (index != -1)
        d_box_page_size->setCurrentIndex(index);
    else {
        index = d_box_page_size->findData(QVariant(QPageSize::A4));
        Q_ASSERT(index != -1);
        d_box_page_size->setCurrentIndex(index);
    }
}

void ImageExportDialog::setOrientation(QPrinter::Orientation orientation)
{
    int index = d_box_page_orientation->findData(QVariant(orientation));
    if (index != -1)
        d_box_page_orientation->setCurrentIndex(index);
    else
        d_box_page_orientation->setCurrentIndex(0);
}

void ImageExportDialog::selectFilter(const QString &filter)
{
    // Settings from older builds may hold ".png", "*.jpg" or "*.eps": map them onto the
    // supported list (PNG when nothing matches).
    const QString f = normalizedFilter(filter);
    QFileDialog::selectNameFilter(f);
    updateAdvancedOptions(f);
}
