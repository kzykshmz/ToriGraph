/***************************************************************************
    File                 : ImageExportDialog.h
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
#ifndef IMAGEEXPORTDIALOG_H
#define IMAGEEXPORTDIALOG_H

#include "ExtensibleFileDialog.h"

#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPrinter>

class QStackedWidget;
class QGroupBox;
class QLabel;
class QShowEvent;

//! QFileDialog extended with options for image export
class ImageExportDialog : public ExtensibleFileDialog
{
    Q_OBJECT

private:
    //! Create #d_advanced_options and everything it contains.
    void initAdvancedOptions();

    //! Container widget for all advanced options.
    QStackedWidget *d_advanced_options;
    // vector format options
    //! Container widget for all options available for vector formats.
    QGroupBox *d_vector_options;
    //! Hidden DPI holder of the PDF page (D2-3: a vector PDF has no DPI, so the row is not shown;
    //! the value only follows the raster DPI so resolution() stays meaningful).
    QSpinBox *d_resolution;
    QCheckBox *d_color;
    QCheckBox *d_keep_aspect;
    // raster format options
    //! Container widget for all options available for raster formats.
    QGroupBox *d_raster_options;
    //! Hidden (D2-5): every offered raster format (PNG/TIFF) is lossless, so quality is fixed.
    QSpinBox *d_quality;
    QSpinBox *d_raster_resolution;
    QCheckBox *d_transparency;
    //! Format purpose + "w mm x h mm, dpi -> px" summary (B9).
    QLabel *d_info_label = nullptr;
    void saveOptionsToApp();
    void updateInfoLabel();

public:
    //! Constructor
    /**
     * \param parent parent widget
     * \param vector_options whether advanced options are to be provided for export to vector
     * formats \param extended flag: show/hide the advanced options on start-up \param flags window
     * flags
     */
    ImageExportDialog(QWidget *parent = 0, bool vector_options = true, bool extended = true,
                      Qt::WindowFlags flags = Qt::Widget);
    //! For vector and raster formats: returns the output resolution (DPI) the user selected.
    int resolution() const;
    void setResolution(int dpi);
    //! For raster formats: returns whether transparent background is enabled.
    bool transparency() const { return d_transparency ? d_transparency->isChecked() : false; }
    void setTransparency(bool on);
    //! For vector formats: returns whether colors should be enabled for ouput (default: true).
    bool color() const { return d_color->isChecked(); }
    //! For vector formats: returns whether the output should preserve aspect ratio of the plot (default: true).
    bool keepAspect() const { return d_keep_aspect->isChecked(); }
    //! The standard-page-size option was removed (D2-5): always QPageSize::Custom.
    QPageSize pageSize() const;
    void setPageSize(QPageSize size);
    //! Always portrait (only meaningful together with a standard page size).
    QPageLayout::Orientation pageOrientation() const;
    void setOrientation(QPrinter::Orientation orientation);
    //! Return the quality (in percent) the user selected for export to raster formats.
    int quality() const { return d_quality->value(); }

    void selectFilter(const QString &filter);

    //! Hides QFileDialog::selectedNameFilter(): returns the bare pattern ("*.png", "*.tif",
    //! "*.pdf", ...) even though the dialog lists descriptive names (D2-7), so callers that store
    //! or compare the filter keep working.
    QString selectedNameFilter() const;

    //! B9: the offered formats ("*.png", "*.tif", "*.pdf", "*.svg", "*.emf").
    static QStringList supportedFilters();
    //! D2-7: descriptive names shown in the dialog, e.g. "PNG 画像 (*.png)".
    static QStringList displayNameFilters();
    //! D2-7: the descriptive name that belongs to a (bare or stored) filter.
    static QString displayFilterFor(const QString &filter);
    //! Maps any stored filter text (".png", "*.jpg", "*.eps", ...) to a supported filter.
    static QString normalizedFilter(const QString &filter);
    //! One-line description of what the format is for.
    static QString formatDescription(const QString &filter);
    //! "Size: w mm x h mm, dpi -> W x H px" (raster) or the vector variant.
    static QString sizeSummary(double wMm, double hMm, int dpi, bool raster);
    QString infoText() const;

protected slots:
    void done(int r) override;
    void closeEvent(QCloseEvent *) override;
    void showEvent(QShowEvent *) override;
    //! Update which options are visible and enabled based on the output format.
    void updateAdvancedOptions(const QString &filter);
};

#endif
