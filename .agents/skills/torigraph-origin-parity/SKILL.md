---
name: torigraph-origin-parity
description: ToriGraphとOriginPro 2021（originlab MCPサーバ経由）に同一データを投入し、非線形フィット・統計量・600dpi描画・PowerPoint OLE出力の同等性を検証するときに使う。「Originと比較して」「Originと同じ結果になるか確かめて」「パリティ検証をして」が合図。
---

# ToriGraph / OriginPro 2021 パリティ検証手順

## 1. 安全条件（必読）

- すでに起動中の OriginPro や PowerPoint のユーザー作業ウィンドウを閉じたり上書きしたりしない。
- 研究の生データ原本を無断で変更・外部送信しない。原則として境界条件（欠損値 `NaN`、非等間隔 X、複数系列、極端な桁数）を含む再現可能な合成データで比較ケースを固定する。
- Origin のベンダーアイコンやプロプライエタリな内部コードを ToriGraph へ転用しない。

## 2. 数値パリティ検証（フィッティング・信号処理・力学解析）

1. Python スクリプトで同一の入力配列 `(x, y, yerr)` を生成し、CSV または配列として保持する。
2. **Origin 側**: MCP サーバ `migrated-settings_originlab`（`origin_ping` → `origin_worksheet_write` → `origin_labtalk`）でフィットまたは解析を実行し、パラメータ推定値・標準誤差（SE）・自由度（DoF）・Reduced $\chi^2$・決定係数 $R^2$ を取得する。
3. **ToriGraph 側**: `with torigraph.launch(headless=True) as app:` で同じデータを投入して解析を実行する。
4. **比較基準**:
   - パラメータ定義（例：Gaussian の振幅 vs 面積、$\sigma$ vs FWHM、重みの定義 $1/\sigma_i^2$）が一致しているか確認する。
   - 失敗後に許容誤差を都合よく広げず、値のスケールに応じた相対許容差（解析解がある場合は解析解とも三者比較）で判定する。

## 3. 視覚・エクスポート・OLE パリティ検証

1. 両ソフトで軸範囲・目盛り刻み・フォント（Arial）・図枠寸法（例：ISIJ 85 mm 正方形）を揃えて 600 DPI PNG およびベクター PDF を書き出す。
2. `python/tests/visual_regression.py` の `load_rgb_image()` と `compare_images(..., diff_heatmap_path=...)` を使い、差分ヒートマップ PNG を出力して余白・目盛り内向き長・凡例配置・サブピクセル描画の差異を確認する。
3. PowerPoint 埋め込みを確認する場合は、`g.to_pptx(..., preset="slide_28pt")` で生成した `.pptx` が `ToriGraph.Graph.1` のネイティブ OLE Compound File（CLSID `{8E6B9541-3C74-4D28-A915-7D84E5A12001}`）とベクター EMF プレビューを保持しているか検査する。
