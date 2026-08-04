---
title: AI usage meter
description: Show Claude, Cursor, and Codex quotas on the 240×240 display, fed over WiFi from your PC.
---

Switch **Display → Mode** to **AI usage** and the device shows a compact dashboard of your AI tool quotas. The PC-side [ai-usage-daemon](../../../tools/ai-usage-daemon/) reads local auth files — tokens never leave your machine.

## What it shows

- **Stats** (when data is flowing): three rows — **Claude** (5h % or 30-day token total on Bedrock), **Cursor** (included requests), **Codex** (monthly credits + reset date).
- **Idle animation** when data stops: animated pixel mascot until the daemon reconnects.

## Setup

1. On the PC that runs Claude Code / Cursor / Codex:

   ```sh
   cd smalltv-mod/tools/ai-usage-daemon
   pip install -r requirements.txt
   python daemon.py --serve          # http://0.0.0.0:8787/
   ```

   Or push mode (device cannot reach PC):

   ```sh
   python daemon.py --push-to http://192.168.5.11/api/usage
   python daemon.py --push             # mDNS discover all SmallTVs
   ```

2. In the web UI open **AI usage** tab. Set **Usage daemon URL** to `http://<pc-ip>:8787/` (pull) or leave blank (push). Use **Show on screen** to hide providers you do not need. Save.

3. Switch **Display → Mode** to **AI usage** (or add to carousel).

## Display options

| Where | What you control |
|-------|------------------|
| **Device web UI** | Show/hide Claude, Cursor, Codex rows |
| **Daemon `config.json`** | Which metrics each row shows (`display.claude`, `display.cursor`, `display.codex`) and which providers are fetched (`providers`) |

Daemon `display` examples:

```json
{
  "display": {
    "claude": "tokens",
    "cursor": "pct",
    "codex": "remaining"
  }
}
```

## Provider notes

| Provider | Source | If unavailable |
|----------|--------|----------------|
| **Claude** | Claude Code OAuth, or local session logs on Bedrock (`~/.claude/projects/`) | OAuth shows 5h %; Bedrock shows token totals |
| **Cursor** | Cursor IDE session (`state.vscdb`) or cursor-usage config | Row shows `N/A` |
| **Codex** | `~/.codex/auth.json` → ChatGPT wham/usage | Row shows `N/A` |

Legacy [clawdmeter-daemon](https://github.com/giovi321/clawdmeter-daemon) still works for Claude-only (v1 JSON).

## JSON contract (v2)

```json
{
  "v": 2,
  "claude": { "ok": true, "s": 29, "pct": 29 },
  "cursor": { "ok": true, "used": 78, "limit": 1000, "pct": 7.8 },
  "codex":  { "ok": true, "used": 563, "limit": 5000, "unit": "credits", "pct": 11, "remaining_pct": 89, "reset_label": "Aug 31" }
}
```

Top-level `s`/`w` fields are duplicated for v1 firmware compatibility.

## Multiple devices

Each device advertises `_clawdmeter._tcp` mDNS. Run `python daemon.py --push` to update all devices on the LAN.
