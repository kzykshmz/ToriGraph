/***************************************************************************
    File                 : GraphInspectorDock.h
    Project              : ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : OriginPro-Style Non-Modal Object Manager &
                           Context-Sensitive Live Property Inspector Dock
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
#ifndef GRAPH_INSPECTOR_DOCK_H
#define GRAPH_INSPECTOR_DOCK_H

#include <QColor>
#include <QDockWidget>
#include <QIcon>
#include <QPointer>
#include <QVector>

#include "Graph.h"

class ApplicationWindow;
class MultiLayer;
class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QEvent;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

/**
 * \brief Non-modal right-side OriginPro-style Graph Inspector Dock.
 *
 * Combines:
 *  1) Top Pane: Object Manager (Page -> Layer 1..N -> Curves, Legend, Annotations)
 *     with live curve visibility checkboxes, line+hollow/filled symbol swatches,
 *     Z-order reorder, non-modal Add Curve popover, and 1-click delete.
 *  2) Bottom Pane: Live Context-Sensitive Property Inspector (Page/Layer, Axis,
 *     Curve/Series, Legend/Annotation) guarded by QPointer + QSignalBlocker and
 *     backed by per-MultiLayer snapshot Undo/Redo.
 *  3) Responsive 32 px Vertical Icon Rail mode vs 268 px (240..330 px) Expanded mode.
 */
class GraphInspectorDock : public QDockWidget
{
    Q_OBJECT

public:
    enum InspectorPageIndex {
        PageTabPageLayer = 0,
        PageTabAxis = 1,
        PageTabCurve = 2,
        PageTabLegend = 3
    };

    explicit GraphInspectorDock(ApplicationWindow *app, QWidget *parent = nullptr);
    ~GraphInspectorDock() override;

    void setTargetMultiLayer(MultiLayer *ml, bool forceRefresh = false);
    //! Re-selects the default curve palette in the series scheme combo unless the user has
    //! picked one there (called after the default palette changes).
    void syncSeriesSchemeToDefaultPalette();
    //! Palette currently chosen in the series scheme combo (for tests).
    QString seriesSchemePaletteName() const;
    MultiLayer *targetMultiLayer() const { return d_target_ml.data(); }
    Graph *activeLayer() const;

    void setCollapsedRail(bool collapsed);
    bool isCollapsedRail() const { return d_collapsed_rail; }

    void setUserPinned(bool pinned);
    bool isUserPinned() const { return d_user_pinned; }

    void switchToInspectorPage(InspectorPageIndex pageIdx);
    InspectorPageIndex currentInspectorPage() const;

    void refreshAll();
    void refreshObjectTree();
    void syncInspectorFromSelection();

    void applyTheme();
    void retranslateUi();

    QTreeWidget *objectTree() const { return d_object_tree; }
    QStackedWidget *propertyStack() const { return d_prop_stack; }
    QWidget *quickStartPanel() const { return d_empty_quickstart_panel; }
    bool isShowingQuickStart() const;

    static bool hasZeroReferenceLine(const Graph *g, int orientation);
    static void toggleZeroReferenceLine(Graph *g, int orientation);
    static void addReferenceLineInteractive(Graph *g, int defaultOrientation = 0);

signals:
    void collapsedStateChanged(bool collapsed);
    void userPinToggled(bool pinned);
    void closeRequestedByUser();

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    QWidget *buildQuickStartEmptyState();
    QWidget *buildPageLayerInspector();
    QWidget *buildAxisInspector();
    QWidget *buildCurveInspector();
    QWidget *buildLegendInspector();

    void connectMultiLayerSignals(MultiLayer *ml);
    void bindLayerSignals(Graph *g);

    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    //! C2: open the full dialog for the visible inspector section ("Details..." button)
    void openDetailsDialogForPage();
    void onObjectTreeContextMenu(const QPoint &pos);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onGraphSelectionChanged(MultiLayer *page, Graph *layer, const Graph::GraphSelection &sel);

    void openAddCurvePopover();
    void moveSelectedCurve(int delta);
    void removeSelectedObject();

    void syncPageLayerControls();
    void syncAxisControls();
    void syncCurveControls();
    void syncLegendControls();

    void applyAxisScaleFromUi(int mergeId = -1);
    void applyAxisBreakFromUi();
    void applyTickFormatFromUi(int mergeId = -1);
    void applyTickDivideFromUi();
    void refreshCategoryColumnCombo();
    void applyCategoryColumnFromUi();
    void applyAxisTitleMarkup(const QString &openTag, const QString &closeTag);
    void insertAxisTitleToken(const QString &token);
    void rebuildGreekSymbolMenu();
    void rebuildUnitsSymbolMenu();
    void applyQuickErrorBars(int mode /*0=Adjacent, 1=SD, 2=5pct*/);
    void matchRightYAxisColorToCurve();
    void refreshPaletteSchemeCombo();
    void applyGroupAutoStyleFromUi();
    QVector<Graph *> targetLayersForScope() const;

    static QIcon buildCurveSwatchIcon(Graph *g, int curveIndex);
    static QColor pickColorInteractive(QWidget *parent, const QColor &initial, const QString &title);

    QPointer<ApplicationWindow> d_app;
    QPointer<MultiLayer> d_target_ml;
    QVector<QPointer<Graph>> d_bound_layers;
    QTimer *d_coalesce_refresh_timer = nullptr;

    bool d_collapsed_rail = false;
    bool d_user_pinned = false;
    bool d_syncing_tree = false;
    bool d_syncing_props = false;
    bool d_scope_all_panels = false;
    int d_active_axis = QwtPlot::xBottom;

    // Root containers (Expanded vs 40 px Rail)
    QWidget *d_root_container = nullptr;
    QStackedWidget *d_mode_stack = nullptr;
    QWidget *d_expanded_panel = nullptr;
    QFrame *d_rail_panel = nullptr;

    // Rail buttons (40 px width)
    QToolButton *d_rail_expand_btn = nullptr;
    QToolButton *d_rail_page_btn = nullptr;
    QToolButton *d_rail_axis_btn = nullptr;
    QToolButton *d_rail_curve_btn = nullptr;
    QToolButton *d_rail_legend_btn = nullptr;

    // Expanded Header & Multi-Panel Scope Bar
    QFrame *d_header_bar = nullptr;
    QLabel *d_header_title_lbl = nullptr;
    QLabel *d_header_target_lbl = nullptr;
    QToolButton *d_btn_pin = nullptr;
    QToolButton *d_btn_collapse = nullptr;

    // Content stack: 0 = Beginner Quick Start (no active graph), 1 = Active Graph Inspector
    QStackedWidget *d_content_stack = nullptr;
    QWidget *d_empty_quickstart_panel = nullptr;
    QWidget *d_inspector_active_panel = nullptr;

    // Quick Start Empty State Widgets
    QLabel *d_qs_title_lbl = nullptr;
    QLabel *d_qs_subtitle_lbl = nullptr;
    QLabel *d_qs_step1_lbl = nullptr;
    QLabel *d_qs_step2_lbl = nullptr;
    QLabel *d_qs_step3_lbl = nullptr;
    QPushButton *d_btn_qs_import_csv = nullptr;
    QPushButton *d_btn_qs_new_graph = nullptr;
    QPushButton *d_btn_qs_sample_gallery = nullptr;

    QFrame *d_scope_strip = nullptr;
    QLabel *d_lbl_scope = nullptr;
    QButtonGroup *d_scope_group = nullptr;
    QToolButton *d_btn_scope_active = nullptr;
    QToolButton *d_btn_scope_all = nullptr;

    // Splitter: Top Object Manager + Bottom Inspector
    QSplitter *d_splitter = nullptr;

    // Top Pane: Object Manager
    QFrame *d_tree_header_strip = nullptr;
    QLabel *d_obj_mgr_title_lbl = nullptr;
    QToolButton *d_btn_add_curve = nullptr;
    QToolButton *d_btn_move_up = nullptr;
    QToolButton *d_btn_move_down = nullptr;
    QToolButton *d_btn_remove_obj = nullptr;
    QTreeWidget *d_object_tree = nullptr;
    QLabel *d_empty_hint_lbl = nullptr;

    // Bottom Pane: Context Tabs + Scrollable Stacked Inspector
    QFrame *d_tab_strip = nullptr;
    QButtonGroup *d_page_tab_group = nullptr;
    QToolButton *d_tab_btn_page = nullptr;
    QToolButton *d_tab_btn_axis = nullptr;
    QToolButton *d_tab_btn_curve = nullptr;
    QToolButton *d_tab_btn_legend = nullptr;
    QScrollArea *d_prop_scroll = nullptr;
    QStackedWidget *d_prop_stack = nullptr;

    // --- Page 0: Page / Layer Controls ---
    QLabel *d_sec_presets_lbl = nullptr;
    QPushButton *d_btn_preset_isij = nullptr;
    QPushButton *d_btn_preset_slide28 = nullptr;
    QComboBox *d_cb_user_presets = nullptr;
    QPushButton *d_btn_apply_user_preset = nullptr;
    QPushButton *d_btn_manage_presets = nullptr;
    QPushButton *d_btn_square_frame70 = nullptr;

    QLabel *d_sec_aspect_lbl = nullptr;
    QCheckBox *d_chk_keep_aspect = nullptr;
    QPushButton *d_btn_ratio_1_1 = nullptr;
    QPushButton *d_btn_ratio_4_3 = nullptr;
    QPushButton *d_btn_ratio_16_9 = nullptr;
    QPushButton *d_btn_ratio_3_4 = nullptr;
    QPushButton *d_btn_ratio_free = nullptr;

    QLabel *d_sec_page_size_lbl = nullptr;
    QLabel *d_lbl_page_w = nullptr;
    QDoubleSpinBox *d_spin_page_w_mm = nullptr;
    QLabel *d_lbl_page_h = nullptr;
    QDoubleSpinBox *d_spin_page_h_mm = nullptr;
    QLabel *d_lbl_page_dpi = nullptr;
    QSpinBox *d_spin_page_dpi = nullptr;
    QComboBox *d_combo_page_unit = nullptr;
    int inspectorPageUnit() const; // MultiLayer::PageUnit
    void configurePageSizeSpins(int unit, double wMm, double hMm);
    QCheckBox *d_chk_page_view = nullptr;
    QLabel *d_lbl_page_zoom = nullptr;
    QComboBox *d_combo_page_zoom = nullptr;

    QLabel *d_sec_layer_frame_lbl = nullptr;
    QPushButton *d_btn_box_inward = nullptr;
    QPushButton *d_btn_link_right_y = nullptr;
    QToolButton *d_btn_link_top_x = nullptr;
    QMenu *d_menu_link_top_x = nullptr;
    QCheckBox *d_chk_panel_labels = nullptr;
    QComboBox *d_combo_panel_pos = nullptr;

    QLabel *d_sec_export_lbl = nullptr;
    QPushButton *d_btn_copy_ole = nullptr;
    QPushButton *d_btn_export_img_pdf = nullptr;

    // --- Page 1: Axis Controls ---
    QButtonGroup *d_axis_sel_group = nullptr;
    QToolButton *d_btn_axis_bottom = nullptr;
    QToolButton *d_btn_axis_left = nullptr;
    QToolButton *d_btn_axis_top = nullptr;
    QToolButton *d_btn_axis_right = nullptr;
    QCheckBox *d_chk_axis_visible = nullptr;
    QCheckBox *d_chk_tick_labels = nullptr;

    QLabel *d_sec_axis_range_lbl = nullptr;
    QLabel *d_lbl_axis_min = nullptr;
    QLineEdit *d_edit_axis_min = nullptr;
    QLabel *d_lbl_axis_max = nullptr;
    QLineEdit *d_edit_axis_max = nullptr;
    QLabel *d_lbl_axis_step = nullptr;
    QLineEdit *d_edit_axis_step = nullptr;
    QPushButton *d_btn_axis_autoscale = nullptr;
    QLabel *d_lbl_axis_scale_type = nullptr;
    QComboBox *d_combo_axis_scale = nullptr;
    QCheckBox *d_chk_axis_reverse = nullptr;
    QLabel *d_lbl_minor_subdiv = nullptr;
    QComboBox *d_combo_minor_subdiv = nullptr;

    QCheckBox *d_chk_axis_break = nullptr;
    QLabel *d_lbl_break_from = nullptr;
    QLineEdit *d_edit_break_from = nullptr;
    QLabel *d_lbl_break_to = nullptr;
    QLineEdit *d_edit_break_to = nullptr;
    QLabel *d_lbl_break_scale_before = nullptr;
    QComboBox *d_combo_break_scale_before = nullptr;
    QLabel *d_lbl_break_scale_after = nullptr;
    QComboBox *d_combo_break_scale_after = nullptr;

    QLabel *d_sec_ref_lines_lbl = nullptr;
    QPushButton *d_btn_ref_zero_y = nullptr;
    QPushButton *d_btn_ref_zero_x = nullptr;
    QPushButton *d_btn_ref_add_custom = nullptr;

    QLabel *d_sec_axis_ticks_lbl = nullptr;
    QLabel *d_lbl_tick_dir = nullptr;
    QComboBox *d_combo_tick_dir = nullptr;
    QLabel *d_lbl_tick_format = nullptr;
    QComboBox *d_combo_tick_format = nullptr;
    QLabel *d_lbl_tick_prec = nullptr;
    QSpinBox *d_spin_tick_prec = nullptr;
    QLabel *d_lbl_tick_divide = nullptr;
    QComboBox *d_combo_tick_divide = nullptr;
    QLabel *d_lbl_category_col = nullptr;
    QComboBox *d_combo_category_col = nullptr;
    QCheckBox *d_chk_major_grid = nullptr;
    QCheckBox *d_chk_minor_grid = nullptr;

    QLabel *d_sec_axis_title_lbl = nullptr;
    QLabel *d_lbl_unit_format = nullptr;
    QComboBox *d_combo_unit_format = nullptr;
    QLineEdit *d_edit_axis_title = nullptr;
    QToolButton *d_btn_fmt_italic = nullptr;
    QToolButton *d_btn_fmt_sub = nullptr;
    QToolButton *d_btn_fmt_sup = nullptr;
    QToolButton *d_btn_fmt_greek = nullptr;
    QMenu *d_menu_greek = nullptr;
    QToolButton *d_btn_fmt_unit = nullptr;
    QMenu *d_menu_units = nullptr;
    QCheckBox *d_chk_lock_all_axes_font = nullptr;
    QLabel *d_lbl_title_pt = nullptr;
    QDoubleSpinBox *d_spin_axis_title_pt = nullptr; // pt, 0.1 steps shown (12.9)
    QLabel *d_lbl_tick_pt = nullptr;
    QDoubleSpinBox *d_spin_axis_tick_pt = nullptr;

    // --- Page 2: Curve / Series / ErrorBar / Heatmap Controls ---
    QLabel *d_lbl_active_curve_name = nullptr;
    QWidget *d_curve_regular_wrap = nullptr;
    QLabel *d_lbl_plot_style = nullptr;
    QComboBox *d_combo_curve_style = nullptr;

    QLabel *d_sec_line_style_lbl = nullptr;
    QLabel *d_lbl_line_color = nullptr;
    QPushButton *d_btn_line_color = nullptr;
    QLabel *d_lbl_line_width = nullptr;
    QDoubleSpinBox *d_spin_line_width = nullptr;
    QLabel *d_lbl_line_pen = nullptr;
    QComboBox *d_combo_line_pen = nullptr;
    QLabel *d_lbl_curve_opacity = nullptr;
    QSpinBox *d_spin_curve_opacity = nullptr;

    QLabel *d_sec_symbol_style_lbl = nullptr;
    QLabel *d_lbl_sym_shape = nullptr;
    QComboBox *d_combo_sym_shape = nullptr;
    QLabel *d_lbl_sym_size = nullptr;
    QSpinBox *d_spin_sym_size = nullptr;
    QLabel *d_lbl_sym_skip = nullptr;
    QSpinBox *d_spin_sym_skip = nullptr;
    QCheckBox *d_chk_sym_hollow = nullptr;
    QCheckBox *d_chk_link_sym_color = nullptr;
    QWidget *d_sym_custom_colors_wrap = nullptr;
    QLabel *d_lbl_sym_edge_color = nullptr;
    QPushButton *d_btn_sym_edge_color = nullptr;
    QLabel *d_lbl_sym_fill_color = nullptr;
    QPushButton *d_btn_sym_fill_color = nullptr;
    QLabel *d_lbl_sym_edge_width = nullptr;
    QDoubleSpinBox *d_spin_sym_edge_width = nullptr;
    QCheckBox *d_chk_curve_right_y = nullptr;
    QPushButton *d_btn_match_right_y_color = nullptr;

    QLabel *d_sec_err_bars_lbl = nullptr;
    QPushButton *d_btn_err_adjacent = nullptr;
    QPushButton *d_btn_err_sd = nullptr;
    QPushButton *d_btn_err_5pct = nullptr;
    QWidget *d_err_style_wrap = nullptr;
    QLabel *d_sec_err_style_lbl = nullptr;
    QLabel *d_lbl_err_width = nullptr;
    QSpinBox *d_spin_err_width = nullptr;
    QLabel *d_lbl_err_cap = nullptr;
    QSpinBox *d_spin_err_cap = nullptr;
    QLabel *d_lbl_err_dir = nullptr;
    QComboBox *d_combo_err_dir = nullptr;
    QLabel *d_lbl_err_color = nullptr;
    QPushButton *d_btn_err_color = nullptr;
    QPushButton *d_btn_err_match_curve = nullptr;
    QCheckBox *d_chk_err_band = nullptr;
    QLabel *d_lbl_err_band_opacity = nullptr;
    QSpinBox *d_spin_err_band_opacity = nullptr;

    // Color-Mapped Scatter (PointColorMap) Controls
    QWidget *d_point_cmap_wrap = nullptr;
    QLabel *d_sec_pcm_lbl = nullptr;
    QLabel *d_lbl_pcm_cmap = nullptr;
    QComboBox *d_combo_pcm_cmap = nullptr;
    QCheckBox *d_chk_pcm_reverse = nullptr;
    QCheckBox *d_chk_pcm_colorbar = nullptr;
    QLabel *d_lbl_pcm_cb_title = nullptr;
    QLineEdit *d_edit_pcm_cb_title = nullptr;
    QCheckBox *d_chk_pcm_custom_z = nullptr;
    QLabel *d_lbl_pcm_zmin = nullptr;
    QLineEdit *d_edit_pcm_zmin = nullptr;
    QLabel *d_lbl_pcm_zmax = nullptr;
    QLineEdit *d_edit_pcm_zmax = nullptr;

    QLabel *d_sec_series_scheme_lbl = nullptr;
    QComboBox *d_combo_series_scheme = nullptr;
    bool d_series_scheme_user_chosen = false; //!< user picked a scheme in the combo
    QCheckBox *d_chk_cycle_sym_shapes = nullptr;
    QCheckBox *d_chk_paired_hollow = nullptr;
    QPushButton *d_btn_apply_scheme_all = nullptr;

    QLabel *d_sec_y_stack_lbl = nullptr;
    QLabel *d_lbl_stack_gap = nullptr;
    QDoubleSpinBox *d_spin_stack_gap = nullptr;
    QPushButton *d_btn_apply_stack = nullptr;
    QPushButton *d_btn_reset_stack = nullptr;

    // Spectrogram / Heatmap Color Scale Controls
    QWidget *d_spectrogram_wrap = nullptr;
    QLabel *d_sec_spec_lbl = nullptr;
    QLabel *d_lbl_spec_cmap = nullptr;
    QComboBox *d_combo_spec_cmap = nullptr;
    QCheckBox *d_chk_spec_reverse = nullptr;
    QCheckBox *d_chk_spec_colorbar = nullptr;
    QLabel *d_lbl_spec_cb_title = nullptr;
    QLineEdit *d_edit_spec_cb_title = nullptr;
    QWidget *d_spec_cb_sym_bar = nullptr;
    QLabel *d_lbl_spec_cb_width = nullptr;
    QSpinBox *d_spin_spec_cb_width = nullptr;
    QCheckBox *d_chk_spec_contours = nullptr;
    QLabel *d_lbl_spec_levels = nullptr;
    QSpinBox *d_spin_spec_levels = nullptr;
    QCheckBox *d_chk_spec_custom_z = nullptr;
    QLabel *d_lbl_spec_zmin = nullptr;
    QLineEdit *d_edit_spec_zmin = nullptr;
    QLabel *d_lbl_spec_zmax = nullptr;
    QLineEdit *d_edit_spec_zmax = nullptr;

    // --- Page 3: Legend / Annotation Controls ---
    QLabel *d_lbl_legend_target = nullptr;
    QLabel *d_sec_legend_pos_lbl = nullptr;
    QPushButton *d_btn_leg_tl = nullptr;
    QPushButton *d_btn_leg_tc = nullptr;
    QPushButton *d_btn_leg_tr = nullptr;
    QPushButton *d_btn_leg_bl = nullptr;
    QPushButton *d_btn_leg_br = nullptr;
    QPushButton *d_btn_leg_out_r = nullptr;
    QPushButton *d_btn_leg_auto_avoid = nullptr;
    QPushButton *d_btn_leg_rebuild = nullptr;

    QLabel *d_sec_legend_style_lbl = nullptr;
    QLabel *d_lbl_leg_frame = nullptr;
    QComboBox *d_combo_leg_frame = nullptr;
    QLabel *d_lbl_leg_header = nullptr;
    QLineEdit *d_edit_leg_header = nullptr;
    // C16: text annotations can have several lines, so they get a multi-line editor.
    QPlainTextEdit *d_edit_annot_text = nullptr;
    QTimer *d_annot_commit_timer = nullptr;
    QPointer<Graph> d_annot_edit_layer;
    int d_annot_edit_id = -1;
    void flushPendingAnnotationEdit();
    QWidget *d_leg_sym_bar = nullptr;
    QLabel *d_lbl_leg_cols = nullptr;
    QSpinBox *d_spin_leg_cols = nullptr;
    QLabel *d_lbl_leg_font_pt = nullptr;
    QDoubleSpinBox *d_spin_leg_font_pt = nullptr;
};

#endif // GRAPH_INSPECTOR_DOCK_H
