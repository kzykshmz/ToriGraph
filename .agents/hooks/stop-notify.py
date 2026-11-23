#!/usr/bin/env python3
"""Antigravity Stop hook: optionally notify smartphone via ntfy.sh when an agent turn ends.

Reads JSON hook context from stdin and always writes valid JSON `{}` to stdout.
Fires a notification when either:
  - AGY_AUTO_NOTIFY=1 is set in the environment, or
  - <workspace>/.ntfy-auto exists.
"""
from __future__ import annotations

import json
import os
import pathlib
import sys
import urllib.request


def _resolve_topic(repo_root: pathlib.Path) -> str:
    for env_key in ("TORIGRAPH_NTFY_TOPIC", "AGY_NTFY_TOPIC"):
        val = os.environ.get(env_key, "").strip()
        if val:
            return val
    for candidate in (repo_root / ".ntfy-topic", pathlib.Path.home() / ".ntfy-topic"):
        if candidate.is_file():
            txt = candidate.read_text(encoding="utf-8", errors="ignore").strip()
            if txt:
                return txt
    return ""


def main() -> None:
    try:
        raw = sys.stdin.read()
        ctx = json.loads(raw) if raw.strip() else {}
    except Exception:
        ctx = {}

    cwd = pathlib.Path(ctx.get("cwd") or os.getcwd())
    auto_flag = os.environ.get("AGY_AUTO_NOTIFY", "").strip() == "1" or (cwd / ".ntfy-auto").exists()
    if auto_flag:
        topic = _resolve_topic(cwd)
        if topic:
            last_input = str(ctx.get("lastUserInput") or "").strip().replace("\n", " ")
            if len(last_input) > 60:
                last_input = last_input[:57] + "..."
            msg = f"タスクが完了しました ({cwd.name})" + (f": {last_input}" if last_input else "")
            payload = json.dumps(
                {
                    "topic": topic,
                    "title": f"Antigravity [{cwd.name}]",
                    "message": msg,
                    "priority": 3,
                    "tags": ["white_check_mark"],
                },
                ensure_ascii=False,
            ).encode("utf-8")
            req = urllib.request.Request(
                "https://ntfy.sh",
                data=payload,
                headers={"Content-Type": "application/json; charset=utf-8"},
                method="POST",
            )
            try:
                with urllib.request.urlopen(req, timeout=8):
                    pass
            except Exception:
                pass

    sys.stdout.write("{}\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
