---
name: torigraph-phone-notify
description: 長時間のビルド・全スイート回帰テスト・マルチエージェント監査が完了した際、または判断待ちで手が止まる際に、スマートフォン（ntfyアプリ）へプッシュ通知を送るときに使う。「完了したらスマホに通知して」「終わったら通知して」が合図。
---

# スマートフォン通知（ntfy）手順

## 1. 守るべきルール

- Slack は本人名義投稿となり通知が鳴らないため、通知手段に使わない。
- 秘密トピック名（`<repo>/.ntfy-topic` または `$HOME/.ntfy-topic` に保存）はパスワードと同等の扱いとし、**Git 管理下のファイルやコミットメッセージ・公開ログには絶対に書き込まない**。

## 2. 通知コマンドの実行方法

作業が完了した時点（または確認が必要な時点）で `scripts/notify-phone.ps1` を呼び出す。JSON UTF-8 API で送信するため日本語の件名・本文も文字化けしない。

```powershell
powershell -ExecutionPolicy Bypass -File scripts/notify-phone.ps1 `
  -Title "ToriGraph 完了通知" `
  -Message "ビルドと全回帰テスト（Suite 1-78 / Python SDK）が正常に完了しました。"
```

エラーや要確認で止まった場合は優先度を上げて通知できる（`-Priority 4 -Tags warning`）。

## 3. ターン終了時の自動通知（Stop フック）

- `.agents/hooks.json` に `stop-notify.py` が登録されている。
- 毎ターン自動でスマホ通知を鳴らしたいセッションでは、リポジトリ直下に空ファイル `.ntfy-auto`（`.gitignore` 済み）を置くか環境変数 `AGY_AUTO_NOTIFY=1` を設定する。通常時は明示的に `scripts/notify-phone.ps1` を1回呼ぶ運用でよい。
