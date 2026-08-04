"""OpenAI Codex / ChatGPT usage via wham/usage API."""

from __future__ import annotations

import json
from datetime import datetime
from pathlib import Path

import httpx

AUTH_PATH = Path.home() / ".codex" / "auth.json"
USAGE_URL = "https://chatgpt.com/backend-api/wham/usage"


def _load_auth() -> dict | None:
    try:
        return json.loads(AUTH_PATH.read_text())
    except (OSError, json.JSONDecodeError):
        return None


def _access_token(auth: dict) -> str | None:
    tokens = auth.get("tokens") or {}
    tok = tokens.get("access_token")
    return tok if isinstance(tok, str) and tok else None


def _reset_label(reset_at: int | float | str | None) -> str | None:
    if reset_at is None:
        return None
    try:
        ts = int(float(reset_at))
        dt = datetime.fromtimestamp(ts)
        return dt.strftime("%b %d").replace(" 0", " ")
    except (OSError, ValueError, OverflowError, TypeError):
        return None


def fetch() -> dict:
    auth = _load_auth()
    if not auth:
        return {"ok": False, "mode": "no_auth"}
    token = _access_token(auth)
    if not token:
        return {"ok": False, "mode": "no_token"}

    try:
        resp = httpx.get(
            USAGE_URL,
            headers={
                "Authorization": f"Bearer {token}",
                "Accept": "application/json",
                "User-Agent": "Codex/1.0",
            },
            timeout=20.0,
        )
        resp.raise_for_status()
        data = resp.json()
    except httpx.HTTPError:
        return {"ok": False, "mode": "api_error"}

    sc = (data.get("spend_control") or {}).get("individual_limit")
    if sc:
        used = float(sc.get("used") or 0)
        limit = float(sc.get("limit") or 0)
        pct = float(sc.get("used_percent") or (used / limit * 100 if limit else 0))
        remaining_pct = sc.get("remaining_percent")
        reset_at = sc.get("reset_at")
        reset_sec = sc.get("reset_after_seconds")
        out: dict = {
            "ok": True,
            "pct": round(pct, 1),
            "used": round(used),
            "limit": round(limit),
            "unit": "credits",
            "label": "monthly",
        }
        if remaining_pct is not None:
            out["remaining_pct"] = round(float(remaining_pct), 1)
        if reset_at is not None:
            out["reset_at"] = int(reset_at)
            label = _reset_label(reset_at)
            if label:
                out["reset_label"] = label
        if reset_sec is not None:
            out["reset_sec"] = int(reset_sec)
        return out

    rl = data.get("rate_limit") or {}
    primary = rl.get("primary_window") or rl.get("secondary_window")
    if primary:
        pct = float(primary.get("used_percent") or 0)
        reset = int(primary.get("reset_after_seconds") or 0)
        return {
            "ok": True,
            "pct": round(pct, 1),
            "label": "5h",
            "reset_sec": reset,
        }

    return {"ok": False, "mode": "unknown_shape"}
