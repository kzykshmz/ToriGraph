---
name: torigraph-build-verify
description: ToriGraph（d:\agy\ToriGraph）のC++/Qtビルド、個別Suite回帰テスト、MSVC AddressSanitizerメモリ安全性検証、Python SDK並列テスト、静的解析・辞書整合ゲートを安全かつ高速に回すときに使う。「ToriGraphをビルドして」「テストを回して」「ASanでメモリリークや解放後参照を調べて」「品質ゲートを通して」が合図。
---

# ToriGraph ビルド・品質検証手順

## 1. 基本方針

- 手作業で Visual Studio 環境変数や Qt DLL パスを組み立てず、リポジトリ同梱の `scripts/dev.ps1` を使用する（`VSLANG=1033` による Ninja `/showIncludes` 文字化け防止、`TORIGRAPH_NO_OLE_REGISTER=1` による実機 OLE 登録保護、`sccache`・PCH が自動で有効になる）。
- 複数のサブエージェントが並行してビルドする場合は、必ず `-BuildDir build-<agent名>` を付けて共有 `build/` への同時書き込みを避ける。

## 2. 変更内容に応じた最短検証ルート

1. **Python SDK / スクリプト / 翻訳辞書の変更**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 lint
   powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 py-test
   ```
2. **C++ 特定機能（UI・描画・フィッティング・インポート・JSON-RPC）の変更**:
   - まずビルドし、関連するモジュラースイート（36〜78）だけを `--suite=<N>` で即座に検証する（数秒で完了する）：
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 build
   .\build\scidavis\scidavis_ole_test.exe --list-suites
   powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 test -Suite 78
   ```
3. **ポインタ寿命・Undo/Redo・破棄順序・パーサ周りの変更（メモリ安全性検証）**:
   - MSVC AddressSanitizer ビルド（`build-asan/`）で対象スイートを実行し、Use-After-Free やヒープ境界外アクセスがないことを確認する：
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 asan -Suite 78
   ```
4. **描画・DPI スケール・PDF/PNG エクスポートの変更**:
   - Python の視覚回帰テスト（150 DPI vs 600 DPI スケール不変性 ＋ PNG vs PDF ベクターラスタライズ比較）を実行する：
   ```powershell
   $env:TORIGRAPH_EXE = (Resolve-Path .\build\scidavis\ToriGraph.exe).Path
   python -m pytest python/tests/test_visual_regression.py -v
   ```
5. **マージ・リリース前の統合ゲート**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 gate
   ```

## 3. クラッシュダンプ解析（MiniDump + PDB）

- Release ビルドでも `build/scidavis/ToriGraph.pdb` が分離生成される。
- 実機でクラッシュした場合は `%LOCALAPPDATA%\ToriGraph\CrashDumps\ToriGraph-*.dmp`（または `TORIGRAPH_CRASH_DUMP_DIR` 指定先）にミニダンプが自動保存されるため、対応する `.pdb` と合わせてデバッガでスタックトレースと行番号を特定する。
