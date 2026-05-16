#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Export Cursor parent-agent transcripts (.jsonl) to a folder:
  - transcript.jsonl  (copy)
  - transcript.md     (role + flattened text / tool blocks, truncated)
  - manifest.json     (index of exported chats)

Start time rule:
  1) <timestamp>...</timestamp> inside the *first* user message only (avoids follow-up turns hijacking the date).
  2) Else: file creation time (Windows: st_ctime is usually birth time).

Usage (2026-04-23 ~ 2026-04-25 inclusive of 25th full day):
  python scripts/export_cursor_agent_transcripts.py ^
    --since 2026-04-23 --until 2026-04-26 ^
    --out D:\\export\\cursor_chats_0423_0425

Default transcript root (override with --root):
  %USERPROFILE%\\.cursor\\projects\\d-ziliao-quanbu\\agent-transcripts

Export ALL parent transcripts (no date filter):
  python scripts/export_cursor_agent_transcripts.py --all --out D:\\export\\cursor_chats_all
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
from dataclasses import dataclass
from datetime import datetime, timezone, timedelta
from pathlib import Path
from typing import Any, Iterable, Optional

TS_PATTERN = re.compile(r"<timestamp>([^<]+)</timestamp>")
USER_Q_PATTERN = re.compile(
    r"<user_query>\s*([\s\S]*?)\s*</user_query>", re.IGNORECASE
)

# China standard time for naive CLI dates (matches earlier inventory)
CST = timezone(timedelta(hours=8))


def _parse_cli_datetime(s: str) -> datetime:
    """Parse 'YYYY-MM-DD' or 'YYYY-MM-DD HH:MM:SS' as CST-naive -> aware UTC equivalent for compare."""
    s = s.strip()
    for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%d"):
        try:
            dt = datetime.strptime(s, fmt)
            break
        except ValueError:
            continue
    else:
        raise ValueError(f"Bad datetime: {s!r} (use YYYY-MM-DD or YYYY-MM-DD HH:MM:SS)")
    # Treat as local wall clock in CST (same as prior PowerShell filter)
    return dt.replace(tzinfo=CST)


def default_transcripts_root() -> Path:
    slug = os.environ.get("CURSOR_PROJECT_TRANSCRIPTS_SLUG", "d-ziliao-quanbu")
    home = Path(os.environ.get("USERPROFILE", str(Path.home())))
    return home / ".cursor" / "projects" / slug / "agent-transcripts"


def iter_parent_jsonl(root: Path) -> Iterable[Path]:
    if not root.is_dir():
        return
    for p in sorted(root.iterdir()):
        if not p.is_dir():
            continue
        if p.name == "subagents" or "subagents" in p.parts:
            continue
        f = p / f"{p.name}.jsonl"
        if f.is_file():
            yield f


def _parse_timestamp_value(ts: str) -> Optional[datetime]:
    ts = ts.strip()
    main = ts.split("(", 1)[0].strip()
    for fmt in ("%A, %b %d, %Y, %I:%M %p", "%A, %B %d, %Y, %I:%M %p"):
        try:
            return datetime.strptime(main, fmt).replace(tzinfo=CST)
        except ValueError:
            continue
    try:
        dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=CST)
        return dt
    except ValueError:
        return None


def _user_text_from_record(obj: dict[str, Any]) -> str:
    if obj.get("role") != "user":
        return ""
    msg = obj.get("message") or {}
    content = msg.get("content")
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts: list[str] = []
        for block in content:
            if isinstance(block, dict) and block.get("type") == "text":
                parts.append(str(block.get("text", "")))
        return "\n".join(parts)
    return ""


def file_start_time(path: Path) -> datetime:
    """
    Prefer <timestamp> inside the *first* user turn (opening message).
    If absent, fall back to file creation time (Windows st_ctime).
    """
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            continue
        ut = _user_text_from_record(obj)
        if not ut:
            continue
        m = TS_PATTERN.search(ut)
        if m:
            parsed = _parse_timestamp_value(m.group(1))
            if parsed:
                return parsed
        # First user line had no usable timestamp — stop scanning further user
        # messages so later turns (e.g. May follow-up) do not hijack start time.
        break

    st = path.stat()
    return datetime.fromtimestamp(st.st_ctime, tz=CST)


def first_user_preview(path: Path, limit: int = 240) -> str:
    raw = path.read_text(encoding="utf-8", errors="replace")
    m = USER_Q_PATTERN.search(raw)
    text = m.group(1).strip() if m else ""
    text = text.replace("\r\n", "\n").strip()
    if len(text) > limit:
        return text[: limit - 3] + "..."
    return text


def flatten_message_content(content: Any, tool_json_max: int) -> str:
    if content is None:
        return ""
    if isinstance(content, str):
        return content
    if not isinstance(content, list):
        return json.dumps(content, ensure_ascii=False, indent=2)

    parts: list[str] = []
    for block in content:
        if not isinstance(block, dict):
            parts.append(str(block))
            continue
        btype = block.get("type")
        if btype == "text":
            parts.append(str(block.get("text", "")))
        elif btype == "tool_use":
            name = block.get("name", "")
            inp = block.get("input", {})
            try:
                js = json.dumps(inp, ensure_ascii=False, indent=2)
            except (TypeError, ValueError):
                js = str(inp)
            if len(js) > tool_json_max:
                js = js[: tool_json_max] + "\n... [truncated]"
            parts.append(f"\n**tool_use** `{name}`\n```json\n{js}\n```\n")
        elif btype == "tool_result":
            try:
                js = json.dumps(block, ensure_ascii=False, indent=2)
            except (TypeError, ValueError):
                js = str(block)
            if len(js) > tool_json_max:
                js = js[:tool_json_max] + "\n... [truncated]"
            parts.append("\n**tool_result**\n```json\n" + js + "\n```\n")
        else:
            try:
                parts.append(json.dumps(block, ensure_ascii=False, indent=2))
            except (TypeError, ValueError):
                parts.append(str(block))
    return "\n".join(parts)


def jsonl_to_markdown(path: Path, tool_json_max: int) -> str:
    lines_out: list[str] = []
    for i, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            lines_out.append(f"## Line {i} (invalid JSON)\n\n```\n{line[:2000]}\n```\n")
            continue
        role = obj.get("role", "?")
        msg = obj.get("message") or {}
        content = msg.get("content")
        body = flatten_message_content(content, tool_json_max=tool_json_max)
        lines_out.append(f"## {i} — {role}\n\n{body}\n")
    return "\n".join(lines_out)


@dataclass
class Row:
    uuid: str
    source: str
    start: datetime
    preview: str


def main() -> int:
    ap = argparse.ArgumentParser(description="Export Cursor agent transcripts to folder.")
    ap.add_argument(
        "--root",
        type=Path,
        default=None,
        help="agent-transcripts directory (default: %%USERPROFILE%%\\.cursor\\projects\\d-ziliao-quanbu\\agent-transcripts)",
    )
    ap.add_argument("--out", type=Path, required=True, help="Output directory (created).")
    ap.add_argument("--since", type=str, default=None, help="Inclusive start (CST), e.g. 2026-04-23")
    ap.add_argument(
        "--until",
        type=str,
        default=None,
        help="Exclusive end (CST), e.g. 2026-04-26 to include all of 2026-04-25",
    )
    ap.add_argument("--all", action="store_true", help="Export every parent transcript (ignore dates).")
    ap.add_argument(
        "--tool-json-max",
        type=int,
        default=6000,
        help="Max chars per tool JSON in .md (default 6000).",
    )
    args = ap.parse_args()

    root: Path = args.root or default_transcripts_root()
    out: Path = args.out
    out.mkdir(parents=True, exist_ok=True)

    if not args.all:
        if not args.since or not args.until:
            ap.error("Provide both --since and --until, or use --all")
        t0 = _parse_cli_datetime(args.since)
        t1 = _parse_cli_datetime(args.until)
    else:
        t0 = t1 = None  # type: ignore

    rows: list[Row] = []
    for jpath in iter_parent_jsonl(root):
        st = file_start_time(jpath)
        if t0 is not None and t1 is not None:
            if not (t0 <= st < t1):
                continue
        uid = jpath.parent.name
        preview = first_user_preview(jpath)
        rows.append(Row(uuid=uid, source=str(jpath), start=st, preview=preview))

    rows.sort(key=lambda r: r.start)

    manifest = {
        "transcripts_root": str(root.resolve()),
        "export_time_cst": datetime.now(CST).isoformat(),
        "filter": {"since": args.since, "until": args.until, "all": args.all},
        "count": len(rows),
        "chats": [
            {
                "uuid": r.uuid,
                "start_time": r.start.isoformat(),
                "first_user_preview": r.preview,
                "source_jsonl": r.source,
                "exported_jsonl": str((out / r.uuid / "transcript.jsonl").resolve()),
                "exported_md": str((out / r.uuid / "transcript.md").resolve()),
            }
            for r in rows
        ],
    }

    for r in rows:
        d = out / r.uuid
        d.mkdir(parents=True, exist_ok=True)
        shutil.copy2(r.source, d / "transcript.jsonl")
        md = jsonl_to_markdown(Path(r.source), tool_json_max=args.tool_json_max)
        header = (
            f"# Transcript `{r.uuid}`\n\n"
            f"- **start_time**: {r.start.isoformat()}\n"
            f"- **source**: `{r.source}`\n\n"
            f"## First user message preview\n\n{r.preview or '(empty)'}\n\n---\n\n"
        )
        (d / "transcript.md").write_text(header + md, encoding="utf-8")

    (out / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    print(f"Transcripts root: {root}")
    print(f"Exported {len(rows)} chat(s) -> {out.resolve()}")
    print(f"Wrote {out / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
