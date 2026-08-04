"""OpenAI Codex / ChatGPT usage via wham/usage API."""

from __future__ import annotations

import json
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
        return {
            "ok": True,
            "pct": round(pct, 1),
            "used": used,
            "limit": limit,
            "unit": "usd",
            "label": "spend",
        }

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
