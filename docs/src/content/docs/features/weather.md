---
title: Weather mode
description: OpenWeatherMap current conditions and a 3-day forecast on the 240×240 display.
---

Weather mode fetches live data from [OpenWeatherMap](https://openweathermap.org/api) over HTTPS — the same provider the stock GeekMagic Ultra firmware uses. You need a free API key; the stock firmware dropped its shared key in V9.0.11, so every device must register its own.

## Setup

1. Create a free account at [openweathermap.org](https://home.openweathermap.org/users/sign_up).
2. Generate an API key under **My API keys**.
3. In the device web UI, open the **Weather** tab.
4. Paste the key, set your **City** (optionally with country code, e.g. `Zurich,CH` or `London,UK`), and save.

Switch **Display → Mode** to **Weather**, or add Weather to a **Carousel** rotation.

## What it shows

- City name and large current temperature (°C or °F).
- Text condition description (no icon bitmaps in v1 — keeps flash and RAM free).
- Today’s high/low and humidity.
- Optional **3-day forecast** row with weekday labels and high/low per day.
- Optional **clock** (HH:MM) and **WiFi signal** dot in the status strip (same idea as the ticker overlay).

## Refresh interval

Default poll is **1200 seconds (20 minutes)**, matching the stock Ultra cadence. OpenWeather’s free tier allows 1000 calls/day — at 20 minutes that is well within limits.

Use **Status → Refresh data now** to force an immediate fetch after changing city or key.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `Set API key` on screen | Add your OpenWeather key in the Weather tab |
| `Err city!` | Check spelling; try `City,CountryCode` |
| `Fetch failed` | WiFi or TLS issue; check Status tab signal and heap |
| Forecast row empty | Enable **3-day forecast row**; wait for the second API call after current weather |

For how this compares to stock Ultra themes and what else could be imported, see [Stock Ultra audit](/smalltv-mod/reference/stock-ultra-audit/).
