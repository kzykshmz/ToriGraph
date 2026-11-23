# ToriGraph エージェント開発規約 (`AGENTS.md`)

このリポジトリ（`d:\agy\ToriGraph`）で作業するすべての AI エージェント・サブエージェントは、以下の手順と安全条件に従うこと。

## 1. アーキテクチャ概要と単一情報源

- **C++ / Qt5 コア**: `libscidavis/`（静的ライブラリ）＋ `scidavis/`（実行ファイル `ToriGraph.exe` および自動回帰テスト `scidavis_ole_test.exe`）＋ `fitPlugins/`（GSL フィットプラグイン群）。
- **Python JSON-RPC 2.0 SDK**: `python/torigraph/`（標準ライブラリのみで動作するクライアント `client.py` と Zero-COM ネイティブ OLE スライド生成 `pptx.py`）。
- **バージョンの単一情報源**: ルート `CMakeLists.txt` の `project(scidavis VERSION x.y.z)` が唯一の正本。変更時は `python/torigraph/__init__.py` の `__version__` と `TORIGRAPH_RELEASE_DATE` を同時に更新し、`python scripts/check_version.py` で検証する。
- **日本語翻訳辞書**: `libscidavis/src/I18nJapanese.cpp` および `libscidavis/src/i18n/`。変更後は必ず `python scripts/check_ja_glossary.py` を通す。

## 2. 標準ビルド・テスト・解析コマンド（`scripts/dev.ps1`）

日本語 Windows での MSVC `/showIncludes` 文字化け防止（`VSLANG=1033`）、`sccache`、PCH、`.deps` の Qt ランタイムパス通しはすべて `scripts/dev.ps1` に集約されている。素の `cmake` を手打ちするより `scripts/dev.ps1` を優先する。

```powershell
# 1. CMake 構成＋インクリメンタルビルド（Release + PCH + sccache + 分離 PDB）
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 build

# 2. C++ 個別スイート高速実行（Suites 36..78 は任意の番号をカンマ区切りで指定可）
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 test -Suite 78
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 test -Suite 40,77,78

# 3. C++ 全スイート一括回帰実行（Suites 1..78）
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 test

# 4. MSVC AddressSanitizer（Use-After-Free / ヒープ破壊検査、build-asan/ に分離ビルド）
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 asan -Suite 78

# 5. Python SDK 並列テスト（pytest-xdist + Hypothesis + 視覚回帰テスト）
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 py-test

# 6. バージョン整合・日本語用語集・Ruff リンター一括チェック
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 lint

# 7. C++ 静的解析 / フォーマット（変更ファイル対象）
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 format
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 cppcheck -Files libscidavis/src/Foo.cpp
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 tidy -Files libscidavis/src/Foo.cpp
```

## 3. 厳守すべき安全条件・並列作業ルール

1. **OLE レジストリ汚染の禁止**:
   - 開発・テスト実行時は必ず `TORIGRAPH_NO_OLE_REGISTER=1` を維持する（`scripts/dev.ps1` と `CMakePresets.json` では自動設定済み）。ユーザー環境にインストール済みの `ToriGraph.Graph.1`（`HKCU\Software\Classes\CLSID\{8E6B9541-3C74-4D28-A915-7D84E5A12001}`）を開発ビルドで勝手に上書きしてはならない。
2. **並列サブエージェントでのビルドディレクトリ分離**:
   - 複数のサブエージェントやワークツリーで同時にコンパイルする場合、共有の `build/` を奪い合わないよう `-BuildDir build-worker1` のように専用の出力先を指定する（`sccache` が有効なため別ディレクトリでもキャッシュヒットして高速に終わる）。
3. **PowerPoint / クリップボード安全条件**:
   - 自動テストや SDK から既存の `POWERPNT.EXE` や Origin インスタンスを強制終了しない。OLE `.pptx` 出力は Zero-COM（`torigraph.pptx` / `PptxExport.cpp`）を使う。
4. **テスト追加場所**:
   - `scidavis/src/ole_test.cpp` は `ole_test_common.h` と 6 つの翻訳単位（`ole_test.cpp` および `ole_test_suites_36_46.cpp` 〜 `ole_test_suites_73_78.cpp`）に分割されている。新規 C++ 回帰スイートを追加する際は 1 ファイルを肥大化させず、新しいスイートファイルまたは末尾のモジュールに追加して `ole_test_common.h` と `kModularSuites[]`（および `scidavis/CMakeLists.txt` の CTest 範囲）に登録する。
5. **Git 運用と通知**:
   - 作業前後で `git status` と `origin/main` との競合有無を確認する。
   - **`git push` はユーザーから明示的に頼まれたときだけ行う。**
   - 完了時のスマホ通知を頼まれている場合は `powershell -ExecutionPolicy Bypass -File scripts/notify-phone.ps1 -Title "ToriGraph" -Message "..."` を実行する（Slack は本人名義投稿で通知が鳴らないため通知手段に使わない）。
