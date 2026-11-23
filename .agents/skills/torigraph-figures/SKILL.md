---
name: torigraph-figures
description: ToriGraphのPython SDK（torigraph）を使って論文投稿品質のグラフ（ISIJ International体裁・600dpi PNG/PDF）やPowerPoint編集可能OLEスライド（28pt体裁）を作成するときに使う。「ToriGraphでグラフを作って」「ToriGraphでプロットして」「ToriGraphでスライドに貼って」と明示されたときに使う（ツール未指定の既定はorigin-figures）。
---

# ToriGraph 論文・スライド作図スキル

> 注記: ツールの指定がない通常の作図依頼ではグローバル規約どおり `origin-figures` を既定とし、「ToriGraph で描いて」と指定された場合や ToriGraph の実戦ドッグフーディングを行う場合にこのスキルを使う。

## 1. 体裁の基本規約

- **論文用グラフ（ISIJ International 基準）**:
  - プロット枠は正方形（1:1）、四辺枠あり、四辺とも内向き目盛り、フォントは Arial を基本とする（ToriGraph の `isij_85mm` プリセット準拠）。
  - 出力は 600 DPI PNG およびベクター PDF / SVG を併せて書き出す。
- **発表スライド用グラフ（PowerPoint OLE 埋め込み）**:
  - スライド貼付時は軸ラベル・目盛り数字を **28 pt** 相当にする（`to_pptx(..., preset="slide_28pt")`）。
  - 画像ベタ貼りではなく `ToriGraph.Graph.1` ネイティブ OLE オブジェクトとして出力し、PowerPoint 上でダブルクリックして再編集できる状態を保つ。

## 2. 標準レシピ（ヘッドレス Python SDK）

```python
import pandas as pd
import torigraph as tg

with tg.launch(headless=True, language="ja") as app:
    df = pd.read_csv("data.csv")
    df.attrs["units"] = {"Strain": "%", "Stress": "MPa"}

    g = app.plot_xy(df, "Strain", "Stress", style="line", lw=1.8, color="#0072B2")
    # 論文用ベクター＆高解像度出力
    g.export("fig_stress_strain.pdf")
    g.export("fig_stress_strain.png", dpi=600)
    # 発表用 PowerPoint OLE スライド出力（Zero-COM・クリップボード非汚染）
    g.to_pptx("fig_slides.pptx", preset="slide_28pt")
```

## 3. 実戦での気づきのフィードバック

- 実データの作図中に見つけた軸表記・凡例配置・単位フォーマット・UI 上の違和感は、再現最小ケースにして `python/tests/` または `scidavis/src/ole_test_suites_*.cpp` の回帰テストへ還元する。
