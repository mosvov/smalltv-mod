---
title: Stock and crypto ticker
description: Show live prices, change, and a sparkline for up to 8 rotating symbols.
---

The ticker is the default mode. It can show **one symbol at a time** (classic rotation) or **several tickers on one screen** (2–6 per page) without swapping between them. For each symbol it draws the price, the absolute change, the percent change with an up or down arrow, and optionally a small sparkline chart.

## Layout presets and multi-tile mode

In the **Ticker** tab, **Tickers per screen** controls how many symbols appear at once:

| Setting | Layout |
|---------|--------|
| 1 | Full detail, one symbol fills the screen; the list rotates on the timer |
| 2–3 | Stacked rows; sparklines enabled |
| 4 | 2×2 grid |
| 5–6 | 2×3 grid; sparklines off (too small to read) |

**Layout presets** apply common combinations in one click:

- **Classic** — 1-up with chart and portfolio page
- **Desk** — 4-up grid, no chart or portfolio (good for a quick desk glance)
- **Minimal** — 6-up, no chart
- **Portfolio** — 1-up with chart and portfolio summary

When you have more enabled tickers than fit on one screen (or the portfolio page is on), pages **rotate** on the timer below. If everything fits on one page, rotation is hidden.

Optional **status strip** at the top (Ticker → What to show):

- **Clock (HH:MM)** — needs NTP; enable night mode or this overlay to sync time
- **WiFi signal dot** — green / yellow / red by RSSI; red when disconnected

## What it shows

- The current price, in the symbol's currency.
- Absolute change and percent change with an arrow, coloured green for up and red for down.
- A sparkline over the selected timeframe (when there is room on screen).
- Optional extras in the Ticker tab: the name, the timeframe label, an "updated N s ago" line (on by default, and shown automatically when data is stale), rotation dots, and the portfolio page.

The **Change & % basis** setting in the Ticker tab picks what the change measures. The default, *Chart timeframe*, computes it over the same span the sparkline shows (live price versus the first charted point) and appends the live price as the chart's newest point, so the number, arrow, colours, and chart agree. Three caveats: it needs chart data, so with fewer than 2 chart points, a webhook that sends no `spark` series, or a failed chart fetch, the device falls back to the 1-day change until the data is there; at the 1-day timeframe the reference is the session's first data point, so an overnight gap is not part of the number; and GitHub-source tickers chart the span baked into `quotes-config.json`, so their change covers that span rather than the device timeframe. *1 day* shows the classic change since the previous close instead; a stock can be up on the day but down over a longer chart, so with this basis the number and the chart can legitimately point in opposite directions.

Non-USD currencies show as their ISO code, for example `CHF 79.73`, because the built-in bitmap font has no glyph for symbols like the euro sign.

## Symbols

Add up to 8 tickers in the **Ticker** tab. Each row has:

- **Enable** — disable a symbol without deleting it (greyed out in the web UI; skipped on the device)
- **Reorder** — up/down arrows change rotation order
- Symbol, optional name, data source, and optional `qty` / `cost` for positions

Yahoo examples that work:

| What | Examples |
|------|----------|
| US and global stocks and ETFs | `AAPL`, `MSFT`, `VOO` |
| Swiss and European stocks | `NESN.SW`, `ROG.SW`, `UBSG.SW`, `BMW.DE` |
| Crypto | `BTC-USD`, `ETH-EUR` |
| FX | `EURUSD=X`, `EURCHF=X` |

With the cash.ch source the `symbol` field takes a cash.ch listing key instead (`valor-marketId-currencyId`, e.g. `147478611-246-333`), which covers Swiss structured products and AMCs that Yahoo does not list. The built-in finder in the Ticker tab turns a cash.ch link, ISIN, or name into the key; [Data sources](/smalltv-mod/reference/data-sources/) has the details.

Use **Skip to next page on device** in the Ticker tab to advance rotation immediately from your phone.

## Positions and the portfolio page

Give a ticker a `qty` and a per-unit `cost` and it becomes a position: its page shows a P/L line (absolute and percent versus your cost basis), and a portfolio summary page joins the rotation with one row per position and a total per currency. The "Position P/L & portfolio page" toggle in the Ticker tab turns both off. Cost is per unit in the instrument's own currency; totals are kept per currency and are not converted.

## Timing and data

Two intervals control the display: how often each symbol or page is shown (rotation) and how often data is refreshed (poll). Both are set in the Ticker tab. The default poll of 120 seconds is fine for 8 symbols.

The **Status** tab lists each ticker with source, price, and age since the last good fetch. Tap **Refresh data now** to force an immediate poll.

Where the prices come from is chosen per ticker. By default a ticker fetches Yahoo Finance directly over HTTPS with no backend; cash.ch works the same way for Swiss instruments, GitHub is a serverless cash.ch proxy that needs nothing of yours running, and a webhook ticker calls your own endpoint. All four are covered in [Data sources](/smalltv-mod/reference/data-sources/).
