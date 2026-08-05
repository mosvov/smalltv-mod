"""Cursor IDE included-requests usage via dashboard API."""

from __future__ import annotations

import base64
import json
import re
import sqlite3
import sys
from pathlib import Path

import httpx

CURSOR_API = "https://cursor.com"


def _cursor_state_db() -> Path | None:
    home = Path.home()
    if sys.platform == "darwin":
        p = home / "Library" / "Application Support" / "Cursor" / "User" / "globalStorage" / "state.vscdb"
    elif sys.platform == "win32":
        p = Path.home() / "AppData" / "Roaming" / "Cursor" / "User" / "globalStorage" / "state.vscdb"
    else:
        p = home / ".config" / "Cursor" / "User" / "globalStorage" / "state.vscdb"
    return p if p.is_file() else None


def _read_sqlite_token() -> tuple[str | None, str | None]:
    db_path = _cursor_state_db()
    if not db_path:
        return None, None
    try:
        conn = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
        cur = conn.cursor()
        cur.execute("SELECT value FROM ItemTable WHERE key = ?", ("cursorAuth/accessToken",))
        row = cur.fetchone()
        conn.close()
        if not row or not row[0]:
            return None, None
        jwt = row[0].decode() if isinstance(row[0], bytes) else str(row[0])
        sub = _jwt_sub(jwt)
        if not sub:
            return jwt, None
        cookie = f"{sub}::{jwt}"
        return cookie, sub
    except (sqlite3.Error, OSError):
        return None, None


def _jwt_sub(jwt: str) -> str | None:
    try:
        parts = jwt.split(".")
        if len(parts) < 2:
            return None
        payload = parts[1] + "=" * (-len(parts[1]) % 4)
        data = json.loads(base64.urlsafe_b64decode(payload))
        sub = data.get("sub") or ""
        if sub.startswith("user_"):
            return sub
        m = re.search(r"(user_[A-Za-z0-9]+)", sub)
        return m.group(1) if m else None
    except (json.JSONDecodeError, ValueError):
        return None


def _config_token() -> str | None:
    paths = [
        Path.home() / "Library" / "Application Support" / "cursor-usage" / "config.json",
        Path.home() / ".config" / "cursor-usage" / "config.json",
    ]
    for p in paths:
        try:
            data = json.loads(p.read_text())
            tok = data.get("token") or data.get("session_token")
            if tok:
                return str(tok)
        except (OSError, json.JSONDecodeError):
            continue
    return None


def _session_cookie() -> str | None:
    tok = _config_token()
    if tok:
        return tok
    cookie, _ = _read_sqlite_token()
    return cookie


def _parse_usage_summary(data: dict) -> dict | None:
    """Extract used/limit from usage-summary or similar shapes."""
    individual = (data.get("individualUsage") or {}).get("plan") or {}
    if individual:
        used = individual.get("used") or individual.get("requestsUsed")
        limit = individual.get("limit") or individual.get("requestsLimit") or individual.get("includedRequests")
        if used is not None and limit:
            used_i, limit_i = int(used), int(limit)
            pct = round(used_i / limit_i * 100, 1) if limit_i else 0.0
            return {"ok": True, "used": used_i, "limit": limit_i, "pct": pct, "label": "monthly"}

    for key in ("gpt4", "premium", "default"):
        block = data.get(key)
        if isinstance(block, dict) and "maxRequestUsage" in block:
            used = int(block.get("numRequests") or block.get("numRequestUsage") or 0)
            limit = int(block.get("maxRequestUsage") or 0)
            if limit > 0:
                return {"ok": True, "used": used, "limit": limit, "pct": round(used / limit * 100, 1), "label": "monthly"}

    auto = data.get("autoPercentUsed")
    if auto is not None:
        pct = float(auto)
        return {"ok": True, "pct": round(pct, 1), "label": "cycle"}

    return None


def fetch() -> dict:
    cookie = _session_cookie()
    if not cookie:
        return {"ok": False, "mode": "no_token"}

    headers = {"Cookie": f"WorkosCursorSessionToken={cookie}", "Accept": "application/json"}

    for path in ("/api/usage-summary",):
        try:
            resp = httpx.get(f"{CURSOR_API}{path}", headers=headers, timeout=20.0)
            if resp.status_code == 401:
                return {"ok": False, "mode": "auth_failed"}
            if resp.status_code >= 400:
                continue
            parsed = _parse_usage_summary(resp.json())
            if parsed:
                return parsed
        except httpx.HTTPError:
            continue

    _, user_id = _read_sqlite_token()
    if user_id:
        try:
            resp = httpx.get(
                f"{CURSOR_API}/api/usage",
                params={"user": user_id},
                headers=headers,
                timeout=20.0,
            )
            if resp.status_code == 200:
                data = resp.json()
                gpt4 = data.get("gpt-4") or data.get("gpt4") or {}
                used = int(gpt4.get("numRequests") or gpt4.get("numRequestUsage") or 0)
                limit = int(gpt4.get("maxRequestUsage") or 0)
                if limit > 0:
                    return {
                        "ok": True,
                        "used": used,
                        "limit": limit,
                        "pct": round(used / limit * 100, 1),
                    }
        except httpx.HTTPError:
            pass

    return {"ok": False, "mode": "api_error"}
