/***************************************************************************
    File                 : ModernRibbonAndHud.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : Hybrid Modern Contextual Single-Row Slim Ribbon,
                           Ctrl+K Command Palette, Split Workspace Controller,
                           and F11 Non-Redundant 2-Stage Focus Canvas HUD
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

#include "ModernRibbonAndHud.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QCursor>
#include <QDockWidget>
#include <QEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QIdentityProxyModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMetaObject>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "ApplicationWindow.h"
#include "CurveMathTools.h"
#include "Graph.h"
#include "GraphInspectorDock.h"
#include "IconLoader.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "MyWidget.h"
#include "Note.h"
#include "PlotCurve.h"
#include "RecalcEngine.h"
#include "Table.h"
#include "Graph3D.h"
#include "future/core/column/Column.h"
#include "future/table/TableModel.h"

namespace {

enum PaletteItemRoles {
    RoleEntryIndex = Qt::UserRole + 1,
    RoleShortcutText = Qt::UserRole + 2,
    RoleCategoryText = Qt::UserRole + 3
};

static QString cleanMnemonicText(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        if (raw.at(i) == QLatin1Char('&')) {
            if (i + 1 < raw.size() && raw.at(i + 1) == QLatin1Char('&')) {
                out.append(QLatin1Char('&'));
                ++i;
            }
            continue;
        }
        out.append(raw.at(i));
    }
    return out.trimmed();
}

static Table *sourceTableFromMultiLayer(MultiLayer *ml,
                                        const QSet<QObject *> &ignored = QSet<QObject *>())
{
    if (!ml)
        return nullptr;

    auto extractFromGraph = [&ignored](Graph *g) -> Table * {
        if (!g || g->curves() <= 0)
            return nullptr;
        const int selIdx = g->selectedCurveIndex();
        if (selIdx >= 0 && selIdx < g->curves()) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(selIdx))) {
                if (dc->table() && !ignored.contains(dc->table()))
                    return dc->table();
            }
        }
        for (int i = 0; i < g->curves(); ++i) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(i))) {
                if (dc->table() && !ignored.contains(dc->table()))
                    return dc->table();
            }
        }
        return nullptr;
    };

    if (Table *t = extractFromGraph(ml->activeGraph()))
        return t;

    const QList<Graph *> layers = ml->layersList();
    for (Graph *g : layers) {
        if (Table *t = extractFromGraph(g))
            return t;
    }
    return nullptr;
}

static bool multiLayerUsesTable(MultiLayer *ml, Table *targetTable)
{
    if (!ml || !targetTable)
        return false;
    const QList<Graph *> layers = ml->layersList();
    for (Graph *g : layers) {
        if (!g)
            continue;
        for (int i = 0; i < g->curves(); ++i) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(g->curve(i))) {
                if (dc->table() == targetTable)
                    return true;
            }
        }
    }
    return false;
}

class CommandPaletteItemDelegate : public QStyledItemDelegate
{
public:
    explicit CommandPaletteItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(480, 38);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect rowRect = option.rect.adjusted(4, 2, -4, -2);
        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered = (option.state & QStyle::State_MouseOver);

        if (selected) {
            painter->setPen(QPen(QColor(37, 99, 235, 90), 1.0));
            painter->setBrush(QColor(239, 246, 255));
            painter->drawRoundedRect(rowRect, 7.0, 7.0);
        } else if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(248, 250, 252));
            painter->drawRoundedRect(rowRect, 7.0, 7.0);
        }

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const int iconPx = 20;
        const QRect iconRect(rowRect.left() + 10,
                             rowRect.top() + (rowRect.height() - iconPx) / 2, iconPx, iconPx);
        if (!icon.isNull())
            icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        const QString shortcut = index.data(RoleShortcutText).toString();
        const QString category = index.data(RoleCategoryText).toString();
        const QString title = index.data(Qt::DisplayRole).toString();

        int rightEdge = rowRect.right() - 10;
        if (!shortcut.isEmpty()) {
            QFont badgeFont(QStringLiteral("Segoe UI"), 10);
            badgeFont.setWeight(QFont::DemiBold);
            painter->setFont(badgeFont);
            const QFontMetrics bfm(badgeFont);
            const int bw = bfm.horizontalAdvance(shortcut) + 14;
            const int bh = 24;
            const QRect badgeRect(rightEdge - bw,
                                  rowRect.top() + (rowRect.height() - bh) / 2, bw, bh);
            painter->setPen(QPen(QColor(203, 213, 225), 1.0));
            painter->setBrush(selected ? QColor(255, 255, 255) : QColor(241, 245, 249));
            painter->drawRoundedRect(badgeRect, 5.0, 5.0);
            painter->setPen(QColor(71, 85, 105));
            painter->drawText(badgeRect, Qt::AlignCenter, shortcut);
            rightEdge = badgeRect.left() - 8;
        }

        if (!category.isEmpty()) {
            QFont catFont(QStringLiteral("Segoe UI"), 10);
            painter->setFont(catFont);
            const QFontMetrics cfm(catFont);
            const int cw = qMin(148, cfm.horizontalAdvance(category) + 6);
            const QRect catRect(rightEdge - cw, rowRect.top(), cw, rowRect.height());
            painter->setPen(selected ? QColor(37, 99, 235) : QColor(100, 116, 139));
            painter->drawText(catRect, Qt::AlignRight | Qt::AlignVCenter,
                              cfm.elidedText(category, Qt::ElideRight, cw));
            rightEdge = catRect.left() - 8;
        }

        QFont titleFont(QStringLiteral("Segoe UI"), 11);
        titleFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(titleFont);
        painter->setPen(selected ? QColor(29, 78, 216) : QColor(15, 23, 42));
        const int textLeft = iconRect.right() + 10;
        const int availW = qMax(40, rightEdge - textLeft);
        const QFontMetrics tfm(titleFont);
        const QRect textRect(textLeft, rowRect.top(), availW, rowRect.height());
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          tfm.elidedText(title, Qt::ElideRight, availW));

        painter->restore();
    }
};

static QString escapeTabMnemonicText(const QString &raw)
{
    QString out;
    out.reserve(raw.size() + 4);
    for (int i = 0; i < raw.size(); ++i) {
        const QChar ch = raw.at(i);
        if (ch == QLatin1Char('&')) {
            if (i + 1 < raw.size() && raw.at(i + 1) == QLatin1Char('&')) {
                out.append(QStringLiteral("&&"));
                ++i;
            } else {
                out.append(QStringLiteral("&&"));
            }
        } else {
            out.append(ch);
        }
    }
    return out;
}

static QString unescapeTabMnemonicText(const QString &raw)
{
    QString out = raw;
    out.replace(QStringLiteral("&&"), QStringLiteral("&"));
    return out;
}

class RibbonHorizontalScrollArea : public QScrollArea
{
public:
    explicit RibbonHorizontalScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setFocusPolicy(Qt::NoFocus);

        const QString chevronStyle = QStringLiteral(
                "QToolButton {"
                "  background-color: rgba(248, 250, 252, 242);"
                "  color: #1E293B;"
                "  border: 1px solid #CBD5E1;"
                "  border-radius: 6px;"
                "  font-family: 'Segoe UI', sans-serif;"
                "  font-size: 9pt;"
                "  font-weight: 700;"
                "  padding: 0px;"
                "}"
                "QToolButton:hover {"
                "  background-color: #EFF6FF;"
                "  color: #1D4ED8;"
                "  border: 1px solid #60A5FA;"
                "}"
                "QToolButton:disabled {"
                "  color: #CBD5E1;"
                "  background-color: rgba(248, 250, 252, 180);"
                "}");

        d_btn_left = new QToolButton(this);
        d_btn_left->setText(QString::fromUtf8("\xE2\x80\xB9"));
        d_btn_left->setCursor(Qt::PointingHandCursor);
        d_btn_left->setFocusPolicy(Qt::NoFocus);
        d_btn_left->setAutoRepeat(true);
        d_btn_left->setStyleSheet(chevronStyle);
        d_btn_left->hide();

        d_btn_right = new QToolButton(this);
        d_btn_right->setText(QString::fromUtf8("\xE2\x80\xBA"));
        d_btn_right->setCursor(Qt::PointingHandCursor);
        d_btn_right->setFocusPolicy(Qt::NoFocus);
        d_btn_right->setAutoRepeat(true);
        d_btn_right->setStyleSheet(chevronStyle);
        d_btn_right->hide();

        connect(d_btn_left, &QToolButton::clicked, this, [this]() {
            if (QScrollBar *hbar = horizontalScrollBar())
                hbar->setValue(qMax(hbar->minimum(), hbar->value() - 180));
        });
        connect(d_btn_right, &QToolButton::clicked, this, [this]() {
            if (QScrollBar *hbar = horizontalScrollBar())
                hbar->setValue(qMin(hbar->maximum(), hbar->value() + 180));
        });

        if (QScrollBar *hbar = horizontalScrollBar()) {
            connect(hbar, &QScrollBar::rangeChanged, this,
                    [this](int, int) { updateScrollChevrons(); });
            connect(hbar, &QScrollBar::valueChanged, this,
                    [this](int) { updateScrollChevrons(); });
        }
    }

    void updateScrollChevrons()
    {
        if (d_updating_chevrons)
            return;
        QScrollBar *hbar = horizontalScrollBar();
        if (!hbar || !d_btn_left || !d_btn_right)
            return;

        d_updating_chevrons = true;
        const bool hasOverflow = hbar->maximum() > hbar->minimum();
        if (!hasOverflow) {
            d_btn_left->hide();
            d_btn_right->hide();
            d_updating_chevrons = false;
            return;
        }

        const int btnW = 18;
        const int availH = qMax(24, height() - (hbar->isVisible() ? hbar->height() : 6) - 6);
        const int btnY = 3;
        d_btn_left->setGeometry(2, btnY, btnW, availH);
        d_btn_right->setGeometry(qMax(2, width() - btnW - 2), btnY, btnW, availH);
        const bool canLeft = hbar->value() > hbar->minimum();
        const bool canRight = hbar->value() < hbar->maximum();
        d_btn_left->setEnabled(canLeft);
        d_btn_right->setEnabled(canRight);
        d_btn_left->setVisible(canLeft);
        d_btn_right->setVisible(canRight);
        if (canLeft)
            d_btn_left->raise();
        if (canRight)
            d_btn_right->raise();
        d_updating_chevrons = false;
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QScrollArea::resizeEvent(event);
        updateScrollChevrons();
    }

    void showEvent(QShowEvent *event) override
    {
        QScrollArea::showEvent(event);
        updateScrollChevrons();
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (QScrollBar *hbar = horizontalScrollBar()) {
            const int delta = (event->angleDelta().x() != 0) ? event->angleDelta().x()
                                                             : event->angleDelta().y();
            if (delta != 0 && hbar->maximum() > hbar->minimum()) {
                hbar->setValue(hbar->value() - (delta / 2));
                event->accept();
                return;
            }
        }
        QScrollArea::wheelEvent(event);
    }

private:
    QToolButton *d_btn_left = nullptr;
    QToolButton *d_btn_right = nullptr;
    bool d_updating_chevrons = false;
};

} // namespace

// ============================================================================
// CommandPalettePopup
// ============================================================================

CommandPalettePopup::CommandPalettePopup(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                         QWidget *parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
      d_app(app),
      d_ribbon(ribbon)
{
    setObjectName(QStringLiteral("ToriCommandPalettePopup"));
    setAttribute(Qt::WA_TranslucentBackground, false);
    setModal(false);
    setFixedSize(580, 410);

    setStyleSheet(QStringLiteral(
            "QDialog#ToriCommandPalettePopup {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "}"
            "QLineEdit#CommandPaletteSearchEdit {"
            "  background-color: #F8FAFC;"
            "  color: #0F172A;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 8px;"
            "  padding: 6px 10px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 12pt;"
            "}"
            "QLineEdit#CommandPaletteSearchEdit:focus {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #2563EB;"
            "}"
            "QListWidget#CommandPaletteList {"
            "  background-color: #FFFFFF;"
            "  border: none;"
            "  outline: none;"
            "}"
            "QLabel#CommandPaletteHintLabel {"
            "  color: #64748B;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10.5pt;"
            "  padding: 2px 4px;"
            "}"));

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 10);
    rootLayout->setSpacing(8);

    QHBoxLayout *searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(8);

    QLabel *searchIconLbl = new QLabel(this);
    searchIconLbl->setPixmap(
            IconLoader::duotonePixmap(QStringLiteral("search-command"), 20, 0.0, false));
    searchIconLbl->setFixedSize(22, 22);
    searchIconLbl->setAlignment(Qt::AlignCenter);
    searchRow->addWidget(searchIconLbl);

    d_search_edit = new QLineEdit(this);
    d_search_edit->setObjectName(QStringLiteral("CommandPaletteSearchEdit"));
    d_search_edit->setClearButtonEnabled(true);
    d_search_edit->installEventFilter(this);
    searchRow->addWidget(d_search_edit, 1);

    rootLayout->addLayout(searchRow);

    d_list_widget = new QListWidget(this);
    d_list_widget->setObjectName(QStringLiteral("CommandPaletteList"));
    d_list_widget->setItemDelegate(new CommandPaletteItemDelegate(d_list_widget));
    d_list_widget->setUniformItemSizes(true);
    d_list_widget->setMouseTracking(true);
    d_list_widget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d_list_widget->installEventFilter(this);
    rootLayout->addWidget(d_list_widget, 1);

    d_hint_label = new QLabel(this);
    d_hint_label->setObjectName(QStringLiteral("CommandPaletteHintLabel"));
    rootLayout->addWidget(d_hint_label);

    connect(d_search_edit, &QLineEdit::textChanged, this,
            &CommandPalettePopup::rebuildFilteredList);
    connect(d_list_widget, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item)
            executeItemAt(d_list_widget->row(item));
    });
    connect(d_list_widget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item)
            executeItemAt(d_list_widget->row(item));
    });

    retranslateUi();
}

CommandPalettePopup::~CommandPalettePopup() = default;

void CommandPalettePopup::retranslateUi()
{
    setWindowTitle(tr("Command Palette"));
    if (d_search_edit)
        d_search_edit->setPlaceholderText(tr("Type a command, plot, fit, or preset..."));
    if (d_hint_label)
        d_hint_label->setText(tr("Up/Down to navigate  ·  Enter to run  ·  Esc to close"));
    refreshCommands();
}

void CommandPalettePopup::refreshCommands()
{
    d_entries.clear();

    auto addQuickEntry = [this](const QString &id, const QString &title, const QString &category,
                                const QString &shortcut, const QString &keywords,
                                const QString &iconId, std::function<void()> fn, int boost = 80) {
        CommandEntry e;
        e.id = id;
        e.title = title;
        e.category = category;
        e.shortcutText = shortcut;
        e.searchKey = (title + QLatin1Char(' ') + category + QLatin1Char(' ') + shortcut
                       + QLatin1Char(' ') + keywords)
                              .toLower();
        e.iconId = iconId;
        e.callback = std::move(fn);
        e.priorityBoost = boost;
        d_entries.append(std::move(e));
    };

    const QString catQuick = tr("Quick Preset");
    const QString catWorkspace = tr("Workspace");
    const QString catTools = tr("Tools");

    addQuickEntry(
            QStringLiteral("quick.isij85"), tr("ISIJ 85mm Journal Preset"), catQuick, QString(),
            QStringLiteral("isij 85mm journal style preset 600dpi"),
            QStringLiteral("preset-isij"),
            [this]() {
                if (d_app)
                    d_app->applyStyleISIJJournal();
            },
            100);

    addQuickEntry(
            QStringLiteral("quick.slide28"), tr("Presentation Slide 28pt Preset"), catQuick,
            QString(), QStringLiteral("slide 28pt presentation powerpoint style preset"),
            QStringLiteral("preset-slide"),
            [this]() {
                if (d_app)
                    d_app->applyStylePresentationSlide();
            },
            98);

    addQuickEntry(
            QStringLiteral("quick.sample_gallery"),
            tr("Sample Project Gallery..."), catQuick, QString(),
            QString::fromUtf8("sample gallery demo template example dataset project "
                              "\xE3\x82\xB5\xE3\x83\xB3\xE3\x83\x97\xE3\x83\xAB "
                              "\xE3\x82\xAE\xE3\x83\xA3\xE3\x83\xA9\xE3\x83\xAA\xE3\x83\xBC "
                              "\xE4\xBE\x8B\xE9\xA1\x8C"),
            QStringLiteral("sample-gallery"),
            [this]() {
                if (d_app)
                    CurveMathTools::showSampleGalleryDialog(d_app);
            },
            99);

    addQuickEntry(
            QStringLiteral("quick.tensile_yield"),
            tr("Tensile / 0.2% Yield Analysis..."), catQuick, QString(),
            QString::fromUtf8("tensile stress strain 0.2% offset yield proof youngs modulus uts "
                              "\xE5\xBC\x95\xE5\xBC\xB5 \xE8\x80\x90\xE5\x8A\x9B "
                              "\xE5\xBF\x9C\xE5\x8A\x9B \xE3\x81\xB2\xE3\x81\x9A\xE3\x81\xBF "
                              "\xE3\x83\xA4\xE3\x83\xB3\xE3\x82\xB0\xE7\x8E\x87"),
            QStringLiteral("curvemath"),
            [this]() {
                if (d_app)
                    CurveMathTools::showTensileYieldDialog(d_app);
            },
            97);

    addQuickEntry(
            QStringLiteral("quick.kissinger_tds"),
            tr("Kissinger / Thermal Desorption (TDS)..."), catQuick, QString(),
            QString::fromUtf8("kissinger tds thermal desorption dsc activation energy heating rate "
                              "\xE6\x98\x87\xE6\xB8\xA9\xE8\x84\xB1\xE9\x9B\xA2 "
                              "\xE6\xB4\xBB\xE6\x80\xA7\xE5\x8C\x96\xE3\x82\xA8\xE3\x83\x8D\xE3\x83\xAB\xE3\x82\xAE\xE3\x83\xBC"),
            QStringLiteral("kissinger-tds"),
            [this]() {
                if (d_app)
                    CurveMathTools::showKissingerDialog(d_app);
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.work_hardening"),
            tr("Work Hardening / Considere Analysis..."), catQuick, QString(),
            QString::fromUtf8("work hardening considere Hollomon Crussard Jaoul Kocks Mecking true stress strain "
                              "\xE5\x8A\xA0\xE5\xB7\xA5\xE7\xA1\xAC\xE5\x8C\x96 "
                              "\xE7\x9C\x9F\xE5\xBF\x9C\xE5\x8A\x9B \xE7\x9C\x9F\xE3\x81\xB2\xE3\x81\x9A\xE3\x81\xBF"),
            QStringLiteral("work-hardening"),
            [this]() {
                if (d_app)
                    CurveMathTools::showWorkHardeningDialog(d_app);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.xrd_analyzer"),
            tr("XRD Peak & Williamson-Hall Analyzer..."), catQuick, QString(),
            QString::fromUtf8("xrd diffraction scherrer williamson hall crystallite microstrain ka2 "
                              "X\xE7\xB7\x9A\xE5\x9B\x9E\xE6\x8A\x98 \xE7\xB5\x90\xE6\x99\xB6\xE5\xAD\x90"),
            QStringLiteral("xrd-peaks"),
            [this]() {
                if (d_app)
                    CurveMathTools::showXRDAnalyzerDialog(d_app);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.vasp_plotter"),
            tr("VASP Electronic Band & DOS Plotter..."), catQuick, QString(),
            QString::fromUtf8("vasp band structure doscar eigenval fermi electronic density of states "
                              "\xE3\x83\x90\xE3\x83\xB3\xE3\x83\x89\xE6\xA7\x8B\xE9\x80\xA0 "
                              "\xE7\x8A\xB6\xE6\x85\x8B\xE5\xAF\x86\xE5\xBA\xA6 \xE9\x9B\xBB\xE5\xAD\x90\xE7\x8A\xB6\xE6\x85\x8B"),
            QStringLiteral("vasp-bands"),
            [this]() {
                if (d_app)
                    CurveMathTools::showVaspPlotterDialog(d_app);
            },
            93);

    addQuickEntry(
            QStringLiteral("quick.export_python"),
            tr("Export Active Graph to Python Script..."), catTools, QString(),
            QString::fromUtf8("export python script code reproduce matplotlib torigraph generate py "
                              "Python\xE5\x87\xBA\xE5\x8A\x9B \xE3\x82\xB9\xE3\x82\xAF\xE3\x83\xAA\xE3\x83\x97\xE3\x83\x88\xE7\x94\x9F\xE6\x88\x90"),
            QStringLiteral("code-export"),
            [this]() {
                if (d_app)
                    d_app->exportActiveGraphToPython();
            },
            91);

    addQuickEntry(
            QStringLiteral("quick.merge_graphs"), tr("Merge Graph Windows..."), catWorkspace,
            QString(),
            QStringLiteral("merge graph windows multi panel figure a b c d combine layout"),
            QStringLiteral("merge-graphs"),
            [this]() {
                if (d_app)
                    d_app->mergeOpenGraphsDialog();
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.split_view"), tr("Split View (Table + Graph)"), catWorkspace,
            QString(), QStringLiteral("split view side by side table graph workspace"),
            QStringLiteral("split-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.tabbed_view"), tr("Tabbed Workspace View"), catWorkspace,
            QString(), QStringLiteral("tabbed tabs workspace view"), QStringLiteral("tab-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
            },
            90);

    addQuickEntry(
            QStringLiteral("quick.free_mdi"), tr("Free MDI Workspace View"), catWorkspace,
            QString(), QStringLiteral("free mdi cascade subwindow workspace view"),
            QStringLiteral("mdi-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
            },
            86);

    addQuickEntry(
            QStringLiteral("quick.focus_hud"), tr("Toggle Focus HUD Mode (F11)"), catWorkspace,
            QStringLiteral("F11"), QStringLiteral("focus hud canvas fullscreen f11"),
            QStringLiteral("focus-hud"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->toggleFocusHudMode();
            },
            92);

    addQuickEntry(
            QStringLiteral("quick.graph_inspector"),
            tr("Toggle Graph Inspector Dock (Ctrl+Shift+I)"), catWorkspace,
            QStringLiteral("Ctrl+Shift+I"),
            QStringLiteral("graph inspector dock object manager property sidebar rail ctrl+shift+i"),
            QStringLiteral("table-drawer"),
            [this]() {
                if (d_app)
                    d_app->toggleGraphInspectorDock();
            },
            93);

    addQuickEntry(
            QStringLiteral("quick.python_console"), tr("Open Python Console"), catTools,
            QStringLiteral("F9"), QStringLiteral("python console repl script automation"),
            QStringLiteral("python-console"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->openPythonConsoleRequested();
            },
            88);

    addQuickEntry(
            QStringLiteral("quick.python_run"), tr("Run Python Script..."), catTools, QString(),
            QStringLiteral("run python script py file automation"), QStringLiteral("python-run"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->runPythonScriptRequested();
            },
            84);

    addQuickEntry(
            QStringLiteral("quick.check_updates"), tr("Check for Updates..."), catTools,
            QString(), QStringLiteral("check updates github release version upgrade"),
            QStringLiteral("update-check"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->checkForUpdatesRequested();
            },
            82);

    addQuickEntry(
            QStringLiteral("quick.switch_language"), tr("Switch Language (Japanese / English)"),
            catTools, QString(), QStringLiteral("language japanese english i18n locale ja en"),
            QStringLiteral("language-ja-en"),
            [this]() {
                if (!d_app)
                    return;
                if (d_app->appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive))
                    d_app->switchToEnglish();
                else
                    d_app->switchToJapanese();
            },
            85);

    if (d_app) {
        QSet<QString> seenTitles;
        for (const CommandEntry &ce : d_entries)
            seenTitles.insert(ce.title.toLower());

        const QList<QAction *> actions = d_app->findChildren<QAction *>();
        const QString catCommand = tr("Command");
        for (QAction *act : actions) {
            if (!act || act->isSeparator() || act->menu() != nullptr)
                continue;
            if (!act->isVisible() || !act->isEnabled())
                continue;

            const QString cleanTitle = cleanMnemonicText(act->text());
            if (cleanTitle.isEmpty() || cleanTitle.size() <= 1)
                continue;

            const QString lowerTitle = cleanTitle.toLower();
            if (seenTitles.contains(lowerTitle))
                continue;
            seenTitles.insert(lowerTitle);

            CommandEntry e;
            e.id = act->objectName();
            e.title = cleanTitle;
            e.category = catCommand;
            e.shortcutText = act->shortcut().toString(QKeySequence::NativeText);
            const QString tip = cleanMnemonicText(act->toolTip());
            e.searchKey = (cleanTitle + QLatin1Char(' ') + tip + QLatin1Char(' ')
                           + act->objectName() + QLatin1Char(' ') + e.shortcutText)
                                  .toLower();
            e.iconId = IconLoader::resolveIconId(
                    act->objectName().isEmpty() ? cleanTitle : act->objectName());
            e.action = act;
            e.priorityBoost = 0;
            d_entries.append(std::move(e));
        }
    }

    rebuildFilteredList(d_search_edit ? d_search_edit->text() : QString());
}

void CommandPalettePopup::setFilterText(const QString &query)
{
    if (!d_search_edit)
        return;
    if (d_search_edit->text() == query)
        rebuildFilteredList(query);
    else
        d_search_edit->setText(query);
}

QString CommandPalettePopup::filterText() const
{
    return d_search_edit ? d_search_edit->text() : QString();
}

int CommandPalettePopup::visibleCommandCount() const
{
    return d_list_widget ? d_list_widget->count() : 0;
}

int CommandPalettePopup::fuzzyMatchScore(const QString &needleLower, const CommandEntry &entry)
{
    if (needleLower.isEmpty())
        return 100 + entry.priorityBoost;

    const QString titleLower = entry.title.toLower();
    if (titleLower == needleLower)
        return 1200 + entry.priorityBoost;
    if (titleLower.startsWith(needleLower))
        return 900 + entry.priorityBoost;

    const int subIdx = titleLower.indexOf(needleLower);
    if (subIdx >= 0)
        return 700 - qMin(120, subIdx * 3) + entry.priorityBoost;

    const QStringList tokens = needleLower.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.size() > 1) {
        bool allTokensFound = true;
        int tokenBonus = 0;
        for (const QString &tok : tokens) {
            if (!entry.searchKey.contains(tok)) {
                allTokensFound = false;
                break;
            }
            if (titleLower.contains(tok))
                tokenBonus += 40;
        }
        if (allTokensFound)
            return 520 + tokenBonus + entry.priorityBoost;
    }

    if (entry.searchKey.contains(needleLower))
        return 420 + entry.priorityBoost;

    // Subsequence fuzzy match across titleLower
    int pos = 0;
    int streak = 0;
    int score = 200;
    for (int i = 0; i < needleLower.size(); ++i) {
        const QChar ch = needleLower.at(i);
        if (ch.isSpace())
            continue;
        const int found = titleLower.indexOf(ch, pos);
        if (found < 0)
            return -1;
        if (found == pos) {
            streak += 1;
            score += 18 + streak * 6;
        } else {
            streak = 0;
            score -= qMin(25, found - pos);
        }
        pos = found + 1;
    }
    return qMax(1, score + (entry.priorityBoost / 2));
}

void CommandPalettePopup::rebuildFilteredList(const QString &query)
{
    if (!d_list_widget)
        return;

    const QString needle = query.trimmed().toLower();
    struct ScoredIdx
    {
        int index;
        int score;
    };
    QVector<ScoredIdx> matches;
    matches.reserve(d_entries.size());

    for (int i = 0; i < d_entries.size(); ++i) {
        const int sc = fuzzyMatchScore(needle, d_entries.at(i));
        if (sc > 0)
            matches.append({ i, sc });
    }

    std::stable_sort(matches.begin(), matches.end(),
                     [](const ScoredIdx &a, const ScoredIdx &b) { return a.score > b.score; });

    d_list_widget->clear();
    d_visible_entry_indices.clear();

    const int maxItems = qMin(matches.size(), 80);
    d_visible_entry_indices.reserve(maxItems);
    for (int i = 0; i < maxItems; ++i) {
        const int entryIdx = matches.at(i).index;
        const CommandEntry &ce = d_entries.at(entryIdx);
        d_visible_entry_indices.append(entryIdx);

        QIcon itemIcon = (!ce.iconId.isEmpty())
                ? IconLoader::duotoneIcon(ce.iconId, 20, false)
                : (ce.action ? ce.action->icon() : QIcon());
        if (itemIcon.isNull())
            itemIcon = IconLoader::duotoneIcon(QStringLiteral("search-command"), 20, false);

        QListWidgetItem *item = new QListWidgetItem(itemIcon, ce.title, d_list_widget);
        item->setData(RoleEntryIndex, entryIdx);
        item->setData(RoleShortcutText, ce.shortcutText);
        item->setData(RoleCategoryText, ce.category);
    }

    if (d_list_widget->count() > 0)
        d_list_widget->setCurrentRow(0);
}

bool CommandPalettePopup::executeCurrentSelection()
{
    if (!d_list_widget || d_list_widget->count() <= 0)
        return false;
    const int row = qMax(0, d_list_widget->currentRow());
    return executeItemAt(row);
}

bool CommandPalettePopup::executeItemAt(int row)
{
    if (row < 0 || row >= d_visible_entry_indices.size())
        return false;

    const int entryIdx = d_visible_entry_indices.at(row);
    if (entryIdx < 0 || entryIdx >= d_entries.size())
        return false;

    const QPointer<QAction> act = d_entries.at(entryIdx).action;
    const std::function<void()> cb = d_entries.at(entryIdx).callback;

    hide();

    if (cb) {
        cb();
        return true;
    }
    if (act && act->isEnabled()) {
        act->trigger();
        return true;
    }
    return false;
}

bool CommandPalettePopup::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (watched == d_search_edit || watched == d_list_widget) {
            if (ke->key() == Qt::Key_Down) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int next = qMin(d_list_widget->count() - 1,
                                          d_list_widget->currentRow() + 1);
                    d_list_widget->setCurrentRow(next);
                }
                return true;
            }
            if (ke->key() == Qt::Key_Up) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int prev = qMax(0, d_list_widget->currentRow() - 1);
                    d_list_widget->setCurrentRow(prev);
                }
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                executeCurrentSelection();
                return true;
            }
            if (ke->key() == Qt::Key_Escape) {
                hide();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPalettePopup::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void CommandPalettePopup::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (d_search_edit) {
        d_search_edit->setFocus(Qt::PopupFocusReason);
        d_search_edit->selectAll();
    }
}

// ============================================================================
// FocusTableDrawer
// ============================================================================

namespace {

static QString formatDrawerColumnHeader(Table *tbl, int c, QString *outFormula = nullptr)
{
    if (!tbl || c < 0 || c >= tbl->numCols())
        return QString();

    QString roleTag;
    switch (tbl->colPlotDesignation(c)) {
    case SciDAVis::X:
        roleTag = QStringLiteral("[X]");
        break;
    case SciDAVis::Y:
        roleTag = QStringLiteral("[Y]");
        break;
    case SciDAVis::Z:
        roleTag = QStringLiteral("[Z]");
        break;
    case SciDAVis::xErr:
        roleTag = QStringLiteral("[dX]");
        break;
    case SciDAVis::yErr:
        roleTag = QStringLiteral("[dY]");
        break;
    default:
        break;
    }

    Column *colObj = tbl->column(c);
    QString colFormula;
    if (colObj && !colObj->formulaIntervals().isEmpty()) {
        const Interval<int> iv = colObj->formulaIntervals().first();
        colFormula = colObj->formula(iv.start()).trimmed();
    }
    if (outFormula)
        *outFormula = colFormula;

    const QString longName = tbl->colLongName(c).trimmed();
    const QString units = tbl->colUnits(c).trimmed();
    QString h = longName.isEmpty() ? tbl->colLabel(c) : longName;
    if (!roleTag.isEmpty())
        h += QLatin1Char(' ') + roleTag;
    if (!colFormula.isEmpty())
        h += QLatin1Char(' ') + FocusTableDrawer::tr("[fx]");
    if (!units.isEmpty())
        h += QStringLiteral("\n(%1)").arg(units);
    return h;
}

class FocusDrawerProxyModel : public QIdentityProxyModel
{
public:
    explicit FocusDrawerProxyModel(FocusTableDrawer *drawer)
        : QIdentityProxyModel(drawer), d_drawer(drawer)
    {
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid() || !d_drawer)
            return Qt::NoItemFlags;
        Table *tbl = d_drawer->currentTable();
        if (!tbl)
            return Qt::NoItemFlags;
        Column *colObj = tbl->column(index.column());
        const bool isFormula =
                (colObj && index.row() < tbl->numRows()
                 && !colObj->formula(index.row()).trimmed().isEmpty());
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (!isFormula)
            f |= Qt::ItemIsEditable;
        return f;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || !d_drawer)
            return QVariant();
        Table *tbl = d_drawer->currentTable();
        if (!tbl || index.row() < 0 || index.row() >= tbl->numRows() || index.column() < 0
            || index.column() >= tbl->numCols()) {
            return QVariant();
        }

        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return tbl->text(index.row(), index.column());

        if (role == Qt::BackgroundRole || role == Qt::ForegroundRole || role == Qt::ToolTipRole) {
            Column *colObj = tbl->column(index.column());
            const QString cellFormula =
                    colObj ? colObj->formula(index.row()).trimmed() : QString();
            if (!cellFormula.isEmpty()) {
                if (role == Qt::BackgroundRole)
                    return QColor(241, 245, 249);
                if (role == Qt::ForegroundRole)
                    return QColor(30, 64, 175);
                if (role == Qt::ToolTipRole)
                    return FocusTableDrawer::tr("Formula (read-only): %1").arg(cellFormula);
            }
            return QVariant();
        }

        return QIdentityProxyModel::data(index, role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (!d_drawer)
            return QVariant();
        Table *tbl = d_drawer->currentTable();
        if (!tbl)
            return QVariant();

        if (orientation == Qt::Horizontal) {
            if (section < 0 || section >= tbl->numCols())
                return QVariant();
            if (role == Qt::DisplayRole)
                return formatDrawerColumnHeader(tbl, section);
            if (role == Qt::ToolTipRole) {
                QString colFormula;
                formatDrawerColumnHeader(tbl, section, &colFormula);
                if (!colFormula.isEmpty())
                    return FocusTableDrawer::tr("Formula (read-only): %1").arg(colFormula);
                return QVariant();
            }
            return QIdentityProxyModel::headerData(section, orientation, role);
        }

        if (orientation == Qt::Vertical && role == Qt::DisplayRole)
            return QString::number(section + 1);

        return QIdentityProxyModel::headerData(section, orientation, role);
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if (role != Qt::EditRole || !index.isValid() || !d_drawer)
            return false;
        if (!d_drawer->setCellValueFromDrawer(index.row(), index.column(), value.toString()))
            return false;
        emit dataChanged(index, index);
        return true;
    }

    void notifyAllHeadersChanged()
    {
        const int cols = columnCount();
        if (cols > 0)
            emit headerDataChanged(Qt::Horizontal, 0, cols - 1);
    }

private:
    FocusTableDrawer *d_drawer = nullptr;
};

} // namespace

FocusTableDrawer::FocusTableDrawer(ApplicationWindow *app, QWidget *parent)
    : QFrame(parent), d_app(app)
{
    setObjectName(QStringLiteral("FocusTableDrawerCard"));
    setFixedWidth(360);
    setStyleSheet(QStringLiteral(
            "QFrame#FocusTableDrawerCard {"
            "  background-color: rgba(255, 255, 255, 247);"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "}"
            "QLabel#FocusDrawerTitle {"
            "  color: #0F172A;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11.5pt;"
            "  font-weight: 700;"
            "}"
            "QLabel#FocusDrawerEmpty {"
            "  color: #64748B;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  padding: 12px;"
            "}"
            "QToolButton.FocusDrawerBtn {"
            "  background-color: #F8FAFC;"
            "  color: #1E293B;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 6px;"
            "  padding: 3px 6px;"
            "  font-size: 10.8pt;"
            "}"
            "QToolButton.FocusDrawerBtn:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QTableView#FocusDrawerGrid, QTableWidget#FocusDrawerCompatGrid {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 8px;"
            "  gridline-color: #E2E8F0;"
            "  selection-background-color: #DBEAFE;"
            "  selection-color: #0F172A;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "}"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    QHBoxLayout *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);

    QLabel *iconLbl = new QLabel(this);
    iconLbl->setPixmap(IconLoader::duotonePixmap(QStringLiteral("table-drawer"), 18, 0.0, false));
    iconLbl->setFixedSize(20, 20);
    headerRow->addWidget(iconLbl);

    d_title_label = new QLabel(this);
    d_title_label->setObjectName(QStringLiteral("FocusDrawerTitle"));
    headerRow->addWidget(d_title_label, 1);

    d_btn_new_table = new QToolButton(this);
    d_btn_new_table->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_new_table->setIcon(IconLoader::duotoneIcon(QStringLiteral("new-table"), 16, false));
    d_btn_new_table->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_new_table);

    d_btn_add_col = new QToolButton(this);
    d_btn_add_col->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_add_col->setIcon(IconLoader::duotoneIcon(QStringLiteral("add-column"), 16, false));
    d_btn_add_col->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_add_col);

    d_btn_close = new QToolButton(this);
    d_btn_close->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_close->setIcon(IconLoader::duotoneIcon(QStringLiteral("clear"), 16, false));
    d_btn_close->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_close);

    mainLayout->addLayout(headerRow);

    d_table_combo = new QComboBox(this);
    d_table_combo->setMinimumHeight(28);
    mainLayout->addWidget(d_table_combo);

    d_empty_label = new QLabel(this);
    d_empty_label->setObjectName(QStringLiteral("FocusDrawerEmpty"));
    d_empty_label->setAlignment(Qt::AlignCenter);
    d_empty_label->setWordWrap(true);
    mainLayout->addWidget(d_empty_label);

    d_proxy_model = new FocusDrawerProxyModel(this);
    d_table_view = new QTableView(this);
    d_table_view->setObjectName(QStringLiteral("FocusDrawerGrid"));
    d_table_view->setModel(d_proxy_model);
    d_table_view->setAlternatingRowColors(true);
    d_table_view->horizontalHeader()->setDefaultSectionSize(92);
    d_table_view->horizontalHeader()->setMinimumSectionSize(54);
    d_table_view->verticalHeader()->setDefaultSectionSize(24);
    d_table_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    mainLayout->addWidget(d_table_view, 1);

    connect(d_btn_new_table, &QToolButton::clicked, this, [this]() {
        if (d_shutting_down || !d_app)
            return;
        Table *t = d_app->newTable();
        refreshTableList(false);
        if (t)
            setCurrentTable(t);
    });

    connect(d_btn_add_col, &QToolButton::clicked, this, [this]() {
        if (d_shutting_down || !d_current_table)
            return;
        d_current_table->addCol(SciDAVis::Y);
        syncGridFromTable();
    });

    connect(d_btn_close, &QToolButton::clicked, this, &FocusTableDrawer::closeDrawerRequested);

    connect(d_table_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                if (d_shutting_down || d_syncing || !d_app || idx < 0)
                    return;
                const QString tblName = d_table_combo->itemData(idx).toString();
                Table *target = d_app->table(tblName);
                if (target && !d_dying_tables.contains(target))
                    setCurrentTable(target);
            });

    if (d_app) {
        connect(&d_app->folders, &QObject::destroyed, this,
                &FocusTableDrawer::prepareForAppShutdown);
    }

    retranslateUi();
}

FocusTableDrawer::~FocusTableDrawer()
{
    prepareForAppShutdown();
}

void FocusTableDrawer::prepareForAppShutdown()
{
    d_shutting_down = true;
    d_syncing = true;

    if (d_proxy_model)
        d_proxy_model->setSourceModel(nullptr);

    if (d_current_table) {
        disconnect(d_current_table.data(), nullptr, this, nullptr);
        d_current_table.clear();
    }
    d_dying_tables.clear();

    if (d_compat_grid)
        disconnect(d_compat_grid, nullptr, this, nullptr);
    if (d_table_combo)
        disconnect(d_table_combo, nullptr, this, nullptr);
    if (d_btn_new_table)
        disconnect(d_btn_new_table, nullptr, this, nullptr);
    if (d_btn_add_col)
        disconnect(d_btn_add_col, nullptr, this, nullptr);
    if (d_btn_close)
        disconnect(d_btn_close, nullptr, this, nullptr);

    d_app.clear();
}

Table *FocusTableDrawer::currentTable() const
{
    return d_current_table.data();
}

QTableWidget *FocusTableDrawer::tableWidget() const
{
    syncCompatGrid();
    return d_compat_grid;
}

void FocusTableDrawer::retranslateUi()
{
    if (d_shutting_down)
        return;
    if (d_title_label)
        d_title_label->setText(tr("Linked Worksheet"));
    if (d_btn_new_table)
        d_btn_new_table->setToolTip(tr("New Table"));
    if (d_btn_add_col)
        d_btn_add_col->setToolTip(tr("Add Column"));
    if (d_btn_close)
        d_btn_close->setToolTip(tr("Close Worksheet Drawer"));
    if (d_empty_label)
        d_empty_label->setText(tr("No worksheet available in the current project."));
    d_compat_grid_dirty = true;
    syncGridFromTable();
}

Table *FocusTableDrawer::detectPreferredTable(const QSet<QObject *> &ignoredObjects) const
{
    if (d_shutting_down || !d_app)
        return nullptr;

    QSet<QObject *> effectiveIgnored = ignoredObjects;
    for (MyWidget *mw : d_app->hiddenWindows) {
        if (mw)
            effectiveIgnored.insert(mw);
    }

    auto isUsableTable = [this, &effectiveIgnored](Table *tbl) -> bool {
        if (!tbl || effectiveIgnored.contains(tbl))
            return false;
        if (tbl->status() == MyWidget::Hidden)
            return false;
        if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
            return false;
        if (Folder *curFolder = d_app->currentFolder()) {
            if (!curFolder->windowsList().contains(tbl))
                return false;
        }
        return true;
    };

    QMdiSubWindow *activeSub = d_app->d_workspace.activeSubWindow();
    if (activeSub && !effectiveIgnored.contains(activeSub)) {
        if (Table *activeTbl = qobject_cast<Table *>(activeSub)) {
            if (isUsableTable(activeTbl))
                return activeTbl;
        }
        if (MultiLayer *activeMl = qobject_cast<MultiLayer *>(activeSub)) {
            if (Table *srcTbl = sourceTableFromMultiLayer(activeMl, effectiveIgnored)) {
                if (isUsableTable(srcTbl))
                    return srcTbl;
            }
        }
    }

    const QList<QMdiSubWindow *> ordered =
            d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
    for (int i = ordered.size() - 1; i >= 0; --i) {
        QMdiSubWindow *sw = ordered.at(i);
        if (!sw || effectiveIgnored.contains(sw))
            continue;
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(sw)) {
            if (Folder *curFolder = d_app->currentFolder()) {
                if (!curFolder->windowsList().contains(ml))
                    continue;
            }
            if (Table *srcTbl = sourceTableFromMultiLayer(ml, effectiveIgnored)) {
                if (isUsableTable(srcTbl))
                    return srcTbl;
            }
        }
    }
    for (int i = ordered.size() - 1; i >= 0; --i) {
        QMdiSubWindow *sw = ordered.at(i);
        if (!sw || effectiveIgnored.contains(sw))
            continue;
        if (Table *tbl = qobject_cast<Table *>(sw)) {
            if (isUsableTable(tbl))
                return tbl;
        }
    }
    return nullptr;
}

void FocusTableDrawer::refreshTableList(bool autoSelectActiveCurveTable)
{
    if (d_shutting_down)
        return;
    refreshTableListInternal(autoSelectActiveCurveTable, d_dying_tables);
}

void FocusTableDrawer::refreshTableListInternal(bool autoSelectActiveCurveTable,
                                                const QSet<QObject *> &ignoredObjects)
{
    if (d_shutting_down || !d_table_combo)
        return;

    QVector<Table *> liveTables;
    if (d_app) {
        auto appendIfUserTable = [this, &ignoredObjects, &liveTables](MyWidget *mw) {
            if (!mw || ignoredObjects.contains(mw))
                return;
            if (mw->status() == MyWidget::Hidden || d_app->hiddenWindows.contains(mw))
                return;
            if (Table *tbl = qobject_cast<Table *>(mw)) {
                if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
                    return;
                if (!liveTables.contains(tbl))
                    liveTables.append(tbl);
            }
        };

        if (Folder *curFolder = d_app->currentFolder()) {
            const QList<MyWidget *> folderWins = curFolder->windowsList();
            for (MyWidget *mw : folderWins)
                appendIfUserTable(mw);
        }
        const QList<QMdiSubWindow *> subs = d_app->d_workspace.subWindowList();
        for (QMdiSubWindow *sw : subs)
            appendIfUserTable(qobject_cast<MyWidget *>(sw));
        for (MyWidget *mw : d_app->outWindows)
            appendIfUserTable(mw);
    }

    Table *desired = nullptr;
    if (autoSelectActiveCurveTable)
        desired = detectPreferredTable(ignoredObjects);
    if (!desired && d_current_table && !ignoredObjects.contains(d_current_table.data())
        && liveTables.contains(d_current_table.data())) {
        desired = d_current_table.data();
    }
    if (!desired && !liveTables.isEmpty())
        desired = liveTables.first();

    {
        QSignalBlocker blocker(d_table_combo);
        d_table_combo->clear();
        int selectIdx = -1;
        for (int i = 0; i < liveTables.size(); ++i) {
            Table *t = liveTables.at(i);
            const QString lbl = t->windowLabel().trimmed();
            const QString disp = lbl.isEmpty()
                    ? t->name()
                    : QStringLiteral("%1 — %2").arg(t->name(), lbl);
            d_table_combo->addItem(
                    IconLoader::duotoneIcon(QStringLiteral("new-table"), 16, false), disp,
                    t->name());
            if (t == desired)
                selectIdx = i;
        }
        if (selectIdx >= 0)
            d_table_combo->setCurrentIndex(selectIdx);
    }

    setCurrentTable(desired);
}

void FocusTableDrawer::bindTableSignals(Table *table)
{
    if (d_shutting_down) {
        if (d_current_table) {
            disconnect(d_current_table.data(), nullptr, this, nullptr);
            d_current_table.clear();
        }
        return;
    }

    if (d_current_table == table)
        return;

    if (d_current_table)
        disconnect(d_current_table.data(), nullptr, this, nullptr);

    d_current_table = table;

    if (d_current_table) {
        connect(d_current_table.data(), &QObject::destroyed, this,
                &FocusTableDrawer::onCurrentTableDestroyed, Qt::UniqueConnection);
        connect(d_current_table.data(), &Table::modifiedData, this,
                [this](Table *, const QString &) {
                    if (!d_shutting_down && !d_syncing)
                        syncGridFromTable();
                });
        connect(d_current_table.data(), &Table::changedColHeader, this,
                [this](const QString &, const QString &) {
                    if (!d_shutting_down && !d_syncing)
                        syncGridFromTable();
                });
        connect(d_current_table.data(), &Table::removedCol, this, [this](const QString &) {
            if (!d_shutting_down && !d_syncing)
                syncGridFromTable();
        });
    }
}

void FocusTableDrawer::setCurrentTable(Table *table)
{
    if (d_shutting_down)
        return;

    if (table && d_dying_tables.contains(table))
        table = nullptr;

    bindTableSignals(table);

    if (d_table_combo && d_current_table) {
        const int idx = d_table_combo->findData(d_current_table->name());
        if (idx >= 0 && d_table_combo->currentIndex() != idx) {
            QSignalBlocker blocker(d_table_combo);
            d_table_combo->setCurrentIndex(idx);
        }
    }

    if (d_btn_add_col)
        d_btn_add_col->setEnabled(!d_current_table.isNull());

    syncGridFromTable();
}

void FocusTableDrawer::onCurrentTableDestroyed(QObject *destroyedObj)
{
    if (d_shutting_down || !d_app) {
        d_current_table.clear();
        return;
    }
    if (destroyedObj)
        d_dying_tables.insert(destroyedObj);
    d_current_table.clear();
    refreshTableListInternal(true, d_dying_tables);
    if (destroyedObj)
        d_dying_tables.remove(destroyedObj);
}

void FocusTableDrawer::syncGridFromTable()
{
    if (d_shutting_down || !d_table_view || !d_proxy_model || !d_empty_label)
        return;

    d_compat_grid_dirty = true;

    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl)) {
        d_proxy_model->setSourceModel(nullptr);
        d_table_view->hide();
        d_empty_label->show();
        if (d_compat_grid) {
            QSignalBlocker blocker(d_compat_grid);
            d_compat_grid->clear();
            d_compat_grid->setRowCount(0);
            d_compat_grid->setColumnCount(0);
            d_compat_grid_dirty = false;
        }
        return;
    }

    d_empty_label->hide();
    d_table_view->show();

    if (isHidden()) {
        // Defer binding while drawer is closed so zero signals/layout work run when hidden
        d_dirty = true;
        if (d_proxy_model->sourceModel() != nullptr)
            d_proxy_model->setSourceModel(nullptr);
        return;
    }

    d_dirty = false;
    TableModel *srcModel = tbl->tableModel();
    if (d_proxy_model->sourceModel() != srcModel)
        d_proxy_model->setSourceModel(srcModel);
    if (auto *fp = static_cast<FocusDrawerProxyModel *>(d_proxy_model))
        fp->notifyAllHeadersChanged();
    if (d_table_view->viewport())
        d_table_view->viewport()->update();

    if (d_compat_grid && tbl->numRows() <= 200)
        syncCompatGrid();
}

void FocusTableDrawer::syncCompatGrid() const
{
    if (d_shutting_down)
        return;

    FocusTableDrawer *self = const_cast<FocusTableDrawer *>(this);
    if (!d_compat_grid) {
        d_compat_grid = new QTableWidget(self);
        d_compat_grid->setObjectName(QStringLiteral("FocusDrawerCompatGrid"));
        d_compat_grid->hide();
        connect(d_compat_grid, &QTableWidget::cellChanged, self,
                &FocusTableDrawer::onGridCellChanged);
        d_compat_grid_dirty = true;
    }

    if (!d_compat_grid_dirty)
        return;

    d_compat_grid_dirty = false;
    QSignalBlocker blocker(d_compat_grid);

    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl)) {
        d_compat_grid->clear();
        d_compat_grid->setRowCount(0);
        d_compat_grid->setColumnCount(0);
        return;
    }

    const int rows = qBound(0, tbl->numRows(), 200);
    const int cols = qMax(0, tbl->numCols());
    d_compat_grid->setRowCount(rows);
    d_compat_grid->setColumnCount(cols);

    for (int c = 0; c < cols; ++c) {
        QString colFormula;
        const QString h = formatDrawerColumnHeader(tbl, c, &colFormula);
        QTableWidgetItem *hdrItem = new QTableWidgetItem(h);
        if (!colFormula.isEmpty())
            hdrItem->setToolTip(tr("Formula (read-only): %1").arg(colFormula));
        d_compat_grid->setHorizontalHeaderItem(c, hdrItem);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Column *colObj = tbl->column(c);
            const QString cellFormula = colObj ? colObj->formula(r).trimmed() : QString();
            const bool isFormulaCell = !cellFormula.isEmpty();
            const QString val = tbl->text(r, c);
            QTableWidgetItem *it = d_compat_grid->item(r, c);
            if (!it) {
                it = new QTableWidgetItem(val);
                d_compat_grid->setItem(r, c, it);
            } else if (it->text() != val) {
                it->setText(val);
            }

            if (isFormulaCell) {
                it->setFlags((it->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable)
                             & ~Qt::ItemIsEditable);
                it->setBackground(QColor(241, 245, 249));
                it->setForeground(QColor(30, 64, 175));
                it->setToolTip(tr("Formula (read-only): %1").arg(cellFormula));
            } else {
                it->setFlags(it->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable
                             | Qt::ItemIsEditable);
                it->setData(Qt::BackgroundRole, QVariant());
                it->setData(Qt::ForegroundRole, QVariant());
                it->setToolTip(QString());
            }
        }
    }
}

void FocusTableDrawer::onGridCellChanged(int row, int col)
{
    if (d_shutting_down || d_syncing || !d_current_table || !d_compat_grid)
        return;
    QTableWidgetItem *it = d_compat_grid->item(row, col);
    if (!it || !(it->flags() & Qt::ItemIsEditable))
        return;
    setCellValueFromDrawer(row, col, it->text());
}

bool FocusTableDrawer::setCellValueFromDrawer(int row, int col, const QString &text)
{
    if (d_shutting_down)
        return false;
    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl))
        return false;
    if (row < 0 || col < 0 || col >= tbl->numCols())
        return false;
    if (Column *colObj = tbl->column(col)) {
        if (row < tbl->numRows() && !colObj->formula(row).trimmed().isEmpty())
            return false;
        if (row >= tbl->numRows() && !colObj->formulaIntervals().isEmpty()) {
            const Interval<int> lastIv = colObj->formulaIntervals().last();
            if (tbl->numRows() > 0 && lastIv.end() >= tbl->numRows() - 1
                && !colObj->formula(lastIv.start()).trimmed().isEmpty()) {
                return false;
            }
        }
    }
    if (row >= tbl->numRows())
        tbl->setNumRows(row + 1);

    d_syncing = true;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        tbl->clearCell(row, col);
    } else if (tbl->columnType(col) == SciDAVis::ColumnMode::Numeric) {
        bool ok = false;
        double numericVal = QLocale().toDouble(trimmed, &ok);
        if (!ok)
            numericVal = trimmed.toDouble(&ok);
        if (ok) {
            tbl->setCell(row, col, numericVal);
        } else {
            Column *colObj = tbl->column(col);
            const QString lower = trimmed.toLower();
            const bool isNullSentinel =
                    (trimmed == QLatin1String("-") || trimmed == QLatin1String("--")
                     || lower == QLatin1String("nan") || lower == QLatin1String("na")
                     || lower == QLatin1String("n/a") || lower == QLatin1String("null"));
            bool hasAnyFinite = false;
            if (colObj && !isNullSentinel && colObj->formulaIntervals().isEmpty()) {
                const int nRows = colObj->rowCount();
                for (int r = 0; r < nRows; ++r) {
                    if (!colObj->isInvalid(r) && std::isfinite(colObj->valueAt(r))) {
                        hasAnyFinite = true;
                        break;
                    }
                }
                if (!hasAnyFinite) {
                    colObj->setColumnMode(SciDAVis::ColumnMode::Text);
                    colObj->setTextAt(row, trimmed);
                } else {
                    tbl->setText(row, col, trimmed);
                }
            } else {
                tbl->setText(row, col, trimmed);
            }
        }
    } else {
        tbl->setText(row, col, text);
    }
    d_syncing = false;

    d_compat_grid_dirty = true;
    syncGridFromTable();
    return true;
}

void FocusTableDrawer::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void FocusTableDrawer::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    if (!d_shutting_down && (d_dirty || !d_proxy_model || !d_proxy_model->sourceModel())) {
        syncGridFromTable();
    }
}

void FocusTableDrawer::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    if (d_proxy_model && d_proxy_model->sourceModel() != nullptr) {
        d_proxy_model->setSourceModel(nullptr);
        d_dirty = true;
    }
}

// ============================================================================
// FocusCanvasHudOverlay
// ============================================================================

FocusCanvasHudOverlay::FocusCanvasHudOverlay(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                             QWidget *viewportParent)
    : QFrame(viewportParent), d_app(app), d_ribbon(ribbon), d_viewport(viewportParent)
{
    setObjectName(QStringLiteral("FocusCanvasDarkPill"));
    setStyleSheet(QStringLiteral(
            "QFrame#FocusCanvasDarkPill {"
            "  background-color: rgba(15, 23, 42, 235);"
            "  border: 1px solid rgba(148, 163, 184, 95);"
            "  border-radius: 14px;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton {"
            "  background-color: transparent;"
            "  color: #F8FAFC;"
            "  border: 1px solid transparent;"
            "  border-radius: 9px;"
            "  padding: 4px 8px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton:hover {"
            "  background-color: rgba(59, 130, 246, 75);"
            "  border: 1px solid rgba(96, 165, 250, 140);"
            "  color: #FFFFFF;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"));

    QHBoxLayout *pillLayout = new QHBoxLayout(this);
    pillLayout->setContentsMargins(8, 4, 8, 4);
    pillLayout->setSpacing(3);

    auto makeDarkBtn = [this, pillLayout](const QString &iconId,
                                          std::function<void()> onClick) -> QToolButton * {
        QToolButton *btn = new QToolButton(this);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setIcon(IconLoader::duotoneIcon(iconId, 18, true));
        btn->setIconSize(QSize(18, 18));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        if (onClick)
            connect(btn, &QToolButton::clicked, this, std::move(onClick));
        pillLayout->addWidget(btn);
        return btn;
    };

    auto addPillSep = [this, pillLayout]() {
        QFrame *sep = new QFrame(this);
        sep->setFixedSize(1, 18);
        sep->setStyleSheet(QStringLiteral("background-color: rgba(148, 163, 184, 75);"));
        pillLayout->addWidget(sep);
    };

    d_btn_plot = makeDarkBtn(QStringLiteral("plot-line-symbol"), nullptr);
    d_btn_plot->setPopupMode(QToolButton::InstantPopup);
    d_plot_menu = new QMenu(d_btn_plot);
    connect(d_plot_menu, &QMenu::aboutToShow, this, &FocusCanvasHudOverlay::populateQuickPlotMenu);
    d_btn_plot->setMenu(d_plot_menu);

    d_btn_peaks = makeDarkBtn(QStringLiteral("peak-analyzer"), [this]() {
        if (!d_shutting_down && d_app)
            d_app->showPeakAnalyzerDialog(0);
    });
    d_btn_roi = makeDarkBtn(QStringLiteral("gadget-roi"), [this]() {
        if (!d_shutting_down && d_app)
            d_app->attachRegionStatsGadget();
    });
    d_btn_profile2d = makeDarkBtn(QStringLiteral("gadget-profile-2d"), [this]() {
        if (!d_shutting_down && d_app)
            d_app->attachImageProfileGadget();
    });

    addPillSep();

    d_btn_isij = makeDarkBtn(QStringLiteral("preset-isij"), [this]() {
        if (!d_shutting_down && d_app)
            d_app->applyStyleISIJJournal();
    });
    d_btn_slide = makeDarkBtn(QStringLiteral("preset-slide"), [this]() {
        if (!d_shutting_down && d_app)
            d_app->applyStylePresentationSlide();
    });
    d_btn_ole = makeDarkBtn(QStringLiteral("copy-ole"), [this]() {
        if (!d_shutting_down && d_app)
            d_app->copyGraphAsOleObject();
    });

    addPillSep();

    d_btn_palette = makeDarkBtn(QStringLiteral("search-command"), [this]() {
        if (!d_shutting_down && d_ribbon)
            d_ribbon->openCommandPalette();
    });

    const QString edgePillStyle = QStringLiteral(
            "QToolButton {"
            "  background-color: rgba(15, 23, 42, 230);"
            "  color: #F8FAFC;"
            "  border: 1px solid rgba(148, 163, 184, 95);"
            "  border-radius: 12px;"
            "  padding: 5px 11px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton:hover {"
            "  background-color: rgba(37, 99, 235, 225);"
            "  border: 1px solid #60A5FA;"
            "  color: #FFFFFF;"
            "}"
            "QToolButton:checked {"
            "  background-color: #2563EB;"
            "  border: 1px solid #93C5FD;"
            "  color: #FFFFFF;"
            "}");

    d_btn_worksheet = new QToolButton(viewportParent);
    d_btn_worksheet->setObjectName(QStringLiteral("FocusHudWorksheetButton"));
    d_btn_worksheet->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_worksheet->setIcon(IconLoader::duotoneIcon(QStringLiteral("table-drawer"), 18, true));
    d_btn_worksheet->setIconSize(QSize(18, 18));
    d_btn_worksheet->setCheckable(true);
    d_btn_worksheet->setCursor(Qt::PointingHandCursor);
    d_btn_worksheet->setFocusPolicy(Qt::NoFocus);
    d_btn_worksheet->setStyleSheet(edgePillStyle);

    d_btn_exit_focus = new QToolButton(viewportParent);
    d_btn_exit_focus->setObjectName(QStringLiteral("FocusHudExitButton"));
    d_btn_exit_focus->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_exit_focus->setIcon(IconLoader::duotoneIcon(QStringLiteral("exit-focus-hud"), 18, true));
    d_btn_exit_focus->setIconSize(QSize(18, 18));
    d_btn_exit_focus->setCursor(Qt::PointingHandCursor);
    d_btn_exit_focus->setFocusPolicy(Qt::NoFocus);
    d_btn_exit_focus->setStyleSheet(edgePillStyle);

    d_table_drawer = new FocusTableDrawer(app, viewportParent);

    connect(d_btn_worksheet.data(), &QToolButton::toggled, this,
            &FocusCanvasHudOverlay::setTableDrawerVisible);
    connect(d_table_drawer.data(), &FocusTableDrawer::closeDrawerRequested, this,
            [this]() { setTableDrawerVisible(false); });
    connect(d_btn_exit_focus.data(), &QToolButton::clicked, this, [this]() {
        if (!d_shutting_down && d_ribbon)
            d_ribbon->setFocusHudMode(false);
    });

    if (d_viewport)
        d_viewport->installEventFilter(this);

    hide();
    d_btn_worksheet->hide();
    d_btn_exit_focus->hide();
    d_table_drawer->hide();

    retranslateUi();
}

FocusCanvasHudOverlay::~FocusCanvasHudOverlay()
{
    prepareForAppShutdown();
    if (d_btn_worksheet)
        delete d_btn_worksheet.data();
    if (d_btn_exit_focus)
        delete d_btn_exit_focus.data();
    if (d_table_drawer)
        delete d_table_drawer.data();
}

void FocusCanvasHudOverlay::prepareForAppShutdown()
{
    d_shutting_down = true;
    d_active = false;

    if (d_viewport) {
        d_viewport->removeEventFilter(this);
        d_viewport.clear();
    }
    if (d_table_drawer) {
        disconnect(d_table_drawer.data(), nullptr, this, nullptr);
        d_table_drawer->prepareForAppShutdown();
    }
    if (d_btn_worksheet)
        disconnect(d_btn_worksheet.data(), nullptr, this, nullptr);
    if (d_btn_exit_focus)
        disconnect(d_btn_exit_focus.data(), nullptr, this, nullptr);
    if (d_plot_menu)
        disconnect(d_plot_menu, nullptr, this, nullptr);

    d_app.clear();
    d_ribbon.clear();
}

FocusTableDrawer *FocusCanvasHudOverlay::tableDrawer() const
{
    return d_table_drawer.data();
}

QToolButton *FocusCanvasHudOverlay::worksheetToggleButton() const
{
    return d_btn_worksheet.data();
}

QToolButton *FocusCanvasHudOverlay::exitFocusButton() const
{
    return d_btn_exit_focus.data();
}

void FocusCanvasHudOverlay::populateQuickPlotMenu()
{
    if (d_shutting_down || !d_plot_menu)
        return;
    d_plot_menu->clear();
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-line"), 18, false),
                           tr("Line"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Line);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-scatter"), 18, false),
                           tr("Scatter"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Scatter);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 18, false),
                           tr("Line + Symbol"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::LineSymbols);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-bars"), 18, false),
                           tr("Bar Chart"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::VerticalBars);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-box"), 18, false),
                           tr("Box Plot"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Box);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-violin"), 18, false),
                           tr("Violin Plot"), this, [this]() {
                               if (!d_shutting_down && d_app)
                                   d_app->plotViolin();
                           });
    d_plot_menu->addSeparator();
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-heatmap"), 18, false),
                           tr("Heatmap"), this, [this]() {
                               if (!d_shutting_down && d_app)
                                   d_app->plotColorMap();
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-contour"), 18, false),
                           tr("Contour"), this, [this]() {
                               if (!d_shutting_down && d_app)
                                   d_app->plotContour();
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-3d-surface"), 18, false),
                           tr("3D Surface"), this, [this]() {
                               if (!d_shutting_down && d_app)
                                   d_app->plot3DWireSurface();
                           });
}

void FocusCanvasHudOverlay::retranslateUi()
{
    if (d_shutting_down)
        return;
    if (d_btn_plot) {
        d_btn_plot->setText(tr("Plot"));
        d_btn_plot->setToolTip(tr("Quick Plot from Active Worksheet"));
    }
    if (d_btn_peaks) {
        d_btn_peaks->setText(tr("Peak Analyzer"));
        d_btn_peaks->setToolTip(tr("Open Peak Analyzer & Deconvolution"));
    }
    if (d_btn_roi) {
        d_btn_roi->setText(tr("ROI Gadget"));
        d_btn_roi->setToolTip(tr("Attach Interactive ROI Statistics Gadget"));
    }
    if (d_btn_profile2d) {
        d_btn_profile2d->setText(tr("2D Profile"));
        d_btn_profile2d->setToolTip(tr("Attach Interactive 2D Image/Matrix Profile Gadget"));
    }
    if (d_btn_isij) {
        d_btn_isij->setText(tr("ISIJ 85mm Preset"));
        d_btn_isij->setToolTip(tr("Apply ISIJ 85mm Journal Style Preset (600 DPI, Arial)"));
    }
    if (d_btn_slide) {
        d_btn_slide->setText(tr("Slide 28pt Preset"));
        d_btn_slide->setToolTip(tr("Apply Presentation Slide 28pt Style Preset (Arial)"));
    }
    if (d_btn_ole) {
        d_btn_ole->setText(tr("Copy OLE"));
        d_btn_ole->setToolTip(tr("Copy Editable OLE Graph Object for PowerPoint"));
    }
    if (d_btn_palette) {
        d_btn_palette->setText(tr("Search (Ctrl+K)"));
        d_btn_palette->setToolTip(tr("Open Command Palette (Ctrl+K)"));
    }
    if (d_btn_worksheet) {
        d_btn_worksheet->setText(tr("Worksheet"));
        d_btn_worksheet->setToolTip(tr("Toggle Slide-Over Linked Worksheet Drawer"));
    }
    if (d_btn_exit_focus) {
        d_btn_exit_focus->setText(tr("Exit Focus (F11)"));
        d_btn_exit_focus->setToolTip(tr("Exit Focus HUD Mode and Restore Ribbon (F11)"));
    }
    if (d_table_drawer)
        d_table_drawer->retranslateUi();

    populateQuickPlotMenu();
    if (d_active)
        repositionOverlayWidgets();
}

void FocusCanvasHudOverlay::setHostViewport(QWidget *host)
{
    if (d_shutting_down || !host || d_viewport == host)
        return;

    if (d_viewport)
        d_viewport->removeEventFilter(this);

    d_viewport = host;
    d_viewport->installEventFilter(this);

    auto reparentKeepVis = [host](QWidget *w) {
        if (!w || w->parentWidget() == host)
            return;
        const bool wasVis = !w->isHidden();
        w->setParent(host);
        w->setVisible(wasVis);
    };

    reparentKeepVis(this);
    reparentKeepVis(d_btn_worksheet.data());
    reparentKeepVis(d_btn_exit_focus.data());
    reparentKeepVis(d_table_drawer.data());

    if (d_active)
        repositionOverlayWidgets();
}

void FocusCanvasHudOverlay::setOverlayActive(bool active)
{
    if (d_shutting_down) {
        d_active = false;
        return;
    }
    d_active = active;
    if (!d_active) {
        hide();
        if (d_btn_worksheet) {
            QSignalBlocker blocker(d_btn_worksheet.data());
            d_btn_worksheet->setChecked(false);
            d_btn_worksheet->hide();
        }
        if (d_btn_exit_focus)
            d_btn_exit_focus->hide();
        if (d_table_drawer)
            d_table_drawer->hide();
        if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
            && !d_ribbon->isFocusHudMode()) {
            d_ribbon->applySplitWorkspaceGeometry();
        }
        return;
    }

    if (d_app) {
        QWidget *preferredHost = d_app->centralWidget() ? d_app->centralWidget()
                                                        : d_app->d_workspace.viewport();
        if (preferredHost)
            setHostViewport(preferredHost);
    }

    if (d_table_drawer && isTableDrawerVisible())
        d_table_drawer->refreshTableList(true);

    show();
    if (d_btn_worksheet)
        d_btn_worksheet->show();
    if (d_btn_exit_focus)
        d_btn_exit_focus->show();

    repositionOverlayWidgets();
    if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph)
        d_ribbon->applySplitWorkspaceGeometry();
}

void FocusCanvasHudOverlay::setTableDrawerVisible(bool visible)
{
    if (d_shutting_down)
        return;

    auto findVisibleSplitTable = [this]() -> Table * {
        if (!d_app || !d_ribbon
            || d_ribbon->workspaceLayoutMode() != ModernRibbonBar::LayoutSplitTableGraph) {
            return nullptr;
        }
        const QList<QMdiSubWindow *> subs =
                d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = subs.size() - 1; i >= 0; --i) {
            if (Table *tbl = qobject_cast<Table *>(subs.at(i))) {
                if (!tbl->isHidden() && tbl->status() != MyWidget::Hidden
                    && !d_app->hiddenWindows.contains(tbl)
                    && !d_app->outWindows.contains(tbl)
                    && !tbl->name().startsWith(QLatin1String("RpcFitCurve"))) {
                    return tbl;
                }
            }
        }
        return nullptr;
    };

    if (visible && d_active) {
        if (Table *splitTbl = findVisibleSplitTable()) {
            // Left Split worksheet is already visible — focus/activate it directly
            // instead of floating a duplicate FocusTableDrawer card on top of it.
            if (d_btn_worksheet && d_btn_worksheet->isChecked()) {
                QSignalBlocker blocker(d_btn_worksheet.data());
                d_btn_worksheet->setChecked(false);
            }
            if (d_table_drawer) {
                d_table_drawer->hide();
                d_table_drawer->setCurrentTable(splitTbl);
            }
            d_app->d_workspace.setActiveSubWindow(splitTbl);
            splitTbl->setFocus();
            repositionOverlayWidgets();
            return;
        }
    }

    if (d_btn_worksheet && d_btn_worksheet->isChecked() != visible) {
        QSignalBlocker blocker(d_btn_worksheet.data());
        d_btn_worksheet->setChecked(visible);
    }
    if (!d_table_drawer)
        return;

    if (visible && d_active) {
        d_table_drawer->refreshTableList(true);
        d_table_drawer->show();
        repositionOverlayWidgets();
    } else {
        d_table_drawer->hide();
    }
}

bool FocusCanvasHudOverlay::isTableDrawerVisible() const
{
    return d_table_drawer && !d_table_drawer->isHidden();
}

void FocusCanvasHudOverlay::refreshActiveContext()
{
    if (d_shutting_down)
        return;
    if (d_active && isTableDrawerVisible() && d_table_drawer)
        d_table_drawer->refreshTableList(true);
}

void FocusCanvasHudOverlay::repositionOverlayWidgets()
{
    if (d_shutting_down || !d_active)
        return;

    const QSize vpSz = (d_viewport && d_viewport->width() >= 160 && d_viewport->height() >= 100)
            ? d_viewport->size()
            : (d_app ? d_app->size() : QSize(1024, 720));

    int topY = 10;
    if (d_app && d_ribbon && d_viewport == d_app->centralWidget()) {
        if (d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutTabbed) {
            if (QTabBar *mdiTb = d_app->d_workspace.findChild<QTabBar *>()) {
                if (!mdiTb->isHidden() && mdiTb->height() > 0 && mdiTb->height() < 64)
                    topY = mdiTb->height() + 8;
            }
        } else if (d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
                   || d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutFreeMdi) {
            topY = 4;
        }
    }

    int wsW = 100;
    if (d_btn_worksheet) {
        d_btn_worksheet->adjustSize();
        const QSize wsSz = d_btn_worksheet->sizeHint();
        wsW = qMax(100, wsSz.width());
        d_btn_worksheet->setGeometry(12, topY, wsW, qMax(32, wsSz.height()));
        d_btn_worksheet->raise();
    }

    int ew = 110;
    if (d_btn_exit_focus) {
        d_btn_exit_focus->adjustSize();
        const QSize exSz = d_btn_exit_focus->sizeHint();
        ew = qMax(110, exSz.width());
        const int eh = qMax(32, exSz.height());
        d_btn_exit_focus->setGeometry(qMax(12, vpSz.width() - ew - 12), topY, ew, eh);
        d_btn_exit_focus->raise();
    }

    const int leftLimit = 12 + wsW + 10;
    const int rightLimit = qMax(leftLimit + 160, vpSz.width() - 12 - ew - 10);
    const int availW = qMax(160, rightLimit - leftLimit);

    QToolButton *pillBtns[] = { d_btn_plot, d_btn_peaks, d_btn_roi,   d_btn_profile2d,
                                d_btn_isij, d_btn_slide, d_btn_ole,   d_btn_palette };

    auto setPillButtonStyle = [&pillBtns, this](Qt::ToolButtonStyle st) {
        bool changed = false;
        for (QToolButton *pb : pillBtns) {
            if (pb && pb->toolButtonStyle() != st) {
                pb->setToolButtonStyle(st);
                changed = true;
            }
        }
        if (changed && layout())
            layout()->activate();
    };

    if (vpSz.width() <= 1080) {
        setPillButtonStyle(Qt::ToolButtonIconOnly);
    } else {
        setPillButtonStyle(Qt::ToolButtonTextBesideIcon);
        adjustSize();
        if (sizeHint().width() > availW)
            setPillButtonStyle(Qt::ToolButtonIconOnly);
    }

    adjustSize();
    const QSize pillSz = sizeHint();
    const int pw = qMin(pillSz.width(), availW);
    const int ph = qMax(36, pillSz.height());
    const int centeredPx = (vpSz.width() - pw) / 2;
    const int px = qBound(leftLimit, centeredPx, qMax(leftLimit, rightLimit - pw));
    setGeometry(px, topY, pw, ph);
    raise();

    if (d_table_drawer && !d_table_drawer->isHidden()) {
        if (d_app && d_ribbon
            && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
            bool hasVisibleSplitTbl = false;
            for (QMdiSubWindow *sw : d_app->d_workspace.subWindowList()) {
                if (Table *tbl = qobject_cast<Table *>(sw)) {
                    if (!tbl->isHidden() && tbl->status() != MyWidget::Hidden
                        && !d_app->hiddenWindows.contains(tbl)
                        && !d_app->outWindows.contains(tbl)
                        && !tbl->name().startsWith(QLatin1String("RpcFitCurve"))) {
                        hasVisibleSplitTbl = true;
                        break;
                    }
                }
            }
            if (hasVisibleSplitTbl) {
                d_table_drawer->hide();
                if (d_btn_worksheet && d_btn_worksheet->isChecked()) {
                    QSignalBlocker blocker(d_btn_worksheet.data());
                    d_btn_worksheet->setChecked(false);
                }
                return;
            }
        }
        const int drawerTop = topY + ph + 10;
        const int drawerH = qMax(200, vpSz.height() - drawerTop - 14);
        d_table_drawer->setGeometry(12, drawerTop, 360, drawerH);
        d_table_drawer->raise();
    }
}

bool FocusCanvasHudOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (d_shutting_down)
        return QFrame::eventFilter(watched, event);
    if (d_active && watched == d_viewport && event) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show
            || event->type() == QEvent::ChildAdded) {
            repositionOverlayWidgets();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void FocusCanvasHudOverlay::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

// ============================================================================
// ModernRibbonBar
// ============================================================================

ModernRibbonBar::ModernRibbonBar(ApplicationWindow *app, QWidget *parent)
    : QFrame(parent), d_app(app)
{
    setObjectName(QStringLiteral("ModernRibbonBarRoot"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(120);
    setStyleSheet(QStringLiteral(
            "QFrame#ModernRibbonBarRoot {"
            "  background-color: #F8FAFC;"
            "  border-bottom: 1px solid #CBD5E1;"
            "}"
            "QWidget#ModernRibbonTopStrip {"
            "  background-color: #F1F5F9;"
            "  border-bottom: 1px solid #E2E8F0;"
            "}"
            "QTabBar#ModernRibbonTabBar {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QTabBar#ModernRibbonTabBar::tab {"
            "  background: transparent;"
            "  color: #475569;"
            "  border: 1px solid transparent;"
            "  border-top-left-radius: 7px;"
            "  border-top-right-radius: 7px;"
            "  padding: 5px 11px;"
            "  margin-right: 1px;"
            "  margin-top: 3px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QTabBar#ModernRibbonTabBar::tab:hover {"
            "  background-color: #E2E8F0;"
            "  color: #0F172A;"
            "}"
            "QTabBar#ModernRibbonTabBar::tab:selected {"
            "  background-color: #FFFFFF;"
            "  color: #2563EB;"
            "  border: 1px solid #CBD5E1;"
            "  border-bottom: 1px solid #FFFFFF;"
            "}"
            "QStackedWidget#ModernRibbonTabStack {"
            "  background-color: #FFFFFF;"
            "}"
            "#ModernRibbonTabStack QScrollBar:horizontal {"
            "  height: 6px;"
            "  margin: 0px;"
            "  background: #F1F5F9;"
            "  border: none;"
            "}"
            "#ModernRibbonTabStack QScrollBar::handle:horizontal {"
            "  background: #CBD5E1;"
            "  border-radius: 3px;"
            "  min-width: 24px;"
            "  margin: 0px;"
            "}"
            "#ModernRibbonTabStack QScrollBar::handle:horizontal:hover {"
            "  background: #94A3B8;"
            "}"
            "#ModernRibbonTabStack QScrollBar::add-line:horizontal,"
            "#ModernRibbonTabStack QScrollBar::sub-line:horizontal,"
            "#ModernRibbonTabStack QScrollBar::add-page:horizontal,"
            "#ModernRibbonTabStack QScrollBar::sub-page:horizontal {"
            "  width: 0px;"
            "  height: 0px;"
            "  background: none;"
            "}"
            "QToolButton.RibbonActionBtn {"
            "  background-color: transparent;"
            "  color: #1E293B;"
            "  border: 1px solid transparent;"
            "  border-radius: 6px;"
            "  padding: 3px 6px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10.5pt;"
            "  font-weight: 500;"
            "  min-height: 30px;"
            "}"
            "QToolButton.RibbonActionBtn::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"
            "QToolButton.RibbonActionBtn:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #BFDBFE;"
            "}"
            "QToolButton.RibbonActionBtn:checked {"
            "  background-color: #DBEAFE;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "  font-weight: 600;"
            "}"
            "QToolButton.RibbonActionBtn:disabled {"
            "  color: #94A3B8;"
            "}"
            "QToolButton.RibbonPresetBtn {"
            "  background-color: #FFF7ED;"
            "  color: #9A3412;"
            "  border: 1px solid #FDBA74;"
            "  border-radius: 7px;"
            "  padding: 3px 8px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10.5pt;"
            "  font-weight: 700;"
            "  min-height: 30px;"
            "}"
            "QToolButton.RibbonPresetBtn::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"
            "QToolButton.RibbonPresetBtn:hover {"
            "  background-color: #FFEDD5;"
            "  color: #7C2D12;"
            "  border: 1px solid #EA580C;"
            "}"
            "QMenu.RibbonDropdownMenu {"
            "  background-color: #FFFFFF;"
            "  color: #0F172A;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 8px;"
            "  padding: 4px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10.2pt;"
            "}"
            "QMenu.RibbonDropdownMenu::item {"
            "  padding: 6px 22px 6px 10px;"
            "  border-radius: 5px;"
            "}"
            "QMenu.RibbonDropdownMenu::item:selected {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "}"
            "QMenu.RibbonDropdownMenu::separator {"
            "  height: 1px;"
            "  background: #E2E8F0;"
            "  margin: 4px 8px;"
            "}"
            "QToolButton#RibbonCommandSearchPill {"
            "  background-color: #F8FAFC;"
            "  color: #64748B;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 13px;"
            "  padding: 2px 12px;"
            "  min-height: 24px;"
            "  text-align: left;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 500;"
            "}"
            "QToolButton#RibbonCommandSearchPill:hover {"
            "  background-color: #FFFFFF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QFrame#RibbonLayoutSegmentTrack {"
            "  background-color: #F1F5F9;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 7px;"
            "}"
            "QToolButton.RibbonSegmentBtn {"
            "  background-color: transparent;"
            "  color: #475569;"
            "  border: 1px solid transparent;"
            "  border-radius: 5px;"
            "  padding: 2px 7px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton.RibbonSegmentBtn:hover {"
            "  background-color: rgba(255, 255, 255, 180);"
            "  color: #0F172A;"
            "}"
            "QToolButton.RibbonSegmentBtn:checked {"
            "  background-color: #FFFFFF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #CBD5E1;"
            "}"
            "QToolButton.RibbonHeaderPill {"
            "  background-color: #FFFFFF;"
            "  color: #334155;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 6px;"
            "  padding: 2px 8px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton.RibbonHeaderPill:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "}"
            "QToolButton.RibbonHeaderPill:checked {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "}"
            "QLabel#RibbonContextBadge {"
            "  background-color: #DBEAFE;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "  border-radius: 10px;"
            "  padding: 2px 10px;"
            "  font-family: 'Segoe UI', 'Yu Gothic UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10.5pt;"
            "  font-weight: 700;"
            "}"));

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Top Header Row: 7 Contextual Tabs + Command Search Pill + Segmented Workspace Mode + Focus + Language + Context Badge
    QWidget *topStrip = new QWidget(this);
    topStrip->setObjectName(QStringLiteral("ModernRibbonTopStrip"));
    QHBoxLayout *topLayout = new QHBoxLayout(topStrip);
    topLayout->setContentsMargins(6, 2, 6, 0);
    topLayout->setSpacing(6);

    d_tab_bar = new QTabBar(topStrip);
    d_tab_bar->setObjectName(QStringLiteral("ModernRibbonTabBar"));
    d_tab_bar->setFont(QFont(QStringLiteral("Yu Gothic UI"), 11, QFont::DemiBold));
    d_tab_bar->setExpanding(false);
    d_tab_bar->setDrawBase(false);
    d_tab_bar->setUsesScrollButtons(true);
    d_tab_bar->setMinimumWidth(80);
    d_tab_bar->setFocusPolicy(Qt::NoFocus);
    topLayout->addWidget(d_tab_bar, 1);

    d_btn_cmd_palette = new QToolButton(topStrip);
    d_btn_cmd_palette->setObjectName(QStringLiteral("RibbonCommandSearchPill"));
    d_btn_cmd_palette->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_cmd_palette->setIcon(IconLoader::duotoneIcon(QStringLiteral("search-command"), 16, false));
    d_btn_cmd_palette->setIconSize(QSize(14, 14));
    d_btn_cmd_palette->setMinimumWidth(180);
    d_btn_cmd_palette->setMaximumWidth(210);
    d_btn_cmd_palette->setCursor(Qt::PointingHandCursor);
    d_btn_cmd_palette->setFocusPolicy(Qt::NoFocus);
    connect(d_btn_cmd_palette, &QToolButton::clicked, this, &ModernRibbonBar::openCommandPalette);
    topLayout->addWidget(d_btn_cmd_palette);

    d_layout_btn_group = new QButtonGroup(this);
    d_layout_btn_group->setExclusive(true);

    d_layout_segment_frame = new QFrame(topStrip);
    d_layout_segment_frame->setObjectName(QStringLiteral("RibbonLayoutSegmentTrack"));
    QHBoxLayout *segLayout = new QHBoxLayout(d_layout_segment_frame);
    segLayout->setContentsMargins(2, 2, 2, 2);
    segLayout->setSpacing(2);

    auto makeSegmentBtn = [this](const QString &iconId) -> QToolButton * {
        QToolButton *b = new QToolButton(d_layout_segment_frame);
        b->setProperty("class", QStringLiteral("RibbonSegmentBtn"));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setIcon(IconLoader::duotoneIcon(iconId, 16, false));
        b->setIconSize(QSize(14, 14));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    d_btn_layout_tabbed = makeSegmentBtn(QStringLiteral("tab-view"));
    d_btn_layout_split = makeSegmentBtn(QStringLiteral("split-view"));
    d_btn_layout_mdi = makeSegmentBtn(QStringLiteral("mdi-view"));

    d_layout_btn_group->addButton(d_btn_layout_tabbed, int(LayoutTabbed));
    d_layout_btn_group->addButton(d_btn_layout_split, int(LayoutSplitTableGraph));
    d_layout_btn_group->addButton(d_btn_layout_mdi, int(LayoutFreeMdi));
    d_btn_layout_tabbed->setChecked(true);

    segLayout->addWidget(d_btn_layout_tabbed);
    segLayout->addWidget(d_btn_layout_split);
    segLayout->addWidget(d_btn_layout_mdi);
    topLayout->addWidget(d_layout_segment_frame);

    auto makeHeaderPill = [topStrip](const QString &iconId, bool checkable) -> QToolButton * {
        QToolButton *b = new QToolButton(topStrip);
        b->setProperty("class", QStringLiteral("RibbonHeaderPill"));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setIcon(IconLoader::duotoneIcon(iconId, 16, false));
        b->setIconSize(QSize(14, 14));
        b->setCheckable(checkable);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    connect(d_layout_btn_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            [this](int modeId) { setWorkspaceLayoutMode(WorkspaceLayoutMode(modeId)); });

    d_btn_focus_mode = makeHeaderPill(QStringLiteral("focus-hud"), false);
    connect(d_btn_focus_mode, &QToolButton::clicked, this,
            [this]() { setFocusHudMode(true); });
    topLayout->addWidget(d_btn_focus_mode);

    d_btn_lang_switch = makeHeaderPill(QStringLiteral("language-ja-en"), false);
    connect(d_btn_lang_switch, &QToolButton::clicked, this, [this]() {
        if (!d_app)
            return;
        if (d_app->appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive))
            d_app->switchToEnglish();
        else
            d_app->switchToJapanese();
    });
    topLayout->addWidget(d_btn_lang_switch);

    d_context_pill = new QLabel(topStrip);
    d_context_pill->setObjectName(QStringLiteral("RibbonContextBadge"));
    d_context_pill->setAlignment(Qt::AlignCenter);
    d_context_pill->setMinimumWidth(0);
    d_context_pill->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    topLayout->addWidget(d_context_pill);

    rootLayout->addWidget(topStrip);

    // Single-Row Slim Contextual Action Strip
    d_tab_stack = new QStackedWidget(this);
    d_tab_stack->setObjectName(QStringLiteral("ModernRibbonTabStack"));
    rootLayout->addWidget(d_tab_stack);

    connect(d_tab_bar, &QTabBar::currentChanged, this, [this](int idx) {
        if (d_tab_stack)
            d_tab_stack->setCurrentIndex(idx);
        updateTabStackHeight();
    });

    buildAllTabs();
    updateTabStackHeight();

    if (d_app) {
        connect(&d_app->folders, &QObject::destroyed, this,
                [this]() { prepareForAppShutdown(); });

        if (QWidget *vp = d_app->d_workspace.viewport())
            vp->installEventFilter(this);
        QWidget *hudHost = d_app->centralWidget() ? d_app->centralWidget()
                                                  : d_app->d_workspace.viewport();
        if (hudHost)
            d_focus_hud_overlay = new FocusCanvasHudOverlay(d_app, this, hudHost);
        d_layout_mode = d_app->isTabbedWorkspaceView() ? LayoutTabbed : LayoutFreeMdi;
        if (QAbstractButton *b = d_layout_btn_group->button(int(d_layout_mode)))
            b->setChecked(true);

        connect(&d_app->d_workspace, &QMdiArea::subWindowActivated, this,
                [this](QMdiSubWindow *sw) {
                    if (d_shutting_down)
                        return;
                    MyWidget *mw = qobject_cast<MyWidget *>(sw);
                    updateActiveContext(mw);
                    if (d_layout_mode == LayoutTabbed && mw && !mw->isHidden()
                        && mw->status() != MyWidget::Hidden
                        && !d_app->hiddenWindows.contains(mw)
                        && !d_app->outWindows.contains(mw) && !mw->isMaximized()) {
                        mw->showMaximized();
                    }
                    if (d_layout_mode == LayoutSplitTableGraph && !d_applying_split_geometry)
                        applySplitWorkspaceGeometry();
                });
    }

    this->installEventFilter(this);

    retranslateUi();
    updateActiveContext(d_app ? qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow())
                              : nullptr);
}

ModernRibbonBar::~ModernRibbonBar()
{
    prepareForAppShutdown();
}

QSize ModernRibbonBar::minimumSizeHint() const
{
    return QSize(120, sizeHint().height());
}

void ModernRibbonBar::updateTabStackHeight()
{
    if (d_shutting_down || !d_tab_stack)
        return;

    const int fmH = fontMetrics().height();
    const int btnH = qMax(34, fmH + 14);

    int maxContentH = 0;
    for (int i = 0; i < d_tab_stack->count(); ++i) {
        if (QScrollArea *sa = qobject_cast<QScrollArea *>(d_tab_stack->widget(i))) {
            if (QWidget *cw = sa->widget()) {
                cw->ensurePolished();
                if (cw->layout())
                    cw->layout()->activate();
                maxContentH = qMax(maxContentH,
                                   qMax(cw->sizeHint().height(), cw->minimumSizeHint().height()));
            }
        }
    }

    // Sleek single-row action strip (~46-54px tall, zero vertical clipping)
    const int baseH = qMax(46, qMax(btnH + 10, maxContentH + 4));

    bool hScrollActive = false;
    if (QScrollArea *sa = qobject_cast<QScrollArea *>(d_tab_stack->currentWidget())) {
        if (QScrollBar *hb = sa->horizontalScrollBar())
            hScrollActive = hb->isVisible() || (hb->maximum() > hb->minimum());
    }
    const int targetH = hScrollActive ? (baseH + 8) : baseH;
    if (d_tab_stack->height() != targetH || d_tab_stack->minimumHeight() != targetH
        || d_tab_stack->maximumHeight() != targetH) {
        d_tab_stack->setFixedHeight(targetH);
        if (layout())
            layout()->activate();
        updateGeometry();
    }
    if (QScrollArea *curSa = qobject_cast<QScrollArea *>(d_tab_stack->currentWidget())) {
        if (QWidget *cw = curSa->widget()) {
            if (cw->layout())
                cw->layout()->activate();
        }
    }
}

void ModernRibbonBar::updateHeaderResponsiveness()
{
    if (d_shutting_down)
        return;
    const int w = width();
    if (w <= 0)
        return;

    int tabsNeededW = 0;
    if (d_tab_bar) {
        d_tab_bar->ensurePolished();
        for (int i = 0; i < d_tab_bar->count(); ++i)
            tabsNeededW += d_tab_bar->tabRect(i).width() + 1;
        if (d_tab_bar->count() > 0) {
            const int spanW = d_tab_bar->tabRect(d_tab_bar->count() - 1).right()
                    - d_tab_bar->tabRect(0).left() + 2;
            tabsNeededW = qMax(tabsNeededW, spanW);
        }
        tabsNeededW = qMax(tabsNeededW, d_tab_bar->sizeHint().width());
    }
    if (tabsNeededW <= 0)
        tabsNeededW = 560;

    struct HeaderTier
    {
        bool showLang;
        bool shortCmdFocus;
        bool compactLayout;
        bool modeIconOnly;
        bool cmdIconOnly;
        bool showCtx;
    };

    const HeaderTier kTiers[] = {
        { true, false, false, false, false, true },   // Ultra-wide: full labels + language
        { false, false, false, false, false, true },  // Wide: search pill + full mode labels + ctx
        { false, true, true, false, false, true },    // Medium: search pill + short mode labels + ctx
        { false, true, true, true, false, true },     // 1280px+: always-visible 180-210px search pill + icon segmented modes + ctx
        { false, true, true, true, false, false },    // Compact: search pill + icon segmented modes
        { false, true, true, true, true, false }      // Minimal icon-only fallback
    };

    auto applyButtonsForTier = [this](const HeaderTier &t) {
        if (d_btn_cmd_palette) {
            const QString wantCmd = tr("Ctrl+K Search Commands...");
            if (d_btn_cmd_palette->text() != wantCmd)
                d_btn_cmd_palette->setText(wantCmd);
            const Qt::ToolButtonStyle searchStyle = t.cmdIconOnly
                    ? Qt::ToolButtonIconOnly
                    : Qt::ToolButtonTextBesideIcon;
            if (d_btn_cmd_palette->toolButtonStyle() != searchStyle)
                d_btn_cmd_palette->setToolButtonStyle(searchStyle);
            const int minW = t.cmdIconOnly ? 0 : 180;
            const int maxW = t.cmdIconOnly ? 36 : 210;
            if (d_btn_cmd_palette->minimumWidth() != minW)
                d_btn_cmd_palette->setMinimumWidth(minW);
            if (d_btn_cmd_palette->maximumWidth() != maxW)
                d_btn_cmd_palette->setMaximumWidth(maxW);
        }
        if (d_btn_focus_mode) {
            const QString wantFocus = t.shortCmdFocus ? tr("Focus") : tr("Focus (F11)");
            if (d_btn_focus_mode->text() != wantFocus)
                d_btn_focus_mode->setText(wantFocus);
        }
        if (d_btn_layout_tabbed) {
            const QString wantTab = t.compactLayout ? tr("Tabs") : tr("Tabbed");
            if (d_btn_layout_tabbed->text() != wantTab)
                d_btn_layout_tabbed->setText(wantTab);
        }
        if (d_btn_layout_split) {
            const QString wantSplit = t.compactLayout ? tr("Split") : tr("Split View");
            if (d_btn_layout_split->text() != wantSplit)
                d_btn_layout_split->setText(wantSplit);
        }
        if (d_btn_layout_mdi) {
            const QString wantMdi = t.compactLayout ? tr("Free View") : tr("Free MDI");
            if (d_btn_layout_mdi->text() != wantMdi)
                d_btn_layout_mdi->setText(wantMdi);
        }

        const Qt::ToolButtonStyle pillStyle =
                t.modeIconOnly ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon;
        QToolButton *modePills[] = { d_btn_layout_tabbed, d_btn_layout_split, d_btn_layout_mdi,
                                     d_btn_focus_mode, d_btn_lang_switch };
        for (QToolButton *hb : modePills) {
            if (hb && hb->toolButtonStyle() != pillStyle)
                hb->setToolButtonStyle(pillStyle);
        }
    };

    auto measureTierRightW = [this](const HeaderTier &t) -> int {
        int rw = 0;
        auto addW = [&rw](QWidget *wgt, bool vis) {
            if (!wgt || !vis)
                return;
            wgt->ensurePolished();
            rw += qMax(wgt->minimumWidth(), wgt->sizeHint().width()) + 6;
        };
        addW(d_btn_cmd_palette, true);
        if (d_layout_segment_frame) {
            d_layout_segment_frame->ensurePolished();
            if (d_layout_segment_frame->layout())
                d_layout_segment_frame->layout()->invalidate();
            rw += d_layout_segment_frame->sizeHint().width() + 6;
        }
        addW(d_btn_focus_mode, true);
        addW(d_btn_lang_switch, t.showLang);
        addW(d_context_pill, t.showCtx);
        return rw;
    };

    const int nTiers = static_cast<int>(sizeof(kTiers) / sizeof(kTiers[0]));
    HeaderTier chosen = kTiers[nTiers - 1];
    int chosenRightW = 0;
    bool found = false;

    for (int idx = 0; idx < nTiers; ++idx) {
        const HeaderTier &cand = kTiers[idx];
        if (cand.showLang && w < qMax(1560, tabsNeededW + 820))
            continue;

        applyButtonsForTier(cand);
        const int rw = measureTierRightW(cand);
        if (tabsNeededW + rw + 24 <= w || idx == nTiers - 1) {
            chosen = cand;
            chosenRightW = rw;
            found = true;
            break;
        }
    }
    if (!found) {
        applyButtonsForTier(chosen);
        chosenRightW = measureTierRightW(chosen);
    }

    if (d_btn_lang_switch && d_btn_lang_switch->isVisible() != chosen.showLang)
        d_btn_lang_switch->setVisible(chosen.showLang);
    if (d_context_pill && d_context_pill->isVisible() != chosen.showCtx)
        d_context_pill->setVisible(chosen.showCtx);

    if (d_tab_bar) {
        const int maxAvailTabW = qMax(360, qMax(w - 260, w - chosenRightW - 18));
        const int wantMinTabW = qMin(tabsNeededW + 10, maxAvailTabW);
        if (d_tab_bar->minimumWidth() != wantMinTabW)
            d_tab_bar->setMinimumWidth(wantMinTabW);
        if (QWidget *ts = d_tab_bar->parentWidget()) {
            if (ts->layout())
                ts->layout()->activate();
        }
    }
}

void ModernRibbonBar::prepareForAppShutdown()
{
    const bool wasShuttingDown = d_shutting_down;
    d_shutting_down = true;
    d_saved_focus_visibility.clear();
    d_last_active_window.clear();

    if (!wasShuttingDown && d_app) {
        disconnect(&d_app->d_workspace, nullptr, this, nullptr);
        if (QWidget *vp = d_app->d_workspace.viewport())
            vp->removeEventFilter(this);
    }

    if (d_focus_hud_overlay) {
        d_focus_hud_overlay->prepareForAppShutdown();
        delete d_focus_hud_overlay.data();
    }
    if (d_command_palette)
        delete d_command_palette.data();

    d_app.clear();
}

QWidget *ModernRibbonBar::createRibbonScrollPage(QHBoxLayout **outRowLayout)
{
    RibbonHorizontalScrollArea *scroll = new RibbonHorizontalScrollArea(d_tab_stack);
    QWidget *container = new QWidget(scroll);
    container->setStyleSheet(QStringLiteral("background: #FFFFFF;"));
    QHBoxLayout *row = new QHBoxLayout(container);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(2);
    scroll->setWidget(container);
    if (QScrollBar *hbar = scroll->horizontalScrollBar()) {
        connect(hbar, &QScrollBar::rangeChanged, this,
                [this](int, int) { updateTabStackHeight(); });
    }
    if (outRowLayout)
        *outRowLayout = row;
    return scroll;
}

QWidget *ModernRibbonBar::createRibbonGroup(QHBoxLayout *pageRow, QHBoxLayout **outBtnLayout,
                                            bool addDividerAfter)
{
    QWidget *grpWidget = new QWidget();
    QHBoxLayout *btnRow = new QHBoxLayout(grpWidget);
    btnRow->setContentsMargins(2, 0, 2, 0);
    btnRow->setSpacing(2);

    pageRow->addWidget(grpWidget, 0, Qt::AlignVCenter);

    if (addDividerAfter) {
        QFrame *div = new QFrame();
        div->setFixedSize(1, 28);
        div->setStyleSheet(QStringLiteral("background-color: #CBD5E1; margin: 0px 3px;"));
        pageRow->addWidget(div, 0, Qt::AlignVCenter);
    }

    if (outBtnLayout)
        *outBtnLayout = btnRow;
    return grpWidget;
}

QToolButton *ModernRibbonBar::addRibbonButton(QHBoxLayout *groupLayout, const QString &iconId,
                                              std::function<QString()> textFn,
                                              std::function<QString()> tooltipFn,
                                              std::function<void()> onClicked, bool checkable,
                                              bool accentPreset)
{
    QToolButton *btn = new QToolButton();
    btn->setProperty("class",
                     accentPreset ? QStringLiteral("RibbonPresetBtn")
                                  : QStringLiteral("RibbonActionBtn"));
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIcon(IconLoader::duotoneIcon(iconId, 20, false));
    btn->setIconSize(QSize(16, 16));
    btn->setCheckable(checkable);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);

    if (onClicked)
        connect(btn, &QToolButton::clicked, this, std::move(onClicked));

    groupLayout->addWidget(btn);
    d_button_bindings.append(
            { btn, iconId, std::move(textFn), std::move(tooltipFn), accentPreset });
    return btn;
}

QToolButton *ModernRibbonBar::addRibbonMenuButton(QHBoxLayout *groupLayout, const QString &iconId,
                                                  std::function<QString()> textFn,
                                                  std::function<QString()> tooltipFn,
                                                  QMenu **outMenu, bool accentPreset)
{
    QToolButton *btn = new QToolButton();
    btn->setProperty("class",
                     accentPreset ? QStringLiteral("RibbonPresetBtn")
                                  : QStringLiteral("RibbonActionBtn"));
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIcon(IconLoader::duotoneIcon(iconId, 20, false));
    btn->setIconSize(QSize(16, 16));
    btn->setPopupMode(QToolButton::InstantPopup);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);

    QMenu *menu = new QMenu(btn);
    menu->setProperty("class", QStringLiteral("RibbonDropdownMenu"));
    menu->setToolTipsVisible(true);
    btn->setMenu(menu);

    groupLayout->addWidget(btn);
    d_button_bindings.append(
            { btn, iconId, std::move(textFn), std::move(tooltipFn), accentPreset });
    if (outMenu)
        *outMenu = menu;
    return btn;
}

QAction *ModernRibbonBar::addRibbonMenuAction(QMenu *menu, const QString &iconId,
                                              std::function<QString()> textFn,
                                              std::function<QString()> tooltipFn,
                                              std::function<void()> onTriggered, bool checkable)
{
    if (!menu)
        return nullptr;
    QAction *act = new QAction(menu);
    act->setCheckable(checkable);
    if (!iconId.isEmpty())
        act->setIcon(IconLoader::duotoneIcon(iconId, 18, false));
    if (onTriggered)
        connect(act, &QAction::triggered, this, std::move(onTriggered));
    menu->addAction(act);
    d_menu_action_bindings.append({ act, iconId, std::move(textFn), std::move(tooltipFn) });
    return act;
}

void ModernRibbonBar::buildAllTabs()
{
    auto addTabPage = [this](std::function<QString()> titleFn, QWidget *page) {
        const int idx = d_tab_bar->addTab(QString());
        Q_UNUSED(idx);
        d_tab_stack->addWidget(page);
        d_tab_title_fns.append(std::move(titleFn));
    };

    const QString kCaret = QString::fromUtf8(" \xE2\x96\xBE");

    // ------------------------------------------------------------------------
    // Tab 1: Home & Data
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gFile = nullptr;
        QHBoxLayout *gCreate = nullptr;
        QHBoxLayout *gRecalc = nullptr;
        QHBoxLayout *gPanels = nullptr;

        createRibbonGroup(row, &gFile, true);
        addRibbonButton(
                gFile, QStringLiteral("file-new"), [this]() { return tr("New Project"); },
                [this]() { return tr("Create a New Project"); },
                [this]() {
                    if (d_app)
                        d_app->newProject();
                });
        addRibbonButton(
                gFile, QStringLiteral("file-open"), [this]() { return tr("Open"); },
                [this]() { return tr("Open Project File"); },
                [this]() {
                    if (d_app)
                        d_app->open();
                });
        addRibbonButton(
                gFile, QStringLiteral("file-save"), [this]() { return tr("Save"); },
                [this]() { return tr("Save Current Project"); },
                [this]() {
                    if (d_app)
                        d_app->saveProject();
                });
        QMenu *importMenu = nullptr;
        addRibbonMenuButton(
                gFile, QStringLiteral("import-csv"),
                [this, kCaret]() { return tr("Import") + kCaret; },
                [this]() { return tr("Import Data or Origin Project"); }, &importMenu);
        addRibbonMenuAction(
                importMenu, QStringLiteral("import-csv"),
                [this]() { return tr("Import CSV / ASCII..."); },
                [this]() { return tr("Import ASCII / CSV Data into Worksheet"); },
                [this]() {
                    if (d_app)
                        d_app->importASCII();
                });
        addRibbonMenuAction(
                importMenu, QStringLiteral("import-opj"),
                [this]() { return tr("Import Origin OPJ..."); },
                [this]() { return tr("Import Origin OPJ Project"); },
                [this]() { importOpjInteractive(); });
        addRibbonButton(
                gFile, QStringLiteral("sample-gallery"), [this]() { return tr("Samples"); },
                [this]() { return tr("Open Built-In Scientific Sample Project Gallery"); },
                [this]() {
                    if (d_app)
                        CurveMathTools::showSampleGalleryDialog(d_app);
                },
                false, true);

        createRibbonGroup(row, &gCreate, true);
        addRibbonButton(
                gCreate, QStringLiteral("new-table"), [this]() { return tr("New Table"); },
                [this]() { return tr("Create New Worksheet Table"); },
                [this]() {
                    if (d_app)
                        d_app->newTable();
                });
        addRibbonButton(
                gCreate, QStringLiteral("new-graph"), [this]() { return tr("New Graph"); },
                [this]() { return tr("Create New 2D Graph Window"); },
                [this]() {
                    if (d_app)
                        d_app->newGraph();
                });
        QMenu *moreNewMenu = nullptr;
        addRibbonMenuButton(
                gCreate, QStringLiteral("new-matrix"),
                [this, kCaret]() { return tr("More New") + kCaret; },
                [this]() { return tr("Create Matrix, Add Column, or Merge Graphs"); }, &moreNewMenu);
        addRibbonMenuAction(
                moreNewMenu, QStringLiteral("new-matrix"), [this]() { return tr("New Matrix"); },
                [this]() { return tr("Create New 2D Data Matrix"); },
                [this]() {
                    if (d_app)
                        d_app->newMatrix();
                });
        addRibbonMenuAction(
                moreNewMenu, QStringLiteral("add-column"), [this]() { return tr("Add Column"); },
                [this]() { return tr("Append Y Column to Active Worksheet"); },
                [this]() {
                    if (d_app)
                        d_app->addColToTable();
                });
        moreNewMenu->addSeparator();
        addRibbonMenuAction(
                moreNewMenu, QStringLiteral("split-view"), [this]() { return tr("Merge Graphs"); },
                [this]() { return tr("Merge Multiple Open Graph Windows into Multi-Panel Figure"); },
                [this]() {
                    if (d_app)
                        d_app->mergeOpenGraphsDialog();
                });

        createRibbonGroup(row, &gRecalc, true);
        d_btn_auto_recalc = addRibbonButton(
                gRecalc, QStringLiteral("recalc-auto"), [this]() { return tr("Auto"); },
                [this]() { return tr("Toggle Automatic Analysis Recalculation"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (d_btn_auto_recalc && d_btn_auto_recalc->isChecked())
                        d_app->setRecalcModeAuto();
                    else
                        d_app->setRecalcModeManual();
                    syncCheckableStates();
                },
                true);
        addRibbonButton(
                gRecalc, QStringLiteral("recalc-now"), [this]() { return tr("Recalc"); },
                [this]() { return tr("Recalculate Pending Analysis Links Now"); },
                [this]() {
                    if (d_app)
                        d_app->recalculatePendingLinksNow();
                });

        createRibbonGroup(row, &gPanels, false);
        QMenu *panelsMenu = nullptr;
        addRibbonMenuButton(
                gPanels, QStringLiteral("table-drawer"),
                [this, kCaret]() { return tr("Panels") + kCaret; },
                [this]() { return tr("Show or Hide Explorer, Inspector & Results Log Panels"); },
                &panelsMenu);
        d_act_explorer = addRibbonMenuAction(
                panelsMenu, QStringLiteral("explorer"), [this]() { return tr("Explorer"); },
                [this]() { return tr("Show or Hide Project Explorer Panel"); },
                [this]() {
                    if (!d_app)
                        return;
                    d_app->explorerWindow.setVisible(d_app->explorerWindow.isHidden());
                    syncCheckableStates();
                },
                true);
        d_act_inspector = addRibbonMenuAction(
                panelsMenu, QStringLiteral("table-drawer"),
                [this]() { return tr("Inspector"); },
                [this]() { return tr("Toggle Graph Inspector Dock (Ctrl+Shift+I)"); },
                [this]() {
                    if (!d_app)
                        return;
                    d_app->toggleGraphInspectorDock();
                    syncCheckableStates();
                },
                true);
        d_act_results_log = addRibbonMenuAction(
                panelsMenu, QStringLiteral("log-window"),
                [this]() { return tr("Results Log"); },
                [this]() { return tr("Show or Hide Results Log Panel"); },
                [this]() {
                    if (!d_app)
                        return;
                    d_app->logWindow.setVisible(d_app->logWindow.isHidden());
                    syncCheckableStates();
                },
                true);

        row->addStretch(1);
        addTabPage([this]() { return tr("Home & Data"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 2: Plot 2D/3D (Decluttered: 1-click primary 2D + Specialty + 3D + Layers)
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *g2d = nullptr;
        QHBoxLayout *gMore = nullptr;
        QHBoxLayout *gLayer = nullptr;

        createRibbonGroup(row, &g2d, true);
        addRibbonButton(
                g2d, QStringLiteral("plot-line"), [this]() { return tr("Line"); },
                [this]() { return tr("Plot Line Graph"); },
                [this]() { triggerPlot2D(Graph::Line); });
        addRibbonButton(
                g2d, QStringLiteral("plot-line-symbol"), [this]() { return tr("Line + Symbol"); },
                [this]() { return tr("Plot Line and Symbol Graph"); },
                [this]() { triggerPlot2D(Graph::LineSymbols); });
        addRibbonButton(
                g2d, QStringLiteral("plot-scatter"), [this]() { return tr("Scatter"); },
                [this]() { return tr("Plot Scatter Graph"); },
                [this]() { triggerPlot2D(Graph::Scatter); });
        addRibbonButton(
                g2d, QStringLiteral("plot-bars"), [this]() { return tr("Bar Chart"); },
                [this]() { return tr("Plot Vertical Bar Chart"); },
                [this]() { triggerPlot2D(Graph::VerticalBars); });
        addRibbonButton(
                g2d, QStringLiteral("plot-double-y"), [this]() { return tr("Double-Y"); },
                [this]() { return tr("Plot Dual Left/Right Y-Axis Graph"); },
                [this]() {
                    if (d_app)
                        d_app->plotDoubleY();
                });

        createRibbonGroup(row, &gMore, true);
        QMenu *specialtyMenu = nullptr;
        addRibbonMenuButton(
                gMore, QStringLiteral("plot-box"),
                [this, kCaret]() { return tr("Specialty") + kCaret; },
                [this]() { return tr("Specialized 2D, Statistical & Matrix Plots"); },
                &specialtyMenu);
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-spline"), [this]() { return tr("Spline"); },
                [this]() { return tr("Plot Cubic Spline Curve"); },
                [this]() { triggerPlot2D(Graph::Spline); });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-box"), [this]() { return tr("Box Plot"); },
                [this]() { return tr("Plot Statistical Box-and-Whisker Chart"); },
                [this]() { triggerPlot2D(Graph::Box); });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-violin"),
                [this]() { return tr("Violin Plot"); },
                [this]() { return tr("Plot Kernel Density Violin Chart"); },
                [this]() {
                    if (d_app)
                        d_app->plotViolin();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-histogram"),
                [this]() { return tr("Histogram"); },
                [this]() { return tr("Plot Frequency Histogram"); },
                [this]() { triggerPlot2D(Graph::Histogram); });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-waterfall"),
                [this]() { return tr("Waterfall / Y-Stack"); },
                [this]() { return tr("Plot Y-Offset Stacked Waterfall Graph"); },
                [this]() {
                    if (d_app)
                        d_app->plotYOffsetStack();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-ternary"),
                [this]() { return tr("Ternary Plot (A-B-C)"); },
                [this]() { return tr("Plot Ternary Composition Diagram (A-B-C)"); },
                [this]() {
                    if (d_app)
                        d_app->plotTernary();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-spline"),
                [this]() { return tr("Polar / Function Plot..."); },
                [this]() { return tr("Plot Polar, Parametric, or Explicit Mathematical Function"); },
                [this]() {
                    if (d_app)
                        d_app->showFunctionDialog();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-vectors"),
                [this]() { return tr("Vector Field (XYXY)"); },
                [this]() { return tr("Plot 2D Vector Field (XYXY)"); },
                [this]() {
                    if (d_app)
                        d_app->plotVectXYXY();
                });
        specialtyMenu->addSeparator();
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-heatmap"),
                [this]() { return tr("Heatmap (Color Map)"); },
                [this]() { return tr("Plot Matrix Color Map / Heatmap"); },
                [this]() {
                    if (d_app)
                        d_app->plotColorMap();
                });
        addRibbonMenuAction(
                specialtyMenu, QStringLiteral("plot-contour"),
                [this]() { return tr("Contour Lines"); },
                [this]() { return tr("Plot Matrix Contour Lines"); },
                [this]() {
                    if (d_app)
                        d_app->plotContour();
                });

        QMenu *plot3dMenu = nullptr;
        addRibbonMenuButton(
                gMore, QStringLiteral("plot-3d-surface"),
                [this, kCaret]() { return tr("3D Plots") + kCaret; },
                [this]() { return tr("3D Surface, Scatter & Bar Plots"); }, &plot3dMenu);
        addRibbonMenuAction(
                plot3dMenu, QStringLiteral("plot-3d-surface"),
                [this]() { return tr("3D Surface"); }, [this]() { return tr("Plot 3D Surface Mesh"); },
                [this]() {
                    if (d_app)
                        d_app->plot3DWireSurface();
                });
        addRibbonMenuAction(
                plot3dMenu, QStringLiteral("plot-3d-scatter"),
                [this]() { return tr("3D Scatter"); },
                [this]() { return tr("Plot 3D Scatter Points"); },
                [this]() {
                    if (d_app)
                        d_app->plot3DScatter();
                });
        addRibbonMenuAction(
                plot3dMenu, QStringLiteral("plot-3d-bars"), [this]() { return tr("3D Bars"); },
                [this]() { return tr("Plot 3D Bars"); },
                [this]() {
                    if (d_app)
                        d_app->plot3DBars();
                });

        createRibbonGroup(row, &gLayer, false);
        addRibbonButton(
                gLayer, QStringLiteral("layer-linked"), [this]() { return tr("Linked Layer"); },
                [this]() { return tr("Add Linked Top-X / Right-Y Layer"); },
                [this]() {
                    if (d_app)
                        d_app->addLinkedTopRightLayer();
                });
        addRibbonButton(
                gLayer, QStringLiteral("layer-inset"), [this]() { return tr("Zoom Inset"); },
                [this]() { return tr("Add Magnified Zoom Inset Layer"); },
                [this]() {
                    if (d_app)
                        d_app->addZoomInsetLayer();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Plot 2D/3D"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 3: Peaks & Fit
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gPeaks = nullptr;
        QHBoxLayout *gFit = nullptr;

        createRibbonGroup(row, &gPeaks, true);
        addRibbonButton(
                gPeaks, QStringLiteral("peak-analyzer"), [this]() { return tr("Peak Analyzer"); },
                [this]() { return tr("Interactive Baseline & Multi-Peak Deconvolution"); },
                [this]() {
                    if (d_app)
                        d_app->showPeakAnalyzerDialog(0);
                });
        QMenu *morePeaksMenu = nullptr;
        addRibbonMenuButton(
                gPeaks, QStringLiteral("peak-voigt"),
                [this, kCaret]() { return tr("More Peaks") + kCaret; },
                [this]() { return tr("Auto Voigt, Batch & 2D Matrix Peak Analysis"); },
                &morePeaksMenu);
        addRibbonMenuAction(
                morePeaksMenu, QStringLiteral("peak-voigt"),
                [this]() { return tr("Auto Voigt Peaks"); },
                [this]() { return tr("Automatic Pseudo-Voigt Peak Detection & Fit"); },
                [this]() {
                    if (d_app)
                        d_app->fitAutoPeakPseudoVoigt();
                });
        addRibbonMenuAction(
                morePeaksMenu, QStringLiteral("peak-batch"),
                [this]() { return tr("Batch Peaks"); },
                [this]() { return tr("Batch Multi-Spectrum Peak Analysis"); },
                [this]() {
                    if (d_app)
                        d_app->showBatchPeakAnalysisDialog();
                });
        addRibbonMenuAction(
                morePeaksMenu, QStringLiteral("peak-2d"), [this]() { return tr("2D Peaks"); },
                [this]() { return tr("Matrix 2D Peak Detection & Fitting"); },
                [this]() {
                    if (d_app)
                        d_app->show2DPeakAnalysisDialog();
                });

        createRibbonGroup(row, &gFit, false);
        addRibbonButton(
                gFit, QStringLiteral("fit-linear"), [this]() { return tr("Linear Fit"); },
                [this]() { return tr("Linear Regression Fit"); },
                [this]() {
                    if (d_app)
                        d_app->fitLinear();
                });
        addRibbonButton(
                gFit, QStringLiteral("fit-poly"), [this]() { return tr("Polynomial"); },
                [this]() { return tr("Polynomial Regression Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showFitPolynomDialog();
                });
        addRibbonButton(
                gFit, QStringLiteral("fit-nonlinear"), [this]() { return tr("Nonlinear Fit"); },
                [this]() { return tr("Nonlinear Curve Fit Wizard"); },
                [this]() {
                    if (d_app)
                        d_app->showFitDialog();
                });
        QMenu *advFitMenu = nullptr;
        addRibbonMenuButton(
                gFit, QStringLiteral("fit-global"),
                [this, kCaret]() { return tr("Advanced Fit") + kCaret; },
                [this]() {
                    return tr("York, Global, Piecewise, Robust, Logistic, 3D Surface & Model Compare");
                },
                &advFitMenu);
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-york"), [this]() { return tr("York XY Fit"); },
                [this]() { return tr("York Linear Regression with Both X and Y Errors"); },
                [this]() {
                    if (d_app)
                        d_app->showYorkLinearFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-global"), [this]() { return tr("Global Fit"); },
                [this]() { return tr("Multi-Curve Global Fit with Shared Parameters"); },
                [this]() {
                    if (d_app)
                        d_app->showGlobalCurveFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-piecewise"),
                [this]() { return tr("Piecewise Fit"); },
                [this]() { return tr("Piecewise Segmented Linear Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showPiecewiseLinearFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-robust"), [this]() { return tr("Robust Fit"); },
                [this]() { return tr("Huber / Bisquare Outlier-Resistant Robust Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showRobustLinearFitDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-logistic"),
                [this]() { return tr("Logistic Fit"); },
                [this]() { return tr("Binary Logistic Regression"); },
                [this]() {
                    if (d_app)
                        d_app->showLogisticRegressionDialog();
                });
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-surface-3d"),
                [this]() { return tr("3D Surface Fit"); },
                [this]() { return tr("2D / 3D Surface Regression Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showSurfaceFit3DDialog();
                });
        advFitMenu->addSeparator();
        addRibbonMenuAction(
                advFitMenu, QStringLiteral("fit-compare"),
                [this]() { return tr("Model Compare"); },
                [this]() { return tr("Compare Candidate Fit Models (AIC / BIC / F-Test)"); },
                [this]() {
                    if (d_app)
                        d_app->showModelComparisonDialog();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Peaks & Fit"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 4: Gadgets & Signal
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gGadgets = nullptr;
        QHBoxLayout *gMasks = nullptr;
        QHBoxLayout *gSignal = nullptr;

        createRibbonGroup(row, &gGadgets, true);
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-roi"), [this]() { return tr("ROI Stats"); },
                [this]() { return tr("Interactive Region-of-Interest Statistics Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachRegionStatsGadget();
                });
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-integrate"), [this]() { return tr("Integrate"); },
                [this]() { return tr("Interactive Peak / Curve Area Integration Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachIntegrateGadget();
                });
        addRibbonButton(
                gGadgets, QStringLiteral("gadget-quick-fit"), [this]() { return tr("Quick Fit"); },
                [this]() { return tr("Interactive ROI Quick Fit Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachQuickFitGadget();
                });
        QMenu *moreGadgetsMenu = nullptr;
        addRibbonMenuButton(
                gGadgets, QStringLiteral("gadget-delta"),
                [this, kCaret]() { return tr("More Gadgets") + kCaret; },
                [this]() {
                    return tr("Vertical Cursor, Quick Peaks, Delta Cursor, 2D Profile, Differentiate & Clear");
                },
                &moreGadgetsMenu);
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-cursor"),
                [this]() { return tr("Vertical Cursor"); },
                [this]() { return tr("Vertical Crosshair Curve Readout Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachVerticalCursorGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-quick-peaks"),
                [this]() { return tr("Quick Peaks"); },
                [this]() { return tr("Interactive ROI Quick Peak Finder Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachQuickPeaksGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-delta"),
                [this]() { return tr("Delta Cursor"); },
                [this]() { return tr("Dual-Cursor Delta X / Delta Y / Slope Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachDeltaCursorGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-profile-2d"),
                [this]() { return tr("2D Profile"); },
                [this]() { return tr("Interactive Heatmap Horizontal/Vertical Profile Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachImageProfileGadget();
                });
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-diff"),
                [this]() { return tr("Differentiate"); },
                [this]() { return tr("Interactive Derivative Gadget"); },
                [this]() {
                    if (d_app)
                        d_app->attachDifferentiateGadget();
                });
        moreGadgetsMenu->addSeparator();
        addRibbonMenuAction(
                moreGadgetsMenu, QStringLiteral("gadget-clear"),
                [this]() { return tr("Clear Gadget"); },
                [this]() { return tr("Remove Active Gadget from Graph"); },
                [this]() {
                    if (d_app)
                        d_app->clearActiveGadget();
                });

        createRibbonGroup(row, &gMasks, true);
        QMenu *masksMenu = nullptr;
        addRibbonMenuButton(
                gMasks, QStringLiteral("mask-points"),
                [this, kCaret]() { return tr("Data Masks") + kCaret; },
                [this]() { return tr("Mask Outlier Points, Mask Region, or Clear Masks"); },
                &masksMenu);
        addRibbonMenuAction(
                masksMenu, QStringLiteral("mask-points"), [this]() { return tr("Mask Points"); },
                [this]() { return tr("Mask Individual Outlier Data Points"); },
                [this]() {
                    if (d_app)
                        d_app->maskDataPointsTool();
                });
        addRibbonMenuAction(
                masksMenu, QStringLiteral("range-selector"), [this]() { return tr("Mask ROI"); },
                [this]() { return tr("Mask Rectangular Data Region"); },
                [this]() {
                    if (d_app)
                        d_app->maskRegionGadget();
                });
        masksMenu->addSeparator();
        addRibbonMenuAction(
                masksMenu, QStringLiteral("clear-masks"), [this]() { return tr("Clear Masks"); },
                [this]() { return tr("Clear All Data Masks on Active Graph"); },
                [this]() {
                    if (d_app)
                        d_app->clearAllMasksOnGraph();
                });

        createRibbonGroup(row, &gSignal, false);
        addRibbonButton(
                gSignal, QStringLiteral("signal-savgol"),
                [this]() { return tr("Savitzky-Golay Smooth"); },
                [this]() { return tr("Savitzky-Golay Smoothing & Derivative"); },
                [this]() {
                    if (d_app)
                        d_app->showSavGolSmoothDerivativeDialog();
                });
        addRibbonButton(
                gSignal, QStringLiteral("signal-fft"), [this]() { return tr("FFT Spectrum"); },
                [this]() { return tr("Fast Fourier Transform (FFT)"); },
                [this]() {
                    if (d_app)
                        d_app->showFFTDialog();
                });
        QMenu *filterMenu = nullptr;
        addRibbonMenuButton(
                gSignal, QStringLiteral("signal-smooth"),
                [this, kCaret]() { return tr("Filter") + kCaret; },
                [this]() { return tr("LOWESS, Whittaker, IIR & Matrix 2D Filters"); },
                &filterMenu);
        addRibbonMenuAction(
                filterMenu, QStringLiteral("signal-smooth"),
                [this]() { return tr("LOWESS Smooth"); },
                [this]() { return tr("Locally Weighted Scatterplot Smoothing (LOWESS)"); },
                [this]() {
                    if (d_app)
                        d_app->showSmoothLOWESSDialog();
                });
        addRibbonMenuAction(
                filterMenu, QStringLiteral("signal-smooth"),
                [this]() { return tr("Whittaker Smooth"); },
                [this]() { return tr("Penalized Least-Squares Whittaker Smoothing"); },
                [this]() {
                    if (d_app)
                        d_app->showSmoothWhittakerDialog();
                });
        addRibbonMenuAction(
                filterMenu, QStringLiteral("signal-iir"), [this]() { return tr("IIR Filter"); },
                [this]() { return tr("Zero-Phase Butterworth / Chebyshev / Bessel IIR Filter"); },
                [this]() {
                    if (d_app)
                        d_app->showIirDigitalFilterDialog();
                });
        addRibbonMenuAction(
                filterMenu, QStringLiteral("matrix-filter2d"),
                [this]() { return tr("Matrix 2D Filter"); },
                [this]() { return tr("2D Matrix Gaussian / Median / Detrend Filter"); },
                [this]() {
                    if (d_app)
                        d_app->showMatrixFilter2DDialog();
                });

        QMenu *spectralMenu = nullptr;
        addRibbonMenuButton(
                gSignal, QStringLiteral("signal-stft"),
                [this, kCaret]() { return tr("Spectral") + kCaret; },
                [this]() {
                    return tr("STFT Spectrogram, Wavelet, Welch PSD & FSD Deconvolution");
                },
                &spectralMenu);
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-stft"),
                [this]() { return tr("STFT Spectrogram"); },
                [this]() { return tr("Short-Time Fourier Transform Spectrogram"); },
                [this]() {
                    if (d_app)
                        d_app->showStftSpectrogramDialog();
                });
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-wavelet"),
                [this]() { return tr("Wavelet Transform"); },
                [this]() { return tr("Continuous & Discrete Wavelet Denoising / CWT"); },
                [this]() {
                    if (d_app)
                        d_app->showWaveletTransformDialog();
                });
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-psd"), [this]() { return tr("Welch PSD"); },
                [this]() { return tr("Welch Power Spectral Density & Coherence"); },
                [this]() {
                    if (d_app)
                        d_app->showWelchPsdCoherenceDialog();
                });
        addRibbonMenuAction(
                spectralMenu, QStringLiteral("signal-fsd"),
                [this]() { return tr("Deconvolve FSD"); },
                [this]() { return tr("Fourier Self-Deconvolution (Resolution Enhancement)"); },
                [this]() {
                    if (d_app)
                        d_app->showFourierSelfDeconvolveDialog();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Gadgets & Signal"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 5: Statistics & MatSci (Materials & Domain FIRST on the left)
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gDomain = nullptr;
        QHBoxLayout *gStats = nullptr;

        createRibbonGroup(row, &gDomain, true);
        addRibbonButton(
                gDomain, QStringLiteral("curvemath"),
                [this]() { return tr("Tensile / 0.2% Yield"); },
                [this]() {
                    return tr("Tensile Stress-Strain: Young's Modulus E, 0.2% Proof Stress & UTS");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showTensileYieldDialog(d_app);
                });
        addRibbonButton(
                gDomain, QStringLiteral("work-hardening"),
                [this]() { return tr("Work Hardening"); },
                [this]() {
                    return tr("True Stress-Strain, Hollomon n, Crussard-Jaoul & Considere Necking");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showWorkHardeningDialog(d_app);
                });
        addRibbonButton(
                gDomain, QStringLiteral("kissinger-tds"),
                [this]() { return tr("Kissinger TDS"); },
                [this]() {
                    return tr("Kissinger Thermal Desorption (TDS / DSC) Activation Energy Analysis");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showKissingerDialog(d_app);
                });
        addRibbonButton(
                gDomain, QStringLiteral("xrd-peaks"), [this]() { return tr("XRD / W-H"); },
                [this]() {
                    return tr("Powder XRD K-alpha2 Stripping, Scherrer & Williamson-Hall Analysis");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showXRDAnalyzerDialog(d_app);
                });
        QMenu *matToolsMenu = nullptr;
        addRibbonMenuButton(
                gDomain, QStringLiteral("vasp-bands"),
                [this, kCaret]() { return tr("Materials") + kCaret; },
                [this]() { return tr("VASP, Arrhenius, Tauc, EIS, Curve Math, Fill & Digitizer"); },
                &matToolsMenu);
        addRibbonMenuAction(
                matToolsMenu, QStringLiteral("vasp-bands"),
                [this]() { return tr("VASP Bands/DOS"); },
                [this]() {
                    return tr("VASP EIGENVAL / DOSCAR Electronic Band Structure & DOS Plotter");
                },
                [this]() {
                    if (d_app)
                        CurveMathTools::showVaspPlotterDialog(d_app);
                });
        addRibbonMenuAction(
                matToolsMenu, QStringLiteral("arrhenius"),
                [this]() { return tr("Arrhenius Plot"); },
                [this]() { return tr("Arrhenius ln(k) vs 1000/T Activation Energy Analysis"); },
                [this]() {
                    if (d_app)
                        d_app->showArrheniusPlotDialog();
                });
        addRibbonMenuAction(
                matToolsMenu, QStringLiteral("tauc"), [this]() { return tr("Tauc Bandgap"); },
                [this]() { return tr("Tauc Optical Bandgap Extrapolation"); },
                [this]() {
                    if (d_app)
                        d_app->showTaucPlotDialog();
                });
        addRibbonMenuAction(
                matToolsMenu, QStringLiteral("eis-nyquist"),
                [this]() { return tr("EIS Analyzer"); },
                [this]() { return tr("Electrochemical Impedance Spectroscopy Nyquist/Bode Fit"); },
                [this]() {
                    if (d_app)
                        d_app->showEISAnalyzerDialog();
                });
        matToolsMenu->addSeparator();
        addRibbonMenuAction(
                matToolsMenu, QStringLiteral("curvemath"), [this]() { return tr("Curve Math"); },
                [this]() { return tr("Arithmetic Operations Between Curves"); },
                [this]() {
                    if (d_app)
                        d_app->showSimpleCurveMathDialog();
                });
        addRibbonMenuAction(
                matToolsMenu, QStringLiteral("curve-fill"),
                [this]() { return tr("Fill Between"); },
                [this]() { return tr("Shade Area Between Two Curves"); },
                [this]() {
                    if (d_app)
                        d_app->showFillBetweenCurvesDialog();
                });
        addRibbonMenuAction(
                matToolsMenu, QStringLiteral("digitizer"), [this]() { return tr("Digitizer"); },
                [this]() { return tr("Extract Numerical Data from Graph Image"); },
                [this]() {
                    if (d_app)
                        d_app->showGraphDigitizerDialog();
                });

        createRibbonGroup(row, &gStats, false);
        addRibbonButton(
                gStats, QStringLiteral("stats-anova"), [this]() { return tr("Group ANOVA"); },
                [this]() { return tr("One-Way ANOVA & Tukey / Dunnett Post-Hoc Comparison"); },
                [this]() {
                    if (d_app)
                        d_app->showGroupComparisonDialog();
                });
        QMenu *hypMenu = nullptr;
        addRibbonMenuButton(
                gStats, QStringLiteral("stats-normality"),
                [this, kCaret]() { return tr("Hypothesis Tests") + kCaret; },
                [this]() {
                    return tr("Normality, Outlier, Two-Way ANOVA, Nonparametric & Method Agreement");
                },
                &hypMenu);
        addRibbonMenuAction(
                hypMenu, QStringLiteral("stats-normality"),
                [this]() { return tr("Normality Test"); },
                [this]() { return tr("Shapiro-Wilk / Anderson-Darling Normality Test"); },
                [this]() {
                    if (d_app)
                        d_app->showNormalityTestDialog();
                });
        addRibbonMenuAction(
                hypMenu, QStringLiteral("stats-outlier"), [this]() { return tr("Outlier Test"); },
                [this]() { return tr("Grubbs / Generalized ESD Outlier Detection"); },
                [this]() {
                    if (d_app)
                        d_app->showOutlierTestDialog();
                });
        addRibbonMenuAction(
                hypMenu, QStringLiteral("stats-twoway"), [this]() { return tr("Two-Way ANOVA"); },
                [this]() { return tr("Two-Way Factorial ANOVA & Interaction Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showTwoWayANOVAPlotDialog();
                });
        addRibbonMenuAction(
                hypMenu, QStringLiteral("stats-nonparam"),
                [this]() { return tr("Nonparametric"); },
                [this]() { return tr("Mann-Whitney / Wilcoxon / Kruskal-Wallis Tests"); },
                [this]() {
                    if (d_app)
                        d_app->showNonparametricTestsDialog();
                });
        hypMenu->addSeparator();
        addRibbonMenuAction(
                hypMenu, QStringLiteral("stats-paired"), [this]() { return tr("Paired Plot"); },
                [this]() { return tr("Paired Before-After Comparison Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showPairedComparisonPlotDialog();
                });
        addRibbonMenuAction(
                hypMenu, QStringLiteral("stats-bland-altman"),
                [this]() { return tr("Bland-Altman Plot"); },
                [this]() { return tr("Bland-Altman Method Agreement Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showBlandAltmanPlotDialog();
                });
        addRibbonMenuAction(
                hypMenu, QStringLiteral("stats-bablok"),
                [this]() { return tr("Passing-Bablok Fit"); },
                [this]() { return tr("Passing-Bablok & Deming Method Comparison Regression"); },
                [this]() {
                    if (d_app)
                        d_app->showPassingBablokDemingDialog();
                });

        QMenu *relMultiMenu = nullptr;
        addRibbonMenuButton(
                gStats, QStringLiteral("stats-weibull"),
                [this, kCaret]() { return tr("Reliability") + kCaret; },
                [this]() {
                    return tr("Weibull, Kaplan-Meier, ROC, PCA, Correlation & Clustering");
                },
                &relMultiMenu);
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-weibull"),
                [this]() { return tr("Weibull Plot"); },
                [this]() { return tr("2- and 3-Parameter Weibull Reliability Probability Plot"); },
                [this]() {
                    if (d_app)
                        d_app->showWeibullPlotDialog();
                });
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-survival"),
                [this]() { return tr("Kaplan-Meier Survival"); },
                [this]() { return tr("Kaplan-Meier Survival Curve & Log-Rank Test"); },
                [this]() {
                    if (d_app)
                        d_app->showKaplanMeierDialog();
                });
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-roc"), [this]() { return tr("ROC Curve"); },
                [this]() { return tr("Receiver Operating Characteristic (ROC) Curve & AUC"); },
                [this]() {
                    if (d_app)
                        d_app->showROCCurveDialog();
                });
        relMultiMenu->addSeparator();
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-pca"), [this]() { return tr("PCA Biplot"); },
                [this]() { return tr("Principal Component Analysis (PCA) & Biplot"); },
                [this]() {
                    if (d_app)
                        d_app->showPCABiplotDialog();
                });
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-corr"), [this]() { return tr("Correlation"); },
                [this]() { return tr("Pearson / Spearman Correlation Matrix Heatmap"); },
                [this]() {
                    if (d_app)
                        d_app->showCorrelationMatrixDialog();
                });
        addRibbonMenuAction(
                relMultiMenu, QStringLiteral("stats-cluster"),
                [this]() { return tr("Clustering"); },
                [this]() { return tr("K-Means & Hierarchical Cluster Analysis"); },
                [this]() {
                    if (d_app)
                        d_app->showClusteringDialog();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Statistics & MatSci"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 6: Style & Export
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gPresets = nullptr;
        QHBoxLayout *gExport = nullptr;
        QHBoxLayout *gStyle = nullptr;

        createRibbonGroup(row, &gPresets, true);
        addRibbonButton(
                gPresets, QStringLiteral("preset-isij"),
                [this]() { return tr("ISIJ 85mm Fig"); },
                [this]() { return tr("Apply ISIJ 1-Column 85x85 mm Journal Preset (600 DPI)"); },
                [this]() {
                    if (d_app)
                        d_app->applyStyleISIJJournal();
                },
                false, true);
        addRibbonButton(
                gPresets, QStringLiteral("preset-slide"),
                [this]() { return tr("28pt Slide"); },
                [this]() { return tr("Apply Presentation Slide 28pt Arial Preset"); },
                [this]() {
                    if (d_app)
                        d_app->applyStylePresentationSlide();
                },
                false, true);
        QMenu *customPresetMenu = nullptr;
        addRibbonMenuButton(
                gPresets, QStringLiteral("page-setup"),
                [this]() { return tr("Presets..."); },
                [this]() { return tr("Apply User Presets or Edit / Add Custom Graph Style Presets"); },
                &customPresetMenu, true);
        if (customPresetMenu) {
            connect(customPresetMenu, &QMenu::aboutToShow, this, [this, customPresetMenu]() {
                customPresetMenu->clear();
                const QVector<MultiLayer::GraphStylePresetSpec> presets =
                        MultiLayer::listStylePresets();
                for (const auto &sp : presets) {
                    const QString pid = sp.id;
                    const QString dispName = sp.builtIn
                            ? QCoreApplication::translate("GraphInspectorDock", sp.name.toUtf8().constData())
                            : sp.name;
                    QAction *act = customPresetMenu->addAction(
                            IconLoader::duotoneIcon(
                                    sp.builtIn ? QStringLiteral("preset-isij")
                                               : QStringLiteral("page-setup"),
                                    16),
                            dispName);
                    connect(act, &QAction::triggered, this, [this, pid]() {
                        if (!d_app)
                            return;
                        if (MultiLayer *ml = qobject_cast<MultiLayer *>(
                                    d_app->d_workspace.activeSubWindow())) {
                            ml->applyStylePresetById(pid);
                            d_app->modifiedProject();
                            if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                                insp->refreshAll();
                        }
                    });
                }
                customPresetMenu->addSeparator();
                QAction *mgrAct = customPresetMenu->addAction(
                        IconLoader::duotoneIcon(QStringLiteral("page-setup"), 16),
                        tr("Manage Style Presets..."));
                connect(mgrAct, &QAction::triggered, this, [this]() {
                    if (d_app)
                        d_app->showStylePresetManagerDialog();
                });
            });
        }
        d_btn_lock_aspect = addRibbonButton(
                gPresets, QStringLiteral("lock"),
                [this]() { return tr("Lock Aspect"); },
                [this]() {
                    return tr("Lock or Unlock Active Graph Aspect Ratio on Window Resize");
                },
                [this]() {
                    if (!d_app || !d_btn_lock_aspect)
                        return;
                    if (MultiLayer *ml = qobject_cast<MultiLayer *>(
                                d_app->d_workspace.activeSubWindow())) {
                        const QString before = ml->captureUndoSnapshot();
                        ml->setKeepAspectRatio(d_btn_lock_aspect->isChecked());
                        ml->pushUndoSnapshot(tr("Toggle Lock Aspect Ratio"), before);
                        d_app->modifiedProject();
                        if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                            insp->refreshAll();
                    }
                    syncCheckableStates();
                },
                true);
        d_btn_lock_aspect->setObjectName(QStringLiteral("RibbonLockAspectRatioBtn"));

        createRibbonGroup(row, &gExport, true);
        addRibbonButton(
                gExport, QStringLiteral("copy-ole"), [this]() { return tr("Copy OLE"); },
                [this]() { return tr("Copy Editable OLE Graph Object for PowerPoint / Word"); },
                [this]() {
                    if (d_app)
                        d_app->copyGraphAsOleObject();
                });
        addRibbonButton(
                gExport, QStringLiteral("export-image"), [this]() { return tr("Export"); },
                [this]() { return tr("Export Graph to PNG / TIFF / SVG / EMF"); },
                [this]() {
                    if (d_app)
                        d_app->exportGraph();
                });
        QMenu *moreExportMenu = nullptr;
        addRibbonMenuButton(
                gExport, QStringLiteral("export-pdf"),
                [this, kCaret]() { return tr("More Export") + kCaret; },
                [this]() { return tr("Copy High-DPI Image, Export PDF, or Export Python Script"); },
                &moreExportMenu);
        addRibbonMenuAction(
                moreExportMenu, QStringLiteral("copy-page-image"),
                [this]() { return tr("Copy High-DPI Image"); },
                [this]() { return tr("Copy High-Resolution Bitmap of Graph Page to Clipboard"); },
                [this]() {
                    if (d_app)
                        d_app->copyPageAsHighDpiImage();
                });
        addRibbonMenuAction(
                moreExportMenu, QStringLiteral("export-pdf"),
                [this]() { return tr("Export PDF"); },
                [this]() { return tr("Export Vector PDF Document"); },
                [this]() {
                    if (d_app)
                        d_app->exportPDF();
                });
        addRibbonMenuAction(
                moreExportMenu, QStringLiteral("code-export"),
                [this]() { return tr("Export Python"); },
                [this]() { return tr("Generate Reproducible Python Script from Active Graph"); },
                [this]() {
                    if (d_app)
                        d_app->exportActiveGraphToPython();
                });

        createRibbonGroup(row, &gStyle, false);
        QMenu *framePageMenu = nullptr;
        addRibbonMenuButton(
                gStyle, QStringLiteral("frame-box-inward"),
                [this, kCaret]() { return tr("Frame & Page") + kCaret; },
                [this]() {
                    return tr("Frame Style, Page Size & DPI, Trim Page, Square 70% & Color Palette");
                },
                &framePageMenu);
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("frame-box-inward"),
                [this]() { return tr("Box Inward"); },
                [this]() { return tr("Apply Full Box Frame with Inward Ticks"); },
                [this]() {
                    if (d_app)
                        d_app->applyFrameBoxInward();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("frame-open-l"),
                [this]() { return tr("Open-L Frame"); },
                [this]() { return tr("Apply Open Bottom-Left L-Frame"); },
                [this]() {
                    if (d_app)
                        d_app->applyFrameOpenL();
                });
        framePageMenu->addSeparator();
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("page-setup"),
                [this]() { return tr("Size & DPI"); },
                [this]() { return tr("Configure Physical Page Dimensions, Units & DPI"); },
                [this]() {
                    if (d_app)
                        d_app->showPageSetupDialog();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("fit-page"), [this]() { return tr("Trim Page"); },
                [this]() { return tr("Crop Page Tightly Around Plot Layers"); },
                [this]() {
                    if (d_app)
                        d_app->fitPageToLayers();
                });
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("aspect-square"),
                [this]() { return tr("Square 70%"); },
                [this]() { return tr("Apply 70% Square Inner Canvas Frame"); },
                [this]() {
                    if (!d_app)
                        return;
                    if (MultiLayer *ml =
                                qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                        const QString before = ml->captureUndoSnapshot();
                        ml->applySquareCanvasFrame70Percent();
                        ml->pushUndoSnapshot(tr("Apply 70% Square Canvas Frame"), before);
                        d_app->modifiedProject();
                    }
                });
        framePageMenu->addSeparator();
        addRibbonMenuAction(
                framePageMenu, QStringLiteral("color-palette"),
                [this]() { return tr("Palette"); },
                [this]() { return tr("Apply Publication Color Palette to Curves"); },
                [this]() {
                    if (d_app)
                        d_app->showColorPaletteEditorDialog();
                });

        QMenu *formatBatchMenu = nullptr;
        addRibbonMenuButton(
                gStyle, QStringLiteral("format-copy"),
                [this, kCaret]() { return tr("Format & Batch") + kCaret; },
                [this]() { return tr("Copy/Paste Graph Format & Batch Plot"); }, &formatBatchMenu);
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("format-copy"),
                [this]() { return tr("Copy Format"); },
                [this]() { return tr("Copy Visual Style & Axes Format of Active Graph"); },
                [this]() {
                    if (d_app)
                        d_app->copyGraphFormat();
                });
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("format-paste"),
                [this]() { return tr("Paste Format"); },
                [this]() { return tr("Paste Copied Visual Style onto Active Graph"); },
                [this]() {
                    if (d_app)
                        d_app->pasteGraphFormat();
                });
        formatBatchMenu->addSeparator();
        addRibbonMenuAction(
                formatBatchMenu, QStringLiteral("batch-plot"),
                [this]() { return tr("Batch Plot"); },
                [this]() { return tr("Clone Active Graph Template Across Multiple Worksheets"); },
                [this]() {
                    if (d_app)
                        d_app->showBatchPlotDialog();
                });

        row->addStretch(1);
        addTabPage([this]() { return tr("Style & Export"); }, page);
    }

    // ------------------------------------------------------------------------
    // Tab 7: Python & Tools
    // ------------------------------------------------------------------------
    {
        QHBoxLayout *row = nullptr;
        QWidget *page = createRibbonScrollPage(&row);
        QHBoxLayout *gPython = nullptr;
        QHBoxLayout *gSettings = nullptr;

        createRibbonGroup(row, &gPython, true);
        addRibbonButton(
                gPython, QStringLiteral("python-console"),
                [this]() { return tr("Python Console"); },
                [this]() { return tr("Open Interactive In-Process Python Console"); },
                [this]() { emit openPythonConsoleRequested(); });
        addRibbonButton(
                gPython, QStringLiteral("python-run"),
                [this]() { return tr("Run Python Script"); },
                [this]() { return tr("Select and Execute a .py Automation Script"); },
                [this]() { emit runPythonScriptRequested(); });
        addRibbonButton(
                gPython, QStringLiteral("code-export"),
                [this]() { return tr("Export Python"); },
                [this]() { return tr("Generate Reproducible Python Script from Active Graph"); },
                [this]() {
                    if (d_app)
                        d_app->exportActiveGraphToPython();
                });

        createRibbonGroup(row, &gSettings, false);
        addRibbonButton(
                gSettings, QStringLiteral("settings"), [this]() { return tr("Preferences"); },
                [this]() { return tr("Open Application Preferences Dialog"); },
                [this]() {
                    if (d_app)
                        d_app->showPreferencesDialog();
                });
        addRibbonButton(
                gSettings, QStringLiteral("update-check"),
                [this]() { return tr("Check for Updates"); },
                [this]() { return tr("Check GitHub Releases for New ToriGraph Versions"); },
                [this]() { emit checkForUpdatesRequested(); });
        d_btn_classic_toolbars = addRibbonButton(
                gSettings, QStringLiteral("mdi-view"),
                [this]() { return tr("Classic Toolbars"); },
                [this]() { return tr("Show or Hide Legacy Classic Icon Toolbars"); },
                [this]() { setClassicToolbarsVisible(!d_classic_toolbars_visible); }, true);

        row->addStretch(1);
        addTabPage([this]() { return tr("Python & Tools"); }, page);
    }
}

void ModernRibbonBar::refreshButtonIcons()
{
    for (const RibbonButtonBinding &b : d_button_bindings) {
        if (b.button && !b.iconId.isEmpty())
            b.button->setIcon(IconLoader::duotoneIcon(b.iconId, 20, false));
    }
    for (const RibbonMenuActionBinding &ab : d_menu_action_bindings) {
        if (ab.action && !ab.iconId.isEmpty())
            ab.action->setIcon(IconLoader::duotoneIcon(ab.iconId, 18, false));
    }
    if (d_btn_cmd_palette)
        d_btn_cmd_palette->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("search-command"), 16, false));
    if (d_btn_layout_tabbed)
        d_btn_layout_tabbed->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("tab-view"), 16, false));
    if (d_btn_layout_split)
        d_btn_layout_split->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("split-view"), 16, false));
    if (d_btn_layout_mdi)
        d_btn_layout_mdi->setIcon(IconLoader::duotoneIcon(QStringLiteral("mdi-view"), 16, false));
    if (d_btn_focus_mode)
        d_btn_focus_mode->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("focus-hud"), 16, false));
    if (d_btn_lang_switch)
        d_btn_lang_switch->setIcon(
                IconLoader::duotoneIcon(QStringLiteral("language-ja-en"), 16, false));
}

void ModernRibbonBar::retranslateUi()
{
    if (d_shutting_down)
        return;

    refreshButtonIcons();

    if (d_tab_bar) {
        for (int i = 0; i < d_tab_title_fns.size() && i < d_tab_bar->count(); ++i)
            d_tab_bar->setTabText(i, escapeTabMnemonicText(d_tab_title_fns.at(i)()));
    }

    for (const RibbonButtonBinding &bb : d_button_bindings) {
        if (!bb.button)
            continue;
        if (bb.textFn)
            bb.button->setText(escapeTabMnemonicText(bb.textFn()));
        if (bb.tooltipFn)
            bb.button->setToolTip(bb.tooltipFn());
    }

    for (const RibbonMenuActionBinding &ab : d_menu_action_bindings) {
        if (!ab.action)
            continue;
        if (ab.textFn)
            ab.action->setText(escapeTabMnemonicText(ab.textFn()));
        if (ab.tooltipFn)
            ab.action->setToolTip(ab.tooltipFn());
    }

    if (d_btn_cmd_palette) {
        d_btn_cmd_palette->setText(tr("Ctrl+K Search Commands..."));
        d_btn_cmd_palette->setToolTip(tr("Open Fuzzy-Search Command Palette (Ctrl+K)"));
    }
    if (d_btn_layout_tabbed) {
        d_btn_layout_tabbed->setText(tr("Tabbed"));
        d_btn_layout_tabbed->setToolTip(tr("Tabbed Workspace View"));
    }
    if (d_btn_layout_split) {
        d_btn_layout_split->setText(tr("Split View"));
        d_btn_layout_split->setToolTip(tr("Side-by-Side Split View (40% Worksheet / 60% Graph)"));
    }
    if (d_btn_layout_mdi) {
        d_btn_layout_mdi->setText(tr("Free MDI"));
        d_btn_layout_mdi->setToolTip(tr("Free Subwindow MDI Arrangement"));
    }
    if (d_btn_focus_mode) {
        d_btn_focus_mode->setText(tr("Focus (F11)"));
        d_btn_focus_mode->setToolTip(tr("Enter Full-Canvas Focus HUD Mode (F11)"));
    }
    if (d_btn_lang_switch) {
        d_btn_lang_switch->setText(tr("Language"));
        d_btn_lang_switch->setToolTip(tr("Switch Interface Language (Japanese / English)"));
    }

    updateActiveContext(d_last_active_window.data());

    if (d_command_palette)
        d_command_palette->retranslateUi();
    if (d_focus_hud_overlay)
        d_focus_hud_overlay->retranslateUi();

    updateHeaderResponsiveness();
    updateTabStackHeight();
}

void ModernRibbonBar::syncCheckableStates()
{
    if (d_shutting_down || !d_app)
        return;

    if (d_btn_auto_recalc) {
        QSignalBlocker blocker(d_btn_auto_recalc);
        const bool isAuto = (RecalcManager::instance(d_app)->recalcMode() == RecalcManager::Auto);
        d_btn_auto_recalc->setChecked(isAuto);
    }
    if (d_act_explorer) {
        QSignalBlocker blocker(d_act_explorer);
        d_act_explorer->setChecked(!d_app->explorerWindow.isHidden());
    }
    if (d_act_results_log) {
        QSignalBlocker blocker(d_act_results_log);
        d_act_results_log->setChecked(!d_app->logWindow.isHidden());
    }
    if (d_btn_lock_aspect) {
        QSignalBlocker blocker(d_btn_lock_aspect);
        MultiLayer *activeMl =
                qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow());
        d_btn_lock_aspect->setEnabled(activeMl != nullptr);
        d_btn_lock_aspect->setChecked(activeMl && activeMl->keepAspectRatio());
    }
    if (d_btn_classic_toolbars) {
        QSignalBlocker blocker(d_btn_classic_toolbars);
        const bool anyClassicVisible =
                (d_app->file_tools && !d_app->file_tools->isHidden())
                || (d_app->edit_tools && !d_app->edit_tools->isHidden())
                || (d_app->table_tools && !d_app->table_tools->isHidden())
                || (d_app->plot_tools && !d_app->plot_tools->isHidden())
                || (d_app->graph_tools && !d_app->graph_tools->isHidden())
                || (d_app->matrix_plot_tools && !d_app->matrix_plot_tools->isHidden())
                || (d_app->graph_3D_tools && !d_app->graph_3D_tools->isHidden());
        d_classic_toolbars_visible = anyClassicVisible;
        d_btn_classic_toolbars->setChecked(d_classic_toolbars_visible);
    }

    const GraphInspectorDock *inspDock = d_app->graphInspectorDock();
    const bool inspectorChecked = inspDock && !inspDock->isHidden() && !inspDock->isCollapsedRail();
    if (d_act_inspector) {
        QSignalBlocker blocker(d_act_inspector);
        d_act_inspector->setChecked(inspectorChecked);
    }
}

void ModernRibbonBar::updateActiveContext(MyWidget *activeWindow)
{
    if (d_shutting_down)
        return;

    d_last_active_window = activeWindow;

    if (d_context_pill) {
        QString label;
        QString pillStyle;
        if (qobject_cast<Table *>(activeWindow)) {
            label = tr("Worksheet");
            pillStyle = QStringLiteral(
                    "background-color: #DBEAFE; color: #1D4ED8; border: 1px solid #93C5FD;");
        } else if (qobject_cast<MultiLayer *>(activeWindow)) {
            label = tr("2D Graph");
            pillStyle = QStringLiteral(
                    "background-color: #E0E7FF; color: #3730A3; border: 1px solid #A5B4FC;");
        } else if (qobject_cast<Graph3D *>(activeWindow)) {
            label = tr("3D Graph");
            pillStyle = QStringLiteral(
                    "background-color: #EDE9FE; color: #5B21B6; border: 1px solid #C4B5FD;");
        } else if (qobject_cast<Matrix *>(activeWindow)) {
            label = tr("Matrix");
            pillStyle = QStringLiteral(
                    "background-color: #D1FAE5; color: #065F46; border: 1px solid #6EE7B7;");
        } else if (qobject_cast<Note *>(activeWindow)) {
            label = tr("Notes");
            pillStyle = QStringLiteral(
                    "background-color: #FEF3C7; color: #92400E; border: 1px solid #FCD34D;");
        } else {
            label = tr("Project");
            pillStyle = QStringLiteral(
                    "background-color: #E2E8F0; color: #334155; border: 1px solid #CBD5E1;");
        }

        d_context_pill->setText(label);
        d_context_pill->setStyleSheet(
                QStringLiteral("QLabel#RibbonContextBadge { %1 border-radius: 10px; "
                               "padding: 2px 10px; font-family: 'Segoe UI', 'Yu Gothic UI', "
                               "'Meiryo UI', sans-serif; font-size: 10.5pt; font-weight: 700; }")
                        .arg(pillStyle));
        updateHeaderResponsiveness();
    }

    syncCheckableStates();
    if (d_focus_hud_overlay)
        d_focus_hud_overlay->refreshActiveContext();
}

void ModernRibbonBar::setWorkspaceLayoutMode(WorkspaceLayoutMode mode)
{
    if (d_shutting_down)
        return;

    d_layout_mode = mode;

    if (d_layout_btn_group) {
        if (QAbstractButton *btn = d_layout_btn_group->button(int(mode))) {
            QSignalBlocker blocker(d_layout_btn_group);
            btn->setChecked(true);
        }
    }

    if (d_app) {
        d_updating_layout_mode = true;
        auto unmaximizeCurrentFolderWindows = [this]() {
            if (Folder *curFolder = d_app->currentFolder()) {
                const QList<MyWidget *> folderWins = curFolder->windowsList();
                for (MyWidget *mw : folderWins) {
                    if (!mw || mw->status() == MyWidget::Hidden)
                        continue;
                    if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                        continue;
                    if (mw->isHidden())
                        continue;
                    if (mw->isMaximized()) {
                        mw->setWindowState(mw->windowState() & ~Qt::WindowMaximized);
                        mw->showNormal();
                    }
                }
            }
        };

        switch (mode) {
        case LayoutTabbed: {
            d_app->setTabbedWorkspaceView(true);
            d_app->d_workspace.setTabsClosable(true);
            d_app->d_workspace.setTabsMovable(true);
            if (QTabBar *tb = d_app->d_workspace.findChild<QTabBar *>()) {
                const QList<QMdiSubWindow *> creationOrder =
                        d_app->d_workspace.subWindowList(QMdiArea::CreationOrder);
                for (int i = 0; i < tb->count(); ++i) {
                    MyWidget *mw = (i < creationOrder.size())
                            ? qobject_cast<MyWidget *>(creationOrder.at(i))
                            : nullptr;
                    const bool hideTab = mw
                            && (d_app->hiddenWindows.contains(mw)
                                || d_app->outWindows.contains(mw)
                                || mw->status() == MyWidget::Hidden);
                    tb->setTabVisible(i, !hideTab);
                }
            }
            if (QMdiSubWindow *activeSw = d_app->d_workspace.activeSubWindow()) {
                if (MyWidget *mw = qobject_cast<MyWidget *>(activeSw)) {
                    if (!mw->isHidden() && mw->status() != MyWidget::Hidden
                        && !d_app->hiddenWindows.contains(mw)
                        && !d_app->outWindows.contains(mw) && !mw->isMaximized()) {
                        mw->showMaximized();
                    }
                }
            }
            break;
        }

        case LayoutSplitTableGraph:
            d_app->setTabbedWorkspaceView(false);
            unmaximizeCurrentFolderWindows();
            applySplitWorkspaceGeometry();
            break;

        case LayoutFreeMdi:
            d_app->setTabbedWorkspaceView(false);
            unmaximizeCurrentFolderWindows();
            if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
                hsb->setValue(0);
            if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
                vsb->setValue(0);
            if (Folder *curFolder = d_app->currentFolder()) {
                const bool focusActive = d_focus_hud_mode
                        || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());
                const int minTopY = focusActive ? 44 : 8;
                int cascadeIdx = 0;
                for (MyWidget *mw : curFolder->windowsList()) {
                    if (!mw || mw->status() == MyWidget::Hidden || mw->isHidden())
                        continue;
                    if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                        continue;
                    if (mw->x() < 8 || mw->y() < minTopY) {
                        const int offX = 12 + (cascadeIdx % 6) * 28;
                        const int offY = minTopY + (cascadeIdx % 6) * 22;
                        mw->move(qMax(offX, mw->x()), qMax(offY, mw->y()));
                        ++cascadeIdx;
                    }
                }
            }
            if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
                hsb->setValue(0);
            if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
                vsb->setValue(0);
            break;
        }
        d_updating_layout_mode = false;
    }

    if (d_focus_hud_overlay && d_focus_hud_mode)
        d_focus_hud_overlay->repositionOverlayWidgets();

    emit workspaceLayoutModeChanged(mode);
}

void ModernRibbonBar::syncWorkspaceModeButtons(WorkspaceLayoutMode mode)
{
    if (d_shutting_down || d_updating_layout_mode || d_applying_split_geometry)
        return;

    d_layout_mode = mode;
    if (d_layout_btn_group) {
        if (QAbstractButton *btn = d_layout_btn_group->button(int(mode))) {
            QSignalBlocker blocker(d_layout_btn_group);
            btn->setChecked(true);
        }
    }
}

void ModernRibbonBar::syncWorkspaceModeButtons(bool tabbed)
{
    if (d_shutting_down || d_updating_layout_mode || d_applying_split_geometry)
        return;

    if (tabbed) {
        syncWorkspaceModeButtons(LayoutTabbed);
    } else if (d_layout_mode == LayoutTabbed) {
        syncWorkspaceModeButtons(LayoutFreeMdi);
    }
}

bool ModernRibbonBar::setWorkspaceLayoutModeByName(const QString &modeName)
{
    const QString m = modeName.trimmed().toLower();
    if (m == QLatin1String("tabbed") || m == QLatin1String("tabs")) {
        setWorkspaceLayoutMode(LayoutTabbed);
        return true;
    }
    if (m == QLatin1String("split") || m == QLatin1String("split_table_graph")
        || m == QLatin1String("split-view")) {
        setWorkspaceLayoutMode(LayoutSplitTableGraph);
        return true;
    }
    if (m == QLatin1String("mdi") || m == QLatin1String("free") || m == QLatin1String("free_mdi")
        || m == QLatin1String("subwindow")) {
        setWorkspaceLayoutMode(LayoutFreeMdi);
        return true;
    }
    return false;
}

QString ModernRibbonBar::workspaceLayoutModeName() const
{
    switch (d_layout_mode) {
    case LayoutTabbed:
        return QStringLiteral("tabbed");
    case LayoutSplitTableGraph:
        return QStringLiteral("split");
    case LayoutFreeMdi:
        return QStringLiteral("mdi");
    }
    return QStringLiteral("tabbed");
}

void ModernRibbonBar::applySplitWorkspaceGeometry()
{
    if (d_shutting_down || !d_app || d_applying_split_geometry)
        return;

    d_applying_split_geometry = true;

    if (d_app->d_workspace.viewMode() != QMdiArea::SubWindowView)
        d_app->setTabbedWorkspaceView(false);

    if (d_app->layout())
        d_app->layout()->activate();

    QWidget *vp = d_app->d_workspace.viewport();
    QRect vpRect = vp ? vp->rect() : d_app->d_workspace.rect();
    if (vpRect.width() < 120 || vpRect.height() < 80) {
        const QSize fallback = d_app->size().expandedTo(QSize(800, 560));
        vpRect = QRect(0, 0, qMax(320, fallback.width() - 16), qMax(240, fallback.height() - 110));
    }

    QSet<MyWidget *> eligibleWins;
    if (Folder *curFolder = d_app->currentFolder()) {
        const QList<MyWidget *> folderWins = curFolder->windowsList();
        for (MyWidget *mw : folderWins) {
            if (!mw || mw->status() == MyWidget::Hidden)
                continue;
            if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                continue;
            if (mw->isHidden())
                continue;
            eligibleWins.insert(mw);
        }
    }

    QMdiSubWindow *activeSub = d_app->d_workspace.activeSubWindow();
    MyWidget *activeMw = qobject_cast<MyWidget *>(activeSub);
    if (activeMw && !eligibleWins.contains(activeMw))
        activeMw = nullptr;

    const QList<QMdiSubWindow *> ordered =
            d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);

    Table *targetTable = qobject_cast<Table *>(activeMw);
    MyWidget *targetPlot = qobject_cast<MultiLayer *>(activeMw);
    if (!targetPlot)
        targetPlot = qobject_cast<Graph3D *>(activeMw);

    if (!targetTable && targetPlot) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(targetPlot)) {
            if (Table *srcTbl = sourceTableFromMultiLayer(ml)) {
                if (eligibleWins.contains(srcTbl))
                    targetTable = srcTbl;
            }
        }
    }

    if (!targetTable) {
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (Table *t = qobject_cast<Table *>(ordered.at(i))) {
                if (eligibleWins.contains(t)) {
                    targetTable = t;
                    break;
                }
            }
        }
    }

    if (!targetPlot && targetTable) {
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(ordered.at(i))) {
                if (eligibleWins.contains(ml) && multiLayerUsesTable(ml, targetTable)) {
                    targetPlot = ml;
                    break;
                }
            }
        }
    }

    if (!targetPlot) {
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(ordered.at(i))) {
                if (eligibleWins.contains(ml)) {
                    targetPlot = ml;
                    break;
                }
            }
            if (Graph3D *g3 = qobject_cast<Graph3D *>(ordered.at(i))) {
                if (eligibleWins.contains(g3)) {
                    targetPlot = g3;
                    break;
                }
            }
        }
    }

    auto prepareTileWindow = [](MyWidget *w) {
        if (!w || w->isHidden())
            return;
        if (w->isMaximized() || w->isMinimized()) {
            w->setWindowState(w->windowState() & ~(Qt::WindowMaximized | Qt::WindowMinimized));
            w->showNormal();
        }
    };

    for (MyWidget *mw : eligibleWins) {
        if (mw && mw != targetTable && mw != targetPlot && mw->isMaximized())
            prepareTileWindow(mw);
    }

    const bool focusHudActive =
            d_focus_hud_mode || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());
    const int tileTopY = focusHudActive ? 44 : 0;
    const int leftW = qMax(120, qRound(vpRect.width() * 0.40));
    const int rightW = qMax(120, vpRect.width() - leftW);
    const int fullH = qMax(100, vpRect.height() - tileTopY);

    if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
        hsb->setValue(0);
    if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
        vsb->setValue(0);

    if (targetTable && targetPlot) {
        prepareTileWindow(targetTable);
        prepareTileWindow(targetPlot);
        if (targetTable->minimumWidth() > leftW)
            targetTable->setMinimumWidth(leftW);
        targetTable->setGeometry(0, tileTopY, leftW, fullH);
        const int actualLeftW = qBound(120, targetTable->geometry().width(), qMax(120, vpRect.width() - 120));
        if (actualLeftW != targetTable->geometry().width())
            targetTable->setGeometry(0, tileTopY, actualLeftW, fullH);
        const int actualRightW = qMax(120, vpRect.width() - actualLeftW);
        targetPlot->setGeometry(actualLeftW, tileTopY, actualRightW, fullH);
        if (activeMw == targetTable) {
            targetPlot->raise();
            targetTable->raise();
        } else {
            targetTable->raise();
            targetPlot->raise();
        }
    } else if (targetTable) {
        prepareTileWindow(targetTable);
        targetTable->setGeometry(0, tileTopY, leftW, fullH);
        targetTable->raise();
    } else if (targetPlot) {
        prepareTileWindow(targetPlot);
        targetPlot->setGeometry(leftW, tileTopY, rightW, fullH);
        targetPlot->raise();
    }

    if (activeMw && activeMw != targetTable && activeMw != targetPlot) {
        prepareTileWindow(activeMw);
        if (activeMw->width() < 120 || activeMw->height() < 90) {
            const int fw = qBound(220, int(vpRect.width() * 0.55), qMax(220, vpRect.width() - 32));
            const int fh = qBound(160, int(fullH * 0.60), qMax(160, fullH - 32));
            activeMw->setGeometry(qMax(16, (vpRect.width() - fw) / 2),
                                  qMax(tileTopY + 12, tileTopY + (fullH - fh) / 2), fw, fh);
        } else if (focusHudActive && activeMw->y() < tileTopY) {
            activeMw->move(activeMw->x(), tileTopY);
        }
        activeMw->raise();
    }

    d_applying_split_geometry = false;
    if (d_focus_hud_overlay && focusHudActive)
        d_focus_hud_overlay->repositionOverlayWidgets();
}

void ModernRibbonBar::setFocusHudMode(bool enabled)
{
    if (d_shutting_down)
        return;

    if (d_focus_hud_mode == enabled)
        return;

    d_focus_hud_mode = enabled;

    if (enabled) {
        d_saved_focus_visibility.clear();

        if (d_app) {
            QToolBar *ribbonTb = d_app->modernRibbonToolBar();
            const QList<QToolBar *> toolbars = d_app->findChildren<QToolBar *>();
            for (QToolBar *tb : toolbars) {
                if (!tb || tb == ribbonTb
                    || tb->objectName() == QStringLiteral("ModernRibbonToolBar"))
                    continue;
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(tb), !tb->isHidden()));
                tb->hide();
            }

            const QList<QDockWidget *> docks = d_app->findChildren<QDockWidget *>();
            for (QDockWidget *dw : docks) {
                if (!dw)
                    continue;
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(dw), !dw->isHidden()));
                dw->hide();
            }

            if (QStatusBar *sb = d_app->statusBar()) {
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(sb), !sb->isHidden()));
                sb->hide();
            }
        }

        if (QToolBar *rtb = d_app ? d_app->modernRibbonToolBar() : nullptr)
            rtb->setVisible(!enabled);
        this->setVisible(!enabled);

        if (d_app) {
            if (QWidget *cw = d_app->centralWidget()) {
                if (cw->layout())
                    cw->layout()->activate();
            }
            if (d_app->layout())
                d_app->layout()->activate();
        }

        if (d_focus_hud_overlay)
            d_focus_hud_overlay->setOverlayActive(true);
    } else {
        if (d_focus_hud_overlay)
            d_focus_hud_overlay->setOverlayActive(false);

        if (QToolBar *rtb = d_app ? d_app->modernRibbonToolBar() : nullptr)
            rtb->setVisible(!enabled);
        this->setVisible(!enabled);

        for (const auto &saved : d_saved_focus_visibility) {
            if (saved.first)
                saved.first->setVisible(saved.second);
        }
        d_saved_focus_visibility.clear();

        if (d_app) {
            if (QWidget *cw = d_app->centralWidget()) {
                if (cw->layout())
                    cw->layout()->activate();
            }
            if (d_app->layout())
                d_app->layout()->activate();
        }

        syncCheckableStates();
    }

    if (d_layout_mode == LayoutSplitTableGraph) {
        applySplitWorkspaceGeometry();
    } else if (enabled && d_layout_mode == LayoutFreeMdi && d_app) {
        if (Folder *curFolder = d_app->currentFolder()) {
            for (MyWidget *mw : curFolder->windowsList()) {
                if (!mw || mw->status() == MyWidget::Hidden || mw->isHidden())
                    continue;
                if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                    continue;
                if (!mw->isMaximized() && mw->y() < 44)
                    mw->move(mw->x(), 44);
            }
        }
        if (d_focus_hud_overlay)
            d_focus_hud_overlay->repositionOverlayWidgets();
    }

    emit focusHudModeChanged(d_focus_hud_mode);
}

void ModernRibbonBar::setClassicToolbarsVisible(bool visible)
{
    if (d_shutting_down)
        return;

    d_classic_toolbars_visible = visible;
    if (d_btn_classic_toolbars && d_btn_classic_toolbars->isChecked() != visible) {
        QSignalBlocker blocker(d_btn_classic_toolbars);
        d_btn_classic_toolbars->setChecked(visible);
    }

    if (d_app && !d_focus_hud_mode) {
        QToolBar *legacyBars[] = { d_app->file_tools,        d_app->edit_tools,
                                   d_app->table_tools,       d_app->plot_tools,
                                   d_app->graph_tools,       d_app->matrix_plot_tools,
                                   d_app->graph_3D_tools };
        if (!visible) {
            for (QToolBar *tb : legacyBars) {
                if (tb)
                    tb->hide();
            }
        } else {
            if (d_app->file_tools) {
                if (QToolBar *rtb = d_app->modernRibbonToolBar())
                    d_app->insertToolBar(d_app->file_tools, rtb);
                d_app->insertToolBarBreak(d_app->file_tools);
                d_app->file_tools->show();
            }
            if (d_app->edit_tools)
                d_app->edit_tools->show();
            MyWidget *aw = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
            d_app->customToolBars(aw);
            if (qobject_cast<Table *>(aw)) {
                if (d_app->table_tools)
                    d_app->table_tools->show();
                if (d_app->plot_tools)
                    d_app->plot_tools->show();
            } else if (qobject_cast<MultiLayer *>(aw)) {
                if (d_app->graph_tools)
                    d_app->graph_tools->show();
                if (d_app->plot_tools)
                    d_app->plot_tools->show();
            } else if (qobject_cast<Matrix *>(aw)) {
                if (d_app->matrix_plot_tools)
                    d_app->matrix_plot_tools->show();
            } else if (qobject_cast<Graph3D *>(aw)) {
                if (d_app->graph_3D_tools)
                    d_app->graph_3D_tools->show();
            }
        }
    }

    emit classicToolbarsToggled(visible);
}

void ModernRibbonBar::triggerPlot2D(int curveType)
{
    if (d_shutting_down || !d_app)
        return;

    Table *tbl = qobject_cast<Table *>(d_app->d_workspace.activeSubWindow());
    if (!tbl && d_focus_hud_mode && d_focus_hud_overlay) {
        if (d_focus_hud_overlay->isTableDrawerVisible() && d_focus_hud_overlay->tableDrawer()) {
            tbl = d_focus_hud_overlay->tableDrawer()->currentTable();
        } else if (MultiLayer *activeMl =
                           qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
            QSet<QObject *> ignored;
            for (MyWidget *mw : d_app->hiddenWindows) {
                if (mw)
                    ignored.insert(mw);
            }
            if (Table *srcTbl = sourceTableFromMultiLayer(activeMl, ignored)) {
                if (srcTbl->status() != MyWidget::Hidden
                    && !srcTbl->name().startsWith(QLatin1String("RpcFitCurve"))
                    && (!d_app->currentFolder()
                        || d_app->currentFolder()->windowsList().contains(srcTbl))) {
                    tbl = srcTbl;
                }
            }
        }
    }

    if (tbl) {
        const bool singleColPlot = (curveType == Graph::Histogram || curveType == Graph::Pie
                                    || curveType == Graph::Box);
        if (singleColPlot && tbl->selectedColumns().isEmpty()) {
            const QStringList yCols = tbl->YColumns();
            const QStringList allCols = tbl->columnsList();
            const QStringList pick = !yCols.isEmpty()
                    ? QStringList { yCols.first() }
                    : (!allCols.isEmpty() ? QStringList { allCols.last() } : QStringList());
            if (!pick.isEmpty()) {
                d_app->multilayerPlot(tbl, pick, curveType);
                if (d_layout_mode == LayoutSplitTableGraph)
                    applySplitWorkspaceGeometry();
                return;
            }
        } else if (!singleColPlot && tbl->selectedYColumns().isEmpty() && !tbl->noXColumn()) {
            const QStringList yCols = tbl->YColumns();
            if (!yCols.isEmpty()) {
                d_app->multilayerPlot(tbl, yCols, curveType);
                if (d_layout_mode == LayoutSplitTableGraph)
                    applySplitWorkspaceGeometry();
                return;
            }
        }
    }

    QMetaObject::invokeMethod(d_app, "selectPlotType", Qt::DirectConnection,
                              Q_ARG(int, curveType));
    if (d_layout_mode == LayoutSplitTableGraph)
        applySplitWorkspaceGeometry();
}

void ModernRibbonBar::importOpjInteractive()
{
    if (d_shutting_down || !d_app)
        return;
    const QString fn = QFileDialog::getOpenFileName(
            d_app, tr("Import Origin OPJ / OPJU Project"), d_app->workingDir,
            tr("Origin Projects (*.opj *.OPJ *.opju *.OPJU);;All Files (*)"));
    if (!fn.isEmpty())
        d_app->importOPJ(fn);
}

int ModernRibbonBar::tabCount() const
{
    return d_tab_bar ? d_tab_bar->count() : 0;
}

int ModernRibbonBar::currentTabIndex() const
{
    return d_tab_bar ? d_tab_bar->currentIndex() : -1;
}

void ModernRibbonBar::setCurrentTabIndex(int index)
{
    if (d_tab_bar && index >= 0 && index < d_tab_bar->count())
        d_tab_bar->setCurrentIndex(index);
}

QString ModernRibbonBar::tabTitle(int index) const
{
    return (d_tab_bar && index >= 0 && index < d_tab_bar->count())
            ? unescapeTabMnemonicText(d_tab_bar->tabText(index))
            : QString();
}

QString ModernRibbonBar::activeContextText() const
{
    return d_context_pill ? d_context_pill->text() : QString();
}

CommandPalettePopup *ModernRibbonBar::commandPalette()
{
    if (d_shutting_down)
        return nullptr;
    if (!d_command_palette)
        d_command_palette = new CommandPalettePopup(d_app, this, d_app);
    return d_command_palette.data();
}

FocusCanvasHudOverlay *ModernRibbonBar::focusHudOverlay() const
{
    return d_focus_hud_overlay.data();
}

FocusTableDrawer *ModernRibbonBar::focusTableDrawer() const
{
    return d_focus_hud_overlay ? d_focus_hud_overlay->tableDrawer() : nullptr;
}

void ModernRibbonBar::openCommandPalette()
{
    if (d_shutting_down) {
        if (d_command_palette)
            d_command_palette->hide();
        return;
    }

    CommandPalettePopup *popup = commandPalette();
    if (!popup)
        return;

    popup->refreshCommands();
    popup->setFilterText(QString());

    if (d_app) {
        const QPoint topCenter = d_app->mapToGlobal(QPoint(d_app->width() / 2, 76));
        int px = topCenter.x() - popup->width() / 2;
        int py = topCenter.y();
        if (QScreen *scr = d_app->screen()) {
            const QRect avail = scr->availableGeometry();
            px = qBound(avail.left() + 8, px, qMax(avail.left() + 8, avail.right() - popup->width() - 8));
            py = qBound(avail.top() + 8, py, qMax(avail.top() + 8, avail.bottom() - popup->height() - 8));
        }
        popup->move(px, py);
    }

    popup->show();
    popup->raise();
    popup->activateWindow();
}

bool ModernRibbonBar::eventFilter(QObject *watched, QEvent *event)
{
    if (d_shutting_down)
        return QFrame::eventFilter(watched, event);
    if (watched == this && event
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateHeaderResponsiveness();
        updateTabStackHeight();
    }
    if (d_app && watched == d_app->d_workspace.viewport() && event) {
        if (event->type() == QEvent::Resize && d_layout_mode == LayoutSplitTableGraph
            && !d_applying_split_geometry) {
            applySplitWorkspaceGeometry();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void ModernRibbonBar::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}
