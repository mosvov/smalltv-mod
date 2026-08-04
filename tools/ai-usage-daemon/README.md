# ai-usage-daemon

PC-side daemon that aggregates **Claude**, **Cursor**, and **Codex** usage from local auth files and serves a single JSON snapshot to your SmallTV.

Replaces [clawdmeter-daemon](https://github.com/giovi321/clawdmeter-daemon) for multi-provider setups. v1 Claude-only JSON is still emitted for backward compatibility.

## Prerequisites

| Provider | What you need on this PC |
|----------|--------------------------|
| **Claude** | Claude Code logged in (`~/.claude/.credentials.json`) or `CLAUDE_CODE_OAUTH_TOKEN`. Bedrock-only setups show `N/A`. |
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
  "claude": { "ok": true, "s": 29, "sr": 142, "w": 4, "wr": 9876, "st": "allowed", "pct": 29 },
  "cursor": { "ok": true, "used": 78, "limit": 1000, "pct": 7.8 },
  "codex":  { "ok": true, "pct": 11, "used": 563, "limit": 5000, "unit": "usd", "label": "spend" }
}
```

Top-level `s`/`w`/`sr`/`wr`/`st`/`ok` are duplicated for v1 firmware.

## Config

Optional `config.json` next to `daemon.py`:

- `poll_sec` — refresh interval (default 60)
- `push_interval_sec` — push interval (default 20)
- `push_targets` — list of device URLs for push mode
- `providers` — enable/disable `claude`, `cursor`, `codex`

## Smoke test

```bash
./test_local.sh
```
