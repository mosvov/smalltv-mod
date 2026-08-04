"""Claude Code usage: OAuth rate limits, or local token totals from session logs."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from collections import defaultdict
from datetime import datetime, timedelta, timezone
from pathlib import Path

import httpx

CREDENTIALS_PATH = Path.home() / ".claude" / ".credentials.json"
PROJECTS_DIR = Path.home() / ".claude" / "projects"
CONFIG_PATH = Path(__file__).resolve().parents[1] / "config.json"
DEFAULT_LOCAL_DAYS = 30
TOKEN_ENDPOINT = "https://platform.claude.com/v1/oauth/token"
TOKEN_REFRESH_MARGIN = 300
API_URL = "https://api.anthropic.com/v1/messages"
API_HEADERS = {
    "anthropic-version": "2023-06-01",
    "anthropic-beta": "oauth-2025-04-20",
    "Content-Type": "application/json",
    "User-Agent": "claude-code/2.1.146",
}
API_BODY = {
    "model": "claude-haiku-4-5-20251001",
    "max_tokens": 1,
    "messages": [{"role": "user", "content": "hi"}],
}


def _read_credentials_file() -> dict | None:
    try:
        return json.loads(CREDENTIALS_PATH.read_text())
    except (OSError, json.JSONDecodeError):
        return None


def _write_credentials_file(data: dict) -> None:
    try:
        CREDENTIALS_PATH.write_text(json.dumps(data, indent=2))
    except OSError:
        pass


def _oauth_block(creds: dict) -> dict | None:
    oauth = creds.get("claudeAiOauth") or creds.get("oauth")
    return oauth if isinstance(oauth, dict) else None


def _token_expired(oauth: dict) -> bool:
    exp = oauth.get("expiresAt")
    if not exp:
        return False
    try:
        return time.time() * 1000 >= float(exp) - TOKEN_REFRESH_MARGIN * 1000
    except (TypeError, ValueError):
        return False


def _refresh_token(oauth: dict, creds: dict) -> str | None:
    refresh = oauth.get("refreshToken")
    if not refresh:
        return None
    try:
        resp = httpx.post(
            TOKEN_ENDPOINT,
            data={"grant_type": "refresh_token", "refresh_token": refresh},
            timeout=20.0,
        )
        resp.raise_for_status()
        body = resp.json()
    except httpx.HTTPError:
        return None
    access = body.get("access_token")
    if not access:
        return None
    oauth["accessToken"] = access
    if "refresh_token" in body:
        oauth["refreshToken"] = body["refresh_token"]
    if "expires_in" in body:
        oauth["expiresAt"] = int((time.time() + body["expires_in"]) * 1000)
    elif "expires_at" in body:
        oauth["expiresAt"] = int(body["expires_at"] * 1000)
    _write_credentials_file(creds)
    return access


def _read_token_keychain() -> str | None:
    if sys.platform != "darwin":
        return None
    try:
        import getpass

        out = subprocess.run(
            ["security", "find-generic-password", "-s", "Claude Code-credentials", "-a", getpass.getuser(), "-w"],
            capture_output=True,
            text=True,
            check=False,
        )
        if out.returncode != 0 or not out.stdout.strip():
            return None
        blob = json.loads(out.stdout)
        oauth = _oauth_block(blob) if isinstance(blob, dict) else None
        return oauth.get("accessToken") if oauth else None
    except (OSError, json.JSONDecodeError, subprocess.SubprocessError):
        return None


def read_token() -> str | None:
    env = os.environ.get("CLAUDE_CODE_OAUTH_TOKEN")
    if env:
        return env.strip()
    tok = _read_token_keychain()
    if tok:
        return tok
    creds = _read_credentials_file()
    if not creds:
        return None
    oauth = _oauth_block(creds)
    if not oauth or not isinstance(oauth.get("accessToken"), str):
        return None
    if _token_expired(oauth):
        tok = _refresh_token(oauth, creds)
        return tok or oauth.get("accessToken")
    return oauth.get("accessToken")


def _local_days() -> int:
    try:
        if CONFIG_PATH.is_file():
            cfg = json.loads(CONFIG_PATH.read_text())
            days = int(cfg.get("claude_local_days") or DEFAULT_LOCAL_DAYS)
            return max(1, min(days, 90))
    except (OSError, json.JSONDecodeError, TypeError, ValueError):
        pass
    return DEFAULT_LOCAL_DAYS


def _fmt_compact(n: float) -> str:
    if n >= 1_000_000:
        text = f"{n / 1_000_000:.1f}M"
    elif n >= 1000:
        text = f"{n / 1000:.1f}K"
    else:
        return str(int(n))
    return text.replace(".0M", "M").replace(".0K", "K")


def _aggregate_local_sessions(days: int | None = None) -> dict | None:
    """Sum token usage from ~/.claude/projects session logs (Bedrock-friendly)."""
    if not PROJECTS_DIR.is_dir():
        return None

    period = days or _local_days()
    cutoff = datetime.now(timezone.utc) - timedelta(days=period)
    seen: set[str] = set()
    totals = {"input": 0, "output": 0, "cache_read": 0, "cache_create": 0, "searches": 0}
    by_model: dict[str, dict[str, int]] = defaultdict(
        lambda: {"input": 0, "output": 0, "cache_read": 0, "cache_create": 0}
    )

    for path in PROJECTS_DIR.rglob("*.jsonl"):
        try:
            lines = path.read_text().splitlines()
        except OSError:
            continue
        for raw in lines:
            try:
                row = json.loads(raw)
            except json.JSONDecodeError:
                continue
            msg = row.get("message") or {}
            usage = msg.get("usage")
            if not usage:
                continue
            model = msg.get("model")
            if not model or model == "<synthetic>":
                continue
            ts = row.get("timestamp")
            if ts:
                try:
                    dt = datetime.fromisoformat(str(ts).replace("Z", "+00:00"))
                except ValueError:
                    dt = None
                if dt and dt < cutoff:
                    continue
            req_id = row.get("requestId") or row.get("uuid")
            if not req_id or req_id in seen:
                continue
            seen.add(req_id)

            inp = int(usage.get("input_tokens") or 0)
            out = int(usage.get("output_tokens") or 0)
            cr = int(usage.get("cache_read_input_tokens") or 0)
            cc = int(usage.get("cache_creation_input_tokens") or 0)
            sw = int((usage.get("server_tool_use") or {}).get("web_search_requests") or 0)

            totals["input"] += inp
            totals["output"] += out
            totals["cache_read"] += cr
            totals["cache_create"] += cc
            totals["searches"] += sw
            by_model[model]["input"] += inp
            by_model[model]["output"] += out
            by_model[model]["cache_read"] += cr
            by_model[model]["cache_create"] += cc

    tokens = totals["input"] + totals["output"]
    if tokens <= 0:
        return None

    models = []
    for name, m in sorted(by_model.items(), key=lambda kv: -(kv[1]["input"] + kv[1]["output"]))[:4]:
        models.append(
            {
                "model": name,
                "input": m["input"],
                "output": m["output"],
                "cache_read": m["cache_read"],
                "cache_create": m["cache_create"],
            }
        )

    line = _fmt_compact(tokens)
    sub_parts = []
    if totals["cache_read"] > 0:
        sub_parts.append(f"{_fmt_compact(totals['cache_read'])} cached")
    if totals["searches"] > 0:
        sub_parts.append(f"{totals['searches']} search" + ("es" if totals["searches"] != 1 else ""))
    sub = ", ".join(sub_parts) if sub_parts else f"{period}d code"

    return {
        "ok": True,
        "mode": "local",
        "period_days": period,
        "tokens": tokens,
        "input": totals["input"],
        "output": totals["output"],
        "cache_read": totals["cache_read"],
        "cache_create": totals["cache_create"],
        "searches": totals["searches"],
        "line": line,
        "sub": sub,
        "pct": round(min(tokens / 1_000_000 * 100, 100), 1),
        "models": models,
    }


def _bedrock_mode() -> bool:
    settings = Path.home() / ".claude" / "settings.json"
    try:
        data = json.loads(settings.read_text())
    except (OSError, json.JSONDecodeError):
        return False
    env = data.get("env") or {}
    return bool(env.get("CLAUDE_CODE_USE_ANTHROPIC_AWS") or env.get("CLAUDE_CODE_USE_BEDROCK"))


def _poll_headers(token: str) -> dict | None:
    headers = dict(API_HEADERS)
    headers["Authorization"] = f"Bearer {token}"
    try:
        resp = httpx.post(API_URL, headers=headers, json=API_BODY, timeout=20.0)
    except httpx.HTTPError:
        return None
    if resp.status_code in (401, 403):
        return None
    if resp.status_code >= 400:
        return None

    now = time.time()

    def hdr(name: str, default: str = "0") -> str:
        return resp.headers.get(name, default)

    def reset_minutes(ts: str) -> int:
        try:
            mins = (float(ts) - now) / 60.0
        except ValueError:
            return 0
        return int(round(mins)) if mins > 0 else 0

    def pct(util: str) -> float:
        try:
            return round(float(util) * 100, 1)
        except ValueError:
            return 0.0

    s = pct(hdr("anthropic-ratelimit-unified-5h-utilization"))
    w = pct(hdr("anthropic-ratelimit-unified-7d-utilization"))
    return {
        "ok": True,
        "s": s,
        "sr": reset_minutes(hdr("anthropic-ratelimit-unified-5h-reset")),
        "w": w,
        "wr": reset_minutes(hdr("anthropic-ratelimit-unified-7d-reset")),
        "st": hdr("anthropic-ratelimit-unified-5h-status", "unknown"),
        "pct": s,
    }


def fetch() -> dict:
    token = read_token()
    if token:
        data = _poll_headers(token)
        if data:
            return data

    local = _aggregate_local_sessions()
    if local:
        return local

    if _bedrock_mode():
        return {"ok": False, "mode": "bedrock"}
    if not token:
        return {"ok": False, "mode": "no_token"}
    return {"ok": False, "mode": "api_error"}
