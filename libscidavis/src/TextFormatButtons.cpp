/***************************************************************************
    File                 : TextFormatButtons.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Widget with text format buttons (connected to a QTextEdit)

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

#include "TextFormatButtons.h"
#include "SymbolDialog.h"
#include "IconLoader.h"
#include "I18nJapanese.h"
#include "PlotStyleLimits.h"
#include <QTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QRegularExpression>
#include <QString>

namespace {

void registerTextFormatButtonsTranslations()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("Bold (<b>...</b>)", "太字 (<b>...</b>)");
    reg("Italic (<i>...</i>)", "斜体 (<i>...</i>)");
    reg("Underline (<u>...</u>)", "下線 (<u>...</u>)");
    reg("Subscript (<sub>...</sub>)", "下付き文字 (<sub>...</sub>)");
    reg("Superscript (<sup>...</sup>)", "上付き文字 (<sup>...</sup>)");
    reg("Lowercase Greek Letters", "ギリシャ文字 (小文字)");
    reg("Uppercase Greek Letters", "ギリシャ文字 (大文字)");
    reg("Mathematical Symbols", "数学記号");
    reg("Arrow Symbols", "矢印記号");
    reg("Insert Curve Symbol", "曲線の凡例マーカーを挿入");
    reg("Insert Unit", "単位を挿入");
    reg("Units", "単位");
}
QString sanitizeSymbolToArialUtf8(const QString &raw)
{
    QString out = raw;
    // Replace legacy HTML entities with direct UTF-8 characters rendered natively in Arial
    out.replace(QLatin1String("&sigma;"), QString(QChar(0x03C3)));
    out.replace(QLatin1String("&epsilon;"), QString(QChar(0x03B5)));
    out.replace(QLatin1String("&Delta;"), QString(QChar(0x0394)));
    out.replace(QLatin1String("&mu;"), QString(QChar(0x03BC)));
    out.replace(QLatin1String("&theta;"), QString(QChar(0x03B8)));
    out.replace(QLatin1String("&lambda;"), QString(QChar(0x03BB)));
    out.replace(QLatin1String("&alpha;"), QString(QChar(0x03B1)));
    out.replace(QLatin1String("&beta;"), QString(QChar(0x03B2)));
    out.replace(QLatin1String("&gamma;"), QString(QChar(0x03B3)));
    out.replace(QLatin1String("&rho;"), QString(QChar(0x03C1)));
    out.replace(QLatin1String("&omega;"), QString(QChar(0x03C9)));
    out.replace(QLatin1String("&deg;"), QString(QChar(0x00B0)));
    out.replace(QLatin1String("&plusmn;"), QString(QChar(0x00B1)));
    out.replace(QLatin1String("&times;"), QString(QChar(0x00D7)));
    out.replace(QLatin1String("&Aring;"), QString(QChar(0x00C5)));

    // Strip any legacy <font face="Symbol">...</font> wrapper and map Symbol-encoded ASCII to UTF-8
    static const QRegularExpression symFontRe(
            QStringLiteral("<\\s*font[^>]*face\\s*=\\s*[\"']?Symbol[\"']?[^>]*>(.*?)<\\s*/\\s*font\\s*>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match;
    while ((match = symFontRe.match(out)).hasMatch()) {
        const QString inner = match.captured(1);
        QString mapped;
        mapped.reserve(inner.size());
        for (QChar ch : inner) {
            ushort u = ch.unicode();
            switch (u) {
            case 'a': mapped += QChar(0x03B1); break;
            case 'b': mapped += QChar(0x03B2); break;
            case 'g': mapped += QChar(0x03B3); break;
            case 'd': mapped += QChar(0x03B4); break;
            case 'e': mapped += QChar(0x03B5); break;
            case 'q': mapped += QChar(0x03B8); break;
            case 'l': mapped += QChar(0x03BB); break;
            case 'm': mapped += QChar(0x03BC); break;
            case 'r': mapped += QChar(0x03C1); break;
            case 's': mapped += QChar(0x03C3); break;
            case 'w': mapped += QChar(0x03C9); break;
            case 'D': mapped += QChar(0x0394); break;
            case 'G': mapped += QChar(0x0393); break;
            case 'L': mapped += QChar(0x039B); break;
            case 'S': mapped += QChar(0x03A3); break;
            case 'W': mapped += QChar(0x03A9); break;
            default: mapped += ch; break;
            }
        }
        out.replace(match.capturedStart(0), match.capturedLength(0), mapped);
    }
    return out;
}
} // namespace

TextFormatButtons::TextFormatButtons(QTextEdit *textEdit, QWidget *parent) : QWidget(parent)
{
    registerTextFormatButtonsTranslations();
    connectedTextEdit = textEdit;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);

    QHBoxLayout *row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(2);

    QHBoxLayout *row2 = new QHBoxLayout();
    row2->setContentsMargins(0, 0, 0, 0);
    row2->setSpacing(2);

    QFont arialBase(QStringLiteral("Arial"), 10);

    auto configCompactBtn = [&](QPushButton *btn, const QFont &f) {
        btn->setFont(f);
        btn->setFixedSize(28, 26);
        btn->setProperty("compactBtn", true);
        btn->setStyleSheet(QStringLiteral("padding: 1px 2px;"));
        btn->setAutoDefault(false);
    };

    buttonCurve = new QPushButton(IconLoader::duotoneIcon("plot-line-symbol"), QString(), this);
    configCompactBtn(buttonCurve, arialBase);
    buttonCurve->setToolTip(tr("Insert Curve Symbol"));
    row1->addWidget(buttonCurve);

    QFont boldFont(QStringLiteral("Arial"), 10);
    boldFont.setBold(true);
    buttonBold = new QPushButton(QStringLiteral("B"), this);
    buttonBold->setFont(boldFont);
    buttonBold->setMinimumWidth(30);
    buttonBold->setMaximumWidth(34);
    buttonBold->setFixedHeight(26);
    buttonBold->setProperty("compactBtn", true);
    buttonBold->setStyleSheet(
            QStringLiteral("font-family: 'Arial'; font-weight: bold; padding: 1px 4px;"));
    buttonBold->setAutoDefault(false);
    buttonBold->setToolTip(tr("Bold (<b>...</b>)"));
    row1->addWidget(buttonBold);

    QFont italicFont(QStringLiteral("Arial"), 10);
    italicFont.setItalic(true);
    buttonItalics = new QPushButton(QStringLiteral("I"), this);
    buttonItalics->setFont(italicFont);
    buttonItalics->setMinimumWidth(30);
    buttonItalics->setMaximumWidth(34);
    buttonItalics->setFixedHeight(26);
    buttonItalics->setProperty("compactBtn", true);
    buttonItalics->setStyleSheet(
            QStringLiteral("font-family: 'Arial'; font-style: italic; padding: 1px 4px;"));
    buttonItalics->setAutoDefault(false);
    buttonItalics->setToolTip(tr("Italic (<i>...</i>)"));
    row1->addWidget(buttonItalics);

    QFont underlineFont(QStringLiteral("Arial"), 10);
    underlineFont.setUnderline(true);
    buttonUnderline = new QPushButton(QStringLiteral("U"), this);
    buttonUnderline->setFont(underlineFont);
    buttonUnderline->setMinimumWidth(30);
    buttonUnderline->setMaximumWidth(34);
    buttonUnderline->setFixedHeight(26);
    buttonUnderline->setProperty("compactBtn", true);
    buttonUnderline->setStyleSheet(
            QStringLiteral("font-family: 'Arial'; text-decoration: underline; padding: 1px 4px;"));
    buttonUnderline->setAutoDefault(false);
    buttonUnderline->setToolTip(tr("Underline (<u>...</u>)"));
    row1->addWidget(buttonUnderline);

    buttonSubscript = new QPushButton(IconLoader::duotoneIcon("text-subscript"), QString(), this);
    configCompactBtn(buttonSubscript, arialBase);
    buttonSubscript->setToolTip(tr("Subscript (<sub>...</sub>)"));
    row1->addWidget(buttonSubscript);

    buttonSuperscript =
            new QPushButton(IconLoader::duotoneIcon("text-superscript"), QString(), this);
    configCompactBtn(buttonSuperscript, arialBase);
    buttonSuperscript->setToolTip(tr("Superscript (<sup>...</sup>)"));
    row1->addWidget(buttonSuperscript);

    buttonLowerGreek = new QPushButton(QString(QChar(0x03B1)), this);
    configCompactBtn(buttonLowerGreek, arialBase);
    buttonLowerGreek->setToolTip(tr("Lowercase Greek Letters"));
    row1->addWidget(buttonLowerGreek);

    buttonUpperGreek = new QPushButton(QString(QChar(0x0393)), this);
    configCompactBtn(buttonUpperGreek, arialBase);
    buttonUpperGreek->setToolTip(tr("Uppercase Greek Letters"));
    row1->addWidget(buttonUpperGreek);

    buttonMathSymbols = new QPushButton(QString(QChar(0x222B)), this);
    configCompactBtn(buttonMathSymbols, arialBase);
    buttonMathSymbols->setToolTip(tr("Mathematical Symbols"));
    row1->addWidget(buttonMathSymbols);

    buttonArrowSymbols = new QPushButton(QString(QChar(0x2192)), this);
    configCompactBtn(buttonArrowSymbols, arialBase);
    buttonArrowSymbols->setToolTip(tr("Arrow Symbols"));
    row1->addWidget(buttonArrowSymbols);

    buttonUnits = new QPushButton(tr("Units"), this);
    buttonUnits->setFont(arialBase);
    buttonUnits->setFixedHeight(26);
    buttonUnits->setProperty("compactBtn", true);
    buttonUnits->setStyleSheet(QStringLiteral("padding: 1px 6px;"));
    buttonUnits->setAutoDefault(false);
    buttonUnits->setToolTip(tr("Insert Unit"));
    unitsMenu = new QMenu(buttonUnits);
    unitsMenu->setFont(arialBase);

    struct UnitEntry {
        QString label;
        QString insertText;
    };
    const UnitEntry unitEntries[] = {
        { QStringLiteral("MPa"), QStringLiteral("MPa") },
        { QStringLiteral("GPa"), QStringLiteral("GPa") },
        { QStringLiteral("wt.%"), QStringLiteral("wt.%") },
        { QStringLiteral("at.%"), QStringLiteral("at.%") },
        { QStringLiteral("mass ppm"), QStringLiteral("mass ppm") },
        { QStringLiteral("K"), QStringLiteral("K") },
        { QString(QChar(0x00B0)) + QLatin1String("C"),
          QString(QChar(0x00B0)) + QLatin1String("C") },
        { QStringLiteral("s") + QChar(0x207B) + QChar(0x00B9),
          QStringLiteral("s<sup>-1</sup>") },
        { QStringLiteral("mm") + QChar(0x207B) + QChar(0x00B3),
          QStringLiteral("mm<sup>-3</sup>") },
        { QStringLiteral("cm") + QChar(0x207B) + QChar(0x00B9),
          QStringLiteral("cm<sup>-1</sup>") },
        { QString(QChar(0x03BC)) + QLatin1String("m"),
          QString(QChar(0x03BC)) + QLatin1String("m") },
        { QStringLiteral("nm"), QStringLiteral("nm") },
        { QStringLiteral("2") + QChar(0x03B8), QStringLiteral("2") + QChar(0x03B8) },
    };
    for (const UnitEntry &ue : unitEntries) {
        const QString textToInsert = ue.insertText;
        unitsMenu->addAction(ue.label, this, [this, textToInsert]() { addSymbol(textToInsert); });
    }
    buttonUnits->setMenu(unitsMenu);
    row1->addWidget(buttonUnits);
    row1->addStretch(1);

    auto makeUtf8CharBtn = [&](QPushButton *&memberBtn, ushort codepoint) {
        const QString chStr((QChar(codepoint)));
        memberBtn = new QPushButton(chStr, this);
        configCompactBtn(memberBtn, arialBase);
        memberBtn->setToolTip(chStr);
        row2->addWidget(memberBtn);
        connect(memberBtn, &QPushButton::clicked, this, [this, chStr]() { addSymbol(chStr); });
    };

    // 15 direct UTF-8 Greek & scientific characters rendered natively in Arial:
    // σ, ε, Δ, μ, θ, λ, α, β, γ, ρ, ω, °, ±, ×, Å
    makeUtf8CharBtn(buttonSigma, 0x03C3);
    makeUtf8CharBtn(buttonEpsilon, 0x03B5);
    makeUtf8CharBtn(buttonDelta, 0x0394);
    makeUtf8CharBtn(buttonMu, 0x03BC);
    makeUtf8CharBtn(buttonTheta, 0x03B8);
    makeUtf8CharBtn(buttonLambda, 0x03BB);
    makeUtf8CharBtn(buttonAlpha, 0x03B1);
    makeUtf8CharBtn(buttonBeta, 0x03B2);
    makeUtf8CharBtn(buttonGamma, 0x03B3);
    makeUtf8CharBtn(buttonRho, 0x03C1);
    makeUtf8CharBtn(buttonOmega, 0x03C9);
    makeUtf8CharBtn(buttonDegree, 0x00B0);
    makeUtf8CharBtn(buttonPlusMinus, 0x00B1);
    makeUtf8CharBtn(buttonTimes, 0x00D7);
    makeUtf8CharBtn(buttonAngstrom, 0x00C5);
    row2->addStretch(1);

    mainLayout->addLayout(row1);
    mainLayout->addLayout(row2);

    connect(buttonCurve, SIGNAL(clicked()), this, SLOT(addCurve()));
    connect(buttonSuperscript, SIGNAL(clicked()), this, SLOT(addSuperscript()));
    connect(buttonSubscript, SIGNAL(clicked()), this, SLOT(addSubscript()));
    connect(buttonUnderline, SIGNAL(clicked()), this, SLOT(addUnderline()));
    connect(buttonItalics, SIGNAL(clicked()), this, SLOT(addItalics()));
    connect(buttonBold, SIGNAL(clicked()), this, SLOT(addBold()));
    connect(buttonLowerGreek, SIGNAL(clicked()), this, SLOT(showLowerGreek()));
    connect(buttonUpperGreek, SIGNAL(clicked()), this, SLOT(showUpperGreek()));
    connect(buttonMathSymbols, SIGNAL(clicked()), this, SLOT(showMathSymbols()));
    connect(buttonArrowSymbols, SIGNAL(clicked()), this, SLOT(showArrowSymbols()));
}

void TextFormatButtons::showLowerGreek()
{
    SymbolDialog *greekLetters = new SymbolDialog(SymbolDialog::lowerGreek, this, Qt::Tool);
    greekLetters->setAttribute(Qt::WA_DeleteOnClose);
    QFont f = connectedTextEdit ? connectedTextEdit->font() : QFont(QStringLiteral("Arial"), 14);
    f.setFamily(QStringLiteral("Arial"));
    if (PlotStyleLimits::effectiveFontPointSizeF(f, 14.0) < 14.0)
        f.setPointSizeF(14.0);
    greekLetters->setFont(f);
    connect(greekLetters, SIGNAL(addLetter(const QString &)), this,
            SLOT(addSymbol(const QString &)));
    greekLetters->show();
    greekLetters->setFocus();
}

void TextFormatButtons::showUpperGreek()
{
    SymbolDialog *greekLetters = new SymbolDialog(SymbolDialog::upperGreek, this, Qt::Tool);
    greekLetters->setAttribute(Qt::WA_DeleteOnClose);
    QFont f = connectedTextEdit ? connectedTextEdit->font() : QFont(QStringLiteral("Arial"), 14);
    f.setFamily(QStringLiteral("Arial"));
    if (PlotStyleLimits::effectiveFontPointSizeF(f, 14.0) < 14.0)
        f.setPointSizeF(14.0);
    greekLetters->setFont(f);
    connect(greekLetters, SIGNAL(addLetter(const QString &)), this,
            SLOT(addSymbol(const QString &)));
    greekLetters->show();
    greekLetters->setFocus();
}

void TextFormatButtons::showMathSymbols()
{
    SymbolDialog *mathSymbols = new SymbolDialog(SymbolDialog::mathSymbols, this, Qt::Tool);
    mathSymbols->setAttribute(Qt::WA_DeleteOnClose);
    QFont f = connectedTextEdit ? connectedTextEdit->font() : QFont(QStringLiteral("Arial"), 14);
    f.setFamily(QStringLiteral("Arial"));
    if (PlotStyleLimits::effectiveFontPointSizeF(f, 14.0) < 14.0)
        f.setPointSizeF(14.0);
    mathSymbols->setFont(f);
    connect(mathSymbols, SIGNAL(addLetter(const QString &)), this,
            SLOT(addSymbol(const QString &)));
    mathSymbols->show();
    mathSymbols->setFocus();
}

void TextFormatButtons::showArrowSymbols()
{
    SymbolDialog *arrowSymbols = new SymbolDialog(SymbolDialog::arrowSymbols, this, Qt::Tool);
    arrowSymbols->setAttribute(Qt::WA_DeleteOnClose);
    QFont f = connectedTextEdit ? connectedTextEdit->font() : QFont(QStringLiteral("Arial"), 14);
    f.setFamily(QStringLiteral("Arial"));
    if (PlotStyleLimits::effectiveFontPointSizeF(f, 14.0) < 14.0)
        f.setPointSizeF(14.0);
    arrowSymbols->setFont(f);
    connect(arrowSymbols, SIGNAL(addLetter(const QString &)), this,
            SLOT(addSymbol(const QString &)));
    arrowSymbols->show();
    arrowSymbols->setFocus();
}

void TextFormatButtons::addSymbol(const QString &letter)
{
    if (!connectedTextEdit)
        return;
    connectedTextEdit->textCursor().insertText(sanitizeSymbolToArialUtf8(letter));
    connectedTextEdit->setFocus();
}

void TextFormatButtons::addCurve()
{
    formatText("\\c{", "}");
}

void TextFormatButtons::addUnderline()
{
    formatText("<u>", "</u>");
}

void TextFormatButtons::addItalics()
{
    formatText("<i>", "</i>");
}

void TextFormatButtons::addBold()
{
    formatText("<b>", "</b>");
}

void TextFormatButtons::addSubscript()
{
    formatText("<sub>", "</sub>");
}

void TextFormatButtons::addSuperscript()
{
    formatText("<sup>", "</sup>");
}

void TextFormatButtons::addSigma()
{
    addSymbol(QString(QChar(0x03C3)));
}

void TextFormatButtons::addEpsilon()
{
    addSymbol(QString(QChar(0x03B5)));
}

void TextFormatButtons::addMu()
{
    addSymbol(QString(QChar(0x03BC)));
}

void TextFormatButtons::addDelta()
{
    addSymbol(QString(QChar(0x0394)));
}

void TextFormatButtons::addDegC()
{
    addSymbol(QString(QChar(0x00B0)) + QLatin1String("C"));
}

void TextFormatButtons::addInvMm3()
{
    addSymbol(QStringLiteral("mm<sup>-3</sup>"));
}

void TextFormatButtons::formatText(const QString &prefix, const QString &postfix)
{
    if (!connectedTextEdit)
        return;
    QTextCursor cursor = connectedTextEdit->textCursor();
    QString markedText = cursor.selectedText();
    cursor.insertText(prefix + markedText + postfix);
    if (markedText.isEmpty()) {
        // if no text is marked, place cursor inside the <..></..> statement
        // instead of after it
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor,
                            postfix.size());
        // the next line makes the selection visible to the user
        // (the line above only changes the selection in the
        // underlying QTextDocument)
        connectedTextEdit->setTextCursor(cursor);
    }
    // give focus back to text edit
    connectedTextEdit->setFocus();
}

void TextFormatButtons::toggleCurveButton(bool enable)
{
    buttonCurve->setVisible(enable);
}
