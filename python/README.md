# ToriGraph Python Automation SDK (`torigraph`)

`torigraph` は、科学グラフ作成・データ解析ソフトウェア **ToriGraph** の**すべての操作**を Python スクリプトおよび AI エージェントからヘッドレス・非対話・決定論的に自動実行するための公式クライアント SDK です。

- **コア依存ゼロ**: `import torigraph as tg` は Python 3.8+ 標準ライブラリのみで即時ロードされます。
- **NumPy / Pandas / Polars / `pathlib.Path` ゼロフリクション対応**: `np.ndarray`, `pd.DataFrame`（`df.attrs["units"]` / `"long_names"` 自動保持・往復変換）, `pl.DataFrame` をダックタイピングで直接入出力できます。
- **Zero-COM PowerPoint ネイティブ OLE デッキ生成 (`to_pptx`)**: `POWERPNT.EXE` や OS クリップボードに一切触れずに、PowerPoint 上でダブルクリック再編集可能な `ToriGraph.Graph.1` ベクター OLE スライド（`.pptx`）を生成・差分更新します。
- **AI エージェント自律機能**: 単一往復・失敗時自動ロールバックの `with app.batch():`、自己修復レイアウト `g.auto_layout()`、グラフからの再現スクリプト逆生成 `g.to_python_script()`、および全 RPC 自己発見カタログ `app.describe_api()` を備えています。

---

## 1. セットアップ / インストール

リポジトリ同梱の `python/` ディレクトリを開発インストール（または `PYTHONPATH` 追加）して利用できます。

```bash
pip install -e ./python
# PowerPoint ネイティブ OLE デッキ出力 (to_pptx) を利用する場合:
pip install python-pptx
```

ToriGraph アプリ内の **「Python 自動化コンソール」** や CLI の `--run-python script.py` から実行する場合は、`PYTHONPATH` と `TORIGRAPH_RPC_ENDPOINT` が自動設定されるため、インストール不要で即座に `import torigraph as tg` が使えます。

---

## 2. 接続・起動方法

```python
import torigraph as tg

# A. ヘッドレス子プロセスを新規起動（with ブロック終了時に自動クリーンアップ）
with tg.launch(headless=True, language="ja") as app:
    print(app.info())

# B. 既存セッションがあれば接続、無ければヘッドレス起動（再現スクリプトに最適）
with tg.connect_or_launch() as app:
    print(app.supported_methods()[:5])

# C. 起動中の ToriGraph（アプリ内コンソールや --rpc-port 指定プロセス）に接続
# app = tg.connect("tcp://127.0.0.1:18900")
```

### 待ち時間と版の照合

- `launch(timeout=30)` の `timeout` は起動待ち(接続できるまで)の秒数です。各呼び出しの応答待ちは別で、既定は `tg.DEFAULT_RPC_TIMEOUT`(120 秒)です。
- 重い処理だけ待ち時間を延ばすときは `app.call("analysis.xxx", ..., rpc_timeout=600)`、セッション全体なら `tg.launch(rpc_timeout=600)` または `app.rpc_timeout = 600` とします。
- 応答は JSON-RPC の `id` で照合します。待ち時間を過ぎた場合、そのセッションは使えなくなり(遅れて届いた応答を次の呼び出しの結果と取り違えないため)、以後の呼び出しは `TransportError` になります。接続し直してください。
- 接続時にアプリと SDK の版を照合し、違えば `tg.ToriGraphVersionWarning` を出します(`check_version=False` で省略)。アプリに同梱の SDK(`scripts/install-python-sdk.ps1`)を入れ直すと一致します。

### テスト

```bash
pip install -e "./python[test]"
pytest python/tests -m "not app"      # SDK だけのテスト(ToriGraph 不要)
pytest python/tests -m "not slow"     # 実機テストも含む(TORIGRAPH_EXE を設定)
pytest python/tests                   # 長い統合テスト(slow)も含めた全部
```

コマンドラインからのワンショット実行:

```bash
ToriGraph.exe --headless --run-python batch_analysis.py
```

---

## 3. 実践コード例

### 例 1: Pandas / NumPy 直接プロット + 引張 `0.2%耐力` & Kocks–Mecking 加工硬化率解析

```python
import numpy as np
import pandas as pd
import torigraph as tg

with tg.launch(headless=True) as app:
    strain = np.linspace(0.0, 0.16, 161)
    stress = np.where(
        strain <= 0.004,
        71000.0 * strain,
        284.0 + 310.0 * (1.0 - np.exp(-18.0 * (strain - 0.004))),
    )

    df = pd.DataFrame({"Strain": strain * 100.0, "Stress": stress})
    df.attrs["units"] = {"Strain": "%", "Stress": "MPa"}
    df.attrs["long_names"] = {"Strain": "Nominal strain, e", "Stress": "Nominal stress, σ"}

    # DataFrame から直接プロット（単位・Long Name も自動反映）
    g = app.plot_xy(df, "Strain", "Stress", style="line", lw=1.8, color="#0072B2")
    g.isij_style()  # ISIJ 85mm 幅・内向き目盛・非ボールド Arial 一括適用

    # ヤング率 E・0.2%耐力・引張強さ UTS + Kocks-Mecking (dσ_t/dε_t vs σ_t または Considère) 加工硬化解析
    tensile = g.analyze_tensile(offset=0.002, plot_construction=True)
    km = g.kocks_mecking(smooth_window=11, create_plot=True, plot_mode="kocks_mecking")
    print(f"sigma_0.2 = {tensile['offset_yield_stress']:.1f} MPa, KM = {km}")

    # ワークシートをメタデータ付き DataFrame / NumPy 配列として取得
    tbl = app.table(g.info()["layers"][0]["curves"][0]["table"])
    df_roundtrip = tbl.to_dataframe()
    assert df_roundtrip.attrs["units"]["Stress"] == "MPa"
```

### 例 2: Zero-COM PowerPoint ネイティブ編集可能 OLE スライド生成・差分更新 (`to_pptx`)

`POWERPNT.EXE`（開いているプレゼン画面）や OS クリップボードに一切干渉せず、OpenXML + Win32 `StgCreateDocfile`（`ToriGraph.Graph.1`, CLSID `{8E6B9541-3C74-4D28-A915-7D84E5A12001}`）によって、PowerPoint 上でダブルクリックすると ToriGraph で直接再編集できるベクター EMF 付き OLE オブジェクトを埋め込みます。

```python
from pathlib import Path
import torigraph as tg

with tg.launch(headless=True) as app:
    # 組み込み材料科学サンプルデータ（引張・水素TDS・XRD・拡散・CTひずみ・ワイブル）をロード
    s1 = app.load_sample_dataset("tensile_alznmg", auto_plot=True)
    s2 = app.load_sample_dataset("xrd_aged_alloy", auto_plot=True)
    g1, g2 = app.graph(s1["graph"]), app.graph(s2["graph"])

    # 1. 新規 16:9 スライドへ自動グリッド配置 (per_slide = 1 | 2 | 3 | 4 | 6)
    app.to_pptx(
        Path("weekly_report.pptx"),
        [g1, g2],
        per_slide=2,
        preset="slide_28pt",
        titles=["Al-Zn-Mg 時効材の引張特性と XRD プロファイル"],
    )

    # 2. 既存デッキ内の埋め込み済み OLE 図形 (alt_text="torigraph:<graph_name>") をべき等更新
    #    PowerPoint 側で手動調整した位置・サイズ・重なり順 (Z-order) を維持したまま中身だけ更新！
    #    (deck == path の場合は安全保護のため overwrite=True が必須)
    app.to_pptx("weekly_report.pptx", [g1, g2], deck="weekly_report.pptx", replace=True, overwrite=True)

    # 3. テンプレート上の名前付きプレースホルダ図形を置換、または cm 座標ボックス指定
    # app.to_pptx("out.pptx", [g1], deck="template.pptx", replace=[(2, "Picture 3")])
    # g1.to_pptx("single_slide.pptx", box=(2.5, 3.0, 14.0))  # (left_cm, top_cm, width_cm)
```

### 例 3: アトミック一括実行 (`app.batch`)・自己修復レイアウト (`auto_layout`)・Python コード逆生成

```python
import torigraph as tg

with tg.launch(headless=True) as app:
    # with ブロック内の全プロキシ操作を 1 回の JSON-RPC 往復に集約し、再描画と Undo を統合
    # 途中でエラーが発生した場合は atomic=True によりワークスペース全体が自動ロールバックされます
    with app.batch(atomic=True, defer_replot=True) as b:
        t = app.create_table(rows=5, cols=2)
        t.set_data_dict({"Condition": ["As-Q", "120°C", "160°C", "RRA", "OA"], "HV": [95, 182, 168, 195, 152]})
        g = app.plot_bars(t, x_col="Condition", y_cols="HV")
        g.isij_style()
        g.add_reference_line(value=180.0, orientation="horizontal", label="Target HV")

    # はみ出し・目盛衝突・凡例/コールアウト重なり・パネル不揃いを 1 コールで自動解消
    g.auto_layout(pad_fraction=0.05, allow_outside_legend=True)

    # 現在のグラフからスタンドアロン実行可能な Python スクリプトを逆生成
    script_src = g.to_python_script("reproduce_hv.py", standalone=True, embed_data="inline")
```

### 例 4: 材料科学・水素 TDS・XRD Williamson–Hall・VASP 電子状態ワークベンチ

```python
with tg.launch(headless=True) as app:
    # 1. カテゴリ横軸バー / 100% 積み上げ棒グラフ / 極座標（集合組織・異方性）プロット
    g_stk = app.plot_stacked_bars(tbl_phase, x_col="Temper", y_cols=["Alpha", "Eta"], percent_100=True)
    g_pol = app.plot_polar(tbl_tex, theta_col="Azimuth_deg", r_cols=["Intensity"], theta_in_radians=False)

    # 2. Kissinger 昇温脱離分析 (TDS / DSC) トラップ活性化エネルギー Ed [kJ/mol, eV]
    kis = app.analyze_kissinger(
        tbl_tds,
        rate_col="HeatingRate_K_min",
        temp_col="PeakTemp_C",
        rate_in_k_per_hour=False,
        temp_in_celsius=True,
        top_axis_in_celsius=True,
    )

    # 3. XRD Bragg d 値上軸追加・Scherrer 結晶子径・Williamson-Hall 微視ひずみ解析 (fcc/bcc/sc/hcp 対応)
    xrd = g_xrd.analyze_xrd(
        wavelength=1.540598,       # Cu Kα1 (または xray_energy_kev=12.4 等で放射光対応)
        instr_fwhm=0.05,
        crystal_system="fcc",      # fcc / bcc / sc / hcp (c_over_a=1.587 等) 自動指数付け
        add_top_d_axis=True,
        create_wh_plot=True,
    )

    # 4. VASP / 第一原理計算 スピン偏極状態密度 (DOS) & バンド構造プロット
    dos = app.plot_vasp_dos(
        tbl_dos, energy_col="Energy_eV", dos_cols=["Total_Up", "d_Up"],
        down_dos_cols=["Total_Down", "d_Down"], fermi_energy=5.42,
    )
    bands = app.plot_vasp_bands(
        tbl_bands, k_col="k_dist", band_cols=["B1", "B2", "B3", "B4"],
        k_ticks=[(0.0, "Γ"), (1.05, "X"), (1.79, "M"), (2.84, "Γ")],
        fermi_energy=5.42, e_min=-6.0, e_max=6.0,
    )
```

---

## 4. AI エージェント自己発見・幾何レイアウト診断・実機ラボデータ読込

### A. 自己発見 API カタログ (`app.describe_api`)

全 RPC メソッドの引数型・デフォルト値・列挙値・Python SDK シグネチャ・呼び出し例を JSON で即時取得できます。不正な列名や選択肢を渡した場合は `tg.RpcError`（`-32602`）の `err.available` に有効な候補一覧が返ります。

```python
catalog = app.describe_api()                    # 全ドメイン・全メソッド
matsci = app.describe_api(domain="curvemath")   # curvemath.* ドメインのみ抽出
spec = app.describe_api("graph.auto_layout")    # 単一メソッドの詳細スキーマ
```

### B. 1 コール幾何・視覚診断 (`g.inspect()` / `g.diagnose_layout()`)

`g.inspect()` はページ寸法・グリッド・各レイヤのピクセル矩形・主目盛座標と表示ラベル・曲線の有効点数とクリッピング率・凡例/コールアウト境界を一括返却し、以下の決定論的レイアウト警告を検出します（`g.auto_layout()` で自動解消可能）:

- `EMPTY_CURVE` / `CURVE_CLIPPED_BY_AXIS` / `NONPOSITIVE_DATA_ON_LOG_AXIS`
- `LEGEND_OVERLAPS_DATA` / `CALLOUT_OVERLAPS_LEGEND` / `CALLOUT_OVERLAPS_DATA`
- `TICK_LABEL_COLLISION` / `DISABLED_AXIS_HAS_TITLE_OR_CURVE` / `PANEL_CANVAS_SIZE_MISMATCH`

### C. 実機ラボ CSV / TSV / `.xy` / Rigaku `.ras` / Origin `.opj` インポート

- `app.import_csv(path)`: UTF-8 / UTF-16 / Shift-JIS (CP932) 自動判定、Instron・島津製作所等の 2 行ヘッダ `[列名, 単位]` 自動昇格、`"Nominal stress (MPa)"` 単位分離、ヘッダ無し空白区切り `.xy`/`.dat` XRD データ、および Rigaku SmartLab `.ras` に標準対応。
- `app.import_opj(path)`: Origin `.opj` プロジェクトのワークシート・行列・ヒストグラム・グラフ読込に対応。

---

## 5. 全 API ドメイン一覧

| ドメイン | 主な RPC / SDK メソッド |
|---|---|
| `app.*` | `ping`, `info`, `supported_methods`, `describe_api`, `batch`, `list_sample_datasets`, `load_sample_dataset`, `new_project`, `open_project`, `save_project`, `import_csv`, `import_opj`, `list_windows`, `window_info`, `activate_window`, `rename_window`, `close_window`, `clone_window`, `set_language`, `get_language`, `set_recalc_mode`, `recalculate`, `undo`, `redo`, `set_focus_hud`, `get_focus_hud`, `set_workspace_mode`, `check_updates`, `list_actions`, `trigger_action`, `grab_gui_diagnostic`, `Session.to_pptx` |
| `table.*` | `create`, `get_info`, `resize`, `add_column`, `remove_column`, `set_column_data`, `get_column_data`, `set_data_dict`, `paste_tsv`, `set_cell`, `get_cell`, `set_column_meta`, `get_column_meta`, `set_formula`, `recalculate`, `mask_rows`, `unmask_rows`, `clear_masks`, `get_masked_rows`, `sort`, `column_stats`, `row_stats`, `export_csv`, `to_matrix`, `TableProxy.to_dataframe`, `TableProxy.to_numpy` |
| `matrix.*` | `create`, `get_info`, `set_coordinates`, `set_data`, `get_data`, `set_formula`, `transpose`, `invert`, `determinant`, `flip`, `fft2d`, `filter2d`, `analyze_2d_peaks`, `to_table`, `MatrixProxy.to_numpy` |
| `plot.*` | `xy`, `plot_bars` (`y_err_cols`), `stacked_bars` (`percent_100`), `polar`, `add_curve`, `remove_curve`, `add_error_bars`, `double_y`, `waterfall`, `histogram`, `box`, `violin`, `pareto`, `qq`, `pie`, `vectors`, `ternary`, `function`, `matrix` (`plot_heatmap`), `3d_matrix`, `3d_xyz`, `3d_function` |
| `graph.*` | `get_info`, `inspect`, `auto_layout` (`fix_layout`), `to_python` (`to_python_script`), `export_ole`, `GraphProxy.to_pptx`, `set_category_labels`, `add_layer`, `remove_layer`, `add_linked_layer`, `add_inset_layer`, `arrange_layers`, `merge_graphs`, `set_page_size`, `fit_page_to_layers`, `fit_layers_to_page`, `apply_style_preset`, `apply_frame_preset`, `apply_square_frame`, `set_panel_labels`, `sync_axis_color`, `step_zoom_history`, `drop_columns`, `set_aspect_ratio`, `copy_format`, `paste_format`, `batch_clone`, `set_title`, `set_axis`, `set_axis_break`, `add_reference_line`, `remove_reference_line`, `clear_reference_lines`, `add_shaded_band`, `set_grid`, `set_curve_style`, `set_curve_visible`, `move_curve`, `set_stack_offsets`, `apply_series_scheme`, `apply_palette`, `set_colorbar`, `set_legend`, `auto_place_legend`, `add_text`, `add_arrow`, `remove_marker`, `mask_region`, `clear_masks`, `export`, `copy_image`, `copy_ole`, `set_3d_options` |
| `analysis.*` | `list_fit_models`, `fit_linear`, `fit_york`, `fit_polynomial`, `fit_nonlinear`, `fit_global`, `fit_piecewise`, `fit_robust`, `fit_quantile`, `fit_logistic`, `fit_mlr`, `fit_implicit_odr`, `fit_surface_3d`, `compare_models`, `compare_datasets`, `analyze_peaks`, `batch_peaks` |
| `gadget.*` | `attach`, `set_roi`, `query`, `clear` |
| `signal.*` | `smooth`, `savgol_derivative`, `fft`, `fft_filter`, `iir_filter`, `stft`, `envelope`, `wavelet_cwt`, `wavelet_denoise`, `welch_psd`, `coherence`, `decimate`, `fsd` |
| `curvemath.*` | `eval`, `average_curves`, `normalize`, `integrate`, `differentiate`, `interpolate`, `subtract_line`, `intersections`, `lookup_xy`, `polygon_area`, `fill_between`, `offset_yield` (`analyze_tensile`), `kocks_mecking` (`analyze_work_hardening`), `kissinger` (`analyze_kissinger`), `xrd_analyze` (`analyze_xrd`), `bragg_convert`, `vasp_dos` (`plot_vasp_dos`), `vasp_bands` (`plot_vasp_bands`), `arrhenius`, `tauc`, `eis_fit` |
| `stats.*` | `normality`, `outlier`, `anova_oneway`, `anova_twoway`, `nonparametric`, `paired_plot`, `bland_altman`, `passing_bablok`, `weibull`, `kaplan_meier`, `roc`, `pca`, `correlation`, `clustering`, `confidence_ellipse` |
| `digitizer.*` | `extract` (`digitize` / `digitize_image`) |
