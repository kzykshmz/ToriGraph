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

# B. エンドポイント (`endpoint` 引数または `TORIGRAPH_RPC_ENDPOINT`) があれば既存セッションへ接続、
#    どちらも未指定のときだけヘッドレス起動（再現スクリプトに最適）
with tg.connect_or_launch() as app:
    print(app.supported_methods()[:5])

# C. 起動中の ToriGraph（アプリ内コンソールや --rpc-port 指定プロセス）に接続
#    (endpoint 未指定時はコンソール表示の pipe://... 等を渡すか connect_or_launch() を使う案内が出ます)
# app = tg.connect("tcp://127.0.0.1:18900")
```

### 認証（トークン解決順）

Loopback TCP 接続では `rpc.auth` トークン認証が行われます。トークンは次の優先順で解決されます（上から優先）:

1. `tg.connect(..., token="...")` の明示引数（またはエンドポイント URL の `?token=...` クエリ）
2. 環境変数 `TORIGRAPH_RPC_TOKEN`
3. ローカルのトークンファイル `tcp_<port>.token`（Windows: `%LOCALAPPDATA%\ToriGraph\rpc\`、Linux/macOS: `$XDG_DATA_HOME/ToriGraph/rpc/` 等、loopback ホスト時のみ）

以前のセッションが残した古い `tcp_<port>.token` ファイルがあっても、環境変数 `TORIGRAPH_RPC_TOKEN` や明示引数が優先されます。

### 待ち時間・再接続と版の照合

- `launch(timeout=30)` の `timeout` は起動待ち(接続できるまで)の秒数です。各呼び出しの応答待ちは別で、既定は `tg.DEFAULT_RPC_TIMEOUT`(120 秒)です。
- 重い処理だけ待ち時間を延ばすときは `app.call("analysis.xxx", ..., rpc_timeout=600)`、セッション全体なら `tg.launch(rpc_timeout=600)` または `app.rpc_timeout = 600` とします。
- 応答は JSON-RPC の `id` で照合します。待ち時間を過ぎると `ToriGraphTimeoutError`（`TransportError` のサブクラス）、相手側切断や通信断では `ToriGraphDisconnectedError`（`TransportError` のサブクラス）が発生し、遅れて届いた応答を取り違えないようその接続は無効化されます。
- `connect` / `connect_or_launch` / `launch` で開いたセッションは `app.reconnect()` で同じエンドポイントへ再接続でき、ToriGraph 側のウィンドウ・テーブル・グラフはそのまま保持されます。
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
    km = g.analyze_work_hardening(smooth_window=11, create_plot=True, plot_mode="kocks_mecking")
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
    #    推奨プリセット: "lab_slide_83mm" (83.3mm / 12.9pt、自動で 140% = 18pt 配置)
    #    ※ 既に lab_slide_83mm / g.lab_slide_style() を当てたグラフも自動 140% 配置を維持
    #    ※ 3D グラフ (Graph3D) はスタイル適用をスキップし、2D グラフの RPC エラーはそのまま送出
    app.to_pptx(
        Path("weekly_report.pptx"),
        [g1, g2],
        per_slide=2,
        preset="lab_slide_83mm",
        titles=["Al-Zn-Mg 時効材の引張特性と XRD プロファイル"],
    )

    # 2. 既存デッキ内の埋め込み済み OLE 図形 (alt_text="torigraph:<graph_name>") をべき等更新
    #    PowerPoint 側で手動調整した位置・サイズ・重なり順 (Z-order) を維持したまま中身だけ更新！
    #    ※ 既存の出力ファイルを上書きする場合は (deck == path に限らず) overwrite=True が必須。
    #       未指定時は FileExistsError (deck == path は ValueError) で保護され、保存は os.replace でアトミック実行されます。
    #    ※ per-graph OLE 出力が失敗/未対応のときも、既定 (allow_whole_project=False) ではプロジェクト全体埋め込みを行わず ToriGraphError を送出します。
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
    # ※ ブロック内の同期読み取り (get_column / info / inspect / to_dataframe 等) は BatchReadError を送出します
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

全 RPC メソッドの引数型・デフォルト値・列挙値・Python SDK シグネチャ・呼び出し例を JSON で即時取得できます。不正な列名や選択肢を渡した場合は `tg.RpcError`（`-32602`）の `err.available` に有効な候補一覧が返り、先頭 10 件は例外メッセージ `str(err)` にも表示されます。

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

## 5. API の名前: RPC 名と SDK 名

ToriGraph 本体が受け付ける命令(RPC)と、この SDK のメソッドは名前が別に付いています。

- **RPC 名**は `domain.name` の形(例: `table.set_column_data`)。どの RPC も `app.call("table.set_column_data", table="Data1", col="A", data=[...])` か `app.table.set_column_data(...)` で直接呼べます。一覧と引数は `app.describe_api()` で取れます。
- **SDK 名**は Python から使うときの名前です。プロキシ(`TableProxy` など)では対象が決まっているので `get_`・`_data`・`table` などを省いた短い名前にし(`tbl.set_column()`、`tbl.info()`)、解析は `analyze_*`、作図は `plot_*` にそろえています。それ以外は RPC 名の後半と同じです(`app.fit_linear()` = `analysis.fit_linear`)。
- 同じ働きの名前が複数あったものは 1 つに決め、ほかは**非推奨の別名**として残しました。古いスクリプトはそのまま動きますが、呼ぶと `DeprecationWarning` が出ます(スクリプトを直接実行したときは画面に表示されます)。下の表の右の列が別名です。警告を例外にして洗い出すには `python -W error::DeprecationWarning script.py` とします。

主な対応(全部は下の折りたたみ表):

| RPC 名 | SDK の名前 | 非推奨の別名 |
|---|---|---|
| `table.get_info` | `tbl.info()` | `get_info` |
| `table.set_column_data` | `tbl.set_column()` | `set_column_data`, `set_col` |
| `table.get_column_data` | `tbl.get_column()`(値のリストを返す) | `get_column_data`, `get_col` |
| `table.set_column_meta` / `table.get_column_meta` | `tbl.set_meta()` / `tbl.get_meta()` | `set_column_meta` / `get_column_meta` |
| `table.column_stats` | `tbl.stats()` | `column_stats` |
| (SDK 内で処理) | `tbl.get_data_dict()` | `to_dict` |
| `graph.auto_layout` | `g.auto_layout()` | `fix_layout` |
| `graph.add_inset_layer` | `g.add_inset()` | `add_inset_layer` |
| `graph.to_python` | `g.to_python()`(辞書)/ `g.to_python_script()`(文字列) | `app.export_python_script` |
| `curvemath.offset_yield` | `analyze_tensile()` | `offset_yield` |
| `curvemath.kocks_mecking` | `analyze_work_hardening()` | `kocks_mecking` |
| `curvemath.kissinger` | `app.analyze_kissinger()` | `kissinger`, `kissinger_plot` |
| `curvemath.xrd_analyze` | `analyze_xrd()` | `xrd_analyze` |
| `curvemath.vasp_dos` / `vasp_bands` | `app.plot_vasp_dos()` / `app.plot_vasp_bands()` | `vasp_dos`, `plot_dos` / `vasp_bands`, `plot_bands` |
| `gadget.clear` | `app.clear_gadgets()` | `clear_gadget` |
| `digitizer.extract` | `app.digitize()` | `extract_digitizer`, `digitize_image` |
| `stats.weibull` / `stats.roc` | `app.weibull()` / `app.roc()` | `weibull_fit` / `roc_analysis`, `roc_curve` |

### 型情報

パッケージに `py.typed` を置いています(PEP 561)。エディタや mypy で引数の型と説明が出ます。よく使う `fit_linear`・`fit_nonlinear`・`smooth`・`fft`・`plot_xy`・`g.export`・`to_pptx` は引数を明示し、説明(docstring)を付けました。そのほかのメソッドで `**kwargs` だけのものは、RPC の引数をそのまま渡します(`app.describe_api("signal.fft_filter")` で確認)。

### `plot_xy` のデータの渡し方

```python
g = app.plot_xy(tbl, "Strain", ["Stress"])            # ワークシートの列(DataFrame・dict も可)
g = app.plot_xy(x=strain, y=stress)                    # 配列をそのまま
g = app.plot_xy(x=t, y=[y1, y2], y_err=[e1, e2])       # 同じ x に複数の曲線とエラーバー
g = app.plot_xy(strain, stress)                        # 位置引数でも可(従来どおり)
```

2 番目の位置引数は本来 X 列の指定ですが、1 番目と 2 番目がどちらも配列のときだけ「X の値・Y の値」と読みます。配列のときに 3 番目の位置引数(`y_cols`)を渡すと、誤差や形式の指定と取り違えないよう `TypeError` にしています(誤差は `y_err=`、形式は `style=`)。

### UI の言語

`tg.launch()` と `tg.connect_or_launch()` の `language` の既定は `None` で、起動した ToriGraph の言語を変えません(利用者の設定のまま。新しく入れた PC では日本語)。以前の既定 `"ja"` は、英語で使っている人の画面をスクリプトが黙って日本語に切り替え、その設定が次の起動にも残ることがありました。グラフの文字列を言語に合わせたいスクリプトは `language="ja"` か `"en"` を明示してください。`tg.connect()` で既存のセッションにつないだときは、言語は変えません。

### スタイルプリセット (`isij_style` / `lab_slide_style` / `slide_style`)

- `g.isij_style()` (`isij_85mm`): ISIJ 論文単欄向け（幅 85 mm・内向き目盛）。
- `g.lab_slide_style()` (`lab_slide_83mm`, **スライド推奨**): 研究室スライド向け（幅 83.3 mm・文字 12.9 pt）。`to_pptx()` で `scale=` や幅付き `box=` を省略すると自動で 140 %（スライド上 18 pt）で配置されます（事前に `g.lab_slide_style()` / `g.apply_style_preset("lab_slide_83mm")` を適用してから `to_pptx()` を呼んだ場合も同様）。
- `g.slide_style()` (`slide_28pt`): 従来の大判スライドプリセット（幅 185 mm・文字 28 pt）。全面大判の 28 pt 図が必要な場合のみ使います。

### 戻り値とテーブル作成の補足

- `app.plot_histogram_cumulative()`・`app.plot_density2d(plot=True)`・`app.plot_pole_figure(plot=True)` は他の `plot_*` と同様に `GraphProxy` を返します（詳細結果辞書は `g.result` で参照可能、`plot=False` 時は解析 `dict` を返します）。解析と作図を兼ねる `app.plot_vasp_dos()` / `app.plot_vasp_bands()` / `app.plot_vasp_band_dos()` はメタデータを含む `dict` を返します。
- `app.create_table(df, designations=["X", "Y", "yErr"])` のように列役割をリストで指定した場合も、列順に従って `{列名: 役割}` へ変換して設定されます（誤差棒や Z 列が X/Y に上書きされません）。
- `app.workbook("Book1")` で `WorkbookProxy` を取得でき、`app.workbook.create(...)` のようなドット呼び出しや `app.new_workbook()` / `app.list_workbooks()` も利用できます。

### 全メソッドの対応表

<details><summary>`Session`(`app`): 145 メソッド</summary>

| RPC 名 | SDK の名前 | 非推奨の別名 |
|---|---|---|
| `analysis.analyze_peaks` | `app.analyze_peaks()` |  |
| `analysis.batch_peaks` | `app.batch_peaks()` |  |
| `analysis.compare_datasets` | `app.compare_datasets()` |  |
| `analysis.compare_models` | `app.compare_models()` |  |
| `analysis.fit_global` | `app.fit_global()` |  |
| `analysis.fit_implicit_odr` | `app.fit_implicit_odr()` |  |
| `analysis.fit_linear` | `app.fit_linear()` |  |
| `analysis.fit_logistic` | `app.fit_logistic()` |  |
| `analysis.fit_mlr` | `app.fit_mlr()` |  |
| `analysis.fit_nonlinear` | `app.fit_nonlinear()` |  |
| `analysis.fit_piecewise` | `app.fit_piecewise()` |  |
| `analysis.fit_polynomial` | `app.fit_polynomial()` |  |
| `analysis.fit_quantile` | `app.fit_quantile()` |  |
| `analysis.fit_robust` | `app.fit_robust()` |  |
| `analysis.fit_surface_3d` | `app.fit_surface_3d()` |  |
| `analysis.fit_york` | `app.fit_york()` |  |
| `analysis.list_fit_models` | `app.list_fit_models()` |  |
| `app.activate_window` | `app.activate_window()` |  |
| `app.batch` | `app.batch()` |  |
| `app.check_updates` | `app.check_updates()` |  |
| `app.clone_window` | `app.clone_window()` |  |
| `app.close_window` | `app.close_window()` |  |
| `app.describe_api` | `app.describe_api()` |  |
| `app.get_focus_hud` | `app.get_focus_hud()` |  |
| `app.get_language` | `app.get_language()` |  |
| `app.grab_gui_diagnostic` | `app.grab_gui_diagnostic()` |  |
| `app.import_csv` | `app.import_csv()` |  |
| `app.import_opj` | `app.import_opj()` |  |
| `app.import_xlsx` | `app.import_xlsx()` |  |
| `app.info` | `app.info()` |  |
| `app.list_actions` | `app.list_actions()` |  |
| `app.list_sample_datasets` | `app.list_sample_datasets()` |  |
| `app.list_windows` | `app.list_windows()` |  |
| `app.load_sample_dataset` | `app.load_sample_dataset()` |  |
| `app.new_project` | `app.new_project()` |  |
| `app.open_project` | `app.open_project()` |  |
| `app.recalculate` | `app.recalculate()` |  |
| `app.redo` | `app.redo()` |  |
| `app.rename_window` | `app.rename_window()` |  |
| `app.save_project` | `app.save_project()` |  |
| `app.set_focus_hud` | `app.set_focus_hud()` |  |
| `app.set_language` | `app.set_language()` |  |
| `app.set_recalc_mode` | `app.set_recalc_mode()` |  |
| `app.set_workspace_mode` | `app.set_workspace_mode()` |  |
| `app.supported_methods` | `app.supported_methods()` |  |
| `app.trigger_action` | `app.trigger_action()` |  |
| `app.undo` | `app.undo()` |  |
| `app.window_info` | `app.window_info()` |  |
| `curvemath.arrhenius` | `app.arrhenius()` | `arrhenius_plot` |
| `curvemath.average_curves` | `app.average_curves()` |  |
| `curvemath.bragg_convert` | `app.bragg_convert()` |  |
| `curvemath.differentiate` | `app.differentiate_curve()` |  |
| `curvemath.eis_fit` | `app.eis_fit()` |  |
| `curvemath.eval` | `app.curve_eval()` | `curve_math`, `eval_curves` |
| `curvemath.fill_between` | `app.fill_between()` |  |
| `curvemath.integrate` | `app.integrate_curve()` |  |
| `curvemath.interpolate` | `app.interpolate_curve()` |  |
| `curvemath.intersections` | `app.intersections()` | `curve_intersections`, `find_intersections` |
| `curvemath.kissinger` | `app.analyze_kissinger()` | `kissinger`, `kissinger_plot` |
| `curvemath.kocks_mecking` | `app.analyze_work_hardening()` | `kocks_mecking` |
| `curvemath.lookup_xy` | `app.lookup_xy()` |  |
| `curvemath.normalize` | `app.normalize_curve()` |  |
| `curvemath.offset_yield` | `app.analyze_tensile()` | `offset_yield` |
| `curvemath.polygon_area` | `app.polygon_area()` |  |
| `curvemath.subtract_line` | `app.subtract_line()` | `subtract_straight_line` |
| `curvemath.tauc` | `app.tauc()` | `tauc_bandgap` |
| `curvemath.vasp_band_dos` | `app.plot_vasp_band_dos()` | `plot_band_dos`, `vasp_band_dos` |
| `curvemath.vasp_bands` | `app.plot_vasp_bands()` | `plot_bands`, `vasp_bands` |
| `curvemath.vasp_dos` | `app.plot_vasp_dos()` | `plot_dos`, `vasp_dos` |
| `curvemath.xrd_analyze` | `app.analyze_xrd()` | `xrd_analyze` |
| `digitizer.extract` | `app.digitize()` | `digitize_image`, `extract_digitizer` |
| `gadget.attach` | `app.attach_gadget()` |  |
| `gadget.clear` | `app.clear_gadgets()` | `clear_gadget` |
| `gadget.query` | `app.query_gadget()` |  |
| `gadget.set_roi` | `app.set_gadget_roi()` |  |
| `graph.auto_layout` | `app.auto_layout()` | `fix_layout` |
| `graph.drop_columns` | `app.drop_columns()` |  |
| `graph.export_ole` | `app.export_ole()` |  |
| `graph.merge_graphs` | `app.merge_graphs()` |  |
| `graph.to_python` | `app.to_python()` |  |
| `matrix.create`<br>`matrix.set_data` | `app.create_matrix()` |  |
| `ping` | `app.ping()` |  |
| `plot.3d_function` | `app.plot_3d_function()` |  |
| `plot.3d_matrix` | `app.plot_3d_matrix()` |  |
| `plot.3d_xyz` | `app.plot_3d_xyz()` |  |
| `plot.box` | `app.plot_box()` |  |
| `plot.color_scatter` | `app.plot_color_scatter()` |  |
| `plot.density2d` | `app.plot_density2d()` |  |
| `plot.double_y` | `app.plot_double_y()` |  |
| `plot.function` | `app.plot_function()` |  |
| `plot.histogram` | `app.plot_histogram()` |  |
| `plot.histogram_cumulative` | `app.plot_histogram_cumulative()` |  |
| `plot.matrix` | `app.plot_matrix()` |  |
| `plot.pareto` | `app.plot_pareto()` |  |
| `plot.pie` | `app.plot_pie()` |  |
| `plot.polar` | `app.plot_polar()` |  |
| `plot.pole_figure` | `app.plot_pole_figure()` |  |
| `plot.qq` | `app.plot_qq()` |  |
| `plot.stacked_bars` | `app.plot_stacked_bars()` |  |
| `plot.ternary` | `app.plot_ternary()` |  |
| `plot.vectors` | `app.plot_vectors()` |  |
| `plot.violin` | `app.plot_violin()` |  |
| `plot.waterfall` | `app.plot_waterfall()` |  |
| `plot.xy` | `app.plot_xy()` |  |
| `signal.coherence` | `app.coherence()` |  |
| `signal.decimate` | `app.decimate()` |  |
| `signal.envelope` | `app.envelope()` | `hilbert_envelope` |
| `signal.fft` | `app.fft()` |  |
| `signal.fft_filter` | `app.fft_filter()` |  |
| `signal.fsd` | `app.fsd()` | `fourier_self_deconvolution` |
| `signal.iir_filter` | `app.iir_filter()` |  |
| `signal.savgol_derivative` | `app.savgol_derivative()` |  |
| `signal.smooth` | `app.smooth()` |  |
| `signal.stft` | `app.stft()` |  |
| `signal.wavelet_cwt` | `app.wavelet_cwt()` |  |
| `signal.wavelet_denoise` | `app.wavelet_denoise()` |  |
| `signal.welch_psd` | `app.welch_psd()` |  |
| `stats.anova_oneway` | `app.anova_oneway()` | `anova_one_way` |
| `stats.anova_twoway` | `app.anova_twoway()` | `anova_two_way` |
| `stats.bland_altman` | `app.bland_altman()` |  |
| `stats.clustering` | `app.clustering()` |  |
| `stats.confidence_ellipse` | `app.confidence_ellipse()` | `add_confidence_ellipse` |
| `stats.correlation` | `app.correlation()` | `correlation_matrix` |
| `stats.kaplan_meier` | `app.kaplan_meier()` |  |
| `stats.nonparametric` | `app.nonparametric()` | `nonparametric_test` |
| `stats.normality` | `app.normality()` | `normality_test` |
| `stats.outlier` | `app.outlier()` | `outlier_test` |
| `stats.paired_plot` | `app.paired_plot()` | `paired_estimation_plot` |
| `stats.passing_bablok` | `app.passing_bablok()` |  |
| `stats.pca` | `app.pca()` |  |
| `stats.probability_plot` | `app.probability_plot()` |  |
| `stats.roc` | `app.roc()` | `roc_analysis`, `roc_curve` |
| `stats.weibull` | `app.weibull()` | `weibull_fit` |
| `table.create`<br>`table.set_data_dict` | `app.create_table()` |  |
| `table.paste_tsv` | `app.paste_tsv()` |  |
| `workbook.create` | `app.new_workbook()` |  |
| `workbook.list` | `app.list_workbooks()` |  |
| (SDK 内で処理) | `app.close()` |  |
| (SDK 内で処理) | `app.plot_bars()` |  |
| (SDK 内で処理) | `app.plot_heatmap()` |  |
| (SDK 内で処理) | `app.reconnect()` |  |
| (SDK 内で処理) | `app.set_category_labels()` |  |
| (SDK 内で処理) | `app.to_pptx()` |  |
| (SDK 内で処理) | `app.to_python_script()` | `export_python_script` |

</details>

<details><summary>`TableProxy`(`tbl = app.table("名前")`): 28 メソッド</summary>

| RPC 名 | SDK の名前 | 非推奨の別名 |
|---|---|---|
| `app.redo` | `tbl.redo()` |  |
| `app.undo` | `tbl.undo()` |  |
| `table.add_column` | `tbl.add_column()` |  |
| `table.clear_masks` | `tbl.clear_masks()` |  |
| `table.column_stats` | `tbl.stats()` | `column_stats` |
| `table.export_csv` | `tbl.export_csv()` |  |
| `table.get_cell` | `tbl.get_cell()` |  |
| `table.get_column_data` | `tbl.get_column()` | `get_col`, `get_column_data` |
| `table.get_column_meta` | `tbl.get_meta()` | `get_column_meta` |
| `table.get_info` | `tbl.info()` | `get_info` |
| `table.get_masked_rows` | `tbl.get_masked_rows()` |  |
| `table.mask_rows` | `tbl.mask_rows()` |  |
| `table.paste_tsv` | `tbl.paste_tsv()` |  |
| `table.recalculate` | `tbl.recalculate()` |  |
| `table.remove_column` | `tbl.remove_column()` |  |
| `table.resize` | `tbl.resize()` |  |
| `table.row_stats` | `tbl.row_stats()` |  |
| `table.set_cell` | `tbl.set_cell()` |  |
| `table.set_column_data` | `tbl.set_column()` | `set_col`, `set_column_data` |
| `table.set_column_meta` | `tbl.set_meta()` | `set_column_meta` |
| `table.set_data_dict` | `tbl.set_data_dict()` |  |
| `table.set_formula` | `tbl.set_formula()` |  |
| `table.sort` | `tbl.sort()` |  |
| `table.to_matrix` | `tbl.to_matrix()` |  |
| `table.unmask_rows` | `tbl.unmask_rows()` |  |
| (SDK 内で処理) | `tbl.get_data_dict()` | `to_dict` |
| (SDK 内で処理) | `tbl.to_dataframe()` |  |
| (SDK 内で処理) | `tbl.to_numpy()` |  |

</details>

<details><summary>`MatrixProxy`(`m = app.matrix("名前")`): 20 メソッド</summary>

| RPC 名 | SDK の名前 | 非推奨の別名 |
|---|---|---|
| `app.redo` | `m.redo()` |  |
| `app.undo` | `m.undo()` |  |
| `matrix.analyze_2d_peaks` | `m.analyze_2d_peaks()` | `find_2d_peaks` |
| `matrix.determinant` | `m.determinant()` |  |
| `matrix.fft2d` | `m.fft2d()` |  |
| `matrix.filter2d` | `m.filter2d()` |  |
| `matrix.flip` | `m.flip()` |  |
| `matrix.get_data` | `m.get_data()` |  |
| `matrix.get_info` | `m.info()` | `get_info` |
| `matrix.invert` | `m.invert()` |  |
| `matrix.set_coordinates` | `m.set_coordinates()` |  |
| `matrix.set_data` | `m.set_data()` |  |
| `matrix.set_formula` | `m.set_formula()` |  |
| `matrix.to_table` | `m.to_table()` |  |
| `matrix.transpose` | `m.transpose()` |  |
| (SDK 内で処理) | `m.plot_3d()` |  |
| (SDK 内で処理) | `m.plot_contour()` |  |
| (SDK 内で処理) | `m.plot_grayscale()` |  |
| (SDK 内で処理) | `m.plot_heatmap()` |  |
| (SDK 内で処理) | `m.to_numpy()` |  |

</details>

<details><summary>`GraphProxy`(`g = app.graph("名前")`): 73 メソッド</summary>

| RPC 名 | SDK の名前 | 非推奨の別名 |
|---|---|---|
| `app.redo` | `g.redo()` |  |
| `app.undo` | `g.undo()` |  |
| `curvemath.offset_yield` | `g.analyze_tensile()` | `offset_yield` |
| `gadget.attach` | `g.attach_gadget()` |  |
| `gadget.clear` | `g.clear_gadgets()` | `clear_gadget` |
| `gadget.query` | `g.query_gadget()` |  |
| `gadget.set_roi` | `g.set_gadget_roi()` |  |
| `graph.add_arrow` | `g.add_arrow()` |  |
| `graph.add_error_band` | `g.add_error_band()` |  |
| `graph.add_inset_layer` | `g.add_inset()` | `add_inset_layer` |
| `graph.add_layer` | `g.add_layer()` |  |
| `graph.add_linked_layer` | `g.add_linked_layer()` |  |
| `graph.add_reference_line` | `g.add_reference_line()` |  |
| `graph.add_shaded_band` | `g.add_shaded_band()` |  |
| `graph.add_text` | `g.add_text()` |  |
| `graph.apply_frame_preset` | `g.apply_frame_preset()` |  |
| `graph.apply_palette` | `g.apply_palette()` |  |
| `graph.apply_series_scheme` | `g.apply_series_scheme()` |  |
| `graph.apply_square_frame` | `g.apply_square_frame()` |  |
| `graph.apply_style_preset` | `g.apply_style_preset()` |  |
| `graph.arrange_layers` | `g.arrange_layers()` |  |
| `graph.auto_layout` | `g.auto_layout()` | `fix_layout` |
| `graph.auto_place_legend` | `g.auto_place_legend()` |  |
| `graph.batch_clone` | `g.batch_clone()` |  |
| `graph.clear_masks` | `g.clear_masks()` |  |
| `graph.clear_reference_lines` | `g.clear_reference_lines()` |  |
| `graph.copy_format` | `g.copy_format()` |  |
| `graph.copy_image` | `g.copy_image()` |  |
| `graph.copy_ole` | `g.copy_ole()` |  |
| `graph.drop_columns` | `g.drop_columns()` |  |
| `graph.export` | `g.export()` |  |
| `graph.export_ole` | `g.export_ole()` |  |
| `graph.fit_layers_to_page` | `g.fit_layers_to_page()` |  |
| `graph.fit_page_to_layers` | `g.fit_page_to_layers()` |  |
| `graph.get_info` | `g.info()` | `get_info` |
| `graph.inspect` | `g.inspect()` |  |
| `graph.mask_region` | `g.mask_region()` |  |
| `graph.move_curve` | `g.move_curve()` |  |
| `graph.paste_format` | `g.paste_format()` |  |
| `graph.remove_layer` | `g.remove_layer()` |  |
| `graph.remove_marker` | `g.remove_marker()` |  |
| `graph.remove_reference_line` | `g.remove_reference_line()` |  |
| `graph.set_3d_options` | `g.set_3d_options()` | `set_options` |
| `graph.set_aspect_ratio` | `g.set_aspect_ratio()` |  |
| `graph.set_axis` | `g.set_axis()` |  |
| `graph.set_axis_break` | `g.set_axis_break()` |  |
| `graph.set_category_labels` | `g.set_category_labels()` |  |
| `graph.set_colorbar` | `g.set_colorbar()` |  |
| `graph.set_curve_style` | `g.set_curve_style()` |  |
| `graph.set_curve_visible` | `g.set_curve_visible()` |  |
| `graph.set_grid` | `g.set_grid()` |  |
| `graph.set_legend` | `g.set_legend()` |  |
| `graph.set_page_size` | `g.set_page_size()` |  |
| `graph.set_panel_labels` | `g.set_panel_labels()` |  |
| `graph.set_stack_offsets` | `g.set_stack_offsets()` |  |
| `graph.set_title` | `g.set_title()` |  |
| `graph.step_zoom_history` | `g.step_zoom_history()` |  |
| `graph.sync_axis_color` | `g.sync_axis_color()` |  |
| `graph.to_python` | `g.to_python()` |  |
| `plot.add_curve` | `g.add_curve()` |  |
| `plot.add_error_bars` | `g.add_error_bars()` |  |
| `plot.remove_curve` | `g.remove_curve()` |  |
| (SDK 内で処理) | `g.analyze_work_hardening()` | `kocks_mecking` |
| (SDK 内で処理) | `g.analyze_xrd()` | `xrd_analyze` |
| (SDK 内で処理) | `g.bragg_convert()` |  |
| (任意 RPC) | `g.call()` |  |
| (SDK 内で処理) | `g.diagnose_layout()` |  |
| (SDK 内で処理) | `g.isij_style()` |  |
| (SDK 内で処理) | `g.lab_slide_style()` |  |
| (SDK 内で処理) | `g.rebuild_density2d()` |  |
| (SDK 内で処理) | `g.slide_style()` |  |
| (SDK 内で処理) | `g.to_pptx()` |  |
| (SDK 内で処理) | `g.to_python_script()` |  |

</details>

<details><summary>`WorkbookProxy`(`wb = app.workbook("名前")`): 9 メソッド</summary>

| RPC 名 | SDK の名前 | 非推奨の別名 |
|---|---|---|
| `workbook.add_sheet` | `wb.add_sheet()` |  |
| `workbook.attach` | `wb.attach()` |  |
| `workbook.list` | `wb.info()` |  |
| `workbook.move_sheet` | `wb.move_sheet()` |  |
| `workbook.rename` | `wb.rename()` |  |
| `workbook.rename_sheet` | `wb.rename_sheet()` |  |
| `workbook.set_current` | `wb.set_current()` |  |
| `workbook.list` | `wb.sheets()` |  |
| `workbook.ungroup` | `wb.ungroup()` |  |

</details>

---

## 6. `to_python_script` の再現範囲

`g.to_python_script()`(RPC `graph.to_python`)は、グラフを描き直す Python スクリプトを作ります。描き直すのは見た目(データ・曲線・軸・注釈)で、解析やフィットの計算はやり直しません。描き直せない要素がグラフにあると、スクリプトの冒頭に `# 未再現: ...` のコメントが並びます。実行する前にそこを見て、必要なら手で合わせてください。

| 要素 | 再現 | 補足 |
|---|---|---|
| ワークシートのデータ・単位・Long Name・列の役割・数式・マスク | する | `embed_data="inline"`(既定)。`"csv"` は CSV に書き出して読み込み、`"reference"` は既存のワークシートを名前で参照 |
| 行列(値・座標範囲・数式) | する | |
| 折れ線・散布図・線+シンボル・スプライン・面・ステップ・ドロップライン | する | 曲線の色・線幅・線種・シンボルと大きさ・中抜き・間引き・表示/非表示 |
| 関数曲線 | する | |
| エラーバー | する | 向き(x / y) |
| エラーバンド | する | `g.add_error_band`(非対称のバンドも) |
| 2 軸(右 Y 軸) | する | 2 本目以降の曲線を右軸に付ける。1 本目の曲線が右軸のときは `# 未再現` |
| 棒グラフ(縦・横) | 一部 | 種類とカテゴリ名は再現。棒を横に並べるずらし量は `# 未再現` |
| 積み上げ棒・100% 積み上げ | しない | 棒が重なって描かれる(`# 未再現`)。`app.plot_stacked_bars` で作り直す |
| ヒストグラム(パネル積み重ねを含む)・箱ひげ・バイオリン・円・極座標・三角図 | する | |
| 複数レイヤー・連動レイヤー・挿入図(インセット)・パネルラベル | する | |
| 軸(範囲・対数・反転・刻み・題・文字の大きさ・目盛りの向き・ラベルの回転)・軸の省略・カテゴリ名・グリッド | する | |
| 参照線・塗りつぶし帯・凡例・文字・矢印・ページの大きさ | する | |
| スタイルプリセット | する | そのセッションで最後に当てたプリセットを `g.apply_style_preset` で当て、その後に個々の設定で今の値に合わせる。プロジェクトを開き直した後は記録がないので出ない |
| カラーマップ(等高線・行列の図) | 一部 | 行列、カラーバーの範囲と水準数 |
| 3D グラフ | 一部 | データ・題・回転・正射影だけ。色・軸範囲などは `# 未再現` |
| ウォーターフォール・スタックオフセット | しない | 曲線のずらし(`# 未再現`)。`g.set_stack_offsets` で付け直す |
| ベクトル図 | しない | 矢印ではなく点を結んだ線になる(`# 未再現`) |
| 値で色を変える散布図・確率目盛り(対数正規・Gumbel・ワイブル)・独自の目盛りラベル | しない | `# 未再現` |
| フィット | しない | フィット曲線は数値データとして描き直す。フィットの条件と結果のパラメータは `# 未再現` |
| 解析結果(0.2% 耐力、XRD、TDS などの数値) | しない | 図に描かれた補助線や注釈は描き直すが、計算はしない |
