"""Claude Code OAuth usage via Anthropic rate-limit headers."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from pathlib import Path

import httpx

CREDENTIALS_PATH = Path.home() / ".claude" / ".credentials.json"
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
    if _bedrock_mode() and not read_token():
        return {"ok": False, "mode": "bedrock"}
    token = read_token()
    if not token:
        return {"ok": False, "mode": "no_token"}
    data = _poll_headers(token)
    if not data:
        return {"ok": False, "mode": "api_error"}
    return data
