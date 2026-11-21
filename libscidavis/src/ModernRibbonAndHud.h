/***************************************************************************
    File                 : ModernRibbonAndHud.h
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
#ifndef MODERN_RIBBON_AND_HUD_H
#define MODERN_RIBBON_AND_HUD_H

#include <QDialog>
#include <QFrame>
#include <QPair>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVector>
#include <functional>

class QAction;
class QButtonGroup;
class QComboBox;
class QEvent;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QMdiSubWindow;
class QMenu;
class QStackedWidget;
class QTabBar;
class QTableView;
class QTableWidget;
class QToolButton;
class QWidget;
class QIdentityProxyModel;

class ApplicationWindow;
class ModernRibbonBar;
class MyWidget;
class Table;

/**
 * \brief Instant live fuzzy-search Command Palette popup (Ctrl+K).
 *
 * Searches across all visible & enabled QActions on ApplicationWindow plus
 * curated quick presets, workspace layout modes, Focus HUD toggle, Python
 * automation tools, update check, and JA/EN language toggle.
 */
class CommandPalettePopup : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPalettePopup(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                 QWidget *parent = nullptr);
    ~CommandPalettePopup() override;

    void retranslateUi();
    void refreshCommands();
    void setFilterText(const QString &query);
    QString filterText() const;
    int visibleCommandCount() const;
    bool executeCurrentSelection();
    bool executeItemAt(int row);

    QLineEdit *searchEdit() const { return d_search_edit; }
    QListWidget *resultsList() const { return d_list_widget; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct CommandEntry
    {
        QString id;
        QString title;
        QString category;
        QString shortcutText;
        QString searchKey;
        QString iconId;
        QPointer<QAction> action;
        std::function<void()> callback;
        int priorityBoost = 0;
    };

    void rebuildFilteredList(const QString &query);
    static int fuzzyMatchScore(const QString &needleLower, const CommandEntry &entry);

    QPointer<ApplicationWindow> d_app;
    QPointer<ModernRibbonBar> d_ribbon;
    QLineEdit *d_search_edit = nullptr;
    QLabel *d_hint_label = nullptr;
    QListWidget *d_list_widget = nullptr;
    QVector<CommandEntry> d_entries;
    QVector<int> d_visible_entry_indices;
};

/**
 * \brief Slide-over floating Worksheet Drawer for F11 Focus HUD Mode.
 *
 * Provides two-way live synchronization with any project Table (auto-selecting
 * the active curve's source Table), safely guarded with QPointer<Table> so
 * closing or deleting a Table while the drawer is open never dangles.
 */
class FocusTableDrawer : public QFrame
{
    Q_OBJECT

public:
    explicit FocusTableDrawer(ApplicationWindow *app, QWidget *parent = nullptr);
    ~FocusTableDrawer() override;

    void prepareForAppShutdown();
    void retranslateUi();
    void refreshTableList(bool autoSelectActiveCurveTable = true);
    void setCurrentTable(Table *table);
    Table *currentTable() const;

    bool setCellValueFromDrawer(int row, int col, const QString &text);

    QComboBox *tableSelector() const { return d_table_combo; }
    QTableView *tableView() const { return d_table_view; }
    QTableWidget *tableWidget() const;

signals:
    void closeDrawerRequested();

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    Table *detectPreferredTable(const QSet<QObject *> &ignoredObjects = QSet<QObject *>()) const;
    void refreshTableListInternal(bool autoSelectActiveCurveTable,
                                  const QSet<QObject *> &ignoredObjects);
    void bindTableSignals(Table *table);
    void syncGridFromTable();
    void syncCompatGrid() const;
    void onGridCellChanged(int row, int col);
    void onCurrentTableDestroyed(QObject *destroyedObj);

    QPointer<ApplicationWindow> d_app;
    QPointer<Table> d_current_table;
    QSet<QObject *> d_dying_tables;
    QLabel *d_title_label = nullptr;
    QComboBox *d_table_combo = nullptr;
    QToolButton *d_btn_new_table = nullptr;
    QToolButton *d_btn_add_col = nullptr;
    QToolButton *d_btn_close = nullptr;
    QLabel *d_empty_label = nullptr;
    QTableView *d_table_view = nullptr;
    QIdentityProxyModel *d_proxy_model = nullptr;
    mutable QTableWidget *d_compat_grid = nullptr;
    mutable bool d_compat_grid_dirty = true;
    bool d_dirty = true;
    bool d_syncing = false;
    bool d_shutting_down = false;
};

/**
 * \brief Stage-2 Non-Redundant Floating Dark Command Pill & Edge Controls for F11 Focus HUD Mode.
 *
 * Visible strictly when ModernRibbonBar::isFocusHudMode() == true.
 * Renders a compact top-center dark pill (darkSurface=true 2-color Duotone SVG icons)
 * plus top-left Worksheet drawer button and top-right Exit Focus (F11) button
 * without blocking mouse/wheel interaction on the underlying graph canvas.
 */
class FocusCanvasHudOverlay : public QFrame
{
    Q_OBJECT

public:
    explicit FocusCanvasHudOverlay(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                   QWidget *viewportParent = nullptr);
    ~FocusCanvasHudOverlay() override;

    void prepareForAppShutdown();
    void retranslateUi();
    void setOverlayActive(bool active);
    bool isOverlayActive() const { return d_active; }

    void setTableDrawerVisible(bool visible);
    bool isTableDrawerVisible() const;

    void repositionOverlayWidgets();
    void refreshActiveContext();
    void setHostViewport(QWidget *host);

    FocusTableDrawer *tableDrawer() const;
    QToolButton *worksheetToggleButton() const;
    QToolButton *exitFocusButton() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void populateQuickPlotMenu();

    QPointer<ApplicationWindow> d_app;
    QPointer<ModernRibbonBar> d_ribbon;
    QPointer<QWidget> d_viewport;

    QToolButton *d_btn_plot = nullptr;
    QMenu *d_plot_menu = nullptr;
    QToolButton *d_btn_peaks = nullptr;
    QToolButton *d_btn_roi = nullptr;
    QToolButton *d_btn_profile2d = nullptr;
    QToolButton *d_btn_isij = nullptr;
    QToolButton *d_btn_slide = nullptr;
    QToolButton *d_btn_ole = nullptr;
    QToolButton *d_btn_palette = nullptr;

    QPointer<QToolButton> d_btn_worksheet;
    QPointer<QToolButton> d_btn_exit_focus;
    QPointer<FocusTableDrawer> d_table_drawer;
    bool d_active = false;
    bool d_shutting_down = false;
};

/**
 * \brief Contextual Single-Row Slim Ribbon + Split Workspace + Focus HUD Controller.
 */
class ModernRibbonBar : public QFrame
{
    Q_OBJECT

public:
    enum WorkspaceLayoutMode {
        LayoutTabbed = 0,
        LayoutSplitTableGraph = 1,
        LayoutFreeMdi = 2
    };
    Q_ENUM(WorkspaceLayoutMode)

    explicit ModernRibbonBar(ApplicationWindow *app, QWidget *parent = nullptr);
    ~ModernRibbonBar() override;

    QSize minimumSizeHint() const override;

    void prepareForAppShutdown();
    void retranslateUi();
    void updateActiveContext(MyWidget *activeWindow);

    void setWorkspaceLayoutMode(WorkspaceLayoutMode mode);
    WorkspaceLayoutMode workspaceLayoutMode() const { return d_layout_mode; }
    void syncWorkspaceModeButtons(WorkspaceLayoutMode mode);
    void syncWorkspaceModeButtons(bool tabbed);
    bool setWorkspaceLayoutModeByName(const QString &modeName);
    QString workspaceLayoutModeName() const;
    void applySplitWorkspaceGeometry();
    double splitLeftRatio() const { return d_split_left_ratio; }
    void setSplitLeftRatio(double ratio);
    bool isApplyingSplitGeometry() const { return d_applying_split_geometry; }

    void setFocusHudMode(bool enabled);
    void toggleFocusHudMode() { setFocusHudMode(!d_focus_hud_mode); }
    bool isFocusHudMode() const { return d_focus_hud_mode; }

    void setClassicToolbarsVisible(bool visible);
    bool areClassicToolbarsVisible() const { return d_classic_toolbars_visible; }
    void syncCheckableStates();

    void triggerPlot2D(int curveType);

    int tabCount() const;
    int currentTabIndex() const;
    void setCurrentTabIndex(int index);
    QString tabTitle(int index) const;
    QTabBar *tabBar() const { return d_tab_bar; }

    QLabel *contextPill() const { return d_context_pill; }
    QString activeContextText() const;

    CommandPalettePopup *commandPalette();
    FocusCanvasHudOverlay *focusHudOverlay() const;
    FocusTableDrawer *focusTableDrawer() const;

public slots:
    void openCommandPalette();

signals:
    void focusHudModeChanged(bool enabled);
    void workspaceLayoutModeChanged(ModernRibbonBar::WorkspaceLayoutMode mode);
    void classicToolbarsToggled(bool visible);
    void openPythonConsoleRequested();
    void runPythonScriptRequested();
    void checkForUpdatesRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct RibbonButtonBinding
    {
        QToolButton *button = nullptr;
        QString iconId;
        std::function<QString()> textFn;
        std::function<QString()> tooltipFn;
        bool isAccentPreset = false;
    };

    struct RibbonMenuActionBinding
    {
        QAction *action = nullptr;
        QString iconId;
        std::function<QString()> textFn;
        std::function<QString()> tooltipFn;
    };

    QWidget *createRibbonScrollPage(QHBoxLayout **outRowLayout);
    QWidget *createRibbonGroup(QHBoxLayout *pageRow, QHBoxLayout **outBtnLayout,
                               bool addDividerAfter = true);
    QToolButton *addRibbonButton(QHBoxLayout *groupLayout, const QString &iconId,
                                 std::function<QString()> textFn,
                                 std::function<QString()> tooltipFn,
                                 std::function<void()> onClicked, bool checkable = false,
                                 bool accentPreset = false);
    QToolButton *addRibbonMenuButton(QHBoxLayout *groupLayout, const QString &iconId,
                                     std::function<QString()> textFn,
                                     std::function<QString()> tooltipFn, QMenu **outMenu,
                                     bool accentPreset = false);
    QAction *addRibbonMenuAction(QMenu *menu, const QString &iconId,
                                 std::function<QString()> textFn,
                                 std::function<QString()> tooltipFn,
                                 std::function<void()> onTriggered, bool checkable = false);
    void buildAllTabs();
    void refreshButtonIcons();
    void updateTabStackHeight();
    void updateHeaderResponsiveness();
    void importOpjInteractive();

    QPointer<ApplicationWindow> d_app;
    QPointer<MyWidget> d_last_active_window;

    QTabBar *d_tab_bar = nullptr;
    QStackedWidget *d_tab_stack = nullptr;

    QToolButton *d_btn_cmd_palette = nullptr;
    QFrame *d_layout_segment_frame = nullptr;
    QButtonGroup *d_layout_btn_group = nullptr;
    QToolButton *d_btn_layout_tabbed = nullptr;
    QToolButton *d_btn_layout_split = nullptr;
    QToolButton *d_btn_layout_mdi = nullptr;
    QToolButton *d_btn_focus_mode = nullptr;
    QToolButton *d_btn_lang_switch = nullptr;
    QLabel *d_context_pill = nullptr;

    QToolButton *d_btn_auto_recalc = nullptr;
    QAction *d_act_explorer = nullptr;
    QAction *d_act_results_log = nullptr;
    QAction *d_act_inspector = nullptr;
    QToolButton *d_btn_lock_aspect = nullptr;
    QToolButton *d_btn_classic_toolbars = nullptr;

    QVector<std::function<QString()>> d_tab_title_fns;
    QVector<RibbonButtonBinding> d_button_bindings;
    QVector<RibbonMenuActionBinding> d_menu_action_bindings;

    WorkspaceLayoutMode d_layout_mode = LayoutTabbed;
    double d_split_left_ratio = 0.40;
    QPointer<Table> d_split_table;
    QPointer<MyWidget> d_split_plot;
    bool d_focus_hud_mode = false;
    bool d_classic_toolbars_visible = false;
    bool d_applying_split_geometry = false;
    bool d_updating_layout_mode = false;
    bool d_shutting_down = false;

    QVector<QPair<QPointer<QWidget>, bool>> d_saved_focus_visibility;
    QPointer<CommandPalettePopup> d_command_palette;
    QPointer<FocusCanvasHudOverlay> d_focus_hud_overlay;
};

#endif // MODERN_RIBBON_AND_HUD_H
