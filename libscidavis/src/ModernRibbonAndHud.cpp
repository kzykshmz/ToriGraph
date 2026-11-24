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
#include "ModernRibbonAndHud_Internal.h"
#include "UxNavigation.h"
#include "Workbook.h"
#include "XlsxImport.h"

#include <QToolTip>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QIdentityProxyModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QListWidgetItem>
#include <QLocale>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "ApplicationWindow.h"
#include "CurveMathTools.h"
#include "ExtraPlotTypes.h"
#include "SettingsTransfer.h"
#include "Graph.h"
#include "GraphInspectorDock.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "MyWidget.h"
#include "Note.h"
#include "PlotCurve.h"
#include "RecalcEngine.h"
#include "Table.h"
#include "Graph3D.h"
#include "OleServer.h"
#include "PptxExport.h"
#include "I18nJapanese.h"
#include "future/core/column/Column.h"
#include "future/table/TableModel.h"
#include "future/table/future_Table.h"

#include <QAbstractSpinBox>
#include <QDateTime>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTextEdit>
#include <mutex>

namespace ModernRibbonHudInternal {

MultiLayer *resolveTargetMultiLayer(ApplicationWindow *app, ModernRibbonBar *ribbon)
{
    if (!app)
        return nullptr;
    if (MultiLayer *ml = qobject_cast<MultiLayer *>(app->d_workspace.activeSubWindow()))
        return ml;
    if (ribbon && ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(ribbon->splitPlotTarget()))
            return ml;
    }
    return nullptr;
}

Table *sourceTableFromMultiLayer(MultiLayer *ml,
                                        const QSet<QObject *> &ignored)
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

bool isWorkbookManagedSheet(ApplicationWindow *app, MyWidget *mw)
{
    Table *tbl = qobject_cast<Table *>(mw);
    if (!app || !tbl)
        return false;
    WorkbookManager *wb = WorkbookManager::existing(app);
    if (!wb)
        return false;
    const QString book = wb->bookOf(tbl);
    if (book.isEmpty())
        return false;
    Table *cur = wb->currentSheet(book);
    return cur && cur->status() != MyWidget::Hidden && !app->hiddenWindows.contains(cur);
}

bool isUsableRibbonWorksheet(ApplicationWindow *app, MyWidget *mw)
{
    Table *tbl = qobject_cast<Table *>(mw);
    if (!app || !tbl)
        return false;
    if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
        return false;
    if (tbl->status() != MyWidget::Hidden && !app->hiddenWindows.contains(tbl))
        return true;
    return isWorkbookManagedSheet(app, tbl);
}

MultiLayer *activeMultiLayerOrUxHint(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (MultiLayer *ml = resolveTargetMultiLayer(app, app->modernRibbonBar()))
        return ml;
    app->showUxHint(ModernRibbonBar::tr("Click or create a graph window first."));
    return nullptr;
}

} // namespace ModernRibbonHudInternal

namespace {

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

void hideBackgroundMdiWindowsForFocus(ApplicationWindow *app)
{
    if (!app)
        return;
    static bool s_busy = false;
    if (s_busy)
        return;
    s_busy = true;

    auto isEligibleVisible = [app](MyWidget *mw) -> bool {
        if (!mw || mw->isHidden() || mw->status() == MyWidget::Hidden)
            return false;
        if (app->hiddenWindows.contains(mw) || app->outWindows.contains(mw))
            return false;
        if (Folder *cur = app->currentFolder()) {
            if (!cur->windowsList().contains(mw))
                return false;
        }
        return true;
    };

    MyWidget *focusTarget = nullptr;
    if (MyWidget *aw = qobject_cast<MyWidget *>(app->d_workspace.activeSubWindow())) {
        if (isEligibleVisible(aw)
            && (qobject_cast<MultiLayer *>(aw) || qobject_cast<Graph3D *>(aw))) {
            focusTarget = aw;
        }
    }
    if (!focusTarget) {
        const QList<QMdiSubWindow *> ordered =
                app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = ordered.size() - 1; i >= 0; --i) {
            MyWidget *mw = qobject_cast<MyWidget *>(ordered.at(i));
            if (isEligibleVisible(mw)
                && (qobject_cast<MultiLayer *>(mw) || qobject_cast<Graph3D *>(mw))) {
                focusTarget = mw;
                break;
            }
        }
    }
    if (!focusTarget) {
        if (MyWidget *aw = qobject_cast<MyWidget *>(app->d_workspace.activeSubWindow())) {
            if (isEligibleVisible(aw))
                focusTarget = aw;
        }
    }
    if (!focusTarget) {
        const QList<QMdiSubWindow *> ordered =
                app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = ordered.size() - 1; i >= 0; --i) {
            MyWidget *mw = qobject_cast<MyWidget *>(ordered.at(i));
            if (isEligibleVisible(mw)) {
                focusTarget = mw;
                break;
            }
        }
    }

    if (focusTarget) {
        focusTarget->setProperty("_tg_hidden_for_focus_mdi", QVariant());
        if (app->d_workspace.activeSubWindow() != focusTarget)
            app->d_workspace.setActiveSubWindow(focusTarget);

        const QList<QMdiSubWindow *> subs = app->d_workspace.subWindowList();
        for (QMdiSubWindow *sw : subs) {
            MyWidget *mw = qobject_cast<MyWidget *>(sw);
            if (!mw || mw == focusTarget)
                continue;
            if (!isEligibleVisible(mw))
                continue;
            mw->setProperty("_tg_hidden_for_focus_mdi", true);
            mw->hide();
        }
    }

    s_busy = false;
}

void restoreMdiWindowsWithFlag(ApplicationWindow *app, const char *flagProp, bool &busyFlag)
{
    if (!app || busyFlag)
        return;
    QScopedValueRollback<bool> busyGuard(busyFlag, true);

    QPointer<QMdiSubWindow> keepActive = app->d_workspace.activeSubWindow();
    const QList<QMdiSubWindow *> subs = app->d_workspace.subWindowList(QMdiArea::CreationOrder);
    for (QMdiSubWindow *sw : subs) {
        MyWidget *mw = qobject_cast<MyWidget *>(sw);
        if (!mw || !mw->property(flagProp).toBool())
            continue;
        mw->setProperty(flagProp, QVariant());
        if (mw->status() == MyWidget::Hidden || app->hiddenWindows.contains(mw)
            || app->outWindows.contains(mw)) {
            continue;
        }
        if (Folder *cur = app->currentFolder()) {
            if (!cur->windowsList().contains(mw))
                continue;
        }
        const bool suppressActivate =
                (keepActive && mw != keepActive.data()
                 && !mw->testAttribute(Qt::WA_ShowWithoutActivating));
        if (suppressActivate)
            mw->setAttribute(Qt::WA_ShowWithoutActivating, true);
        if (mw->status() == MyWidget::Minimized)
            mw->showMinimized();
        else if (mw->status() == MyWidget::Maximized)
            mw->showMaximized();
        else
            mw->show();
        if (suppressActivate)
            mw->setAttribute(Qt::WA_ShowWithoutActivating, false);
    }
    if (keepActive && !keepActive->isHidden()) {
        if (app->d_workspace.activeSubWindow() != keepActive.data())
            app->d_workspace.setActiveSubWindow(keepActive.data());
        keepActive->raise();
    }
}

void restoreBackgroundMdiWindowsAfterFocus(ApplicationWindow *app)
{
    static bool s_busy = false;
    restoreMdiWindowsWithFlag(app, "_tg_hidden_for_focus_mdi", s_busy);
}

void restoreSplitTablesHiddenForDrawer(ApplicationWindow *app)
{
    static bool s_busy = false;
    restoreMdiWindowsWithFlag(app, "_tg_hidden_for_focus_drawer", s_busy);
}

} // namespace

// ============================================================================
// ModernRibbonBar
// ============================================================================

ModernRibbonBar::ModernRibbonBar(ApplicationWindow *app, QWidget *parent)
    : QFrame(parent), d_app(app)
{
    ModernRibbonHudInternal::registerRibbonCollapseTranslations();
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
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
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
            "  color: #5B6B80;"
            "}"
            "QToolButton.RibbonPresetBtn {"
            "  background-color: #FFF7ED;"
            "  color: #9A3412;"
            "  border: 1px solid #FDBA74;"
            "  border-radius: 7px;"
            "  padding: 3px 8px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
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
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
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
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
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
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
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
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
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
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  font-weight: 700;"
            "}"
            "QToolTip {"
            "  background-color: #0F172A;"
            "  color: #F8FAFC;"
            "  font-size: 10.5pt;"
            "  border: 1px solid #334155;"
            "  border-radius: 5px;"
            "  padding: 4px 8px;"
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
    d_tab_bar->setFocusPolicy(Qt::TabFocus);
    topLayout->addWidget(d_tab_bar, 1);

    d_btn_cmd_palette = new QToolButton(topStrip);
    d_btn_cmd_palette->setObjectName(QStringLiteral("RibbonCommandSearchPill"));
    d_btn_cmd_palette->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_cmd_palette->setIcon(IconLoader::duotoneIcon(QStringLiteral("search-command"), 16, false));
    d_btn_cmd_palette->setIconSize(QSize(14, 14));
    d_btn_cmd_palette->setMinimumWidth(132);
    d_btn_cmd_palette->setMaximumWidth(168);
    d_btn_cmd_palette->setCursor(Qt::PointingHandCursor);
    d_btn_cmd_palette->setFocusPolicy(Qt::TabFocus);
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
        b->setFocusPolicy(Qt::TabFocus);
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
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };

    connect(d_layout_btn_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            [this](int modeId) { setWorkspaceLayoutMode(WorkspaceLayoutMode(modeId)); });

    d_btn_focus_mode = makeHeaderPill(QStringLiteral("focus-hud"), false);
    connect(d_btn_focus_mode, &QToolButton::clicked, this,
            [this]() { setFocusHudMode(true); });
    topLayout->addWidget(d_btn_focus_mode);

    d_btn_lang_switch = makeHeaderPill(QStringLiteral("language-ja-en"), false);
    d_btn_lang_switch->setObjectName(QStringLiteral("RibbonLanguageSwitchBtn"));
    connect(d_btn_lang_switch, &QToolButton::clicked, this,
            &ModernRibbonBar::invokeAppToggleLanguage);
    topLayout->addWidget(d_btn_lang_switch);

    d_btn_help_guide = makeHeaderPill(QStringLiteral("help"), false);
    d_btn_help_guide->setObjectName(QStringLiteral("RibbonQuickGuideBtn"));
    connect(d_btn_help_guide, &QToolButton::clicked, this, [this]() {
        if (!d_app)
            return;
        if (QAction *a = d_app->findChild<QAction *>(QStringLiteral("actionShowHelp")))
            a->trigger();
        else
            QMetaObject::invokeMethod(d_app.data(), "showHelp", Qt::DirectConnection);
    });
    topLayout->addWidget(d_btn_help_guide);

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
    initStatusBarZoomControls();
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
                    if (d_focus_hud_mode && d_layout_mode == LayoutFreeMdi && mw && !mw->isHidden()
                        && (qobject_cast<MultiLayer *>(mw) || qobject_cast<Graph3D *>(mw))) {
                        hideBackgroundMdiWindowsForFocus(d_app);
                        const int minTop = focusHudTileTopY(d_app->d_workspace.viewport());
                        if (!mw->isMaximized() && mw->y() < minTop)
                            mw->move(mw->x(), minTop);
                    }
                });
    }

    this->installEventFilter(this);

    // C14: double-click a tab (or Ctrl+F1) to show only the tab row, as in Office
    d_tab_bar->installEventFilter(this);
    QShortcut *minimizeShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F1), this);
    minimizeShortcut->setContext(Qt::WindowShortcut);
    connect(minimizeShortcut, &QShortcut::activated, this,
            [this]() { setRibbonMinimized(!d_minimized); });
    if (d_app && !d_app->isHeadlessMode()
        && ApplicationWindow::getSettings()
                   .value(QStringLiteral("/UX/RibbonMinimized"), false)
                   .toBool()) {
        setRibbonMinimized(true);
    }

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

void ModernRibbonBar::prepareForAppShutdown()
{
    const bool wasShuttingDown = d_shutting_down;
    d_shutting_down = true;
    d_saved_focus_visibility.clear();
    d_last_active_window.clear();
    if (d_split_table)
        d_split_table->removeEventFilter(this);
    if (d_split_plot)
        d_split_plot->removeEventFilter(this);
    d_split_table.clear();
    d_split_plot.clear();

    if (!wasShuttingDown && d_app) {
        disconnect(&d_app->d_workspace, nullptr, this, nullptr);
        if (QWidget *vp = d_app->d_workspace.viewport())
            vp->removeEventFilter(this);
        if (QStatusBar *sb = d_app->statusBar())
            sb->removeEventFilter(this);
    }
    if (d_watched_empty_graph_pane) {
        d_watched_empty_graph_pane->removeEventFilter(this);
        d_watched_empty_graph_pane.clear();
    }
    if (d_welcome_hub_card) {
        d_welcome_hub_card->removeEventFilter(this);
        delete d_welcome_hub_card.data();
    }

    if (d_zoom_bound_ml) {
        disconnect(d_zoom_bound_ml.data(), nullptr, this, nullptr);
        d_zoom_bound_ml.clear();
    }
    if (d_status_zoom_btn) {
        if (d_app && d_app->statusBar())
            d_app->statusBar()->removeWidget(d_status_zoom_btn.data());
        delete d_status_zoom_btn.data();
    }
    if (d_status_fit_btn) {
        if (d_app && d_app->statusBar())
            d_app->statusBar()->removeWidget(d_status_fit_btn.data());
        delete d_status_fit_btn.data();
    }

    if (d_focus_hud_overlay) {
        d_focus_hud_overlay->prepareForAppShutdown();
        delete d_focus_hud_overlay.data();
    }
    if (d_command_palette)
        delete d_command_palette.data();

    d_app.clear();
}

Table *ModernRibbonBar::resolvedWorksheetTarget() const
{
    if (d_shutting_down || !d_app)
        return nullptr;

    if (Table *activeTbl = qobject_cast<Table *>(d_app->d_workspace.activeSubWindow())) {
        if (!d_app->hiddenWindows.contains(activeTbl) && !d_app->outWindows.contains(activeTbl)
            && activeTbl->status() != MyWidget::Hidden) {
            return activeTbl;
        }
    }

    if (d_layout_mode == LayoutSplitTableGraph) {
        auto isEligibleTable = [this](Table *tbl) -> bool {
            if (!tbl || tbl->status() == MyWidget::Hidden)
                return false;
            if (tbl->isHidden() && !tbl->property("_tg_hidden_for_focus_drawer").toBool())
                return false;
            if (d_app->hiddenWindows.contains(tbl) || d_app->outWindows.contains(tbl))
                return false;
            if (tbl->name().startsWith(QLatin1String("RpcFitCurve")))
                return false;
            return true;
        };

        if (isEligibleTable(d_split_table.data()))
            return d_split_table.data();

        const QList<QMdiSubWindow *> ordered =
                d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = ordered.size() - 1; i >= 0; --i) {
            if (Table *t = qobject_cast<Table *>(ordered.at(i))) {
                if (isEligibleTable(t))
                    return t;
            }
        }
    }

    return nullptr;
}

void ModernRibbonBar::pasteFromClipboardSmart()
{
    if (d_shutting_down || !d_app)
        return;

    QWidget *aw = d_app->d_workspace.activeSubWindow();
    if (qobject_cast<Matrix *>(aw) || qobject_cast<Note *>(aw)) {
        d_app->pasteSelection();
        return;
    }

    const QClipboard *cb = QApplication::clipboard();
    const QMimeData *mime = cb ? cb->mimeData() : nullptr;
    if (!mime || (!mime->hasText() && !mime->hasHtml()
                  && !mime->hasFormat(QStringLiteral("text/tab-separated-values"))
                  && !mime->hasFormat(QStringLiteral("text/csv"))
                  && !mime->hasUrls())) {
        d_app->showUxHint(
                tr("Clipboard is empty. Copy cells in Excel or TSV/CSV text first, then press Ctrl+V."),
                4500);
        return;
    }

    if (mime->hasUrls() && !mime->hasText()) {
        for (const QUrl &u : mime->urls()) {
            if (u.isLocalFile()) {
                d_app->open(u.toLocalFile());
                return;
            }
        }
    }

    Table *tbl = qobject_cast<Table *>(aw);
    if (!tbl)
        tbl = resolvedWorksheetTarget();
    if (!tbl)
        tbl = d_app->newTable();
    if (tbl) {
        if (d_app->d_workspace.activeSubWindow() != tbl)
            d_app->activateSubWindow(tbl);
        tbl->pasteSelection();
        updateEmptyWelcomeHub();
    } else {
        d_app->pasteSelection();
    }
}

void ModernRibbonBar::retranslateWelcomeHub()
{
    if (d_shutting_down || !d_welcome_hub_card)
        return;
    if (d_welcome_title_lbl)
        d_welcome_title_lbl->setText(tr("Start Analyzing Data in Seconds"));
    if (d_welcome_subtitle_lbl) {
        d_welcome_subtitle_lbl->setText(
                tr("Paste from Excel, drop a CSV/XLSX/XRD/OPJU file here, or open a sample project."));
    }
    if (d_welcome_drop_lbl) {
        d_welcome_drop_lbl->setText(
                tr("Drop CSV / XLSX / XRD / TXT / OPJU files anywhere here"));
    }
    if (d_welcome_paste_btn)
        d_welcome_paste_btn->setText(tr("Paste from Excel (Ctrl+V)"));
    if (d_welcome_open_btn)
        d_welcome_open_btn->setText(tr("Open File..."));
    if (d_welcome_recent_btn)
        d_welcome_recent_btn->setText(tr("Recent"));
    if (d_welcome_sample_btn) {
        d_welcome_sample_btn->setText(tr("Samples"));
        d_welcome_sample_btn->setToolTip(
                tr("Show 10+ Sample Templates (XRD / Raman / Tensile / Cyclic / Peak Fit)"));
    }
    if (d_welcome_new_table_btn)
        d_welcome_new_table_btn->setText(tr("New Worksheet"));
    if (d_welcome_guide_btn)
        d_welcome_guide_btn->setText(tr("Quick Guide"));
}

void ModernRibbonBar::updateEmptyWelcomeHub()
{
    if (d_shutting_down || !d_app || d_updating_welcome_hub)
        return;

    struct ReentrancyGuard {
        bool &flag;
        explicit ReentrancyGuard(bool &f) : flag(f) { flag = true; }
        ~ReentrancyGuard() { flag = false; }
    } guard(d_updating_welcome_hub);

    QWidget *vp = d_app->d_workspace.viewport();
    if (vp) {
        QWidget *emptyPane = vp->findChild<QWidget *>(QStringLiteral("emptyGraphPaneCard"));
        if (emptyPane && emptyPane != d_watched_empty_graph_pane) {
            if (d_watched_empty_graph_pane)
                d_watched_empty_graph_pane->removeEventFilter(this);
            d_watched_empty_graph_pane = emptyPane;
            emptyPane->installEventFilter(this);
        }
    }

    int visibleWindows = 0;
    if (Folder *curFolder = d_app->currentFolder()) {
        for (MyWidget *mw : curFolder->windowsList()) {
            if (!mw || mw->status() == MyWidget::Hidden)
                continue;
            if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                continue;
            if (mw->isHidden())
                continue;
            ++visibleWindows;
        }
    } else {
        for (QMdiSubWindow *sw : d_app->d_workspace.subWindowList()) {
            if (sw && !sw->isHidden())
                ++visibleWindows;
        }
    }

    const bool hasTableOnlySplit = (d_layout_mode == LayoutSplitTableGraph && d_split_table
                                    && !d_split_plot && !d_split_table->isHidden());
    if (d_watched_empty_graph_pane && (visibleWindows == 0 || hasTableOnlySplit))
        d_watched_empty_graph_pane->hide();

    const bool suppressedEnv = !vp || !vp->isVisible() || d_app->isHeadlessMode()
            || d_app->isOleEmbeddedMode() || d_app->batchMode() || d_focus_hud_mode;
    const bool showHub =
            !suppressedEnv && (visibleWindows == 0) && vp->width() >= 260 && vp->height() >= 200;

    if (!showHub) {
        if (d_welcome_hub_card)
            d_welcome_hub_card->hide();
        return;
    }

    if (!d_welcome_hub_card) {
        QFrame *card = new QFrame(vp);
        card->setObjectName(QStringLiteral("CenterWelcomeHubCard"));
        card->setAcceptDrops(true);
        card->setFocusPolicy(Qt::StrongFocus);
        card->installEventFilter(this);
        card->setStyleSheet(QStringLiteral(
                "QFrame#CenterWelcomeHubCard {"
                "  background-color: #FFFFFF;"
                "  border: 1px solid #CBD5E1;"
                "  border-radius: 14px;"
                "}"
                "QLabel#WelcomeHubTitle {"
                "  color: #0F172A;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 14pt;"
                "  font-weight: 700;"
                "}"
                "QLabel#WelcomeHubSubtitle {"
                "  color: #475569;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "}"
                "QLabel#WelcomeHubDropZone {"
                "  background-color: #F8FAFC;"
                "  color: #334155;"
                "  border: 1.5px dashed #94A3B8;"
                "  border-radius: 10px;"
                "  padding: 12px 14px;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "  font-weight: 600;"
                "}"
                "QPushButton#WelcomeHubPasteBtn {"
                "  background-color: #2563EB;"
                "  color: #FFFFFF;"
                "  border: 1px solid #1D4ED8;"
                "  border-radius: 8px;"
                "  padding: 7px 14px;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "  font-weight: 600;"
                "}"
                "QPushButton#WelcomeHubPasteBtn:hover {"
                "  background-color: #1D4ED8;"
                "}"
                "QPushButton.WelcomeHubSecondaryBtn {"
                "  background-color: #F8FAFC;"
                "  color: #0F172A;"
                "  border: 1px solid #CBD5E1;"
                "  border-radius: 8px;"
                "  padding: 7px 12px;"
                "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
                "  font-size: 10pt;"
                "  font-weight: 600;"
                "}"
                "QPushButton.WelcomeHubSecondaryBtn:hover {"
                "  background-color: #EFF6FF;"
                "  border: 1px solid #2563EB;"
                "  color: #1D4ED8;"
                "}"));

        QVBoxLayout *vlay = new QVBoxLayout(card);
        vlay->setContentsMargins(24, 22, 24, 22);
        vlay->setSpacing(12);

        d_welcome_title_lbl = new QLabel(card);
        d_welcome_title_lbl->setObjectName(QStringLiteral("WelcomeHubTitle"));
        d_welcome_title_lbl->setAlignment(Qt::AlignCenter);
        d_welcome_title_lbl->setWordWrap(true);
        vlay->addWidget(d_welcome_title_lbl);

        d_welcome_subtitle_lbl = new QLabel(card);
        d_welcome_subtitle_lbl->setObjectName(QStringLiteral("WelcomeHubSubtitle"));
        d_welcome_subtitle_lbl->setAlignment(Qt::AlignCenter);
        d_welcome_subtitle_lbl->setWordWrap(true);
        vlay->addWidget(d_welcome_subtitle_lbl);

        d_welcome_drop_lbl = new QLabel(card);
        d_welcome_drop_lbl->setObjectName(QStringLiteral("WelcomeHubDropZone"));
        d_welcome_drop_lbl->setAlignment(Qt::AlignCenter);
        d_welcome_drop_lbl->setWordWrap(true);
        vlay->addWidget(d_welcome_drop_lbl);

        QGridLayout *grid = new QGridLayout();
        grid->setSpacing(8);

        auto makeBtn = [this, card](const QString &iconId, bool primary) -> QPushButton * {
            QPushButton *btn = new QPushButton(card);
            btn->setIcon(IconLoader::duotoneIcon(iconId, 18, primary));
            btn->setIconSize(QSize(18, 18));
            btn->setMinimumHeight(36);
            btn->setCursor(Qt::PointingHandCursor);
            btn->installEventFilter(this);
            if (!primary)
                btn->setProperty("class", QStringLiteral("WelcomeHubSecondaryBtn"));
            return btn;
        };

        d_welcome_paste_btn = makeBtn(QStringLiteral("paste"), true);
        d_welcome_paste_btn->setObjectName(QStringLiteral("WelcomeHubPasteBtn"));
        connect(d_welcome_paste_btn, &QPushButton::clicked, this,
                &ModernRibbonBar::pasteFromClipboardSmart);
        grid->addWidget(d_welcome_paste_btn, 0, 0, 1, 2);

        d_welcome_open_btn = makeBtn(QStringLiteral("file-open"), false);
        connect(d_welcome_open_btn, &QPushButton::clicked, this, [this]() {
            if (d_app)
                d_app->open();
        });
        grid->addWidget(d_welcome_open_btn, 1, 0);

        d_welcome_recent_btn = makeBtn(QStringLiteral("folder-open"), false);
        d_welcome_recent_menu = new QMenu(d_welcome_recent_btn);
        d_welcome_recent_menu->setProperty("class", QStringLiteral("RibbonDropdownMenu"));
        connect(d_welcome_recent_menu, &QMenu::aboutToShow, this, [this]() {
            if (!d_welcome_recent_menu || !d_app)
                return;
            d_welcome_recent_menu->clear();
            int added = 0;
            for (const QString &fn : d_app->recentProjects) {
                if (fn.trimmed().isEmpty())
                    continue;
                const QFileInfo fi(fn);
                QAction *act = d_welcome_recent_menu->addAction(
                        fi.fileName().isEmpty() ? fn : fi.fileName(), this,
                        [this, fn]() {
                            if (d_app)
                                d_app->open(fn);
                        });
                act->setToolTip(fn);
                if (++added >= 10)
                    break;
            }
            if (added == 0) {
                QAction *emptyAct = d_welcome_recent_menu->addAction(tr("No recent files"));
                emptyAct->setEnabled(false);
            }
        });
        d_welcome_recent_btn->setMenu(d_welcome_recent_menu);
        grid->addWidget(d_welcome_recent_btn, 1, 1);

        d_welcome_sample_btn = makeBtn(QStringLiteral("sample-gallery"), false);
        connect(d_welcome_sample_btn, &QPushButton::clicked, this, [this]() {
            if (d_app)
                CurveMathTools::showSampleGalleryDialog(d_app);
        });
        grid->addWidget(d_welcome_sample_btn, 2, 0);

        d_welcome_new_table_btn = makeBtn(QStringLiteral("new-table"), false);
        connect(d_welcome_new_table_btn, &QPushButton::clicked, this, [this]() {
            if (d_app)
                d_app->newTable();
        });
        grid->addWidget(d_welcome_new_table_btn, 2, 1);

        d_welcome_guide_btn = makeBtn(QStringLiteral("help"), false);
        connect(d_welcome_guide_btn, &QPushButton::clicked, this, [this]() {
            if (!d_app)
                return;
            if (QAction *a = d_app->findChild<QAction *>(QStringLiteral("actionShowHelp")))
                a->trigger();
            else
                QMetaObject::invokeMethod(d_app.data(), "showHelp", Qt::DirectConnection);
        });
        grid->addWidget(d_welcome_guide_btn, 3, 0, 1, 2);

        vlay->addLayout(grid);
        d_welcome_hub_card = card;
        retranslateWelcomeHub();
    }

    const int cardW = qBound(240, qMin(520, vp->width() - 32), qMax(240, vp->width() - 16));
    if (d_welcome_hub_card->width() != cardW) {
        d_welcome_hub_card->setFixedWidth(cardW);
        d_welcome_hub_card->adjustSize();
    }
    const int cardH = qMin(qMax(220, vp->height() - 24),
                           qMax(280, d_welcome_hub_card->sizeHint().height()));
    const int x = qMax(8, (vp->width() - cardW) / 2);
    const int y = qMax(8, (vp->height() - cardH) / 2);
    const QRect wantRect(x, y, cardW, cardH);
    if (d_welcome_hub_card->geometry() != wantRect)
        d_welcome_hub_card->setGeometry(wantRect);
    const bool becomingVisible = d_welcome_hub_card->isHidden();
    if (becomingVisible) {
        d_welcome_hub_card->show();
        d_welcome_hub_card->raise();
    }
    const bool popupOrModalOpen = (QApplication::activePopupWidget() != nullptr)
            || (QApplication::activeModalWidget() != nullptr)
            || (d_command_palette && d_command_palette->isVisible());
    if (!popupOrModalOpen) {
        QWidget *fw = QApplication::focusWidget();
        const bool focusInsideCard =
                fw && (fw == d_welcome_hub_card || d_welcome_hub_card->isAncestorOf(fw));
        const bool focusInOtherInput = fw && !focusInsideCard && fw != vp
                && fw != d_app.data() && fw != &d_app->d_workspace
                && (qobject_cast<QLineEdit *>(fw) || qobject_cast<QTextEdit *>(fw)
                    || qobject_cast<QPlainTextEdit *>(fw) || qobject_cast<QAbstractSpinBox *>(fw)
                    || qobject_cast<QComboBox *>(fw));
        if ((becomingVisible && !focusInsideCard && !focusInOtherInput)
            || (!focusInsideCard && (!fw || fw == vp || fw == &d_app->d_workspace || fw == d_app.data()))) {
            d_welcome_hub_card->setFocus(Qt::OtherFocusReason);
        }
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
                               "padding: 2px 10px; font-family: 'Yu Gothic UI', 'Segoe UI', "
                               "'Meiryo UI', sans-serif; font-size: 10pt; font-weight: 700; }")
                        .arg(pillStyle));
        updateHeaderResponsiveness();
    }

    if (d_app) {
        if (QStatusBar *sb = d_app->statusBar()) {
            if (QToolButton *cmdHint =
                        sb->findChild<QToolButton *>(QStringLiteral("StatusBarCommandHintBtn"))) {
                sb->removeWidget(cmdHint);
                cmdHint->hide();
                cmdHint->deleteLater();
            }
        }
    }

    Table *activeTable = qobject_cast<Table *>(activeWindow);
    Table *wsTarget = activeTable ? activeTable : resolvedWorksheetTarget();
    MultiLayer *activeMl = qobject_cast<MultiLayer *>(activeWindow);
    const bool hasGraph = (activeMl != nullptr || qobject_cast<Graph3D *>(activeWindow) != nullptr);

    if (activeMl) {
        if (Table *srcTbl = ModernRibbonHudInternal::sourceTableFromMultiLayer(activeMl))
            ToriUx::dismissPlotThisDataBar(srcTbl);
    }

    // Gate Table actions to active Table OR visible paired Split Table.
    if (d_act_add_column)
        d_act_add_column->setEnabled(wsTarget != nullptr);
    for (const QPointer<QWidget> &w : d_table_context_widgets) {
        if (w)
            w->setEnabled(wsTarget != nullptr);
    }
    if (d_btn_ws_reimport)
        d_btn_ws_reimport->setEnabled(wsTarget != nullptr && wsTarget->hasImportSource());

    // A1-8: Disable Copy for PowerPoint when no graph window is active, while keeping the
    // double-click re-editing hint in the tooltip.
    const QString copyOleTip = hasGraph
            ? ToriGraphOleUi::copyForPowerPointToolTip()
            : (ToriGraphOleUi::copyForPowerPointToolTip() + QStringLiteral("\n")
               + tr("Create or select a graph window first to copy for PowerPoint."));
    if (d_app) {
        if (QAction *aCopyOle = d_app->findChild<QAction *>(QStringLiteral("actionCopyGraphOle"))) {
            aCopyOle->setEnabled(hasGraph);
            aCopyOle->setToolTip(copyOleTip);
        }
    }
    if (d_btn_export_copy_ole) {
        d_btn_export_copy_ole->setEnabled(hasGraph);
        d_btn_export_copy_ole->setToolTip(copyOleTip);
    }

    updateStatusBarZoomIndicator(activeMl);

    syncActionBoundButtons();
    syncCheckableStates();
    if (d_focus_hud_overlay)
        d_focus_hud_overlay->refreshActiveContext();
    updateEmptyWelcomeHub();
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
            restoreSplitTablesHiddenForDrawer(d_app);
            restoreBackgroundMdiWindowsAfterFocus(d_app);
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
            restoreSplitTablesHiddenForDrawer(d_app);
            restoreBackgroundMdiWindowsAfterFocus(d_app);
            d_app->setTabbedWorkspaceView(false);
            unmaximizeCurrentFolderWindows();
            applySplitWorkspaceGeometry();
            break;

        case LayoutFreeMdi:
            restoreSplitTablesHiddenForDrawer(d_app);
            restoreBackgroundMdiWindowsAfterFocus(d_app);
            d_app->setTabbedWorkspaceView(false);
            unmaximizeCurrentFolderWindows();
            if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
                hsb->setValue(0);
            if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
                vsb->setValue(0);
            if (Folder *curFolder = d_app->currentFolder()) {
                const bool focusActive = d_focus_hud_mode
                        || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());
                const int minTopY = focusActive
                        ? focusHudTileTopY(d_app->d_workspace.viewport())
                        : 8;
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
            if (d_focus_hud_mode)
                hideBackgroundMdiWindowsForFocus(d_app);
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
    updateEmptyWelcomeHub();

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

void ModernRibbonBar::setSplitLeftRatio(double ratio)
{
    if (!std::isfinite(ratio))
        return;
    const double clamped = qBound(0.15, ratio, 0.85);
    if (qFuzzyCompare(d_split_left_ratio, clamped))
        return;
    d_split_left_ratio = clamped;
    if (d_layout_mode == LayoutSplitTableGraph && !d_applying_split_geometry)
        applySplitWorkspaceGeometry();
}

int ModernRibbonBar::focusHudTileTopY(QWidget *workspaceViewport) const
{
    const bool focusHudActive =
            d_focus_hud_mode || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());
    if (!focusHudActive)
        return 0;
    int hudPillBottom = 0;
    if (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive()) {
        QWidget *hudHost = d_focus_hud_overlay->parentWidget();
        int hostBottom = d_focus_hud_overlay->y() + d_focus_hud_overlay->height();
        if (QToolButton *wsBtn = d_focus_hud_overlay->worksheetToggleButton())
            hostBottom = qMax(hostBottom, wsBtn->y() + wsBtn->height());
        if (QToolButton *exitBtn = d_focus_hud_overlay->exitFocusButton())
            hostBottom = qMax(hostBottom, exitBtn->y() + exitBtn->height());
        if (hudHost && workspaceViewport && hostBottom > 0)
            hudPillBottom = workspaceViewport->mapFrom(hudHost, QPoint(0, hostBottom)).y();
        else
            hudPillBottom = hostBottom;
    }
    return qMax(58, hudPillBottom + 8);
}

void ModernRibbonBar::applySplitWorkspaceGeometry()
{
    if (d_shutting_down || !d_app || d_applying_split_geometry)
        return;

    d_applying_split_geometry = true;

    const bool focusHudActive =
            d_focus_hud_mode || (d_focus_hud_overlay && d_focus_hud_overlay->isOverlayActive());
    const bool drawerOpen = focusHudActive && d_focus_hud_overlay
            && d_focus_hud_overlay->isTableDrawerVisible()
            && d_focus_hud_overlay->tableDrawer();
    if (!drawerOpen)
        restoreSplitTablesHiddenForDrawer(d_app);

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
            if (mw->isHidden()
                && !(drawerOpen && mw->property("_tg_hidden_for_focus_drawer").toBool())) {
                continue;
            }
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
            QSet<QObject *> ignored;
            for (MyWidget *mw : d_app->hiddenWindows) {
                if (mw && !ModernRibbonHudInternal::isWorkbookManagedSheet(d_app, mw))
                    ignored.insert(mw);
            }
            if (Table *srcTbl = ModernRibbonHudInternal::sourceTableFromMultiLayer(ml, ignored)) {
                if (eligibleWins.contains(srcTbl)) {
                    targetTable = srcTbl;
                } else if (ModernRibbonHudInternal::isWorkbookManagedSheet(d_app, srcTbl)) {
                    if (WorkbookManager *wb = WorkbookManager::existing(d_app)) {
                        const QString book = wb->bookOf(srcTbl);
                        if (Table *prevCur = wb->currentSheet(book))
                            eligibleWins.remove(prevCur);
                        wb->setCurrentSheet(srcTbl, false);
                    }
                    eligibleWins.insert(srcTbl);
                    targetTable = srcTbl;
                }
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
        if (mw && mw != targetTable && mw != targetPlot) {
            if (mw->isMaximized())
                prepareTileWindow(mw);
            // 05_focus_hud.png fix: hide non-split background subwindows (e.g. Table1 behind
            // Table2) while F11 Focus HUD is active so they never bleed through above tileTopY.
            if (focusHudActive) {
                mw->setProperty("_tg_hidden_for_focus_mdi", true);
                mw->hide();
            }
        }
    }

    const int tileTopY = focusHudTileTopY(vp);
    const double splitRatio = qBound(0.15, d_split_left_ratio, 0.85);
    const int minPaneW = (vpRect.width() < 1000) ? qMax(80, qMin(120, vpRect.width() / 4)) : 120;
    const int leftW = qBound(minPaneW, qRound(vpRect.width() * splitRatio),
                             qMax(minPaneW, vpRect.width() - minPaneW));
    const int fullW = qMax(minPaneW, vpRect.width());
    const int fullH = qMax(100, vpRect.height() - tileTopY);

    if (d_split_table && d_split_table != targetTable)
        d_split_table->removeEventFilter(this);
    if (d_split_plot && d_split_plot != targetPlot)
        d_split_plot->removeEventFilter(this);
    d_split_table = targetTable;
    d_split_plot = targetPlot;
    if (d_split_table) {
        d_split_table->removeEventFilter(this);
        d_split_table->installEventFilter(this);
    }
    if (d_split_plot) {
        d_split_plot->removeEventFilter(this);
        d_split_plot->installEventFilter(this);
    }

    if (QScrollBar *hsb = d_app->d_workspace.horizontalScrollBar())
        hsb->setValue(0);
    if (QScrollBar *vsb = d_app->d_workspace.verticalScrollBar())
        vsb->setValue(0);

    if (drawerOpen) {
        if (targetTable) {
            targetTable->setProperty("_tg_hidden_for_focus_drawer", true);
            if (!targetTable->isHidden())
                targetTable->hide();
        }
        if (targetPlot) {
            FocusTableDrawer *drawer = d_focus_hud_overlay->tableDrawer();
            QWidget *hudHost = d_focus_hud_overlay->parentWidget();
            int drawerRight = 0;
            if (drawer) {
                const int hostRight = drawer->x() + drawer->width();
                drawerRight = (hudHost && vp)
                        ? vp->mapFrom(hudHost, QPoint(hostRight, 0)).x()
                        : hostRight;
            }
            const int gap = 10;
            const int plotX = qBound(0, drawerRight + gap, qMax(0, vpRect.width() - minPaneW));
            const int plotW = qMax(minPaneW, vpRect.width() - plotX);
            prepareTileWindow(targetPlot);
            if (targetPlot->minimumWidth() > plotW)
                targetPlot->setMinimumWidth(plotW);
            targetPlot->setGeometry(plotX, tileTopY, plotW, fullH);
            targetPlot->raise();
            if (!d_app->d_workspace.activeSubWindow()
                || d_app->d_workspace.activeSubWindow() == targetTable) {
                d_app->d_workspace.setActiveSubWindow(targetPlot);
            }
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(targetPlot)) {
                if (targetTable && multiLayerUsesTable(ml, targetTable))
                    ToriUx::dismissPlotThisDataBar(targetTable);
            }
        }
    } else if (targetTable && targetPlot) {
        prepareTileWindow(targetTable);
        prepareTileWindow(targetPlot);
        if (targetTable->minimumWidth() > leftW)
            targetTable->setMinimumWidth(leftW);
        targetTable->setGeometry(0, tileTopY, leftW, fullH);
        const int actualLeftW = qBound(minPaneW, targetTable->geometry().width(),
                                       qMax(minPaneW, vpRect.width() - minPaneW));
        if (actualLeftW != targetTable->geometry().width())
            targetTable->setGeometry(0, tileTopY, actualLeftW, fullH);
        const int actualRightW = qMax(minPaneW, vpRect.width() - actualLeftW);
        if (targetPlot->minimumWidth() > actualRightW)
            targetPlot->setMinimumWidth(actualRightW);
        targetPlot->setGeometry(actualLeftW, tileTopY, actualRightW, fullH);
        if (activeMw == targetTable) {
            targetPlot->raise();
            targetTable->raise();
        } else {
            targetTable->raise();
            targetPlot->raise();
        }
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(targetPlot)) {
            if (multiLayerUsesTable(ml, targetTable))
                ToriUx::dismissPlotThisDataBar(targetTable);
        }
    } else if (targetTable) {
        prepareTileWindow(targetTable);
        if (targetTable->minimumWidth() > fullW)
            targetTable->setMinimumWidth(fullW);
        targetTable->setGeometry(0, tileTopY, fullW, fullH);
        targetTable->raise();
        for (MyWidget *mw : eligibleWins) {
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(mw)) {
                if (multiLayerUsesTable(ml, targetTable)) {
                    ToriUx::dismissPlotThisDataBar(targetTable);
                    break;
                }
            }
        }
    } else if (targetPlot) {
        prepareTileWindow(targetPlot);
        if (targetPlot->minimumWidth() > fullW)
            targetPlot->setMinimumWidth(fullW);
        targetPlot->setGeometry(0, tileTopY, fullW, fullH);
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

    Table *wsTarget = resolvedWorksheetTarget();
    if (d_act_add_column)
        d_act_add_column->setEnabled(wsTarget != nullptr);
    for (const QPointer<QWidget> &w : d_table_context_widgets) {
        if (w)
            w->setEnabled(wsTarget != nullptr);
    }
    if (d_btn_ws_reimport)
        d_btn_ws_reimport->setEnabled(wsTarget != nullptr && wsTarget->hasImportSource());

    if (d_focus_hud_overlay && focusHudActive)
        d_focus_hud_overlay->repositionOverlayWidgets();
    updateEmptyWelcomeHub();
}

void ModernRibbonBar::setFocusHudMode(bool enabled)
{
    if (d_shutting_down)
        return;

    if (d_focus_hud_mode == enabled)
        return;

    d_focus_hud_mode = enabled;

    QPointer<QMdiSubWindow> keepActive;
    if (!enabled && d_app)
        keepActive = d_app->d_workspace.activeSubWindow();

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

            if (QMenuBar *mb = d_app->menuBar()) {
                d_saved_focus_visibility.append(qMakePair(QPointer<QWidget>(mb), !mb->isHidden()));
                mb->hide();
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

    if (!enabled && d_app) {
        if (!keepActive)
            keepActive = d_app->d_workspace.activeSubWindow();
        QScopedValueRollback<bool> splitGuard(d_applying_split_geometry, true);
        restoreSplitTablesHiddenForDrawer(d_app);
        restoreBackgroundMdiWindowsAfterFocus(d_app);
        if (keepActive && !keepActive->isHidden()
            && d_app->d_workspace.activeSubWindow() != keepActive.data()) {
            d_app->d_workspace.setActiveSubWindow(keepActive);
        }
    }

    if (d_layout_mode == LayoutSplitTableGraph) {
        applySplitWorkspaceGeometry();
    } else if (enabled && d_layout_mode == LayoutFreeMdi && d_app) {
        hideBackgroundMdiWindowsForFocus(d_app);
        const int minTop = focusHudTileTopY(d_app->d_workspace.viewport());
        if (Folder *curFolder = d_app->currentFolder()) {
            for (MyWidget *mw : curFolder->windowsList()) {
                if (!mw || mw->status() == MyWidget::Hidden || mw->isHidden())
                    continue;
                if (d_app->hiddenWindows.contains(mw) || d_app->outWindows.contains(mw))
                    continue;
                if (!mw->isMaximized() && mw->y() < minTop)
                    mw->move(mw->x(), minTop);
            }
        }
        if (d_focus_hud_overlay)
            d_focus_hud_overlay->repositionOverlayWidgets();
    }

    if (!enabled && d_app && keepActive && !keepActive->isHidden()) {
        d_app->d_workspace.setActiveSubWindow(keepActive);
        keepActive->raise();
    }

    updateEmptyWelcomeHub();
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

    if (d_app && d_focus_hud_mode && !visible) {
        // C15: the focus HUD hid every toolbar and restores the saved states when it closes;
        // make that restore keep the classic toolbars closed (their switches may be hidden).
        const QToolBar *const legacyBars[] = { d_app->file_tools,  d_app->edit_tools,
                                               d_app->table_tools, d_app->plot_tools,
                                               d_app->graph_tools, d_app->matrix_plot_tools,
                                               d_app->graph_3D_tools };
        for (auto &saved : d_saved_focus_visibility) {
            for (const QToolBar *tb : legacyBars) {
                if (tb && saved.first.data() == tb)
                    saved.second = false;
            }
        }
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

    MyWidget *active = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
    Table *tbl = qobject_cast<Table *>(active);
    bool fromGraph = false;
    if (!tbl && d_focus_hud_mode && d_focus_hud_overlay
        && d_focus_hud_overlay->isTableDrawerVisible() && d_focus_hud_overlay->tableDrawer()) {
        tbl = d_focus_hud_overlay->tableDrawer()->currentTable();
    }
    MultiLayer *activeMl = qobject_cast<MultiLayer *>(active);
    if (!tbl && activeMl) {
        // B6: a graph is active. The plot buttons still mean "plot worksheet data", so use the
        // worksheet behind this graph (formerly only in the F11 HUD mode; elsewhere the click
        // silently re-styled the last curve).
        QSet<QObject *> ignored;
        for (MyWidget *mw : d_app->hiddenWindows) {
            if (mw && !ModernRibbonHudInternal::isWorkbookManagedSheet(d_app, mw))
                ignored.insert(mw);
        }
        if (Table *srcTbl = ModernRibbonHudInternal::sourceTableFromMultiLayer(activeMl, ignored)) {
            if (ModernRibbonHudInternal::isUsableRibbonWorksheet(d_app, srcTbl)) {
                tbl = srcTbl;
                fromGraph = true;
            }
        }
    }

    if (!tbl) {
        d_app->showUxHint(activeMl
                                  ? tr("The plot buttons make a new graph from worksheet columns. "
                                       "This graph has no source worksheet (function or image "
                                       "data); click a worksheet first.")
                                  : tr("The plot buttons make a graph from worksheet columns. "
                                       "Click a worksheet (or paste data from Excel) first."));
        return;
    }

    if (fromGraph) {
        if (d_app->plotColumnsFromTable(tbl, curveType)) {
            ToriUx::dismissPlotThisDataBar(tbl);
            d_app->showUxHint(
                    tr("Created a new graph from \"%1\", the worksheet of the active graph.")
                            .arg(tbl->name()),
                    5000);
        }
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
        return;
    }

    const bool singleColPlot = (curveType == Graph::Histogram || curveType == Graph::Pie
                                || curveType == Graph::Box);
    const bool tableIsActive = (tbl == active);
    if (singleColPlot && tbl->selectedColumns().isEmpty()) {
        const QStringList yCols = tbl->YColumns();
        const QStringList allCols = tbl->columnsList();
        const QStringList pick = !yCols.isEmpty()
                ? QStringList { yCols.first() }
                : (!allCols.isEmpty() ? QStringList { allCols.last() } : QStringList());
        if (!pick.isEmpty() && d_app->plotBlockerForTable(tbl, curveType).isEmpty()) {
            if (d_app->multilayerPlot(tbl, pick, curveType))
                ToriUx::dismissPlotThisDataBar(tbl);
            if (d_layout_mode == LayoutSplitTableGraph)
                applySplitWorkspaceGeometry();
            return;
        }
    } else if (!singleColPlot && tbl->selectedYColumns().isEmpty() && !tbl->noXColumn()
               && d_app->plotBlockerForTable(tbl, curveType).isEmpty()) {
        const QStringList yCols = tbl->YColumns();
        if (!yCols.isEmpty()) {
            if (d_app->multilayerPlot(tbl, yCols, curveType))
                ToriUx::dismissPlotThisDataBar(tbl);
            if (d_layout_mode == LayoutSplitTableGraph)
                applySplitWorkspaceGeometry();
            return;
        }
    }

    // Validation with messages that say where to fix things (B7), then the normal plot path.
    if (tableIsActive) {
        QMetaObject::invokeMethod(d_app, "selectPlotType", Qt::DirectConnection,
                                  Q_ARG(int, curveType));
        if (Folder *curFolder = d_app->currentFolder()) {
            for (MyWidget *mw : curFolder->windowsList()) {
                if (MultiLayer *ml = qobject_cast<MultiLayer *>(mw)) {
                    if (multiLayerUsesTable(ml, tbl)) {
                        ToriUx::dismissPlotThisDataBar(tbl);
                        break;
                    }
                }
            }
        }
    } else if (d_app->plotColumnsFromTable(tbl, curveType)) {
        ToriUx::dismissPlotThisDataBar(tbl);
    }
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

void ModernRibbonBar::invokeAppToggleLanguage()
{
    if (d_shutting_down || !d_app)
        return;
    if (!QMetaObject::invokeMethod(d_app.data(), "toggleLanguage", Qt::DirectConnection)) {
        if (d_app->appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive))
            d_app->switchToEnglish();
        else
            d_app->switchToJapanese();
    }
}

void ModernRibbonBar::applyWorksheetColumnRole(int roleCode)
{
    if (d_shutting_down || !d_app)
        return;
    Table *tbl = resolvedWorksheetTarget();
    if (!tbl || !tbl->d_future_table || tbl->numCols() <= 0)
        return;

    QList<int> colIndices;
    const QStringList selNames = tbl->selectedColumns();
    for (const QString &name : selNames) {
        const int idx = tbl->colIndex(name);
        if (idx >= 0 && !colIndices.contains(idx))
            colIndices.append(idx);
    }
    if (colIndices.isEmpty()) {
        const int firstSel = tbl->firstSelectedColumn();
        if (firstSel >= 0 && firstSel < tbl->numCols())
            colIndices.append(firstSel);
        else
            colIndices.append(0);
    }

    tbl->d_future_table->beginMacro(
            tr("%1: set column plot designation").arg(tbl->name()));
    for (int ci : colIndices) {
        if (ci < 0 || ci >= tbl->numCols())
            continue;
        Column *c = tbl->column(ci);
        if (!c)
            continue;
        if (roleCode == -100) {
            c->setProperty("_tg_label_role", true);
            if (c->columnMode() != SciDAVis::ColumnMode::Text)
                c->setColumnMode(SciDAVis::ColumnMode::Text);
            c->setPlotDesignation(SciDAVis::noDesignation);
        } else {
            c->setProperty("_tg_label_role", false);
            c->setPlotDesignation(static_cast<SciDAVis::PlotDesignation>(roleCode));
        }
    }
    tbl->d_future_table->endMacro();
    tbl->notifyChanges();
}

void ModernRibbonBar::initStatusBarZoomControls()
{
    if (d_shutting_down || !d_app || !d_app->statusBar())
        return;
    QStatusBar *sb = d_app->statusBar();

    const QString statusZoomStyle = QStringLiteral(
            "QToolButton {"
            "  background-color: #FFFFFF;"
            "  color: #334155;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 5px;"
            "  padding: 1px 6px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 9.5pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #93C5FD;"
            "}"
            "QToolButton::menu-indicator { image: none; width: 0px; }");

    d_status_zoom_btn = new QToolButton(sb);
    d_status_zoom_btn->setObjectName(QStringLiteral("StatusBarPageZoomBtn"));
    d_status_zoom_btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    d_status_zoom_btn->setPopupMode(QToolButton::InstantPopup);
    d_status_zoom_btn->setCursor(Qt::PointingHandCursor);
    d_status_zoom_btn->setFocusPolicy(Qt::TabFocus);
    d_status_zoom_btn->setStyleSheet(statusZoomStyle);
    QMenu *zoomMenu = new QMenu(d_status_zoom_btn);
    connect(zoomMenu, &QMenu::aboutToShow, this, [this, zoomMenu]() {
        zoomMenu->clear();
        QAction *fitAct = zoomMenu->addAction(
                IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 16, false),
                tr("Fit to Window"));
        connect(fitAct, &QAction::triggered, this, [this]() {
            if (!d_app)
                return;
            if (MultiLayer *ml = qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport", Qt::DirectConnection))
                    ml->setPageZoom(1.0);
                updateStatusBarZoomIndicator(ml);
            }
        });
        zoomMenu->addSeparator();
        for (int pct : { 50, 75, 100, 125, 150, 200 }) {
            QAction *pa = zoomMenu->addAction(QStringLiteral("%1%").arg(pct));
            connect(pa, &QAction::triggered, this, [this, pct]() {
                if (!d_app)
                    return;
                if (MultiLayer *ml =
                            qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                    ml->setPageZoom(double(pct) / 100.0);
                    updateStatusBarZoomIndicator(ml);
                }
            });
        }
    });
    d_status_zoom_btn->setMenu(zoomMenu);

    d_status_fit_btn = new QToolButton(sb);
    d_status_fit_btn->setObjectName(QStringLiteral("StatusBarFitWindowBtn"));
    d_status_fit_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_status_fit_btn->setIcon(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 14, false));
    d_status_fit_btn->setIconSize(QSize(13, 13));
    d_status_fit_btn->setCursor(Qt::PointingHandCursor);
    d_status_fit_btn->setFocusPolicy(Qt::TabFocus);
    d_status_fit_btn->setStyleSheet(statusZoomStyle);
    connect(d_status_fit_btn, &QToolButton::clicked, this, [this]() {
        if (!d_app)
            return;
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
            if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport", Qt::DirectConnection))
                ml->setPageZoom(1.0);
            updateStatusBarZoomIndicator(ml);
        }
    });

    sb->addPermanentWidget(d_status_zoom_btn);
    sb->addPermanentWidget(d_status_fit_btn);
    d_status_zoom_btn->hide();
    d_status_fit_btn->hide();

    // Command search is already prominent at top-right of the Ribbon; drop redundant bottom Ctrl+K pill.
    sb->installEventFilter(this);
    if (QToolButton *cmdHint =
                sb->findChild<QToolButton *>(QStringLiteral("StatusBarCommandHintBtn"))) {
        sb->removeWidget(cmdHint);
        cmdHint->hide();
        cmdHint->deleteLater();
    }
}

void ModernRibbonBar::updateStatusBarZoomIndicator(MultiLayer *ml)
{
    if (d_shutting_down)
        return;

    if (d_zoom_bound_ml && d_zoom_bound_ml.data() != ml) {
        disconnect(d_zoom_bound_ml.data(), nullptr, this, nullptr);
        d_zoom_bound_ml.clear();
    }
    if (ml && d_zoom_bound_ml.data() != ml) {
        d_zoom_bound_ml = ml;
        QPointer<MultiLayer> mlPtr(ml);
        connect(ml, &MultiLayer::modifiedPlot, this, [this, mlPtr]() {
            if (!d_shutting_down && mlPtr && d_zoom_bound_ml == mlPtr)
                updateStatusBarZoomIndicator(mlPtr.data());
        });
        connect(ml, &MultiLayer::pageZoomChanged, this, [this, mlPtr](double) {
            if (!d_shutting_down && mlPtr && d_zoom_bound_ml == mlPtr)
                updateStatusBarZoomIndicator(mlPtr.data());
        });
    }

    const bool showZoom = (ml != nullptr);
    if (d_status_zoom_btn) {
        if (showZoom) {
            const int pct = qMax(10, qRound(ml->pageZoom() * 100.0));
            d_status_zoom_btn->setText(QStringLiteral("%1%").arg(pct));
            d_status_zoom_btn->setToolTip(
                    tr("Page Zoom: %1% (click to change or fit to window)").arg(pct));
        }
        d_status_zoom_btn->setVisible(showZoom);
    }
    if (d_status_fit_btn) {
        d_status_fit_btn->setText(tr("Fit"));
        d_status_fit_btn->setToolTip(tr("Fit graph page zoom to the current window"));
        d_status_fit_btn->setVisible(showZoom);
    }
}

void ModernRibbonBar::triggerMatrixOrXyzPlot(int kind)
{
    if (d_shutting_down || !d_app)
        return;

    MyWidget *aw = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
    if (Matrix *mat = qobject_cast<Matrix *>(aw)) {
        if (kind == 0)
            d_app->plotColorMap(mat);
        else if (kind == 1)
            d_app->plotContour(mat);
        else
            d_app->plot3DWireSurface();
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
        return;
    }

    Table *tbl = qobject_cast<Table *>(aw);
    if (!tbl && d_focus_hud_mode && d_focus_hud_overlay
        && d_focus_hud_overlay->isTableDrawerVisible() && d_focus_hud_overlay->tableDrawer()) {
        tbl = d_focus_hud_overlay->tableDrawer()->currentTable();
    }
    if (!tbl) {
        if (MultiLayer *activeMl = qobject_cast<MultiLayer *>(aw))
            tbl = ModernRibbonHudInternal::sourceTableFromMultiLayer(activeMl);
    }
    if (!tbl) {
        d_app->showUxHint(tr("Click a Matrix or Worksheet first to create a Heatmap, Contour, or 3D Surface."));
        return;
    }

    // 2-column worksheet for Heatmap / Contour -> launch 2D Density (2D Histogram) directly.
    if (tbl->numCols() < 3 && kind != 2) {
        ExtraPlotTypes::showDensity2DDialog(d_app);
        return;
    }

    auto plotFromMatrix = [this, kind, tbl](Matrix *m) {
        if (!m || !d_app)
            return;
        if (kind == 0) {
            d_app->plotColorMap(m);
        } else if (kind == 1) {
            d_app->plotContour(m);
        } else {
            d_app->activateSubWindow(m);
            d_app->plot3DWireSurface();
        }
        if (tbl)
            ToriUx::dismissPlotThisDataBar(tbl);
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
    };

    auto gridXyzColumnsToMatrix = [this, tbl](int xCol, int yCol, int zCol, int gridN) -> Matrix * {
        if (!tbl || !d_app || xCol < 0 || yCol < 0 || zCol < 0 || xCol >= tbl->numCols()
            || yCol >= tbl->numCols() || zCol >= tbl->numCols()) {
            return nullptr;
        }
        QVector<double> xs, ys, zs;
        const int nRows = tbl->numRows();
        xs.reserve(nRows);
        ys.reserve(nRows);
        zs.reserve(nRows);
        double xMin = 0.0, xMax = 0.0, yMin = 0.0, yMax = 0.0;
        bool hasBounds = false;
        for (int r = 0; r < nRows; ++r) {
            if (tbl->text(r, xCol).trimmed().isEmpty() || tbl->text(r, yCol).trimmed().isEmpty()
                || tbl->text(r, zCol).trimmed().isEmpty()) {
                continue;
            }
            const double xv = tbl->cell(r, xCol);
            const double yv = tbl->cell(r, yCol);
            const double zv = tbl->cell(r, zCol);
            if (!std::isfinite(xv) || !std::isfinite(yv) || !std::isfinite(zv))
                continue;
            xs.append(xv);
            ys.append(yv);
            zs.append(zv);
            if (!hasBounds) {
                xMin = xMax = xv;
                yMin = yMax = yv;
                hasBounds = true;
            } else {
                xMin = qMin(xMin, xv);
                xMax = qMax(xMax, xv);
                yMin = qMin(yMin, yv);
                yMax = qMax(yMax, yv);
            }
        }
        if (!hasBounds || xs.isEmpty()) {
            d_app->showUxHint(tr("No valid finite XYZ numeric rows found in the selected columns."));
            return nullptr;
        }
        if (qFuzzyCompare(xMin, xMax)) {
            xMin -= 0.5;
            xMax += 0.5;
        }
        if (qFuzzyCompare(yMin, yMax)) {
            yMin -= 0.5;
            yMax += 0.5;
        }

        const int nx = qBound(4, gridN, 200);
        const int ny = qBound(4, gridN, 200);
        Matrix *m = d_app->newMatrix(
                d_app->generateUniqueName(tbl->name() + QStringLiteral("_Grid")), ny, nx);
        if (!m)
            return nullptr;
        m->setCoordinates(xMin, xMax, yMin, yMax);
        const double dx = (xMax - xMin) / double(nx - 1);
        const double dy = (yMax - yMin) / double(ny - 1);
        const double invScaleX = 1.0 / qMax(1e-12, xMax - xMin);
        const double invScaleY = 1.0 / qMax(1e-12, yMax - yMin);
        const int nPts = xs.size();
        for (int r = 0; r < ny; ++r) {
            const double gy = yMin + double(r) * dy;
            for (int c = 0; c < nx; ++c) {
                const double gx = xMin + double(c) * dx;
                double wSum = 0.0;
                double vzSum = 0.0;
                bool exact = false;
                double exactVal = 0.0;
                for (int i = 0; i < nPts; ++i) {
                    const double ux = (gx - xs.at(i)) * invScaleX;
                    const double uy = (gy - ys.at(i)) * invScaleY;
                    const double d2 = ux * ux + uy * uy;
                    if (d2 < 1e-18) {
                        exact = true;
                        exactVal = zs.at(i);
                        break;
                    }
                    const double w = 1.0 / d2;
                    wSum += w;
                    vzSum += w * zs.at(i);
                }
                m->setCell(r, c, exact ? exactVal : (wSum > 0.0 ? vzSum / wSum : 0.0));
            }
        }
        return m;
    };

    // Detect default X, Y, Z columns
    int defX = -1, defY = -1, defZ = -1;
    for (int c = 0; c < tbl->numCols(); ++c) {
        const auto pd = tbl->colPlotDesignation(c);
        if (pd == SciDAVis::X && defX < 0)
            defX = c;
        else if (pd == SciDAVis::Y && defY < 0)
            defY = c;
        else if (pd == SciDAVis::Z && defZ < 0)
            defZ = c;
    }
    if (defX < 0 && tbl->numCols() >= 1)
        defX = 0;
    if (defY < 0 && tbl->numCols() >= 2)
        defY = (defX == 0 ? 1 : 0);
    if (defZ < 0 && tbl->numCols() >= 3) {
        for (int c = 0; c < tbl->numCols(); ++c) {
            if (c != defX && c != defY) {
                defZ = c;
                break;
            }
        }
    }

    // Non-interactive callers (tests/scripts): convert automatically without blocking dialog.
    if (qApp && qApp->property("torigraph_noninteractive").toBool()) {
        if (defX >= 0 && defY >= 0 && defZ >= 0) {
            if (Matrix *m = gridXyzColumnsToMatrix(defX, defY, defZ, 32))
                plotFromMatrix(m);
        } else {
            d_app->activateSubWindow(tbl);
            if (Matrix *m = d_app->convertTableToMatrix())
                plotFromMatrix(m);
        }
        return;
    }

    const QString kindTitle = (kind == 0) ? tr("Heatmap")
            : (kind == 1)                 ? tr("Contour")
                                          : tr("3D Surface");
    QDialog dlg(d_app);
    dlg.setWindowTitle(tr("Plot from Worksheet"));
    QVBoxLayout *vlay = new QVBoxLayout(&dlg);
    QLabel *prompt = new QLabel(tr("How should this worksheet be converted for %1?").arg(kindTitle),
                                &dlg);
    prompt->setWordWrap(true);
    vlay->addWidget(prompt);

    QRadioButton *rbXyz = new QRadioButton(
            tr("Grid XYZ columns into a 2D Matrix (recommended for X, Y, Z tables)"), &dlg);
    QRadioButton *rbDirect = new QRadioButton(
            tr("Treat worksheet cells directly as a 2D Z-value grid (Convert to Matrix)"), &dlg);
    QRadioButton *rbDensity = nullptr;
    if (kind != 2) {
        rbDensity = new QRadioButton(
                tr("2D Density Heatmap (count X-Y points on a grid)"), &dlg);
    }

    const bool hasXyzCols = (tbl->numCols() >= 3);
    rbXyz->setEnabled(hasXyzCols);
    if (hasXyzCols)
        rbXyz->setChecked(true);
    else
        rbDirect->setChecked(true);

    vlay->addWidget(rbXyz);
    QWidget *xyzPanel = new QWidget(&dlg);
    QFormLayout *flay = new QFormLayout(xyzPanel);
    flay->setContentsMargins(20, 2, 4, 4);
    QComboBox *cbX = new QComboBox(xyzPanel);
    QComboBox *cbY = new QComboBox(xyzPanel);
    QComboBox *cbZ = new QComboBox(xyzPanel);
    for (int c = 0; c < tbl->numCols(); ++c) {
        const QString lbl = tbl->colLabel(c);
        cbX->addItem(lbl, c);
        cbY->addItem(lbl, c);
        cbZ->addItem(lbl, c);
    }
    if (defX >= 0)
        cbX->setCurrentIndex(defX);
    if (defY >= 0)
        cbY->setCurrentIndex(defY);
    if (defZ >= 0)
        cbZ->setCurrentIndex(defZ);
    QSpinBox *sbGrid = new QSpinBox(xyzPanel);
    sbGrid->setRange(8, 200);
    sbGrid->setValue(40);
    flay->addRow(tr("X column:"), cbX);
    flay->addRow(tr("Y column:"), cbY);
    flay->addRow(tr("Z column:"), cbZ);
    flay->addRow(tr("Grid size:"), sbGrid);
    xyzPanel->setEnabled(rbXyz->isChecked());
    connect(rbXyz, &QRadioButton::toggled, xyzPanel, &QWidget::setEnabled);
    vlay->addWidget(xyzPanel);

    vlay->addWidget(rbDirect);
    if (rbDensity)
        vlay->addWidget(rbDensity);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vlay->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted)
        return;

    if (rbDensity && rbDensity->isChecked()) {
        ExtraPlotTypes::showDensity2DDialog(d_app);
    } else if (rbXyz->isChecked()) {
        if (Matrix *m = gridXyzColumnsToMatrix(cbX->currentData().toInt(),
                                               cbY->currentData().toInt(),
                                               cbZ->currentData().toInt(), sbGrid->value())) {
            plotFromMatrix(m);
        }
    } else {
        d_app->activateSubWindow(tbl);
        if (Matrix *m = d_app->convertTableToMatrix())
            plotFromMatrix(m);
    }
}

void ModernRibbonBar::triggerTable3DPlot(int graph3dType)
{
    if (d_shutting_down || !d_app)
        return;

    MyWidget *aw = qobject_cast<MyWidget *>(d_app->d_workspace.activeSubWindow());
    if (qobject_cast<Matrix *>(aw)) {
        if (graph3dType == Graph3D::Bars)
            d_app->plot3DBars();
        else
            d_app->plot3DScatter();
        return;
    }

    Table *tbl = qobject_cast<Table *>(aw);
    if (!tbl) {
        d_app->showUxHint(tr("Select a worksheet with X, Y, and Z columns first for 3D plotting."));
        return;
    }

    // C3-1: Accept 1 selected Z column, (X, Y, Z) 3-column selection, or automatic Z column lookup.
    int zCol = -1;
    const QStringList sel = tbl->selectedColumns();
    for (const QString &name : sel) {
        const int idx = tbl->colIndex(name);
        if (idx >= 0 && tbl->colPlotDesignation(idx) == SciDAVis::Z) {
            zCol = idx;
            break;
        }
    }
    if (zCol < 0 && sel.size() == 3) {
        const int idx = tbl->colIndex(sel.last());
        if (idx >= 0) {
            tbl->setColPlotDesignation(idx, SciDAVis::Z);
            zCol = idx;
        }
    }
    if (zCol < 0) {
        for (int c = 0; c < tbl->numCols(); ++c) {
            if (tbl->colPlotDesignation(c) == SciDAVis::Z) {
                zCol = c;
                break;
            }
        }
    }
    if (zCol < 0 && tbl->numCols() >= 3) {
        zCol = 2;
        tbl->setColPlotDesignation(zCol, SciDAVis::Z);
    }
    if (zCol >= 0) {
        d_app->dataPlotXYZ(tbl, tbl->colName(zCol), graph3dType);
        ToriUx::dismissPlotThisDataBar(tbl);
        if (d_layout_mode == LayoutSplitTableGraph)
            applySplitWorkspaceGeometry();
        return;
    }

    if (graph3dType == Graph3D::Bars)
        d_app->plot3DBars();
    else
        d_app->plot3DScatter();
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

    // B4-10: Pressing Ctrl+K while already open toggles the palette closed.
    if (d_command_palette && d_command_palette->isVisible()) {
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
        updateGroupCollapse();
        updateTabStackHeight();
    }
    if (watched == d_tab_bar && event) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                // The first click of this double-click already restored the ribbon.
                const bool justRestored = d_restored_by_click_ms >= 0
                        && now - d_restored_by_click_ms
                                <= qint64(QApplication::doubleClickInterval()) + 100;
                d_restored_by_click_ms = -1;
                if (!justRestored)
                    setRibbonMinimized(!d_minimized);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonPress && d_minimized) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && d_tab_bar->tabAt(me->pos()) >= 0) {
                // a single click on a tab of the minimized ribbon brings the commands back
                setRibbonMinimized(false);
                d_restored_by_click_ms = QDateTime::currentMSecsSinceEpoch();
            }
        }
    }
    if (d_app && watched == d_app->statusBar() && event
        && (event->type() == QEvent::ChildAdded || event->type() == QEvent::Show)) {
        if (QStatusBar *sb = d_app->statusBar()) {
            if (QToolButton *cmdHint =
                        sb->findChild<QToolButton *>(QStringLiteral("StatusBarCommandHintBtn"))) {
                sb->removeWidget(cmdHint);
                cmdHint->hide();
                cmdHint->deleteLater();
            }
        }
    }
    if (d_app && watched == d_app->d_workspace.viewport() && event) {
        const QEvent::Type et = event->type();
        if (et == QEvent::Resize && d_layout_mode == LayoutSplitTableGraph
            && !d_applying_split_geometry) {
            applySplitWorkspaceGeometry();
        }
        if (et == QEvent::Resize || et == QEvent::Show) {
            updateEmptyWelcomeHub();
        } else if (et == QEvent::ChildAdded || et == QEvent::ChildRemoved) {
            QChildEvent *ce = static_cast<QChildEvent *>(event);
            if (!d_updating_welcome_hub && ce->child() != d_welcome_hub_card.data())
                updateEmptyWelcomeHub();
        }
    }
    if (d_watched_empty_graph_pane && watched == d_watched_empty_graph_pane && event
        && (event->type() == QEvent::Show || event->type() == QEvent::Resize)) {
        const bool hasTableOnlySplit = (d_layout_mode == LayoutSplitTableGraph && d_split_table
                                        && !d_split_plot && !d_split_table->isHidden());
        if ((d_welcome_hub_card && !d_welcome_hub_card->isHidden()) || hasTableOnlySplit) {
            d_watched_empty_graph_pane->hide();
            return true;
        }
    }
    if (d_welcome_hub_card
        && (watched == d_welcome_hub_card
            || d_welcome_hub_card->isAncestorOf(qobject_cast<QWidget *>(watched)))
        && event) {
        const QEvent::Type et = event->type();
        if (watched == d_welcome_hub_card && (et == QEvent::DragEnter || et == QEvent::Drop)) {
            if (d_app)
                QApplication::sendEvent(d_app.data(), event);
            if (et == QEvent::Drop)
                updateEmptyWelcomeHub();
            return true;
        }
        if (watched == d_welcome_hub_card && et == QEvent::DragMove) {
            QDragMoveEvent *dme = static_cast<QDragMoveEvent *>(event);
            if (dme->mimeData()
                && (dme->mimeData()->hasUrls()
                    || dme->mimeData()->hasFormat(QLatin1String(MultiLayer::kColumnMimeType)))) {
                dme->acceptProposedAction();
                return true;
            }
        }
        if (et == QEvent::ShortcutOverride || et == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_V && (ke->modifiers() & Qt::ControlModifier)
                && !(ke->modifiers() & (Qt::ShiftModifier | Qt::AltModifier))) {
                if (et == QEvent::KeyPress)
                    pasteFromClipboardSmart();
                ke->accept();
                return true;
            }
        }
    }
    if (d_app && event && d_layout_mode == LayoutSplitTableGraph && !d_applying_split_geometry
        && (QApplication::mouseButtons() & Qt::LeftButton) != 0
        && d_split_table && d_split_plot && !d_split_table->isHidden()
        && !d_split_plot->isHidden() && !d_split_table->isMaximized()
        && !d_split_plot->isMaximized()
        && (watched == d_split_table || watched == d_split_plot)) {
        const QEvent::Type et = event->type();
        if (et == QEvent::Resize || et == QEvent::Move) {
            QWidget *vp = d_app->d_workspace.viewport();
            const QRect vpRect = vp ? vp->rect() : d_app->d_workspace.rect();
            if (vpRect.width() >= 280 && vpRect.height() >= 120) {
                const int tileTopY = focusHudTileTopY(vp);
                const int fullH = qMax(100, vpRect.height() - tileTopY);
                const int minPaneW = 120;
                const int maxLeftW = qMax(minPaneW, vpRect.width() - minPaneW);
                int newSplitX = -1;
                if (watched == d_split_table && et == QEvent::Resize) {
                    const int cand = d_split_table->geometry().right() + 1;
                    if (cand >= minPaneW && cand <= maxLeftW
                        && qAbs(cand - d_split_plot->geometry().left()) >= 2) {
                        newSplitX = cand;
                    }
                } else if (watched == d_split_plot) {
                    const int cand = d_split_plot->geometry().left();
                    if (cand >= minPaneW && cand <= maxLeftW
                        && qAbs(cand - (d_split_table->geometry().right() + 1)) >= 2) {
                        newSplitX = cand;
                    }
                }
                if (newSplitX >= minPaneW && newSplitX <= maxLeftW) {
                    QScopedValueRollback<bool> splitGuard(d_applying_split_geometry, true);
                    d_split_left_ratio =
                            qBound(0.15, double(newSplitX) / double(vpRect.width()), 0.85);
                    if (d_split_table->minimumWidth() > newSplitX)
                        d_split_table->setMinimumWidth(newSplitX);
                    const int newRightW = qMax(minPaneW, vpRect.width() - newSplitX);
                    if (d_split_plot->minimumWidth() > newRightW)
                        d_split_plot->setMinimumWidth(newRightW);
                    d_split_table->setGeometry(0, tileTopY, newSplitX, fullH);
                    d_split_plot->setGeometry(newSplitX, tileTopY, newRightW, fullH);
                }
            }
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
