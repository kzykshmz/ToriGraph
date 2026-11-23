/***************************************************************************
    File                 : TextDialog.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Text label/axis label options dialog

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

#include "TextDialog.h"
#include "ApplicationWindow.h"
#include "PlotStyleLimits.h"

#include <QFontDialog>
#include <QColorDialog>
#include <QFont>
#include <QGroupBox>
#include <QTextEdit>
#include <QTextCursor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>

namespace {
//! Clamp a font's point size to the range shared with every other font-size spin box
//! (PlotStyleLimits::kFontSizeMin..kFontSizeMax), so a value typed or stored elsewhere is never
//! shown clamped in the spin box while the real font keeps a different size.
void clampFontPointSize(QFont &f)
{
    const double pt = PlotStyleLimits::effectiveFontPointSizeF(f, 12.0);
    const double clamped = qBound(PlotStyleLimits::kFontSizeMin, pt, PlotStyleLimits::kFontSizeMax);
    if (f.pointSizeF() <= 0.0 || qAbs(clamped - pt) > 1e-9)
        f.setPointSizeF(clamped);
}
} // namespace

TextDialog::TextDialog(TextType type, QWidget *parent, Qt::WindowFlags fl) : QDialog(parent, fl)
{
    setWindowTitle(tr("Text options"));
    setSizeGripEnabled(true);

    textType = type;
    selectedFont = QFont(QStringLiteral("Arial"));
    selectedFont.setPointSizeF(12.0);

    // top groupbox
    groupBox1 = new QGroupBox(QString());

    // grid layout for top groupbox
    QGridLayout *topLayout = new QGridLayout(groupBox1);
    // add text color label
    topLayout->addWidget(new QLabel(tr("Text Color")), 0, 0);

    colorBtn = new ColorButton();
    // add color button
    topLayout->addWidget(colorBtn, 0, 1);

    buttonOk = new QPushButton(tr("&OK"));
    buttonOk->setAutoDefault(true);
    buttonOk->setDefault(true);

    // add ok button
    topLayout->addWidget(buttonOk, 0, 3);

    // add font label
    topLayout->addWidget(new QLabel(tr("Font")), 1, 0);

    buttonFont = new QPushButton(tr("&Font"));
    buttonFont->setAutoDefault(false);
    boxFontSize = new PlotStyleLimits::FractionalPtSpinBox();
    boxFontSize->setRange(PlotStyleLimits::kFontSizeMin, PlotStyleLimits::kFontSizeMax);
    boxFontSize->setSingleStep(PlotStyleLimits::kFontSizeStep);
    boxFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(selectedFont, 12.0));
    // "Font Size (pt)" is the key already present in ja_dictionary.tsv.
    boxFontSize->setToolTip(tr("Font Size (pt)"));

    QHBoxLayout *fontRowLayout = new QHBoxLayout();
    fontRowLayout->setContentsMargins(0, 0, 0, 0);
    fontRowLayout->setSpacing(4);
    fontRowLayout->addWidget(buttonFont);
    fontRowLayout->addWidget(boxFontSize);

    // add font button + fractional pt spinbox
    topLayout->addLayout(fontRowLayout, 1, 1);

    buttonApply = new QPushButton(tr("&Apply"));
    buttonApply->setAutoDefault(false);

    // add apply button
    topLayout->addWidget(buttonApply, 1, 3);

    if (textType == TextDialog::AxisTitle) {
        // add label "alignment"
        topLayout->addWidget(new QLabel(tr("Alignment")), 2, 0);
        alignmentBox = new QComboBox();
        alignmentBox->addItem(tr("Center"));
        alignmentBox->addItem(tr("Left"));
        alignmentBox->addItem(tr("Right"));
        // add alignment combo box
        topLayout->addWidget(alignmentBox, 2, 1);
    } else {
        // add label "frame"
        topLayout->addWidget(new QLabel(tr("Frame")), 2, 0);
        backgroundBox = new QComboBox();
        backgroundBox->addItem(tr("None"));
        backgroundBox->addItem(tr("Rectangle"));
        backgroundBox->addItem(tr("Shadow"));
        topLayout->addWidget(backgroundBox, 2, 1);
    }

    buttonCancel = new QPushButton(tr("&Cancel"));
    buttonCancel->setAutoDefault(false);
    // add cancel button
    topLayout->addWidget(buttonCancel, 2, 3);

    if (textType == TextDialog::TextMarker) { // TODO: Sometime background features for axes lables
                                              // should be implemented
        topLayout->addWidget(new QLabel(tr("Opacity")), 3, 0);
        boxBackgroundTransparency = new QSpinBox();
        boxBackgroundTransparency->setRange(0, 255);
        boxBackgroundTransparency->setSingleStep(5);
        boxBackgroundTransparency->setWrapping(true);
        boxBackgroundTransparency->setSpecialValueText(tr("Transparent"));
        // add background button
        topLayout->addWidget(boxBackgroundTransparency, 3, 1);

        // add label "background color"
        topLayout->addWidget(new QLabel(tr("Background color")), 4, 0);
        backgroundBtn = new ColorButton(groupBox1);
        backgroundBtn->setEnabled(false);
        // add background button
        topLayout->addWidget(backgroundBtn, 4, 1);

        connect(boxBackgroundTransparency, SIGNAL(valueChanged(int)), this,
                SLOT(updateTransparency(int)));
        connect(backgroundBtn, SIGNAL(changed(const QColor &)), this,
                SLOT(pickBackgroundColor(const QColor &)));

        buttonDefault = new QPushButton(tr("Set As &Default"));
        topLayout->addWidget(buttonDefault, 3, 3);
        connect(buttonDefault, SIGNAL(clicked()), this, SLOT(setDefaultValues()));
    }

    // align the OK, Apply, and Cancel buttons to the right
    topLayout->setColumnStretch(2, 1);

    textEditBox = new QTextEdit();
    textEditBox->setAcceptRichText(false);
    textEditBox->setFont(selectedFont);

    formatButtons = new TextFormatButtons(textEditBox);
    formatButtons->toggleCurveButton(textType == TextDialog::TextMarker);

    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(textEditBox);

    // put everything together
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(groupBox1);
    mainLayout->addWidget(formatButtons);
    mainLayout->addWidget(textEditBox);
    setLayout(mainLayout);

    // signals and slots connections
    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonApply, SIGNAL(clicked()), this, SLOT(apply()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(buttonFont, SIGNAL(clicked()), this, SLOT(customFont()));
    connect(boxFontSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double pt) {
                if (pt <= 0.0)
                    return;
                selectedFont.setPointSizeF(pt);
                if (textEditBox)
                    textEditBox->setFont(selectedFont);
            });
}

void TextDialog::apply()
{
    if (selectedFont.family().trimmed().isEmpty())
        selectedFont.setFamily(QStringLiteral("Arial"));

    if (textType == TextDialog::AxisTitle) {
        emit changeAlignment(alignment());
        emit changeText(textEditBox->toPlainText());
        emit changeColor(colorBtn->color());
        emit changeFont(selectedFont);
    } else {
        QColor c = backgroundBtn->color();
        c.setAlpha(boxBackgroundTransparency->value());
        emit values(textEditBox->toPlainText(), angle(), backgroundBox->currentIndex(),
                    selectedFont, colorBtn->color(), c);
    }
}

void TextDialog::setDefaultValues()
{
    ApplicationWindow *app = (ApplicationWindow *)this->parent();
    if (!app)
        return;

    if (selectedFont.family().trimmed().isEmpty())
        selectedFont.setFamily(QStringLiteral("Arial"));

    QColor c = backgroundBtn->color();
    c.setAlpha(boxBackgroundTransparency->value());
    app->setLegendDefaultSettings(backgroundBox->currentIndex(), selectedFont, colorBtn->color(),
                                  c);
}

void TextDialog::accept()
{
    apply();
    close();
}

void TextDialog::setBackgroundType(int bkg)
{
    backgroundBox->setCurrentIndex(bkg);
}

int TextDialog::alignment()
{
    int align = -1;
    switch (alignmentBox->currentIndex()) {
    case 0:
        align = Qt::AlignHCenter;
        break;

    case 1:
        align = Qt::AlignLeft;
        break;

    case 2:
        align = Qt::AlignRight;
        break;
    }
    return align;
}

void TextDialog::setAlignment(int align)
{
    switch (align) {
    case Qt::AlignHCenter:
        alignmentBox->setCurrentIndex(0);
        break;
    case Qt::AlignLeft:
        alignmentBox->setCurrentIndex(1);
        break;
    case Qt::AlignRight:
        alignmentBox->setCurrentIndex(2);
        break;
    }
}

void TextDialog::customFont()
{
    bool okF = false;
    QFont fnt = QFontDialog::getFont(&okF, selectedFont, this);
    if (okF) {
        if (fnt.family().trimmed().isEmpty())
            fnt.setFamily(QStringLiteral("Arial"));
        selectedFont = PlotStyleLimits::preserveFractionalPtAfterFontDialog(selectedFont, fnt);
        clampFontPointSize(selectedFont);
        if (boxFontSize) {
            QSignalBlocker sb(boxFontSize);
            boxFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(selectedFont, 12.0));
        }
        if (textEditBox)
            textEditBox->setFont(selectedFont);
    }
}

void TextDialog::setAngle(int /*angle*/)
{
    // Rotation is not editable in this dialog; kept for API compatibility with the callers.
}

int TextDialog::angle()
{
    // Rotation is not editable in this dialog; always 0.
    return 0;
}

void TextDialog::setText(const QString &t)
{
    QTextCursor cursor = textEditBox->textCursor();
    // select the whole (old) text
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    // replace old text
    cursor.insertText(t);
    // select the whole (new) text
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    // this line makes the selection visible to the user
    // (the 2 lines above only change the selection in the
    // underlying QTextDocument)
    textEditBox->setTextCursor(cursor);
    // give focus back to text edit
    textEditBox->setFocus();
}

void TextDialog::setTextColor(QColor c)
{
    colorBtn->setColor(c);
}

void TextDialog::setBackgroundColor(QColor c)
{
    boxBackgroundTransparency->setValue(c.alpha());
    backgroundBtn->setEnabled(c.alpha());
    backgroundBtn->setColor(c);
}

void TextDialog::setFont(const QFont &fnt)
{
    selectedFont = fnt;
    if (selectedFont.family().trimmed().isEmpty())
        selectedFont.setFamily(QStringLiteral("Arial"));
    const double pt = PlotStyleLimits::effectiveFontPointSizeF(selectedFont, -1.0);
    if (pt <= 0.0 && selectedFont.pixelSize() <= 0)
        selectedFont.setPointSizeF(12.0);
    else if (pt > 0.0)
        selectedFont.setPointSizeF(pt);
    if (selectedFont.pointSizeF() > 0.0)
        clampFontPointSize(selectedFont);
    if (boxFontSize) {
        QSignalBlocker sb(boxFontSize);
        boxFontSize->setValue(PlotStyleLimits::effectiveFontPointSizeF(selectedFont, 12.0));
    }
    if (textEditBox)
        textEditBox->setFont(selectedFont);
}

void TextDialog::updateTransparency(int alpha)
{
    if (alpha != backgroundBtn->color().alpha()) {
        QColor c = backgroundBtn->color();
        c.setAlpha(alpha);
        backgroundBtn->setColor(c);
    }
    backgroundBtn->setEnabled(alpha);
}

void TextDialog::pickBackgroundColor(const QColor &c)
{
    if (c.alpha() != boxBackgroundTransparency->value()) {
        boxBackgroundTransparency->setValue(c.alpha());
    }
}
