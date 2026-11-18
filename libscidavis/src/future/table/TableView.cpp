/***************************************************************************
    File                 : TableView.cpp
    Project              : SciDAVis
    Description          : View class for Table
    --------------------------------------------------------------------
    Copyright            : (C) 2007 Tilman Benkert (thzs*gmx.net)
                           (replace * with @ in the email addresses)

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

#include "table/future_Table.h"
#include "table/TableView.h"
#include "table/TableModel.h"
#include "table/TableItemDelegate.h"
#include "table/tablecommands.h"
#include "table/TableDoubleHeaderView.h"

#include "core/column/Column.h"
#include "core/AbstractFilter.h"
#include "core/datatypes/SimpleCopyThroughFilter.h"
#include "core/datatypes/Double2StringFilter.h"
#include "core/datatypes/String2DoubleFilter.h"
#include "core/datatypes/DateTime2StringFilter.h"
#include "core/datatypes/String2DateTimeFilter.h"
#include "core/datatypes/Double2DateTimeFilter.h"

#include <QKeyEvent>
#include <QtDebug>
#include <QHeaderView>
#include <QRect>
#include <QPoint>
#include <QSize>
#include <QFontMetrics>
#include <QFont>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QShortcut>
#include <QModelIndex>
#include <QGridLayout>
#include <QScrollArea>
#include <QMenu>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <algorithm>
#include "ApplicationWindow.h"
#include "Graph.h"
#include "IconLoader.h"
#include "MultiLayer.h"
#include "Table.h"

namespace {

static QChar toUnicodeSubChar(QChar ch, bool *converted = nullptr)
{
    if (converted)
        *converted = true;
    switch (ch.unicode()) {
    case '0': return QChar(0x2080);
    case '1': return QChar(0x2081);
    case '2': return QChar(0x2082);
    case '3': return QChar(0x2083);
    case '4': return QChar(0x2084);
    case '5': return QChar(0x2085);
    case '6': return QChar(0x2086);
    case '7': return QChar(0x2087);
    case '8': return QChar(0x2088);
    case '9': return QChar(0x2089);
    case '+': return QChar(0x208A);
    case '-': return QChar(0x208B);
    case '=': return QChar(0x208C);
    case '(': return QChar(0x208D);
    case ')': return QChar(0x208E);
    case 'a': return QChar(0x2090);
    case 'e': return QChar(0x2091);
    case 'o': return QChar(0x2092);
    case 'x': return QChar(0x2093);
    case 'i': return QChar(0x1D62);
    case 'r': return QChar(0x1D63);
    case 'u': return QChar(0x1D64);
    case 'v': return QChar(0x1D65);
    case 'n': return QChar(0x2099);
    default:
        if (converted)
            *converted = false;
        return ch;
    }
}

static QChar toUnicodeSupChar(QChar ch, bool *converted = nullptr)
{
    if (converted)
        *converted = true;
    switch (ch.unicode()) {
    case '0': return QChar(0x2070);
    case '1': return QChar(0x00B9);
    case '2': return QChar(0x00B2);
    case '3': return QChar(0x00B3);
    case '4': return QChar(0x2074);
    case '5': return QChar(0x2075);
    case '6': return QChar(0x2076);
    case '7': return QChar(0x2077);
    case '8': return QChar(0x2078);
    case '9': return QChar(0x2079);
    case '+': return QChar(0x207A);
    case '-': return QChar(0x207B);
    case '=': return QChar(0x207C);
    case '(': return QChar(0x207D);
    case ')': return QChar(0x207E);
    case 'n': return QChar(0x207F);
    case 'i': return QChar(0x2071);
    default:
        if (converted)
            *converted = false;
        return ch;
    }
}

static QString convertPlainRunToUnicodeSubSup(const QString &src, bool isSup)
{
    QString out;
    out.reserve(src.size());
    for (QChar ch : src)
        out.append(isSup ? toUnicodeSupChar(ch) : toUnicodeSubChar(ch));
    return out;
}

static void applyUnicodeSymbolOrSubSupToEditor(QWidget *target, const QString &literalSym,
                                               int subSupMode /* 0=literal, 1=sub, 2=sup */)
{
    if (!target)
        return;
    if (QLineEdit *le = qobject_cast<QLineEdit *>(target)) {
        if (subSupMode == 1 || subSupMode == 2) {
            const bool isSup = (subSupMode == 2);
            if (le->hasSelectedText()) {
                const int start = le->selectionStart();
                const QString sel = le->selectedText();
                const QString conv = convertPlainRunToUnicodeSubSup(sel, isSup);
                le->insert(conv);
                le->setSelection(start, conv.length());
            } else {
                const QString cur = le->text();
                const int pos = le->cursorPosition();
                int runStart = pos;
                while (runStart > 0) {
                    bool ok = false;
                    const QChar prev = cur.at(runStart - 1);
                    if (isSup)
                        toUnicodeSupChar(prev, &ok);
                    else
                        toUnicodeSubChar(prev, &ok);
                    if (!ok && prev != QLatin1Char('.') && prev != QLatin1Char('/'))
                        break;
                    if ((prev == QLatin1Char('.') || prev == QLatin1Char('/'))
                        && runStart == pos)
                        break;
                    --runStart;
                }
                if (runStart < pos) {
                    const QString replaced =
                            convertPlainRunToUnicodeSubSup(cur.mid(runStart, pos - runStart), isSup);
                    le->setSelection(runStart, pos - runStart);
                    le->insert(replaced);
                } else {
                    le->insert(isSup ? QString::fromUtf8("\xC2\xB2")
                                     : QString::fromUtf8("\xE2\x82\x82"));
                }
            }
        } else {
            le->insert(literalSym);
        }
        le->setFocus();
    } else if (QTextEdit *te = qobject_cast<QTextEdit *>(target)) {
        QTextCursor tc = te->textCursor();
        if (subSupMode == 1 || subSupMode == 2) {
            const bool isSup = (subSupMode == 2);
            if (tc.hasSelection())
                tc.insertText(convertPlainRunToUnicodeSubSup(tc.selectedText(), isSup));
            else
                tc.insertText(isSup ? QString::fromUtf8("\xC2\xB2")
                                    : QString::fromUtf8("\xE2\x82\x82"));
        } else {
            tc.insertText(literalSym);
        }
        te->setFocus();
    }
}

enum WorksheetQuickRoleCode {
    QuickRoleNone = int(SciDAVis::noDesignation),
    QuickRoleX = int(SciDAVis::X),
    QuickRoleY = int(SciDAVis::Y),
    QuickRoleZ = int(SciDAVis::Z),
    QuickRoleXErr = int(SciDAVis::xErr),
    QuickRoleYErr = int(SciDAVis::yErr),
    QuickRoleLabel = 100
};

struct ScopedUiFlagGuard
{
    bool &flag;
    bool prev;
    explicit ScopedUiFlagGuard(bool &f) : flag(f), prev(f) { flag = true; }
    ~ScopedUiFlagGuard() { flag = prev; }
};

static ApplicationWindow *findOwningAppWindow(QWidget *w)
{
    for (QWidget *cur = w; cur != nullptr; cur = cur->parentWidget()) {
        if (ApplicationWindow *aw = qobject_cast<ApplicationWindow *>(cur))
            return aw;
        if (ApplicationWindow *winAw = qobject_cast<ApplicationWindow *>(cur->window()))
            return winAw;
    }
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (ApplicationWindow *aw = qobject_cast<ApplicationWindow *>(tlw))
            return aw;
    }
    return nullptr;
}

static void updateWorksheetQuickBarMetrics(QFrame *quickBar, QToolButton *btnX, QToolButton *btnY,
                                           QToolButton *btnYErr, QToolButton *moreBtn,
                                           QToolButton *btnLine, QToolButton *btnLp,
                                           QToolButton *btnScatter, QToolButton *btnAdd,
                                           QToolButton *btnColProps, QLabel *lblFx)
{
    if (!quickBar)
        return;

    auto calcBtnWidth = [](QToolButton *b, Qt::ToolButtonStyle st) -> int {
        if (!b)
            return 0;
        b->ensurePolished();
        if (st == Qt::ToolButtonIconOnly)
            return b->iconSize().width() + 10;
        QFont bf = b->font();
        bf.setBold(true);
        const QFontMetrics fm(bf);
        const int textW = fm.horizontalAdvance(b->text());
        const int iconW = (st == Qt::ToolButtonTextBesideIcon && !b->icon().isNull())
                ? (b->iconSize().width() + 5)
                : 0;
        const int pad = (b->menu() != nullptr) ? 20 : 14;
        return textW + iconW + pad;
    };

    const int fixedRowW = calcBtnWidth(btnX, Qt::ToolButtonTextOnly)
            + calcBtnWidth(btnY, Qt::ToolButtonTextOnly)
            + calcBtnWidth(btnYErr, Qt::ToolButtonTextOnly)
            + calcBtnWidth(moreBtn, Qt::ToolButtonTextOnly)
            + calcBtnWidth(btnColProps, Qt::ToolButtonTextOnly)
            + 31; // 4+4px margins + 5px separator + 9*2px spacing
    const int totalBeside = fixedRowW + calcBtnWidth(btnLine, Qt::ToolButtonTextBesideIcon)
            + calcBtnWidth(btnLp, Qt::ToolButtonTextBesideIcon)
            + calcBtnWidth(btnScatter, Qt::ToolButtonTextBesideIcon)
            + calcBtnWidth(btnAdd, Qt::ToolButtonTextBesideIcon);
    const int totalTextOnly = fixedRowW + calcBtnWidth(btnLine, Qt::ToolButtonTextOnly)
            + calcBtnWidth(btnLp, Qt::ToolButtonTextOnly)
            + calcBtnWidth(btnScatter, Qt::ToolButtonTextOnly)
            + calcBtnWidth(btnAdd, Qt::ToolButtonTextOnly);

    const int barW = quickBar->width();
    Qt::ToolButtonStyle plotStyle = Qt::ToolButtonIconOnly;
    if (barW <= 0) {
        plotStyle = Qt::ToolButtonIconOnly;
    } else if (barW >= qMax(510, totalBeside + 6)) {
        plotStyle = Qt::ToolButtonTextBesideIcon;
    } else if (barW >= qMax(435, totalTextOnly + 6)) {
        plotStyle = Qt::ToolButtonTextOnly;
    } else {
        plotStyle = Qt::ToolButtonIconOnly;
    }

    QToolButton *plotBtns[] = { btnLine, btnLp, btnScatter, btnAdd };
    for (QToolButton *pb : plotBtns) {
        if (pb && pb->toolButtonStyle() != plotStyle)
            pb->setToolButtonStyle(plotStyle);
    }

    QToolButton *textOnlyBtns[] = { btnX, btnY, btnYErr, moreBtn, btnColProps };
    for (QToolButton *tb : textOnlyBtns) {
        if (tb && tb->toolButtonStyle() != Qt::ToolButtonTextOnly)
            tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    QToolButton *visibleRow1[] = { btnX,    btnY,   btnYErr,    moreBtn,     btnLine,
                                   btnLp,   btnScatter, btnAdd, btnColProps };
    for (QToolButton *b : visibleRow1) {
        if (!b)
            continue;
        const int minW = calcBtnWidth(b, b->toolButtonStyle());
        b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        if (b->minimumWidth() != minW)
            b->setMinimumWidth(minW);
    }

    if (lblFx) {
        lblFx->ensurePolished();
        QFont bf = lblFx->font();
        bf.setBold(true);
        const QFontMetrics fm(bf);
        const int fxW = fm.horizontalAdvance(lblFx->text()) + 10;
        lblFx->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        if (lblFx->minimumWidth() != fxW)
            lblFx->setMinimumWidth(fxW);
    }
}

} // namespace

#ifndef LEGACY_CODE_0_2_x
TableView::TableView(future::Table *table) : d_table(table)
#else
TableView::TableView(const QString &label, QWidget *parent, const QString name, Qt::WindowFlags f)
    : MyWidget(label, parent, name, f)
#endif
{
#ifndef LEGACY_CODE_0_2_x
    d_model = new TableModel(table);
    init();
#else
    d_model = NULL;
#endif
}

TableView::~TableView()
{
    delete d_model;
}

void TableView::setTable(future::Table *table)
{
    if (nullptr != table) {
        d_table = table;
        d_model = new TableModel(table);
        init();
        d_table->setView(this);
    }
}

void TableView::init()
{
    QWidget *d_main_widget = new QWidget();
    QVBoxLayout *rootVBox = new QVBoxLayout(d_main_widget);
    rootVBox->setSpacing(0);
    rootVBox->setContentsMargins(0, 0, 0, 0);

    initWorksheetQuickBar(d_main_widget);
    if (d_quick_bar)
        rootVBox->addWidget(d_quick_bar);

    d_main_layout = new QHBoxLayout();
    d_main_layout->setSpacing(0);
    d_main_layout->setContentsMargins(0, 0, 0, 0);
    rootVBox->addLayout(d_main_layout, 1);

    d_view_widget = new TableViewWidget(this);
    auto &settings = ApplicationWindow::getSettings();
    settings.beginGroup("[Table]");
    int defaultRowHeight = settings.value("DefaultRowHeight", 20).toInt();
    settings.endGroup();
    d_view_widget->verticalHeader()->setDefaultSectionSize(defaultRowHeight);
    d_view_widget->setModel(d_model);
    connect(d_view_widget, SIGNAL(advanceCell()), this, SLOT(advanceCell()));
    d_main_layout->addWidget(d_view_widget);

    d_horizontal_header = new TableDoubleHeaderView();
#if QT_VERSION >= 0x050000
    d_horizontal_header->setSectionsClickable(true);
#else
    d_horizontal_header->setClickable(true);
#endif
    d_horizontal_header->setHighlightSections(true);
    d_view_widget->setHorizontalHeader(d_horizontal_header);

    d_hide_button = new QToolButton();
    d_hide_button->setArrowType(Qt::LeftArrow);
    d_hide_button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    d_hide_button->setCheckable(false);
    d_hide_button->hide();
    d_main_layout->addWidget(d_hide_button);
    connect(d_hide_button, SIGNAL(pressed()), this, SLOT(toggleControlTabBar()));
    d_control_tabs = new QWidget();
    ui.setupUi(d_control_tabs);
    {
        QWidget *lnRow = new QWidget(ui.description_tab);
        QHBoxLayout *lnLay = new QHBoxLayout(lnRow);
        lnLay->setContentsMargins(0, 0, 0, 0);
        d_long_name_label = new QLabel(tr("Long Name:"), lnRow);
        d_long_name_label->setWordWrap(false);
        d_long_name_edit = new QLineEdit(lnRow);
        lnLay->addWidget(d_long_name_label);
        lnLay->addWidget(d_long_name_edit);
        ui.verticalLayout->insertWidget(2, lnRow);

        QWidget *uRow = new QWidget(ui.description_tab);
        QHBoxLayout *uLay = new QHBoxLayout(uRow);
        uLay->setContentsMargins(0, 0, 0, 0);
        d_units_label = new QLabel(tr("Units:"), uRow);
        d_units_label->setWordWrap(false);
        d_units_edit = new QLineEdit(uRow);
        uLay->addWidget(d_units_label);
        uLay->addWidget(d_units_edit);
        ui.verticalLayout->insertWidget(3, uRow);

        d_long_name_edit->installEventFilter(this);
        d_units_edit->installEventFilter(this);
        d_last_desc_edit = d_units_edit;

        QWidget *symBar = new QWidget(ui.description_tab);
        QGridLayout *symGrid = new QGridLayout(symBar);
        symGrid->setContentsMargins(0, 1, 0, 1);
        symGrid->setHorizontalSpacing(2);
        symGrid->setVerticalSpacing(2);

        struct WsSymSpec {
            const char *label;
            const char *insertText;
            int subSupMode; // 0=literal, 1=sub, 2=sup
            bool preferUnits;
        };
        static const WsSymSpec kWsSyms[] = {
            { "\xCF\x83", "\xCF\x83", 0, false },                   // σ
            { "\xCE\xB5", "\xCE\xB5", 0, false },                   // ε
            { "2\xCE\xB8", "2\xCE\xB8", 0, false },                 // 2θ
            { "\xCE\xBCm", "\xCE\xBCm", 0, true },                  // μm
            { "\xC2\xB0""C", "\xC2\xB0""C", 0, true },              // °C
            { "\xC3\x85", "\xC3\x85", 0, true },                    // Å
            { "\xC2\xB1", "\xC2\xB1", 0, false },                   // ±
            { "\xC2\xB0", "\xC2\xB0", 0, true },                    // °
            { "cm\xE2\x81\xBB\xC2\xB9", "cm\xE2\x81\xBB\xC2\xB9", 0, true }, // cm⁻¹
            { "x\xE2\x82\x82", "", 1, false },                      // x₂
            { "x\xC2\xB2", "", 2, false },                          // x²
        };
        for (int si = 0; si < int(sizeof(kWsSyms) / sizeof(kWsSyms[0])); ++si) {
            const auto &sp = kWsSyms[si];
            QToolButton *tb = new QToolButton(symBar);
            tb->setText(QString::fromUtf8(sp.label));
            tb->setAutoRaise(true);
            tb->setFocusPolicy(Qt::NoFocus);
            tb->setFixedHeight(20);
            tb->setStyleSheet(QStringLiteral(
                    "QToolButton { border: 1px solid #CBD5E1; border-radius: 3px;"
                    " background: #F8FAFC; color: #1E293B; padding: 0px 3px; font-size: 8.2pt; }"
                    "QToolButton:hover { background: #DBEAFE; border-color: #60A5FA; }"));
            if (sp.subSupMode == 1)
                tb->setToolTip(tr("Unicode Subscript (select digits or insert \xE2\x82\x82)"));
            else if (sp.subSupMode == 2)
                tb->setToolTip(tr("Unicode Superscript (select digits or insert \xC2\xB2)"));
            else
                tb->setToolTip(tr("Insert Materials Science Symbol / Unit"));
            const QString ins = QString::fromUtf8(sp.insertText);
            const int mode = sp.subSupMode;
            const bool prefUnits = sp.preferUnits;
            connect(tb, &QToolButton::clicked, this, [this, ins, mode, prefUnits]() {
                QWidget *target = nullptr;
                QWidget *fw = QApplication::focusWidget();
                if (fw == d_long_name_edit || fw == d_units_edit || fw == ui.comment_box)
                    target = fw;
                else if (d_last_desc_edit)
                    target = d_last_desc_edit.data();
                if (!target)
                    target = prefUnits ? static_cast<QWidget *>(d_units_edit)
                                       : static_cast<QWidget *>(d_long_name_edit);
                applyUnicodeSymbolOrSubSupToEditor(target, ins, mode);
                if (!d_updating_ui && (target == d_long_name_edit || target == d_units_edit))
                    applyDescription();
            });
            symGrid->addWidget(tb, si / 6, si % 6);
        }
        symGrid->setColumnStretch(6, 1);
        ui.verticalLayout->insertWidget(4, symBar);
    }
    QLabel *shortLabels[] = { ui.name_label,       ui.comment_label,
                              ui.type_label,       ui.format_label,
                              ui.format_label2,    ui.digits_label,
                              ui.date_time_interval_label, ui.date_time_0_label };
    for (QLabel *lbl : shortLabels) {
        if (lbl)
            lbl->setWordWrap(false);
    }
    d_control_tabs->hide();
    d_main_layout->addWidget(d_control_tabs);

    d_delegate = new TableItemDelegate(d_view_widget);
    d_view_widget->setItemDelegate(d_delegate);

    d_view_widget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    d_main_layout->setStretchFactor(d_view_widget, 1);

    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    d_view_widget->setFocusPolicy(Qt::StrongFocus);
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
#if QT_VERSION >= 0x040300
    d_view_widget->setCornerButtonEnabled(true);
#endif

    d_view_widget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QHeaderView *v_header = d_view_widget->verticalHeader();
#if QT_VERSION >= 0x050000
    v_header->setSectionResizeMode(QHeaderView::Interactive);
    v_header->setSectionsMovable(false);
    d_horizontal_header->setSectionResizeMode(QHeaderView::Interactive);
    d_horizontal_header->setSectionsMovable(true);
#else
    v_header->setResizeMode(QHeaderView::Interactive);
    v_header->setMovable(false);
    d_horizontal_header->setResizeMode(QHeaderView::Interactive);
    d_horizontal_header->setMovable(true);
#endif
    connect(d_horizontal_header, SIGNAL(sectionMoved(int, int, int)), this,
            SLOT(handleHorizontalSectionMoved(int, int, int)));
    connect(d_horizontal_header, SIGNAL(sectionDoubleClicked(int)), this,
            SLOT(handleHorizontalHeaderDoubleClicked(int)));

    d_horizontal_header->setDefaultSectionSize(future::Table::defaultColumnWidth());

    v_header->installEventFilter(this);
    d_horizontal_header->installEventFilter(this);
    d_view_widget->installEventFilter(this);
    if (d_view_widget->viewport()) {
        d_view_widget->viewport()->setMouseTracking(true);
        d_view_widget->viewport()->installEventFilter(this);
    }

    connect(d_model, SIGNAL(headerDataChanged(Qt::Orientation, int, int)), d_view_widget,
            SLOT(updateHeaderGeometry(Qt::Orientation, int, int)));
    connect(d_model, SIGNAL(headerDataChanged(Qt::Orientation, int, int)), this,
            SLOT(handleHeaderDataChanged(Qt::Orientation, int, int)));
    connect(d_table, SIGNAL(aspectDescriptionChanged(const AbstractAspect *)), this,
            SLOT(handleAspectDescriptionChanged(const AbstractAspect *)));
    connect(d_table, SIGNAL(aspectAdded(const AbstractAspect *)), this,
            SLOT(handleAspectAdded(const AbstractAspect *)));
    connect(d_table, SIGNAL(aspectAboutToBeRemoved(const AbstractAspect *, int)), this,
            SLOT(handleAspectAboutToBeRemoved(const AbstractAspect *, int)));

    this->setWidget(d_main_widget);
    rereadSectionSizes();

    // keyboard shortcuts
    QShortcut *sel_all =
            new QShortcut(QKeySequence(tr("Ctrl+A", "Table: select all")), d_view_widget);
    connect(sel_all, SIGNAL(activated()), d_view_widget, SLOT(selectAll()));

    connect(ui.type_box, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFormatBox()));
    connect(ui.format_box, SIGNAL(currentIndexChanged(int)), this, SLOT(updateTypeInfo()));
    connect(ui.formatLineEdit, SIGNAL(textEdited(const QString)), this,
            SLOT(handleFormatLineEditChange()));
    connect(ui.digits_box, SIGNAL(valueChanged(int)), this, SLOT(updateTypeInfo()));
    connect(ui.previous_column_button, SIGNAL(clicked()), this, SLOT(goToPreviousColumn()));
    connect(ui.next_column_button, SIGNAL(clicked()), this, SLOT(goToNextColumn()));
    retranslateStrings();

    QItemSelectionModel *sel_model = d_view_widget->selectionModel();

    connect(sel_model, SIGNAL(currentColumnChanged(const QModelIndex &, const QModelIndex &)), this,
            SLOT(currentColumnChanged(const QModelIndex &, const QModelIndex &)));
    connect(sel_model, SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
            this, SLOT(selectionChanged(const QItemSelection &, const QItemSelection &)));
    connect(ui.button_set_description, SIGNAL(pressed()), this, SLOT(applyDescription()));
    connect(ui.button_set_type, SIGNAL(pressed()), this, SLOT(applyType()));

    connect(ui.name_edit, &QLineEdit::editingFinished, this, [this]() {
        if (!d_updating_ui)
            applyDescription();
    });
    if (d_long_name_edit) {
        connect(d_long_name_edit, &QLineEdit::editingFinished, this, [this]() {
            if (!d_updating_ui)
                applyDescription();
        });
    }
    if (d_units_edit) {
        connect(d_units_edit, &QLineEdit::editingFinished, this, [this]() {
            if (!d_updating_ui)
                applyDescription();
        });
    }
    if (ui.comment_box) {
        ui.comment_box->installEventFilter(this);
        connect(ui.comment_box, &QTextEdit::textChanged, this, [this]() {
            if (!d_updating_ui)
                d_comment_dirty = true;
        });
    }
    connect(ui.type_box, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        if (!d_updating_ui) {
            updateFormatBox();
            updateTypeInfo();
            applyType();
        }
    });
    connect(ui.format_box, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        if (!d_updating_ui) {
            updateTypeInfo();
            applyType();
        }
    });
    connect(ui.digits_box, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!d_updating_ui)
            applyType();
    });
    connect(ui.formatLineEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!d_updating_ui)
            applyType();
    });

    if (d_table && d_table->columnCount() > 0)
        setColumnForControlTabs(0);
    syncQuickBarState();
}

void TableView::rereadSectionSizes()
{
    disconnect(d_horizontal_header, SIGNAL(sectionResized(int, int, int)), this,
               SLOT(handleHorizontalSectionResized(int, int, int)));

    if (d_table) {
        const int defaultW = future::Table::defaultColumnWidth();
        int cols = d_table->columnCount();
        for (int i = 0; i < cols; i++) {
            const int storedW = d_table->columnWidth(i);
            d_horizontal_header->resizeSection(i, storedW);
            if (storedW <= defaultW)
                autoExpandColumnWidthForHeader(i, false);
        }
    }

    connect(d_horizontal_header, SIGNAL(sectionResized(int, int, int)), this,
            SLOT(handleHorizontalSectionResized(int, int, int)));
}

void TableView::autoExpandColumnWidthForHeader(int col, bool fitDataCellsToo)
{
    if (!d_table || !d_horizontal_header || !d_model || col < 0 || col >= d_table->columnCount())
        return;

    const Column *colPtr = d_table->column(col);
    if (!colPtr)
        return;

    const QFont baseFont = d_horizontal_header->font();
    const QFontMetrics fm(baseFont);
    QFont boldFont = baseFont;
    boldFont.setWeight(QFont::DemiBold);
    const QFontMetrics boldFm(boldFont);

    const QString topText = d_model->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
    int neededW = fm.horizontalAdvance(topText) + 38; // type icon + header padding

    const QString ln = colPtr->longName().trimmed();
    if (!ln.isEmpty())
        neededW = qMax(neededW, boldFm.horizontalAdvance(ln) + 18);

    const QString u = colPtr->units().trimmed();
    if (!u.isEmpty())
        neededW = qMax(neededW, fm.horizontalAdvance(QStringLiteral("[") + u + QStringLiteral("]")) + 18);

    if (fitDataCellsToo && d_view_widget)
        neededW = qMax(neededW, d_view_widget->sizeHintForColumn(col) + 14);

    const int maxAutoW = fitDataCellsToo ? 320 : 220;
    const int currentW = d_horizontal_header->sectionSize(col);
    const int nextW = fitDataCellsToo ? qBound(future::Table::defaultColumnWidth(), neededW, maxAutoW)
                                      : qMax(currentW, qMin(maxAutoW, neededW));
    if (nextW != currentW) {
        QSignalBlocker blockHdr(d_horizontal_header);
        d_table->setColumnWidth(col, nextW);
        d_horizontal_header->resizeSection(col, nextW);
    }
}

void TableView::setColumnWidth(int col, int width)
{
    d_horizontal_header->resizeSection(col, width);
}

int TableView::columnWidth(int col) const
{
    return d_horizontal_header->sectionSize(col);
}

void TableView::handleHorizontalSectionResized(int logicalIndex, int, int newSize)
{
    static bool inside = false;
    if (d_table) {
        d_table->setColumnWidth(logicalIndex, newSize);
        if (inside)
            return;
        inside = true;

        int cols = d_table->columnCount();
        for (int i = 0; i < cols; i++)
            if (isColumnSelected(i, true))
                d_horizontal_header->resizeSection(i, newSize);
    }

    inside = false;
}

void TableView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateStrings();
    MyWidget::changeEvent(event);
}

void TableView::retranslateStrings()
{
    ScopedUiFlagGuard guard(d_updating_ui);
    d_hide_button->setToolTip(tr("Show/hide control tabs"));
    ui.retranslateUi(d_control_tabs);
    if (d_long_name_label) {
        d_long_name_label->setText(tr("Long Name:"));
        d_long_name_label->setWordWrap(false);
    }
    if (d_units_label) {
        d_units_label->setText(tr("Units:"));
        d_units_label->setWordWrap(false);
    }
    QLabel *shortLabels[] = { ui.name_label,       ui.comment_label,
                              ui.type_label,       ui.format_label,
                              ui.format_label2,    ui.digits_label,
                              ui.date_time_interval_label, ui.date_time_0_label };
    for (QLabel *lbl : shortLabels) {
        if (lbl)
            lbl->setWordWrap(false);
    }

    if (d_btn_role_x) {
        d_btn_role_x->setText(QStringLiteral("X"));
        d_btn_role_x->setToolTip(tr("Set selected column(s) as X (Abscissa)"));
    }
    if (d_btn_role_y) {
        d_btn_role_y->setText(QStringLiteral("Y"));
        d_btn_role_y->setToolTip(tr("Set selected column(s) as Y (Ordinate)"));
    }
    if (d_btn_role_yerr) {
        d_btn_role_yerr->setText(tr("\xC2\xB1yErr"));
        d_btn_role_yerr->setToolTip(tr("Set selected column(s) as Y Error"));
    }
    if (d_btn_role_xerr) {
        d_btn_role_xerr->setText(tr("\xC2\xB1xErr"));
        d_btn_role_xerr->setToolTip(tr("Set selected column(s) as X Error"));
    }
    if (d_btn_role_z) {
        d_btn_role_z->setText(QStringLiteral("Z"));
        d_btn_role_z->setToolTip(tr("Set selected column(s) as Z"));
    }
    if (d_btn_role_label) {
        d_btn_role_label->setText(tr("Label"));
        d_btn_role_label->setToolTip(tr("Set selected column(s) as Text Label"));
    }
    if (d_btn_role_none) {
        d_btn_role_none->setText(tr("Disregard"));
        d_btn_role_none->setToolTip(tr("Set selected column(s) as Disregard (None)"));
    }
    if (d_lbl_quick_fx)
        d_lbl_quick_fx->setText(QStringLiteral("F(x) ="));
    if (d_edit_quick_fx) {
        d_edit_quick_fx->setPlaceholderText(
                tr("Column formula (e.g. col(\"1\")*2) - Enter to apply"));
        d_edit_quick_fx->setToolTip(
                tr("Enter formula for active column and press Enter to recalculate"));
    }
    if (d_btn_quick_plot_lp) {
        d_btn_quick_plot_lp->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 16, false));
        d_btn_quick_plot_lp->setText(tr("Line+Sym"));
        d_btn_quick_plot_lp->setToolTip(
                tr("Plot Line + Symbol graph (uses selected Y or all Y columns)"));
    }
    if (d_btn_quick_plot_scatter) {
        d_btn_quick_plot_scatter->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("plot-scatter"), 16, false));
        d_btn_quick_plot_scatter->setText(tr("Scatter"));
        d_btn_quick_plot_scatter->setToolTip(
                tr("Plot Scatter graph (uses selected Y or all Y columns)"));
    }
    if (d_btn_quick_plot_line) {
        d_btn_quick_plot_line->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("plot-line"), 16, false));
        d_btn_quick_plot_line->setText(tr("Line"));
        d_btn_quick_plot_line->setToolTip(
                tr("Plot Line graph (uses selected Y or all Y columns)"));
    }
    if (d_btn_quick_add_graph) {
        d_btn_quick_add_graph->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("new-graph"), 16, false));
        d_btn_quick_add_graph->setText(tr("+ Add"));
        d_btn_quick_add_graph->setToolTip(
                tr("Add selected Y column(s) (or all Y columns) to most recent graph"));
    }
    if (d_btn_quick_col_props) {
        d_btn_quick_col_props->setText(tr("Col Props"));
        d_btn_quick_col_props->setToolTip(tr("Toggle Column Description & Type Side Panel"));
    }
    if (d_quick_bar) {
        if (QToolButton *moreBtn = d_quick_bar->findChild<QToolButton *>(
                    QStringLiteral("WorksheetMoreRoleBtn"))) {
            moreBtn->setToolTip(
                    tr("More column plot roles (\xC2\xB1xErr, Z, Label, Disregard)"));
            if (QMenu *m = moreBtn->menu()) {
                for (QAction *act : m->actions()) {
                    const int rc = act->data().toInt();
                    if (rc == QuickRoleXErr)
                        act->setText(tr("\xC2\xB1xErr"));
                    else if (rc == QuickRoleZ)
                        act->setText(QStringLiteral("Z"));
                    else if (rc == QuickRoleLabel)
                        act->setText(tr("Label"));
                    else if (rc == QuickRoleNone)
                        act->setText(tr("Disregard"));
                }
            }
        }
    }
    syncQuickBarState();

    // prevent losing current selection on retranslate
    auto index = ui.type_box->currentIndex();
    if (-1 == index)
        index = 0;
    ui.type_box->clear();
    ui.type_box->addItem(tr("Numeric"), QVariant(int(SciDAVis::ColumnMode::Numeric)));
    ui.type_box->addItem(tr("Text"), QVariant(int(SciDAVis::ColumnMode::Text)));
    ui.type_box->addItem(tr("Month names"), QVariant(int(SciDAVis::ColumnMode::Month)));
    ui.type_box->addItem(tr("Day names"), QVariant(int(SciDAVis::ColumnMode::Day)));
    ui.type_box->addItem(tr("Date and time"), QVariant(int(SciDAVis::ColumnMode::DateTime)));
    ui.type_box->setCurrentIndex(index);

    // prevent losing current selection on retranslate
    index = ui.date_time_interval->currentIndex();
    if (-1 == index)
        index = 0;
    ui.date_time_interval->clear();
    ui.date_time_interval->addItem(tr("years"), int(NumericDateTimeBaseFilter::UnitInterval::Year));
    ui.date_time_interval->addItem(tr("months"),
                                   int(NumericDateTimeBaseFilter::UnitInterval::Month));
    ui.date_time_interval->addItem(tr("days"), int(NumericDateTimeBaseFilter::UnitInterval::Day));
    ui.date_time_interval->addItem(tr("hours"), int(NumericDateTimeBaseFilter::UnitInterval::Hour));
    ui.date_time_interval->addItem(tr("minutes"),
                                   int(NumericDateTimeBaseFilter::UnitInterval::Minute));
    ui.date_time_interval->addItem(tr("seconds"),
                                   int(NumericDateTimeBaseFilter::UnitInterval::Second));
    ui.date_time_interval->addItem(tr("milliseconds"),
                                   int(NumericDateTimeBaseFilter::UnitInterval::Millisecond));
    ui.date_time_interval->setCurrentIndex(index);

    // TODO: implement formula stuff
    // ui.formula_info->document()->setPlainText("not implemented yet");
}

void TableView::advanceCell()
{
    if (d_table) {
        QModelIndex idx = d_view_widget->currentIndex();
        if (idx.row() + 1 >= d_table->rowCount()) {
            int new_size = d_table->rowCount() + 1;
            d_table->setRowCount(new_size);
        }
        d_view_widget->setCurrentIndex(idx.sibling(idx.row() + 1, idx.column()));
    }
}

void TableView::goToCell(int row, int col)
{
    QModelIndex index = d_model->index(row, col);
    d_view_widget->scrollTo(index);
    d_view_widget->setCurrentIndex(index);
}

void TableView::selectAll()
{
    d_view_widget->selectAll();
}

void TableView::deselectAll()
{
    d_view_widget->clearSelection();
}

void TableView::toggleControlTabBar()
{
    d_control_tabs->setVisible(!d_control_tabs->isVisible());
    if (d_control_tabs->isVisible())
        d_hide_button->setArrowType(Qt::RightArrow);
    else
        d_hide_button->setArrowType(Qt::LeftArrow);
    syncQuickBarState();
}

void TableView::handleHorizontalSectionMoved(int, int from, int to)
{
    static bool inside = false;
    if (inside)
        return;

    inside = true;
    d_view_widget->horizontalHeader()->moveSection(to, from);
    inside = false;
    if (d_table)
        d_table->moveColumn(from, to);
}

void TableView::handleHorizontalHeaderDoubleClicked(int index)
{
    if (d_table && index >= 0 && index < d_table->columnCount()) {
        d_table->setCurrentColumn(index);
        emit d_table->requestColumnPropertiesDialog(index);
        return;
    }
    showControlDescriptionTab();
}

bool TableView::areCommentsShown() const
{
    return d_horizontal_header->areCommentsShown();
}

void TableView::toggleComments()
{
    const bool next = !areCommentsShown();
    d_user_hid_comments = !next;
    showComments(next);
}

void TableView::showComments(bool on)
{
    d_horizontal_header->showComments(on);
    update();
}

void TableView::currentColumnChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    int col = current.column();
    if (col < 0 || (d_table && col >= d_table->columnCount()))
        return;
    setColumnForControlTabs(col);
    d_table->setCurrentColumn(col);
}

void TableView::setColumnForControlTabs(int col)
{
    if (d_table) {
        if (col < 0 || col >= d_table->columnCount())
            return;
        ScopedUiFlagGuard guard(d_updating_ui);
        d_comment_dirty = false;
        Column *col_ptr = d_table->column(col);

        QString str =
                tr("Current column:\nName: %1\nPosition: %2").arg(col_ptr->name()).arg(col + 1);

        QSignalBlocker bName(ui.name_edit);
        ui.name_edit->setText(col_ptr->name());
        if (d_long_name_edit) {
            QSignalBlocker bLn(d_long_name_edit);
            d_long_name_edit->setText(col_ptr->longName());
        }
        if (d_units_edit) {
            QSignalBlocker bUn(d_units_edit);
            d_units_edit->setText(col_ptr->units());
        }
        if (ui.comment_box) {
            QSignalBlocker bCmt(ui.comment_box);
            ui.comment_box->document()->setPlainText(col_ptr->comment());
        }
        ui.type_box->setCurrentIndex(ui.type_box->findData((int)col_ptr->columnMode()));
        switch (col_ptr->columnMode()) {
        case SciDAVis::ColumnMode::Numeric: {
            Double2StringFilter *filter =
                    static_cast<Double2StringFilter *>(col_ptr->outputFilter());
            ui.format_box->setCurrentIndex(ui.format_box->findData(filter->numericFormat()));
            QSignalBlocker bDig(ui.digits_box);
            ui.digits_box->setValue(filter->numDigits());
            ui.date_time_interval->setVisible(false);
            ui.date_time_interval_label->setVisible(false);
            ui.date_time_0->setVisible(false);
            ui.date_time_0_label->setVisible(false);
            break;
        }
        case SciDAVis::ColumnMode::Month:
        case SciDAVis::ColumnMode::Day:
        case SciDAVis::ColumnMode::DateTime: {
            DateTime2StringFilter *filter =
                    static_cast<DateTime2StringFilter *>(col_ptr->outputFilter());
            QSignalBlocker bFmt(ui.formatLineEdit);
            ui.formatLineEdit->setText(filter->format());
            ui.format_box->setCurrentIndex(ui.format_box->findData(filter->format()));
            auto num_filter = col_ptr->numericDateTimeBaseFilter();
            ui.date_time_0->setDateTime(num_filter->getBaseDateTime());
            auto unit = static_cast<int>(num_filter->getUnitInterval());
            ui.date_time_interval->setCurrentIndex(ui.date_time_interval->findData(unit));
            break;
        }
        default:
            ui.date_time_interval->setVisible(false);
            ui.date_time_interval_label->setVisible(false);
            ui.date_time_0->setVisible(false);
            ui.date_time_0_label->setVisible(false);
            break;
        }
        QString fxStr;
        const QList<Interval<int>> ivs = col_ptr->formulaIntervals();
        for (const Interval<int> &iv : ivs) {
            const QString f = col_ptr->formula(iv.start()).trimmed();
            if (!f.isEmpty()) {
                fxStr = f;
                break;
            }
        }
        if (fxStr.isEmpty())
            fxStr = col_ptr->formula(0);
        ui.formula_box->setText(fxStr);
        syncQuickBarState();
    }
}

void TableView::handleAspectDescriptionChanged(const AbstractAspect *aspect)
{
    if (d_table) {
        const Column *col = qobject_cast<const Column *>(aspect);
        if (!col || col->parentAspect() != static_cast<AbstractAspect *>(d_table))
            return;
        const int colIdx = d_table->columnIndex(col);
        ui.add_reference_combobox->setItemText(colIdx, "col(\"" + col->name() + "\")");
        if (!areCommentsShown() && !d_user_hid_comments) {
            if (!col->longName().isEmpty() || !col->units().isEmpty()
                || !col->comment().isEmpty() || !col->formulaIntervals().isEmpty()) {
                showComments(true);
            }
        }
        autoExpandColumnWidthForHeader(colIdx, false);
        if (d_horizontal_header)
            d_horizontal_header->refresh();
        if (!d_updating_ui && d_view_widget && d_view_widget->selectionModel()) {
            const int curCol = d_view_widget->selectionModel()->currentIndex().column();
            if (curCol == colIdx)
                setColumnForControlTabs(curCol);
            else
                syncQuickBarState();
        }
    }
}

void TableView::handleAspectAdded(const AbstractAspect *aspect)
{
    if (d_table) {
        const Column *col = qobject_cast<const Column *>(aspect);
        if (!col || col->parentAspect() != static_cast<AbstractAspect *>(d_table))
            return;
        const int colIdx = d_table->indexOfChild(aspect);
        ui.add_reference_combobox->insertItem(colIdx, "col(\"" + col->name() + "\")");
        autoExpandColumnWidthForHeader(colIdx, false);
    }
}

void TableView::handleAspectAboutToBeRemoved(const AbstractAspect *parent, int index)
{
    if (parent != d_table)
        return;
    ui.add_reference_combobox->removeItem(index);
}

void TableView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    syncQuickBarState();
}

void TableView::updateFormatBox()
{
    ScopedUiFlagGuard guard(d_updating_ui);
    int type_index = ui.type_box->currentIndex();
    if (type_index < 0)
        return; // should never happen
    ui.format_box->clear();
    ui.digits_box->setEnabled(false);
    ui.formatLineEdit->setEnabled(false);
    ui.date_time_interval->setEnabled(false);
    ui.date_time_0->setEnabled(false);
    switch (static_cast<SciDAVis::ColumnMode>(ui.type_box->itemData(type_index).toInt())) {
    case SciDAVis::ColumnMode::Numeric:
        ui.digits_box->setEnabled(true);
        ui.format_box->addItem(tr("Decimal"), QVariant('f'));
        ui.format_box->addItem(tr("Scientific (e)"), QVariant('e'));
        ui.format_box->addItem(tr("Scientific (E)"), QVariant('E'));
        ui.format_box->addItem(tr("Automatic (e)"), QVariant('g'));
        ui.format_box->addItem(tr("Automatic (E)"), QVariant('G'));
        break;
    case SciDAVis::ColumnMode::Text:
        ui.format_box->addItem(tr("Text"), QVariant());
        break;
    case SciDAVis::ColumnMode::Month:
        ui.format_box->addItem(tr("Number without leading zero"), QVariant("M"));
        ui.format_box->addItem(tr("Number with leading zero"), QVariant("MM"));
        ui.format_box->addItem(tr("Abbreviated month name"), QVariant("MMM"));
        ui.format_box->addItem(tr("Full month name"), QVariant("MMMM"));
        break;
    case SciDAVis::ColumnMode::Day:
        ui.format_box->addItem(tr("Number without leading zero"), QVariant("d"));
        ui.format_box->addItem(tr("Number with leading zero"), QVariant("dd"));
        ui.format_box->addItem(tr("Abbreviated day name"), QVariant("ddd"));
        ui.format_box->addItem(tr("Full day name"), QVariant("dddd"));
        break;
    case SciDAVis::ColumnMode::DateTime: {
        // TODO: allow adding of the combo box entries here
        const char *date_strings[] = {
            "yyyy-MM-dd", "yyyy/MM/dd", "dd/MM/yyyy", "dd/MM/yy", "dd.MM.yyyy",
            "dd.MM.yy",   "MM/yyyy",    "dd.MM.",     "yyyyMMdd", 0
        };

        const char *time_strings[] = {
            "hh",           "hh ap",        "hh:mm",     "hh:mm ap", "hh:mm:ss",
            "hh:mm:ss.zzz", "hh:mm:ss:zzz", "mm:ss.zzz", "hhmmss",   0
        };
        int j, i;
        for (i = 0; date_strings[i] != 0; i++)
            ui.format_box->addItem(QString(date_strings[i]), QVariant(date_strings[i]));
        for (j = 0; time_strings[j] != 0; j++)
            ui.format_box->addItem(QString(time_strings[j]), QVariant(time_strings[j]));
        for (i = 0; date_strings[i] != 0; i++)
            for (j = 0; time_strings[j] != 0; j++)
                ui.format_box->addItem(
                        QString("%1 %2").arg(date_strings[i]).arg(time_strings[j]),
                        QVariant(QString(date_strings[i]) + " " + QString(time_strings[j])));
        ui.formatLineEdit->setEnabled(true);
        ui.date_time_interval->setEnabled(true);
        ui.date_time_0->setEnabled(true);
        break;
    }
    default:
        ui.format_box->addItem(QString()); // just for savety to have at least one item in any case
    }
    ui.format_box->setCurrentIndex(0);
    ui.digits_box->setVisible(ui.digits_box->isEnabled());
    ui.digits_label->setVisible(ui.digits_box->isEnabled());
    ui.formatLineEdit->setVisible(ui.formatLineEdit->isEnabled());
    ui.format_label2->setVisible(ui.formatLineEdit->isEnabled());
    if (ui.format_label2->isVisible())
        ui.format_label->setText(tr("Predefined:"));
    else
        ui.format_label->setText(tr("Format:"));
    ui.date_time_interval->setVisible(ui.date_time_interval->isEnabled());
    ui.date_time_interval_label->setVisible(ui.date_time_interval->isEnabled());
    ui.date_time_0->setVisible(ui.date_time_0->isEnabled());
    ui.date_time_0_label->setVisible(ui.date_time_0->isEnabled());
}

void TableView::updateTypeInfo()
{
    int format_index = ui.format_box->currentIndex();
    int type_index = ui.type_box->currentIndex();

    QString str = tr("Selected column type:\n");
    if (format_index >= 0 && type_index >= 0) {
        SciDAVis::ColumnMode type = static_cast<SciDAVis::ColumnMode>(ui.type_box->itemData(type_index).toInt());
        switch (type) {
        case SciDAVis::ColumnMode::Numeric:
            str += tr("Double precision\nfloating point values\n");
            ui.digits_box->setEnabled(true);
            break;
        case SciDAVis::ColumnMode::Text:
            str += tr("Text\n");
            break;
        case SciDAVis::ColumnMode::Month:
            str += tr("Month names\n");
            break;
        case SciDAVis::ColumnMode::Day:
            str += tr("Days of the week\n");
            break;
        case SciDAVis::ColumnMode::DateTime:
            str += tr("Dates and/or times\n");
            ui.formatLineEdit->setEnabled(true);
            break;
        }
        str += tr("Example: ");
        switch (type) {
        case SciDAVis::ColumnMode::Numeric:
            str += QString::number(123.1234567890123456,
                                   ui.format_box->itemData(format_index).toChar().toLatin1(),
                                   ui.digits_box->value());
            break;
        case SciDAVis::ColumnMode::Text:
            str += tr("Hello world!\n");
            break;
        case SciDAVis::ColumnMode::Month:
            str += QLocale().toString(QDate(1900, 1, 1),
                                      ui.format_box->itemData(format_index).toString());
            break;
        case SciDAVis::ColumnMode::Day:
            str += QLocale().toString(QDate(1900, 1, 1),
                                      ui.format_box->itemData(format_index).toString());
            break;
        case SciDAVis::ColumnMode::DateTime:
            ui.formatLineEdit->setText(ui.format_box->itemData(format_index).toString());
            ui.date_time_0->setDisplayFormat(ui.format_box->itemData(format_index).toString());
            str += QDateTime(QDate(1900, 1, 1), QTime(23, 59, 59, 999))
                           .toString(ui.formatLineEdit->text());
            break;
        }
    } else if (format_index == -1 && type_index >= 0
               && static_cast<SciDAVis::ColumnMode>(ui.type_box->itemData(type_index).toInt())
                       == SciDAVis::ColumnMode::DateTime) {
        str += tr("Dates and/or times\n");
        ui.formatLineEdit->setEnabled(true);
        str += tr("Example: ");
        str += QDateTime(QDate(1900, 1, 1), QTime(23, 59, 59, 999))
                       .toString(ui.formatLineEdit->text());
    }

    ui.type_info->setText(str);
    ui.digits_box->setVisible(ui.digits_box->isEnabled());
    ui.digits_label->setVisible(ui.digits_box->isEnabled());
    ui.formatLineEdit->setVisible(ui.formatLineEdit->isEnabled());
    ui.format_label2->setVisible(ui.formatLineEdit->isEnabled());
    if (ui.format_label2->isVisible())
        ui.format_label->setText(tr("Predefined:"));
    else
        ui.format_label->setText(tr("Format:"));
}

void TableView::handleFormatLineEditChange()
{
    int type_index = ui.type_box->currentIndex();

    if (type_index >= 0) {
        SciDAVis::ColumnMode type = static_cast<SciDAVis::ColumnMode>(ui.type_box->itemData(type_index).toInt());
        if (type == SciDAVis::ColumnMode::DateTime) {
            QString str = tr("Selected column type:\n");
            str += tr("Dates and/or times\n");
            str += tr("Example: ");
            str += QDateTime(QDate(1900, 1, 1), QTime(23, 59, 59, 999))
                           .toString(ui.formatLineEdit->text());
            ui.type_info->setText(str);
            ui.date_time_0->setDisplayFormat(ui.formatLineEdit->text());
        }
    }
}

void TableView::showControlDescriptionTab()
{
    d_control_tabs->setVisible(true);
    d_hide_button->setArrowType(Qt::RightArrow);
    ui.tab_widget->setCurrentIndex(0);
    ui.tab_widget->setFocus();
    syncQuickBarState();
}

void TableView::showControlTypeTab()
{
    d_control_tabs->setVisible(true);
    d_hide_button->setArrowType(Qt::RightArrow);
    ui.tab_widget->setCurrentIndex(1);
    ui.tab_widget->setFocus();
    syncQuickBarState();
}

void TableView::showControlFormulaTab()
{
    d_control_tabs->setVisible(true);
    d_hide_button->setArrowType(Qt::RightArrow);
    ui.tab_widget->setCurrentIndex(2);
    ui.tab_widget->setFocus();
    syncQuickBarState();
}

void TableView::applyDescription()
{
    if (d_updating_ui)
        return;
    ScopedUiFlagGuard guard(d_updating_ui);
    QItemSelectionModel *sel_model = d_view_widget ? d_view_widget->selectionModel() : nullptr;
    if (!sel_model)
        return;
    int index = sel_model->currentIndex().column();
    if (d_table && index >= 0 && index < d_table->columnCount()) {
        Column *col = d_table->column(index);
        if (!col)
            return;
        // changing the name triggers an update of the UI, which also resets the content of the
        // comment box => need to cache it so name and comment can be changed simultaneously
        const QString shortName = ui.name_edit->text().trimmed();
        const QString longName = d_long_name_edit ? d_long_name_edit->text() : QString();
        const QString unitsStr = d_units_edit ? d_units_edit->text() : QString();
        const QString comment = ui.comment_box->document()->toPlainText();
        d_comment_dirty = false;
        if (!shortName.isEmpty() && col->name() != shortName)
            col->setName(shortName);
        if (col->longName() != longName)
            col->setLongName(longName);
        if (col->units() != unitsStr)
            col->setUnits(unitsStr);
        if (col->comment() != comment)
            col->setComment(comment);
        if (d_horizontal_header)
            d_horizontal_header->refresh();
    }
}

void TableView::applyType()
{
    if (d_updating_ui)
        return;
    ScopedUiFlagGuard guard(d_updating_ui);
    int format_index = ui.format_box->currentIndex();
    int type_index = ui.type_box->currentIndex();
    if (format_index < 0 && type_index < 0)
        return;

    SciDAVis::ColumnMode new_mode = (SciDAVis::ColumnMode)ui.type_box->itemData(type_index).toInt();
    QList<Column *> list = selectedColumns();
    if ((0 == list.size()) && (nullptr != d_table->currentColumn()))
        list.append(d_table->currentColumn());
    switch (new_mode) {
    case SciDAVis::ColumnMode::Numeric: {
        foreach (Column *col, list) {
            col->beginMacro(QObject::tr("%1: change column type").arg(col->name()));
            col->setColumnMode(new_mode);
            Double2StringFilter *filter = static_cast<Double2StringFilter *>(col->outputFilter());
            int digits =
                    ui.digits_box->value(); // setNumericFormat causes digits_box to be modified...
            filter->setNumericFormat(ui.format_box->itemData(format_index).toChar().toLatin1());
            filter->setNumDigits(digits);
            col->endMacro();
        }
        break;
    }
    case SciDAVis::ColumnMode::Text:
        foreach (Column *col, list)
            col->setColumnMode(new_mode);
        break;
    case SciDAVis::ColumnMode::Month:
    case SciDAVis::ColumnMode::Day:
    case SciDAVis::ColumnMode::DateTime:
        QString format;
        if (ui.formatLineEdit->isEnabled())
            format = ui.formatLineEdit->text();
        else
            format = ui.format_box->itemData(format_index).toString();
        foreach (Column *col, list) {
            col->beginMacro(QObject::tr("%1: change column type").arg(col->name()));
            SciDAVis::ColumnMode old_mode = col->columnMode();
            AbstractFilter *converter = 0;
            switch (old_mode) {
            case SciDAVis::ColumnMode::Numeric: // the mode is changed
            case SciDAVis::ColumnMode::DateTime: // the mode is not changed, but numeric converter parameters is
                                     // (possibly) changed
                if (ui.date_time_interval->isVisible()) {
                    Double2DateTimeFilter::UnitInterval unit =
                            (Double2DateTimeFilter::UnitInterval)ui.date_time_interval
                                    ->itemData(ui.date_time_interval->currentIndex())
                                    .toInt();
                    QDateTime date_time_0 = ui.date_time_0->dateTime();
                    converter = new Double2DateTimeFilter(unit, date_time_0);
                }
                break;
            case SciDAVis::ColumnMode::Text:
                converter = new String2DateTimeFilter(format);
                break;
            default:
                break;
            }
            col->setColumnMode(new_mode, converter);
            {
                auto filter = dynamic_cast<DateTime2StringFilter *>(col->outputFilter());
                if (nullptr != filter)
                    filter->setFormat(format);
            }
            {
                auto filter = dynamic_cast<String2DateTimeFilter *>(col->inputFilter());
                if (nullptr != filter)
                    filter->setFormat(format);
            }
            col->endMacro();
        }
        break;
    }
}

void TableView::handleHeaderDataChanged(Qt::Orientation orientation, int first, int last)
{
    if (orientation != Qt::Horizontal)
        return;

    QItemSelectionModel *sel_model = d_view_widget->selectionModel();

    int col = sel_model->currentIndex().column();
    if (col < first || col > last)
        return;
    setColumnForControlTabs(col);
}

int TableView::selectedColumnCount(bool full)
{
    int count = 0;
    if (d_table) {
        int cols = d_table->columnCount();
        for (int i = 0; i < cols; i++)
            if (isColumnSelected(i, full))
                count++;
    }
    return count;
}

int TableView::selectedColumnCount(SciDAVis::PlotDesignation pd)
{
    int count = 0;
    if (d_table) {
        int cols = d_table->columnCount();
        for (int i = 0; i < cols; i++)
            if (isColumnSelected(i, false) && (d_table->column(i)->plotDesignation() == pd))
                count++;
    }
    return count;
}

bool TableView::isColumnSelected(int col, bool full)
{
    if (full)
        return d_view_widget->selectionModel()->isColumnSelected(col, QModelIndex());
    else
        return d_view_widget->selectionModel()->columnIntersectsSelection(col, QModelIndex());
}

QList<Column *> TableView::selectedColumns(bool full)
{
    QList<Column *> list;
    if (d_table) {
        int cols = d_table->columnCount();
        for (int i = 0; i < cols; i++)
            if (isColumnSelected(i, full))
                list << d_table->column(i);
    }
    return list;
}

QList<int> TableView::selectedColumnIndices(bool full)
{
    QList<int> list;
    if (d_table) {
        int cols = d_table->columnCount();
        for (int i = 0; i < cols; i++) {
            if (isColumnSelected(i, full))
                list << i;
        }
    }
    return list;
}

QByteArray TableView::buildColumnMimePayload(future::Table *table, const QList<int> &colIndices,
                                             int anchorCol)
{
    if (!table || table->columnCount() <= 0)
        return QByteArray();

    const int totalCols = table->columnCount();
    QList<int> orderedCols;
    for (int c : colIndices) {
        if (c >= 0 && c < totalCols && !orderedCols.contains(c))
            orderedCols.append(c);
    }
    if (orderedCols.isEmpty() && anchorCol >= 0 && anchorCol < totalCols)
        orderedCols.append(anchorCol);
    if (orderedCols.isEmpty())
        return QByteArray();
    std::sort(orderedCols.begin(), orderedCols.end());

    QList<int> xCols;
    QList<int> errCols;
    QList<int> plotCols;
    for (int c : orderedCols) {
        Column *col = table->column(c);
        if (!col)
            continue;
        const SciDAVis::PlotDesignation pd = col->plotDesignation();
        if (pd == SciDAVis::X)
            xCols.append(c);
        else if (pd == SciDAVis::xErr || pd == SciDAVis::yErr)
            errCols.append(c);
        else
            plotCols.append(c);
    }

    QString xColName;
    QJsonArray xColumnsJson;
    if (!plotCols.isEmpty()) {
        if (xCols.size() == 1) {
            if (Column *xc = table->column(xCols.first()))
                xColName = xc->name();
            for (int k = 0; k < plotCols.size(); ++k)
                xColumnsJson.append(xColName);
        } else {
            for (int k = 0; k < plotCols.size(); ++k) {
                int xi = table->colX(plotCols[k]);
                if (xi < 0 || xi >= totalCols || xi == plotCols[k]) {
                    if (!xCols.isEmpty())
                        xi = xCols.first();
                    else if (totalCols > 1 && plotCols[k] != 0)
                        xi = 0;
                    else if (totalCols > 1)
                        xi = 1;
                    else
                        xi = 0;
                }
                QString curXName;
                if (xi >= 0 && xi < totalCols && table->column(xi))
                    curXName = table->column(xi)->name();
                xColumnsJson.append(curXName);
            }
        }
    } else {
        plotCols = orderedCols;
        errCols.clear();
        for (int k = 0; k < plotCols.size(); ++k) {
            int xi = table->colX(plotCols[k]);
            if (xi == plotCols[k])
                xi = -1;
            if (xi < 0 && totalCols > 1 && plotCols[k] != 0)
                xi = 0;
            QString curXName;
            if (xi >= 0 && xi < totalCols && table->column(xi))
                curXName = table->column(xi)->name();
            xColumnsJson.append(curXName);
            if (k == 0 && plotCols.size() == 1)
                xColName = curXName;
        }
    }

    QJsonArray columnsJson;
    for (int c : plotCols) {
        if (Column *col = table->column(c))
            columnsJson.append(col->name());
    }
    if (columnsJson.isEmpty())
        return QByteArray();

    QJsonArray errColumnsJson;
    for (int c : errCols) {
        if (Column *col = table->column(c))
            errColumnsJson.append(col->name());
    }

    QJsonObject payload;
    payload[QStringLiteral("table")] = table->name();
    payload[QStringLiteral("x_column")] = xColName;
    payload[QStringLiteral("x_columns")] = xColumnsJson;
    payload[QStringLiteral("columns")] = columnsJson;
    payload[QStringLiteral("err_columns")] = errColumnsJson;
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QByteArray TableView::buildSelectedColumnsMimePayload(int clickedCol)
{
    if (!d_table)
        return QByteArray();
    QList<int> cols = selectedColumnIndices(false);
    if (clickedCol >= 0 && !cols.contains(clickedCol))
        cols = QList<int> { clickedCol };
    return buildColumnMimePayload(d_table, cols, clickedCol);
}

bool TableView::startColumnDrag(const QList<int> &explicitCols, int clickedCol, QWidget *dragSource)
{
    if (!d_table)
        return false;

    const QByteArray payloadBytes = explicitCols.isEmpty()
            ? buildSelectedColumnsMimePayload(clickedCol)
            : buildColumnMimePayload(d_table, explicitCols, clickedCol);
    if (payloadBytes.isEmpty())
        return false;

    QMimeData *mime = new QMimeData();
    mime->setData(QString::fromUtf8(MultiLayer::kColumnMimeType), payloadBytes);

    QDrag *drag = new QDrag(dragSource ? dragSource : this);
    drag->setMimeData(mime);

    const QJsonObject obj = QJsonDocument::fromJson(payloadBytes).object();
    const QJsonArray colsArr = obj.value(QStringLiteral("columns")).toArray();
    if (!colsArr.isEmpty()) {
        QString label = d_table->name() + QStringLiteral(": ") + colsArr.first().toString();
        if (colsArr.size() > 1)
            label += QStringLiteral(" (+%1)").arg(colsArr.size() - 1);
        QFont badgeFont(QStringLiteral("Arial"), 9, QFont::DemiBold);
        QFontMetrics fm(badgeFont);
        const int padX = 10;
        const int padY = 5;
        const QSize badgeSize(qBound(64, fm.horizontalAdvance(label) + padX * 2, 280),
                              fm.height() + padY * 2);
        QPixmap badge(badgeSize);
        badge.fill(Qt::transparent);
        QPainter p(&badge);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(QColor(30, 64, 175, 230));
        p.setPen(QPen(QColor(191, 219, 254), 1));
        p.drawRoundedRect(badge.rect().adjusted(0, 0, -1, -1), 5.0, 5.0);
        p.setFont(badgeFont);
        p.setPen(Qt::white);
        p.drawText(badge.rect().adjusted(padX, 0, -padX, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   fm.elidedText(label, Qt::ElideRight, badgeSize.width() - padX * 2));
        p.end();
        drag->setPixmap(badge);
        drag->setHotSpot(QPoint(12, badgeSize.height() / 2));
    }

    drag->exec(Qt::CopyAction);
    return true;
}

bool TableView::isPointOnSelectionEdge(const QPoint &viewportPos)
{
    if (!d_table || !d_view_widget || !d_view_widget->viewport() || selectedColumnCount(false) <= 0)
        return false;

    const int c1 = firstSelectedColumn(false);
    const int c2 = lastSelectedColumn(false);
    const int r1 = firstSelectedRow(false);
    const int r2 = lastSelectedRow(false);
    if (c1 < 0 || c2 < c1 || r1 < 0 || r2 < r1)
        return false;

    const QRect vpRect = d_view_widget->viewport()->rect();
    const int left = d_view_widget->columnViewportPosition(c1);
    const int right = d_view_widget->columnViewportPosition(c2) + d_view_widget->columnWidth(c2) - 1;
    const int top = d_view_widget->rowViewportPosition(r1);
    const int bottom = d_view_widget->rowViewportPosition(r2) + d_view_widget->rowHeight(r2) - 1;
    if (right < 0 || left > vpRect.right() || bottom < 0 || top > vpRect.bottom())
        return false;

    const QRect selRect(QPoint(left, top), QPoint(right, bottom));
    const QRect outer = selRect.adjusted(-3, -3, 3, 3);
    const QRect inner = selRect.adjusted(5, 5, -5, -5);
    if (!outer.contains(viewportPos))
        return false;
    if (inner.isValid() && inner.contains(viewportPos))
        return false;
    return true;
}

int TableView::selectedRowCount(bool full)
{
    int count = 0;
    if (d_table) {
        int rows = d_table->rowCount();
        for (int i = 0; i < rows; i++)
            if (isRowSelected(i, full))
                count++;
    }
    return count;
}

bool TableView::isRowSelected(int row, bool full)
{
    if (full)
        return d_view_widget->selectionModel()->isRowSelected(row, QModelIndex());
    else
        return d_view_widget->selectionModel()->rowIntersectsSelection(row, QModelIndex());
}

int TableView::firstSelectedColumn(bool full)
{
    if (d_table) {
        int cols = d_table->columnCount();
        for (int i = 0; i < cols; i++) {
            if (isColumnSelected(i, full))
                return i;
        }
    }
    return -1;
}

int TableView::lastSelectedColumn(bool full)
{
    if (d_table) {
        int cols = d_table->columnCount();
        for (int i = cols - 1; i >= 0; i--)
            if (isColumnSelected(i, full))
                return i;
    }
    return -2;
}

int TableView::firstSelectedRow(bool full)
{
    if (d_table) {
        int rows = d_table->rowCount();
        for (int i = 0; i < rows; i++) {
            if (isRowSelected(i, full))
                return i;
        }
    }
    return -1;
}

int TableView::lastSelectedRow(bool full)
{
    if (d_table) {
        int rows = d_table->rowCount();
        for (int i = rows - 1; i >= 0; i--)
            if (isRowSelected(i, full))
                return i;
    }
    return -2;
}

IntervalAttribute<bool> TableView::selectedRows(bool full)
{
    IntervalAttribute<bool> result;
    if (d_table) {
        int rows = d_table->rowCount();
        for (int i = 0; i < rows; i++)
            if (isRowSelected(i, full))
                result.setValue(i, true);
    }
    return result;
}

bool TableView::hasMultiSelection()
{
    QModelIndexList indexes = d_view_widget->selectionModel()->selectedIndexes();
    if (indexes.size() < 2)
        return false;

    QModelIndex index;
    int minrow = indexes.at(0).row();
    int maxrow = minrow;
    int mincol = indexes.at(0).column();
    int maxcol = mincol;
    foreach (index, indexes) {
        minrow = std::min(minrow, index.row());
        maxrow = std::max(maxrow, index.row());
        mincol = std::min(mincol, index.column());
        maxcol = std::max(maxcol, index.column());
    }
    int spanned = (maxrow - minrow + 1) * (maxcol - mincol + 1);
    return (spanned > d_view_widget->selectionModel()->selectedIndexes().size());
}

bool TableView::isCellSelected(int row, int col)
{
    if (!d_table || row < 0 || col < 0 || row >= d_table->rowCount()
        || col >= d_table->columnCount())
        return false;

    return d_view_widget->selectionModel()->isSelected(d_model->index(row, col));
}

void TableView::setCellSelected(int row, int col, bool select)
{
    d_view_widget->selectionModel()->select(d_model->index(row, col),
                                            select ? QItemSelectionModel::Select
                                                   : QItemSelectionModel::Deselect);
}

void TableView::setCellsSelected(int first_row, int first_col, int last_row, int last_col,
                                 bool select)
{
    QModelIndex top_left = d_model->index(first_row, first_col);
    QModelIndex bottom_right = d_model->index(last_row, last_col);
    d_view_widget->selectionModel()->select(QItemSelection(top_left, bottom_right),
                                            select ? QItemSelectionModel::SelectCurrent
                                                   : QItemSelectionModel::Deselect);
}

void TableView::getCurrentCell(int *row, int *col)
{
    QModelIndex index = d_view_widget->selectionModel()->currentIndex();
    if (index.isValid()) {
        *row = index.row();
        *col = index.column();
    } else {
        *row = -1;
        *col = -1;
    }
}

bool TableView::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::FocusIn
        && (watched == d_long_name_edit || watched == d_units_edit || watched == ui.comment_box)) {
        d_last_desc_edit = qobject_cast<QWidget *>(watched);
    }
    if (ui.comment_box && watched == ui.comment_box && event->type() == QEvent::FocusOut) {
        if (d_comment_dirty && !d_updating_ui) {
            d_comment_dirty = false;
            applyDescription();
        }
    }

    QHeaderView *v_header = d_view_widget->verticalHeader();

    if (d_view_widget && watched == d_view_widget->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (d_cell_drag_candidate && !d_cell_drag_started
                && (me->buttons() & Qt::LeftButton)) {
                if ((me->pos() - d_cell_drag_start_pos).manhattanLength()
                    >= QApplication::startDragDistance()) {
                    d_cell_drag_started = true;
                    d_cell_drag_candidate = false;
                    if (d_edge_hover_cursor) {
                        d_view_widget->viewport()->unsetCursor();
                        d_edge_hover_cursor = false;
                    }
                    QList<int> cols = selectedColumnIndices(false);
                    if (cols.isEmpty() && d_cell_drag_col >= 0)
                        cols.append(d_cell_drag_col);
                    else if ((me->modifiers() & Qt::AltModifier) && d_cell_drag_col >= 0
                             && !cols.contains(d_cell_drag_col))
                        cols = QList<int> { d_cell_drag_col };
                    QPointer<TableView> self(this);
                    startColumnDrag(cols, d_cell_drag_col, d_view_widget->viewport());
                    if (!self)
                        return true;
                    d_cell_drag_started = false;
                    d_cell_drag_candidate = false;
                    d_cell_drag_was_edge = false;
                }
                return true;
            } else if (me->buttons() == Qt::NoButton) {
                const bool onEdge = isPointOnSelectionEdge(me->pos());
                if (onEdge != d_edge_hover_cursor) {
                    d_edge_hover_cursor = onEdge;
                    if (onEdge)
                        d_view_widget->viewport()->setCursor(Qt::OpenHandCursor);
                    else
                        d_view_widget->viewport()->unsetCursor();
                }
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            d_cell_drag_candidate = false;
            d_cell_drag_started = false;
            d_cell_drag_was_edge = false;
            if (me->button() == Qt::LeftButton && d_table) {
                const QModelIndex idx = d_view_widget->indexAt(me->pos());
                const bool altHeld = (me->modifiers() & Qt::AltModifier);
                const bool edgeHit = !(me->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))
                        && isPointOnSelectionEdge(me->pos());
                if ((altHeld && (idx.isValid() || selectedColumnCount(false) > 0)) || edgeHit) {
                    d_cell_drag_candidate = true;
                    d_cell_drag_was_edge = edgeHit;
                    d_cell_drag_start_pos = me->pos();
                    d_cell_drag_col = idx.isValid() ? idx.column() : firstSelectedColumn(false);
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && (d_cell_drag_candidate || d_cell_drag_started)) {
                const bool wasCandidateOnly = d_cell_drag_candidate && !d_cell_drag_started;
                const bool wasEdge = d_cell_drag_was_edge;
                d_cell_drag_candidate = false;
                d_cell_drag_started = false;
                d_cell_drag_was_edge = false;
                if (wasCandidateOnly && wasEdge) {
                    const QModelIndex idx = d_view_widget->indexAt(me->pos());
                    if (idx.isValid()) {
                        d_view_widget->clearSelection();
                        d_view_widget->setCurrentIndex(idx);
                    }
                }
                return true;
            }
        } else if (event->type() == QEvent::Leave) {
            if (d_edge_hover_cursor) {
                d_view_widget->viewport()->unsetCursor();
                d_edge_hover_cursor = false;
            }
        }
    }

    if (watched == d_quick_bar && event
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        QToolButton *moreBtn =
                d_quick_bar ? d_quick_bar->findChild<QToolButton *>(
                        QStringLiteral("WorksheetMoreRoleBtn"))
                            : nullptr;
        updateWorksheetQuickBarMetrics(d_quick_bar, d_btn_role_x, d_btn_role_y, d_btn_role_yerr,
                                       moreBtn, d_btn_quick_plot_line, d_btn_quick_plot_lp,
                                       d_btn_quick_plot_scatter, d_btn_quick_add_graph,
                                       d_btn_quick_col_props, d_lbl_quick_fx);
    }

    if (d_table && event->type() == QEvent::ContextMenu) {
        QContextMenuEvent *cm_event = static_cast<QContextMenuEvent *>(event);
        QPoint global_pos = cm_event->globalPos();
        if (watched == v_header)
            d_table->showTableViewRowContextMenu(global_pos);
        else if (watched == d_horizontal_header) {
            int col = d_horizontal_header->logicalIndexAt(cm_event->pos());
            if (col >= 0 && col < d_model->columnCount()) {
                if (!isColumnSelected(col, true)) {
                    QItemSelectionModel *sel_model = d_view_widget->selectionModel();
                    sel_model->clearSelection();
                    sel_model->select(
                            QItemSelection(d_model->index(0, col, QModelIndex()),
                                           d_model->index(d_model->rowCount() - 1, col,
                                                          QModelIndex())),
                            QItemSelectionModel::Select);
                }
                d_table->setCurrentColumn(col);
            }
            d_table->showTableViewColumnContextMenu(global_pos);
        } else if (watched == d_view_widget)
            d_table->showTableViewContextMenu(global_pos);
        else
            return MyWidget::eventFilter(watched, event);

        return true;
    } else
        return MyWidget::eventFilter(watched, event);
}

bool TableView::formulaModeActive() const
{
    return d_model->formulaModeActive();
}

void TableView::activateFormulaMode(bool on)
{
    d_model->activateFormulaMode(on);
}

void TableView::goToNextColumn()
{
    if (!d_table || d_table->columnCount() == 0)
        return;

    QModelIndex idx = d_view_widget->currentIndex();
    int col = idx.column() + 1;
    if (col >= d_table->columnCount())
        col = 0;
    d_view_widget->setCurrentIndex(idx.sibling(idx.row(), col));
}

void TableView::goToPreviousColumn()
{
    if (!d_table || d_table->columnCount() == 0)
        return;

    QModelIndex idx = d_view_widget->currentIndex();
    int col = idx.column() - 1;
    if (col < 0)
        col = d_table->columnCount() - 1;
    d_view_widget->setCurrentIndex(idx.sibling(idx.row(), col));
}

void TableView::initWorksheetQuickBar(QWidget *parentWidget)
{
    d_quick_bar = new QFrame(parentWidget);
    d_quick_bar->setObjectName(QStringLiteral("WorksheetQuickBar"));
    d_quick_bar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    d_quick_bar->setStyleSheet(QStringLiteral(
            "#WorksheetQuickBar {"
            "  background: #F8FAFC;"
            "  border-bottom: 1px solid #E2E8F0;"
            "}"
            "#WorksheetQuickBar QToolButton[wsRoleBtn=\"true\"] {"
            "  background: #FFFFFF;"
            "  color: #334155;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 4px;"
            "  padding: 1px 5px;"
            "  min-height: 20px;"
            "  max-height: 22px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 8.3pt;"
            "  font-weight: 600;"
            "}"
            "#WorksheetQuickBar QToolButton#WorksheetMoreRoleBtn::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"
            "#WorksheetQuickBar QToolButton[wsRoleBtn=\"true\"]:hover {"
            "  background: #EFF6FF;"
            "  border-color: #93C5FD;"
            "  color: #1D4ED8;"
            "}"
            "#WorksheetQuickBar QToolButton[wsRoleBtn=\"true\"]:checked {"
            "  background: #2563EB;"
            "  border-color: #1D4ED8;"
            "  color: #FFFFFF;"
            "}"
            "#WorksheetQuickBar QToolButton[wsPlotBtn=\"true\"] {"
            "  background: #FFFFFF;"
            "  color: #1E293B;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 4px;"
            "  padding: 1px 4px;"
            "  min-height: 20px;"
            "  max-height: 22px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 8.3pt;"
            "  font-weight: 600;"
            "}"
            "#WorksheetQuickBar QToolButton[wsPlotBtn=\"true\"]:hover {"
            "  background: #EFF6FF;"
            "  border-color: #60A5FA;"
            "  color: #1D4ED8;"
            "}"
            "#WorksheetQuickBar QToolButton[wsPlotBtn=\"true\"]:checked {"
            "  background: #DBEAFE;"
            "  border-color: #3B82F6;"
            "  color: #1E40AF;"
            "}"
            "#WorksheetQuickBar QToolButton[wsAddBtn=\"true\"] {"
            "  background: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "  border-radius: 4px;"
            "  padding: 1px 4px;"
            "  min-height: 20px;"
            "  max-height: 22px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 8.3pt;"
            "  font-weight: 700;"
            "}"
            "#WorksheetQuickBar QToolButton[wsAddBtn=\"true\"]:hover {"
            "  background: #DBEAFE;"
            "  border-color: #3B82F6;"
            "}"
            "#WorksheetQuickBar QLineEdit#WorksheetQuickFormulaEdit {"
            "  background: #FFFFFF;"
            "  color: #0F172A;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 4px;"
            "  padding: 1px 6px;"
            "  min-height: 20px;"
            "  max-height: 22px;"
            "  font-family: 'Consolas', 'Segoe UI', 'Yu Gothic UI', sans-serif;"
            "  font-size: 8.5pt;"
            "}"
            "#WorksheetQuickBar QLineEdit#WorksheetQuickFormulaEdit:focus {"
            "  border-color: #2563EB;"
            "}"));

    QVBoxLayout *qv = new QVBoxLayout(d_quick_bar);
    qv->setContentsMargins(4, 4, 4, 4);
    qv->setSpacing(3);

    QHBoxLayout *qh1 = new QHBoxLayout();
    qh1->setContentsMargins(0, 0, 0, 0);
    qh1->setSpacing(2);

    auto makeRolePill = [this](int roleCode) -> QToolButton * {
        QToolButton *b = new QToolButton(d_quick_bar);
        b->setCheckable(true);
        b->setAutoRaise(false);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("wsRoleBtn", true);
        connect(b, &QToolButton::clicked, this, [this, roleCode]() {
            applyQuickColumnRole(roleCode);
        });
        return b;
    };

    d_btn_role_x = makeRolePill(QuickRoleX);
    d_btn_role_y = makeRolePill(QuickRoleY);
    d_btn_role_yerr = makeRolePill(QuickRoleYErr);
    d_btn_role_xerr = makeRolePill(QuickRoleXErr);
    d_btn_role_z = makeRolePill(QuickRoleZ);
    d_btn_role_label = makeRolePill(QuickRoleLabel);
    d_btn_role_none = makeRolePill(QuickRoleNone);

    // Secondary roles live inside the compact More popup so Row 1 never elides to "..."
    d_btn_role_xerr->hide();
    d_btn_role_z->hide();
    d_btn_role_label->hide();
    d_btn_role_none->hide();

    qh1->addWidget(d_btn_role_x);
    qh1->addWidget(d_btn_role_y);
    qh1->addWidget(d_btn_role_yerr);

    QToolButton *moreRoleBtn = new QToolButton(d_quick_bar);
    moreRoleBtn->setObjectName(QStringLiteral("WorksheetMoreRoleBtn"));
    moreRoleBtn->setPopupMode(QToolButton::InstantPopup);
    moreRoleBtn->setCheckable(true);
    moreRoleBtn->setAutoRaise(false);
    moreRoleBtn->setCursor(Qt::PointingHandCursor);
    moreRoleBtn->setProperty("wsRoleBtn", true);
    QMenu *moreMenu = new QMenu(moreRoleBtn);
    const int secondaryRoles[] = { QuickRoleXErr, QuickRoleZ, QuickRoleLabel, QuickRoleNone };
    for (int rc : secondaryRoles) {
        QAction *act = moreMenu->addAction(QString());
        act->setCheckable(true);
        act->setData(rc);
        connect(act, &QAction::triggered, this, [this, rc]() { applyQuickColumnRole(rc); });
    }
    moreRoleBtn->setMenu(moreMenu);
    qh1->addWidget(moreRoleBtn);

    auto makeSep = [this]() -> QFrame * {
        QFrame *sep = new QFrame(d_quick_bar);
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        sep->setFixedHeight(18);
        sep->setStyleSheet(QStringLiteral("background: #CBD5E1; border: none; margin: 0px 2px;"));
        return sep;
    };

    qh1->addWidget(makeSep());

    auto makePlotBtn = [this](const char *propName) -> QToolButton * {
        QToolButton *b = new QToolButton(d_quick_bar);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setIconSize(QSize(14, 14));
        b->setAutoRaise(false);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty(propName, true);
        return b;
    };

    d_btn_quick_plot_line = makePlotBtn("wsPlotBtn");
    connect(d_btn_quick_plot_line, &QToolButton::clicked, this,
            [this]() { triggerQuickPlot(Graph::Line); });
    qh1->addWidget(d_btn_quick_plot_line);

    d_btn_quick_plot_lp = makePlotBtn("wsPlotBtn");
    connect(d_btn_quick_plot_lp, &QToolButton::clicked, this,
            [this]() { triggerQuickPlot(Graph::LineSymbols); });
    qh1->addWidget(d_btn_quick_plot_lp);

    d_btn_quick_plot_scatter = makePlotBtn("wsPlotBtn");
    connect(d_btn_quick_plot_scatter, &QToolButton::clicked, this,
            [this]() { triggerQuickPlot(Graph::Scatter); });
    qh1->addWidget(d_btn_quick_plot_scatter);

    d_btn_quick_add_graph = makePlotBtn("wsAddBtn");
    connect(d_btn_quick_add_graph, &QToolButton::clicked, this,
            [this]() { triggerQuickAddToGraph(); });
    qh1->addWidget(d_btn_quick_add_graph);

    qh1->addStretch(1);

    d_btn_quick_col_props = new QToolButton(d_quick_bar);
    d_btn_quick_col_props->setCheckable(true);
    d_btn_quick_col_props->setAutoRaise(false);
    d_btn_quick_col_props->setCursor(Qt::PointingHandCursor);
    d_btn_quick_col_props->setProperty("wsPlotBtn", true);
    connect(d_btn_quick_col_props, &QToolButton::clicked, this,
            [this]() { toggleControlTabBar(); });
    qh1->addWidget(d_btn_quick_col_props);

    qv->addLayout(qh1);

    // Row 2: Unclipped F(x) = Formula Bar spanning full worksheet width
    QHBoxLayout *qh2 = new QHBoxLayout();
    qh2->setContentsMargins(0, 0, 0, 0);
    qh2->setSpacing(5);

    d_lbl_quick_fx = new QLabel(QStringLiteral("F(x) ="), d_quick_bar);
    d_lbl_quick_fx->setStyleSheet(QStringLiteral(
            "color: #1D4ED8; font-family: 'Segoe UI', 'Yu Gothic UI', sans-serif; font-size: 8.5pt; "
            "font-weight: 700; padding-left: 2px; background: transparent;"));
    d_lbl_quick_fx->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    qh2->addWidget(d_lbl_quick_fx);

    d_edit_quick_fx = new QLineEdit(d_quick_bar);
    d_edit_quick_fx->setObjectName(QStringLiteral("WorksheetQuickFormulaEdit"));
    d_edit_quick_fx->setMinimumWidth(80);
    d_edit_quick_fx->setClearButtonEnabled(true);
    connect(d_edit_quick_fx, &QLineEdit::returnPressed, this, [this]() { applyQuickFormula(); });
    qh2->addWidget(d_edit_quick_fx, 1);

    qv->addLayout(qh2);

    d_quick_bar->installEventFilter(this);
}

void TableView::syncQuickBarState()
{
    if (!d_quick_bar)
        return;

    if (d_btn_quick_col_props) {
        QSignalBlocker sb(d_btn_quick_col_props);
        d_btn_quick_col_props->setChecked(d_control_tabs && d_control_tabs->isVisible());
    }

    QList<Column *> cols;
    if (d_table && d_view_widget && d_view_widget->selectionModel()) {
        cols = selectedColumns(false);
        const int curIdx = d_view_widget->selectionModel()->currentIndex().column();
        if (cols.isEmpty() && curIdx >= 0 && curIdx < d_table->columnCount()) {
            if (Column *cc = d_table->column(curIdx))
                cols.append(cc);
        }
        if (cols.isEmpty() && d_table->columnCount() > 0) {
            if (Column *c0 = d_table->column(0))
                cols.append(c0);
        }
    }

    Column *activeCol = nullptr;
    if (d_table && d_view_widget && d_view_widget->selectionModel()) {
        const int curIdx = d_view_widget->selectionModel()->currentIndex().column();
        if (curIdx >= 0 && curIdx < d_table->columnCount())
            activeCol = d_table->column(curIdx);
    }
    if (!activeCol && !cols.isEmpty())
        activeCol = cols.first();

    if (d_edit_quick_fx && !d_edit_quick_fx->hasFocus()) {
        QSignalBlocker sb(d_edit_quick_fx);
        QString fxStr;
        if (activeCol) {
            const QList<Interval<int>> fIntervals = activeCol->formulaIntervals();
            for (const Interval<int> &iv : fIntervals) {
                const QString f = activeCol->formula(iv.start()).trimmed();
                if (!f.isEmpty()) {
                    fxStr = f;
                    break;
                }
            }
            if (fxStr.isEmpty())
                fxStr = activeCol->formula(0).trimmed();
        }
        d_edit_quick_fx->setText(fxStr);
        d_edit_quick_fx->setEnabled(activeCol != nullptr);
    }

    auto columnRoleCode = [](Column *c) -> int {
        if (!c)
            return -1;
        const SciDAVis::PlotDesignation pd = c->plotDesignation();
        if (pd == SciDAVis::noDesignation) {
            const QVariant prop = c->property("_tg_label_role");
            if (prop.isValid())
                return prop.toBool() ? QuickRoleLabel : QuickRoleNone;
            if (c->columnMode() == SciDAVis::ColumnMode::Text)
                return QuickRoleLabel;
            return QuickRoleNone;
        }
        return int(pd);
    };

    int unifiedRole = -1;
    if (!cols.isEmpty()) {
        unifiedRole = columnRoleCode(cols.first());
        for (int i = 1; i < cols.size(); ++i) {
            if (columnRoleCode(cols.at(i)) != unifiedRole) {
                unifiedRole = -1;
                break;
            }
        }
    }

    auto setCheckedBlocked = [](QToolButton *b, bool on) {
        if (!b)
            return;
        QSignalBlocker sb(b);
        b->setChecked(on);
    };

    setCheckedBlocked(d_btn_role_x, unifiedRole == QuickRoleX);
    setCheckedBlocked(d_btn_role_y, unifiedRole == QuickRoleY);
    setCheckedBlocked(d_btn_role_yerr, unifiedRole == QuickRoleYErr);
    setCheckedBlocked(d_btn_role_xerr, unifiedRole == QuickRoleXErr);
    setCheckedBlocked(d_btn_role_z, unifiedRole == QuickRoleZ);
    setCheckedBlocked(d_btn_role_label, unifiedRole == QuickRoleLabel);
    setCheckedBlocked(d_btn_role_none, unifiedRole == QuickRoleNone);

    QToolButton *moreBtn = d_quick_bar->findChild<QToolButton *>(
            QStringLiteral("WorksheetMoreRoleBtn"));
    if (moreBtn) {
        const bool isSecondary = (unifiedRole == QuickRoleXErr || unifiedRole == QuickRoleZ
                                  || unifiedRole == QuickRoleLabel || unifiedRole == QuickRoleNone);
        setCheckedBlocked(moreBtn, isSecondary);

        const QString arrowSuffix = QString::fromUtf8(" \xE2\x96\xBE");
        QString moreText = tr("More") + arrowSuffix;
        if (unifiedRole == QuickRoleXErr)
            moreText = tr("\xC2\xB1xErr") + arrowSuffix;
        else if (unifiedRole == QuickRoleZ)
            moreText = QStringLiteral("Z") + arrowSuffix;
        else if (unifiedRole == QuickRoleLabel)
            moreText = tr("Label") + arrowSuffix;
        else if (unifiedRole == QuickRoleNone)
            moreText = tr("Disregard") + arrowSuffix;
        if (moreBtn->text() != moreText)
            moreBtn->setText(moreText);

        if (QMenu *m = moreBtn->menu()) {
            for (QAction *act : m->actions()) {
                QSignalBlocker sb(act);
                act->setChecked(act->data().toInt() == unifiedRole);
            }
        }
    }

    updateWorksheetQuickBarMetrics(d_quick_bar, d_btn_role_x, d_btn_role_y, d_btn_role_yerr,
                                   moreBtn, d_btn_quick_plot_line, d_btn_quick_plot_lp,
                                   d_btn_quick_plot_scatter, d_btn_quick_add_graph,
                                   d_btn_quick_col_props, d_lbl_quick_fx);
}

void TableView::applyQuickColumnRole(int roleCode)
{
    if (!d_table || d_table->columnCount() == 0)
        return;

    QList<int> colIndices = selectedColumnIndices(false);
    if (colIndices.isEmpty() && d_view_widget && d_view_widget->selectionModel()) {
        const int curIdx = d_view_widget->selectionModel()->currentIndex().column();
        if (curIdx >= 0 && curIdx < d_table->columnCount())
            colIndices.append(curIdx);
    }
    if (colIndices.isEmpty())
        colIndices.append(0);

    d_table->beginMacro(tr("%1: set column plot designation").arg(d_table->name()));
    for (int ci : colIndices) {
        if (ci < 0 || ci >= d_table->columnCount())
            continue;
        Column *c = d_table->column(ci);
        if (!c)
            continue;
        if (roleCode == QuickRoleLabel) {
            c->setProperty("_tg_label_role", true);
            if (c->columnMode() != SciDAVis::ColumnMode::Text)
                c->setColumnMode(SciDAVis::ColumnMode::Text);
            c->setPlotDesignation(SciDAVis::noDesignation);
        } else {
            c->setProperty("_tg_label_role", false);
            c->setPlotDesignation(static_cast<SciDAVis::PlotDesignation>(roleCode));
        }
    }
    d_table->endMacro();

    for (int ci : colIndices)
        autoExpandColumnWidthForHeader(ci, false);

    if (d_horizontal_header)
        d_horizontal_header->refresh();
    syncQuickBarState();
}

void TableView::applyQuickFormula()
{
    if (!d_table || !d_edit_quick_fx || d_table->columnCount() == 0)
        return;

    int targetCol = -1;
    if (d_view_widget && d_view_widget->selectionModel())
        targetCol = d_view_widget->selectionModel()->currentIndex().column();
    if (targetCol < 0 || targetCol >= d_table->columnCount()) {
        const QList<int> selCols = selectedColumnIndices(false);
        if (!selCols.isEmpty())
            targetCol = selCols.first();
    }
    if (targetCol < 0 || targetCol >= d_table->columnCount())
        targetCol = 0;

    const QString fx = d_edit_quick_fx->text().trimmed();
    if (ui.formula_box) {
        QSignalBlocker sb(ui.formula_box);
        ui.formula_box->setText(fx);
    }

    if (::Table *legacyTbl = qobject_cast<::Table *>(this)) {
        legacyTbl->setCommand(targetCol, fx);
        if (!fx.isEmpty())
            legacyTbl->recalculate(targetCol, false);
    } else if (Column *c = d_table->column(targetCol)) {
        if (fx.isEmpty())
            c->clearFormulas();
        else
            c->setFormula(Interval<int>(0, qMax(0, d_table->rowCount() - 1)), fx);
    }

    if (d_horizontal_header)
        d_horizontal_header->refresh();
    syncQuickBarState();
    if (d_view_widget)
        d_view_widget->setFocus();
}

void TableView::triggerQuickPlot(int curveStyle)
{
    ApplicationWindow *app = findOwningAppWindow(this);
    if (!app)
        return;

    for (QWidget *p = this; p != nullptr; p = p->parentWidget()) {
        if (QMdiSubWindow *sub = qobject_cast<QMdiSubWindow *>(p)) {
            if (QMdiArea *mdi = sub->mdiArea())
                mdi->setActiveSubWindow(sub);
            break;
        }
    }

    app->selectPlotType(curveStyle);
}

void TableView::triggerQuickAddToGraph()
{
    ApplicationWindow *app = findOwningAppWindow(this);
    if (!app)
        return;

    ::Table *legacyTbl = qobject_cast<::Table *>(this);
    if (!legacyTbl)
        return;

    app->addTableSelectionToActiveOrRecentGraph(legacyTbl);
}

/* ================== TableViewWidget ================ */

void TableViewWidget::selectAll()
{
    // the original QTableView::selectAll() toggles all cells which is strange behavior IMHO - thzs
    QItemSelectionModel *sel_model = selectionModel();
    QItemSelection sel(
            model()->index(0, 0, QModelIndex()),
            model()->index(model()->rowCount() - 1, model()->columnCount() - 1, QModelIndex()));
    sel_model->select(sel, QItemSelectionModel::Select);
}

void TableViewWidget::updateHeaderGeometry(Qt::Orientation o, int first, int last)
{
    Q_UNUSED(first)
    Q_UNUSED(last)
    if (o != Qt::Horizontal)
        return;
    horizontalHeader()->setStretchLastSection(
            true); // ugly hack (flaw in Qt? Does anyone know a better way?)
    horizontalHeader()->updateGeometry();
    horizontalHeader()->setStretchLastSection(false); // ugly hack part 2
}

void TableViewWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        emit advanceCell();
    QTableView::keyPressEvent(event);
}
