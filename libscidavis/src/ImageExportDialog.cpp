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
#include "Graph3D.h"
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
    reg("Transparent background", "背景を透明にする");
    reg("PNG image (*.png)", "PNG 画像 (*.png)");
    reg("TIFF image (*.tif *.tiff)", "TIFF 画像 (*.tif *.tiff)");
    reg("EMF image (*.emf)", "EMF 画像 (*.emf)");
    reg("Size: %1 x %2 px (3D graph)", "寸法: 幅 %1 × 高さ %2 px (3D グラフ)");
}

//! The graph the dialog is about (active MDI window), if it is a 2D graph.
MultiLayer *activeGraphWindow(QObject *parentObj)
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parentObj);
    if (!app)
        return nullptr;
    return qobject_cast<MultiLayer *>(app->d_workspace.activeSubWindow());
}

//! The 3D graph the dialog is about (active MDI window), if it is one (D2-1).
Graph3D *activeGraph3D(QObject *parentObj)
{
    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parentObj);
    if (!app)
        return nullptr;
    return qobject_cast<Graph3D *>(app->d_workspace.activeSubWindow());
}

//! Pixel size Graph3D::exportImage() will render (mirrors its render-scale logic).
bool graph3DExportPixelSize(Graph3D *g, int *pxW, int *pxH)
{
    if (!g || !g->sp)
        return false;
    QSize s = g->sp->size();
    if (s.width() <= 0 || s.height() <= 0)
        s = QSize(600, 480);
    const int propDpi = g->property("export_dpi").toInt();
    const int effDpi = (propDpi >= 72) ? qMin(propDpi, 600) : 300;
    double scale = double(effDpi) / 96.0;
    const int maxDim = qMax(s.width(), s.height());
    if (maxDim * scale > 3200.0)
        scale = qMax(1.0, 3200.0 / double(maxDim));
    *pxW = qMax(100, int(std::lround(s.width() * scale)));
    *pxH = qMax(100, int(std::lround(s.height() * scale)));
    return true;
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

QString ImageExportDialog::displayFilterFor(const QString &filter)
{
    registerImageExportDialogTranslationsOnce();
    const QString f = normalizedFilter(filter);
    if (f == QLatin1String("*.tif"))
        return tr("TIFF image (*.tif *.tiff)");
    if (f == QLatin1String("*.pdf"))
        return QStringLiteral("PDF (*.pdf)");
    if (f == QLatin1String("*.svg"))
        return QStringLiteral("SVG (*.svg)");
    if (f == QLatin1String("*.emf"))
        return tr("EMF image (*.emf)");
    return tr("PNG image (*.png)");
}

QStringList ImageExportDialog::displayNameFilters()
{
    QStringList out;
    const QStringList all = supportedFilters();
    for (const QString &cand : all)
        out << displayFilterFor(cand);
    return out;
}

QString ImageExportDialog::selectedNameFilter() const
{
    return normalizedFilter(QFileDialog::selectedNameFilter());
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

    setNameFilters(displayNameFilters());
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
    const bool initKeepAspect = app ? app->d_keep_plot_aspect : true;
    const int initQuality = app ? app->d_export_quality : 100;
    const bool initTransparency = app ? app->d_export_transparency : false;

    d_advanced_options = new QStackedWidget();

    d_vector_options = new QGroupBox();
    QGridLayout *vector_layout = new QGridLayout(d_vector_options);
    d_advanced_options->addWidget(d_vector_options);

    // D2-3: a vector PDF has no DPI, so there is no DPI row on this page. d_resolution only
    // exists as a hidden value holder (it follows the raster DPI one way, never the reverse).
    d_resolution = new QSpinBox(d_vector_options);
    d_resolution->setRange(72, 2400);
    d_resolution->setSingleStep(50);
    d_resolution->setValue(qBound(72, initDpi, 2400));
    d_resolution->hide();

    d_color = new QCheckBox();
    d_color->setText(tr("Export in &color"));
    d_color->setChecked(initColor);
    vector_layout->addWidget(d_color, 0, 0, 1, 2);

    // D2-5: "Export to standard page size" (30 sizes) and its orientation box were removed;
    // pageSize()/pageOrientation() now always return Custom/Portrait.

    d_keep_aspect = new QCheckBox();
    d_keep_aspect->setText(tr("&Keep aspect ratio"));
    d_keep_aspect->setChecked(initKeepAspect);
    vector_layout->addWidget(d_keep_aspect, 1, 0, 1, 2);

    d_raster_options = new QGroupBox();
    QGridLayout *raster_layout = new QGridLayout(d_raster_options);
    d_advanced_options->addWidget(d_raster_options);

    // D2-5: PNG and TIFF (LZW) are lossless, so the quality spin box has no effect and is not
    // shown. The widget stays as a hidden holder because quality() feeds the export calls.
    d_quality = new QSpinBox(d_raster_options);
    d_quality->setRange(1, 100);
    d_quality->setValue(qBound(1, initQuality, 100));
    d_quality->hide();

    raster_layout->addWidget(new QLabel(tr("Export DPI:")), 0, 0);
    d_raster_resolution = new QSpinBox();
    d_raster_resolution->setRange(72, 2400);
    d_raster_resolution->setSingleStep(50);
    d_raster_resolution->setValue(qBound(72, initDpi, 2400));
    raster_layout->addWidget(d_raster_resolution, 0, 1);

    d_transparency = new QCheckBox(tr("Transparent background"));
    d_transparency->setChecked(initTransparency);
    raster_layout->addWidget(d_transparency, 1, 0, 1, 2);

    // One-way link raster DPI -> hidden PDF holder (D2-3: no PDF -> raster back link).
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
        int px3dW = 0, px3dH = 0;
        if (Graph3D *g3d = activeGraph3D(parent())) {
            // D2-1: 3D graphs have no page in mm, only the rendered pixel size.
            if (raster && graph3DExportPixelSize(g3d, &px3dW, &px3dH))
                text += QStringLiteral("\n")
                        + tr("Size: %1 x %2 px (3D graph)").arg(px3dW).arg(px3dH);
        } else if (graphPageSizeMm(activeGraphWindow(parent()), &wMm, &hMm, &physical)) {
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

void ImageExportDialog::updateAdvancedOptions(const QString &filterText)
{
    // The signal delivers the descriptive name ("PNG 画像 (*.png)"); reduce it to the pattern.
    const QString filter = normalizedFilter(filterText);
    updateInfoLabel();
    if (filter == QLatin1String("*.svg") || filter == QLatin1String("*.emf")) {
        // D2-4: only disable/hide for display. The toggle's checked state is the user's own
        // preference (saved by saveOptionsToApp()) and must not be overwritten here.
        d_extension_toggle->setEnabled(false);
        d_advanced_options->setVisible(false);
        return;
    }
    d_extension_toggle->setEnabled(true);
    d_advanced_options->setVisible(d_extension_toggle->isChecked());
    if (filter == QLatin1String("*.pdf"))
        d_advanced_options->setCurrentIndex(0);
    else {
        d_advanced_options->setCurrentIndex(1);
        // PNG and TIFF (the only raster formats offered) both carry an alpha channel.
        if (d_transparency)
            d_transparency->setEnabled(true);
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
        app->d_keep_plot_aspect = d_keep_aspect->isChecked();
        // d_export_vector_size / d_export_orientation are left alone (option removed, D2-5).
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
    // D2-5: the standard-page-size option was removed; the page follows the graph.
    return QPageSize(QPageSize::Custom);
}

QPageLayout::Orientation ImageExportDialog::pageOrientation() const
{
    return QPageLayout::Portrait;
}

void ImageExportDialog::setPageSize(QPageSize size)
{
    Q_UNUSED(size);
}

void ImageExportDialog::setOrientation(QPrinter::Orientation orientation)
{
    Q_UNUSED(orientation);
}

void ImageExportDialog::selectFilter(const QString &filter)
{
    // Settings from older builds may hold ".png", "*.tiff", "*.jpg" or "*.eps": map them onto
    // the supported list (PNG when nothing matches), then select the descriptive entry.
    const QString f = normalizedFilter(filter);
    QFileDialog::selectNameFilter(displayFilterFor(f));
    updateAdvancedOptions(f);
}
