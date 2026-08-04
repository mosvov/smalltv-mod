# ai-usage-daemon

PC-side daemon that aggregates **Claude**, **Cursor**, and **Codex** usage from local auth files and serves a single JSON snapshot to your SmallTV.

Replaces [clawdmeter-daemon](https://github.com/giovi321/clawdmeter-daemon) for multi-provider setups. v1 Claude-only JSON is still emitted for backward compatibility.

## Prerequisites

| Provider | What you need on this PC |
|----------|--------------------------|
| **Claude** | Claude Code logged in (`~/.claude/.credentials.json`) or `CLAUDE_CODE_OAUTH_TOKEN`. Bedrock setups aggregate token totals from local session logs (`~/.claude/projects/`). |
| **Cursor** | Cursor IDE logged in (reads `state.vscdb`) or token in `~/Library/Application Support/cursor-usage/config.json` |
| **Codex** | Codex CLI auth at `~/.codex/auth.json` |

## Install

```bash
cd smalltv-mod/tools/ai-usage-daemon
python3 -m venv .venv
source .venv/bin/activate   # Windows: .venv\\Scripts\\activate
pip install -r requirements.txt
cp config.example.json config.json   # optional
```

## Usage

```bash
# Print JSON once (debug)
python daemon.py --once

# Serve for device pull mode (default port 8787)
python daemon.py --serve

# Push to all SmallTVs on LAN (mDNS _clawdmeter._tcp)
python daemon.py --push

# Push to a specific device
python daemon.py --push-to http://192.168.5.11/api/usage
```

## Device setup

1. Open SmallTV web UI → **AI usage** tab.
2. Set **Usage daemon URL** to `http://<your-pc-ip>:8787/`
3. Save. Switch **Display → Mode** to **AI usage**.

For **push mode**, leave Usage URL blank and run `python daemon.py --push`.

## JSON v2 contract

```json
{
  "v": 2,
  "ok": true,
  "claude": { "ok": true, "mode": "local", "tokens": 623000, "line": "623K", "sub": "47.3M cached", "pct": 62.3, "period_days": 30 },
  "cursor": { "ok": true, "used": 78, "limit": 1000, "pct": 7.8 },
  "codex":  { "ok": true, "pct": 11, "used": 563, "limit": 5000, "unit": "credits", "label": "monthly", "remaining_pct": 89, "reset_label": "Aug 31" }
}
```

Top-level `s`/`w`/`sr`/`wr`/`st`/`ok` are duplicated for v1 firmware.

## Config

Optional `config.json` next to `daemon.py`:

- `poll_sec` — refresh interval (default 60)
- `push_interval_sec` — push interval (default 20)
- `push_targets` — list of device URLs for push mode
- `providers` — enable/disable fetching `claude`, `cursor`, `codex`
- `claude_local_days` — days of local session logs to sum for Bedrock (default 30)
- `display` — shape the `line` / `sub` labels the device shows:
  - `claude`: `auto` (tokens + cached), `tokens`, `cached`, or `pct` (OAuth 5h %)
  - `cursor`: `auto` or `requests` (e.g. `93/1000`), or `pct`
  - `codex`: `auto` or `credits` (e.g. `563/5k`), or `remaining` (e.g. `89% left`)

Example — show Codex remaining % instead of credits used:

```json
"display": { "codex": "remaining" }
```

On the device, **AI usage** tab checkboxes control which rows appear on the 240×240 screen and Status tab.

## Smoke test

```bash
./test_local.sh
```
