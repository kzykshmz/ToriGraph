# ToriGraph Python Automation SDK (`torigraph`)

`torigraph` は、科学グラフ作成・データ解析ソフトウェア **ToriGraph** の**すべての操作**を Python および AI エージェントからヘッドレス・非対話・決定論的に自動実行するための公式クライアント SDK です。外部ライブラリ依存ゼロ（Python 3.8+ 標準ライブラリのみ）で動作し、ループバック TCP または名前付きパイプ上の JSON-RPC 2.0（**201 メソッド**）を介して C++ 解析・描画エンジンを直接駆動します。

---

## 1. セットアップ / インストール

リポジトリ同梱の `python/` ディレクトリをそのまま開発インストール（または `PYTHONPATH` 追加）して利用できます。

```bash
pip install -e ./python
```

ToriGraph アプリ内の **「Python 自動化コンソール」** や CLI の `--run-python script.py` から実行する場合は、`PYTHONPATH` と `TORIGRAPH_RPC_ENDPOINT` が自動的に設定されるため、インストール不要で即座に `import torigraph as tg` が使えます。

---

## 2. 接続・起動方法

### A. バックグラウンド（ヘッドレス）で新規起動して自動処理

```python
import torigraph as tg

with tg.launch(headless=True, language="ja") as app:
    print(app.info())
    # ブロックを抜けると子プロセスも安全に終了します
```

### B. 起動中の ToriGraph（またはアプリ内コンソール）に接続

```python
import torigraph as tg

# アプリ内コンソール / --run-python 実行時は引数なしで自動接続
app = tg.connect()

# ポート指定で外部スクリプトから接続する場合:
# ToriGraph.exe --rpc-port 18900
# app = tg.connect("tcp://127.0.0.1:18900")
```

### C. コマンドラインからワンショット実行

```bash
ToriGraph.exe --headless --run-python batch_analysis.py
```

---

## 3. 実践コード例

### 例 1: 引張応力-ひずみ曲線 + `0.2%耐力` 自動算出 + ISIJ 85mm 論文プリセット + 高解像度出力

```python
import torigraph as tg

with tg.launch(headless=True) as app:
    # Shift-JIS / UTF-8 自動判定・Instron 2行ヘッダ [Name, Units] / "Stress (MPa)" 単位分離にも標準対応:
    # tbl = app.import_csv("shimadzu_tensile.csv", encoding="auto")

    strain_pct = [0.05 * i for i in range(60)]
    stress_mpa = [
        2050.0 * e if e <= 0.20 else 410.0 + 380.0 * (((e - 0.20) / 100.0 + 0.002) ** 0.20 - 0.002 ** 0.20)
        for e in strain_pct
    ]

    tbl = app.create_table(rows=len(strain_pct), cols=2, name="Tensile")
    tbl.set_data_dict(
        {"Strain": strain_pct, "Stress": stress_mpa},
        units={"Strain": "%", "Stress": "MPa"},
        long_names={"Strain": "Nominal Strain", "Stress": "Nominal Stress"},
    )

    # Matplotlib 互換エイリアス (lw, ls, marker, ms, symbol_skip, xlim, ylim) を直接指定可能
    g = app.plot_xy(tbl, x_col="Strain", y_cols=["Stress"], style="line", lw=1.8, color="#1D4ED8")
    g.isij_style()  # 85mm 幅・内向き目盛・非ボールド Arial フォント一括適用
    g.apply_frame_preset("box_inward")

    # ヤング率 E・0.2%耐力 (sigma_0.2)・引張強さ UTS・破断ひずみを自動解析し補助線を描画
    tensile = g.analyze_tensile(offset=0.002, plot_construction=True, create_true_stress_table=True)
    print(f"E = {tensile['youngs_modulus']:.1f} MPa, sigma_0.2 = {tensile['offset_yield_stress']:.1f} MPa")

    # ズームインセット追加 (show_connectors=True で拡大領域枠 + 引き出し線を描画)
    g.add_inset(x_min=0.0, x_max=0.45, y_min=0.0, y_max=520.0, show_connectors=True)

    # AI エージェント向けワンコール幾何・レイアウト診断 (7種の決定論的レイアウト警告)
    warnings = g.diagnose_layout()
    assert not warnings, f"Layout warnings detected: {warnings}"

    # PNG / PDF / SVG / EMF (Windows ベクター) 出力
    g.export("tensile_isij.png", dpi=600)
    g.export("tensile_isij.pdf", dpi=600)
```

### 例 2: 昇温脱離ガス分析（TDS）の Pseudo-Voigt 2 ピーク波形分離 + ALS ベースライン補正

```python
import math
import torigraph as tg

with tg.launch(headless=True) as app:
    temp = [300.0 + 2.0 * i for i in range(180)]
    rate = [
        0.05 + 0.00015 * (t - 300.0)
        + 2.1 * math.exp(-((t - 415.0) / 22.0) ** 2)
        + 1.3 / (1.0 + ((t - 525.0) / 18.0) ** 2)
        for t in temp
    ]

    tbl = app.create_table(len(temp), 2, "TDS")
    tbl.set_data_dict({"Temp": temp, "Flux": rate}, units={"Temp": "K", "Flux": "mol/(m^2 s)"})
    g = app.plot_xy(tbl, x_col="Temp", y_cols=["Flux"], style="scatter")

    res = app.analyze_peaks(
        graph=g.name,
        profile="PseudoVoigt",     # 全 11 プロファイル対応 (Gauss, Lorentz, Voigt, DoniachSunjic, EMG 等)
        peak_count=2,
        baseline="als",            # none / min / linear / als / anchor
        integrate=True,
        plot_components=True,
    )
    print(f"R^2 = {res['r2']:.5f}")
    for p in res["peaks"]:
        print(f"Peak {p['index']+1}: Tc = {p['center']:.2f} K, Area = {p['area']:.3f}, FWHM = {p['fwhm']:.2f} K")
```

### 例 3: 58 種の組み込み非線形フィット・グローバルフィット・ロバスト回帰・区分線形回帰

```python
with tg.launch(headless=True) as app:
    models = app.list_fit_models()
    print(f"Built-in models: {len(models)}")  # 58+ Origin 互換モデル

    # 任意の組み込みモデルまたはカスタム数式でフィット (境界制約・ブートストラップ信頼区間対応)
    fit = app.fit_nonlinear(
        table="Kinetics",
        x_col="Time",
        y_col="Conversion",
        model="Boltzmann",
        bootstrap_ci=True,
        bootstrap_samples=200,
    )
    print(fit["params"], fit["aic"], fit["bootstrap_ci"])
```

### 例 4: 材料科学専用解析（Arrhenius 拡散活性化エネルギー・Tauc バンドギャップ・EIS Randles 等価回路）

```python
# アレニウスプロット (ln D vs 1000/T -> Ea [eV, kJ/mol], D0)
arr = app.arrhenius(table="Diffusivity", temp_col="Temp_K", rate_col="D_m2s")

# Tauc プロット ((alpha*h*nu)^2 vs h*nu -> 直接遷移バンドギャップ Eg [eV])
tauc = app.tauc(table="UVVis", x_col="Wavelength_nm", alpha_col="Alpha", wavelength_nm=True, direct=True)

# 電気化学インピーダンス分光 (EIS Randles + CPE 半円フィット + Nyquist/Bode 自動描画)
eis = app.eis_fit(table="EIS", freq_col="Freq_Hz", z_re_col="Z_re", z_im_col="Z_im")
```

### 例 5: 1D/2D 信号処理・2D 行列フィルタ・統計解析・3 母数ワイブル解析・画像デジタイザ

```python
# 1D 信号処理: Savitzky-Golay / Whittaker / LOWESS / ゼロ位相 Butterworth IIR / Welch PSD / STFT / Wavelet
app.smooth(table="Signal", x_col="t", y_col="raw", method="whittaker", lambda_val=1e3)
app.iir_filter(table="Signal", x_col="t", y_col="raw", design="butterworth", filter_type="lowpass", cutoff=50.0, zero_phase=True)

# 2D 行列フィルタ & カラーマップ
m = app.create_matrix(64, 64, name="StrainMap", x_min=-5.0, x_max=5.0, y_min=-5.0, y_max=5.0)
m.set_formula("0.02*x - 0.01*y + exp(-(x*x + y*y)/1.5)")
m.filter2d("planar_tilt", in_place=True)
g_map = m.plot_heatmap()
g_map.set_colorbar(palette="viridis", show_contour_lines=True, levels=10, title="Strain")

# 3 母数ワイブル信頼性解析（右打ち切り対応） & Passing-Bablok 法・PCA・ROC
wb = app.weibull(table="Fracture", col="Strength_MPa", censor_col="Suspended", three_param=True)

# 論文画像からの自動曲線数値化 (Graph Digitizer)
extracted = app.digitize(
    "paper_figure.png",
    px_x1=100, py_x1=500, val_x1=0.0,
    px_x2=700, py_x2=500, val_x2=10.0,
    px_y1=100, py_y1=500, val_y1=0.0,
    px_y2=100, py_y2=80,  val_y2=100.0,
    color="#dc1e1e",
    tolerance=45,
    samples=150,
)
```

---

## 4. AI エージェント自己発見・レイアウト診断・汎用エイリアス

### A. 自己発見 API カタログ (`app.describe_api`)

ドキュメントを参照せずに、全 201 RPC メソッドの引数型・デフォルト値・選択肢・Python SDK シグネチャ・呼び出し例を JSON で即時取得できます。

```python
catalog = app.describe_api()                    # 全ドメイン・全 201 メソッド
graph_ops = app.describe_api(domain="graph")    # graph.* ドメインのみ抽出
spec = app.describe_api("graph.inspect")        # 単一メソッドの詳細スキーマ
```

また、存在しないテーブル名・列名・曲線名・スタイル名を渡した場合、`tg.RpcError`（コード `-32602`）の `err.available`（`err.data["available"]`）に有効な候補一覧が返るため、AI エージェントが 1 ターンで自己修正できます。

### B. 1 コール幾何・視覚診断 (`g.inspect()` / `g.diagnose_layout()`)

`g.inspect()` はページ寸法・グリッド・各レイヤのピクセル矩形（`layer_rect_px`, `canvas_rect_px`）・主目盛座標と表示ラベル文字列・曲線の有効点数とクリッピング率（`clipped_fraction`）・凡例矩形とデータ重なり点数（`curve_overlap_points`）・参照線・帯・テキスト/矢印マーカ・ガジェットを一括返却し、以下の **7 種の決定論的レイアウト警告** を自動判定します。

- `EMPTY_CURVE`: 有効データ点が 0 点の曲線が存在する
- `CURVE_CLIPPED_BY_AXIS`: 曲線データの大部分が現在の軸範囲外にクリップされている
- `NONPOSITIVE_DATA_ON_LOG_AXIS`: 対数軸上に `<= 0` のデータ点が含まれている
- `LEGEND_OVERLAPS_DATA`: 凡例ボックスがプロット曲線上のデータ点と重なっている（`g.auto_place_legend()` で解消）
- `TICK_LABEL_COLLISION`: 隣接する軸目盛ラベル同士のピクセル境界が衝突している
- `DISABLED_AXIS_HAS_TITLE_OR_CURVE`: 非表示の軸に軸タイトルまたは曲線が割り当てられている
- `PANEL_CANVAS_SIZE_MISMATCH`: マルチパネル図でパネル間のキャンバス寸法が不揃いになっている（`g.arrange_layers()` で解消）

### C. 実機ラボ CSV / TSV / Rigaku `.ras` インポート & クリップボード貼付

- `app.import_csv(path, encoding="auto", units_row=None)`: UTF-8 / UTF-16 BOM / Shift-JIS (CP932) 自動判定、`#` コメント行スキップ、Instron・島津製作所等の 2 行ヘッダ `[列名, 単位]` 自動昇格、`"Nominal stress (MPa)"` / `"Temperature / K"` からの Long Name + Units 自動分離、欠損値（`""`, `"-"`, `"N/A"`, `"NaN"`）の数値列保持、および Rigaku XRD `.ras` ファイル直接読込に対応。
- `app.paste_tsv(text)` / `tbl.paste_tsv(text)`: TSV/CSV 文字列（省略時は OS クリップボード）から直接ワークシートへ高速貼付。

---

## 5. 全 API ドメイン一覧（201 RPC メソッド）

すべてのメソッドは高水準プロキシメソッド（`TableProxy`, `MatrixProxy`, `GraphProxy` は `Session` メソッドのどこにでも直接渡せます）のほか、`app.call("<domain>.<method>", **params)` またはドット記法 `app.<domain>.<method>(**params)` でも直接呼び出せます。また `app.list_actions()` / `app.trigger_action(name)` により任意の安全な GUI アクション・スロットも呼び出し可能です（モーダルダイアログはヘッドレス保護により `-32602` で即時ブロックされます）。

| ドメイン | 主な RPC メソッド |
|---|---|
| `app.*` | `ping`, `app.info`, `app.supported_methods`, `app.describe_api`, `app.new_project`, `app.open_project`, `app.save_project`, `app.import_csv`, `app.import_opj`, `app.list_windows`, `app.window_info`, `app.activate_window`, `app.rename_window`, `app.close_window`, `app.clone_window`, `app.set_language`, `app.get_language`, `app.set_recalc_mode`, `app.recalculate`, `app.undo`, `app.redo`, `app.set_focus_hud`, `app.get_focus_hud`, `app.set_workspace_mode`, `app.check_updates`, `app.list_actions`, `app.trigger_action`, `app.grab_gui_diagnostic` |
| `table.*` | `create`, `get_info`, `resize`, `add_column`, `remove_column`, `set_column_data`, `get_column_data`, `set_data_dict`, `paste_tsv`, `set_cell`, `get_cell`, `set_column_meta`, `get_column_meta`, `set_formula`, `recalculate`, `mask_rows`, `unmask_rows`, `clear_masks`, `get_masked_rows`, `sort`, `column_stats`, `row_stats`, `export_csv`, `to_matrix` |
| `matrix.*` | `create`, `get_info`, `set_coordinates`, `set_data`, `get_data`, `set_formula`, `transpose`, `invert`, `determinant`, `flip`, `fft2d`, `filter2d`, `analyze_2d_peaks`, `to_table` |
| `plot.*` | `xy`, `stacked_bars`, `add_curve`, `remove_curve`, `add_error_bars`, `double_y`, `waterfall`, `histogram`, `box`, `violin`, `pareto`, `qq`, `pie`, `vectors`, `ternary`, `function`, `matrix`, `3d_matrix`, `3d_xyz`, `3d_function` |
| `graph.*` | `get_info`, `inspect`, `add_layer`, `remove_layer`, `add_linked_layer`, `add_inset_layer`, `arrange_layers`, `merge_graphs`, `set_page_size`, `fit_page_to_layers`, `fit_layers_to_page`, `apply_style_preset`, `apply_frame_preset`, `apply_square_frame`, `set_panel_labels`, `sync_axis_color`, `step_zoom_history`, `drop_columns`, `set_aspect_ratio`, `copy_format`, `paste_format`, `batch_clone`, `set_title`, `set_axis`, `set_axis_break`, `add_reference_line`, `remove_reference_line`, `clear_reference_lines`, `add_shaded_band`, `set_grid`, `set_curve_style`, `set_curve_visible`, `move_curve`, `set_stack_offsets`, `apply_series_scheme`, `apply_palette`, `set_colorbar`, `set_legend`, `auto_place_legend`, `add_text`, `add_arrow`, `remove_marker`, `mask_region`, `clear_masks`, `export`, `copy_image`, `copy_ole`, `set_3d_options` |
| `analysis.*` | `list_fit_models`, `fit_linear`, `fit_york`, `fit_polynomial`, `fit_nonlinear`, `fit_global`, `fit_piecewise`, `fit_robust`, `fit_quantile`, `fit_logistic`, `fit_mlr`, `fit_implicit_odr`, `fit_surface_3d`, `compare_models`, `compare_datasets`, `analyze_peaks`, `batch_peaks` |
| `gadget.*` | `attach`, `set_roi`, `query`, `clear` |
| `signal.*` | `smooth`, `savgol_derivative`, `fft`, `fft_filter`, `iir_filter`, `stft`, `envelope`, `wavelet_cwt`, `wavelet_denoise`, `welch_psd`, `coherence`, `decimate`, `fsd` |
| `curvemath.*` | `eval`, `average_curves`, `normalize`, `integrate`, `differentiate`, `interpolate`, `subtract_line`, `intersections`, `lookup_xy`, `polygon_area`, `fill_between`, `offset_yield`, `arrhenius`, `tauc`, `eis_fit` |
| `stats.*` | `normality`, `outlier`, `anova_oneway`, `anova_twoway`, `nonparametric`, `paired_plot`, `bland_altman`, `passing_bablok`, `weibull`, `kaplan_meier`, `roc`, `pca`, `correlation`, `clustering`, `confidence_ellipse` |
| `digitizer.*` | `extract` |
