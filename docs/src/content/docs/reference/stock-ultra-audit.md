---
title: Stock Ultra audit
description: Screens and features in GeekMagic stock Ultra firmware (V9.0.50) and what smalltv-mod covers or could import next.
---

[GeekMagicClock/smalltv-ultra](https://github.com/GeekMagicClock/smalltv-ultra) ships **binary-only** firmware (latest **V9.0.50**). There is no source to port — this page records stock behaviour from the vendor `update_history.txt`, user manuals, and community reverse-engineering, then maps it to smalltv-mod.

## Stock screens / themes

| Screen / theme | Stock behaviour | smalltv-mod |
|----------------|-----------------|-------------|
| **Weather clock** (Simple / full) | OpenWeather current + multi-day forecast, city, °F/°C, 12/24h | **Weather mode** (v2.11+) |
| **Clock faces** (time style 1/2) | Full-screen clock, date, colon blink, RGB colour | Partial — HH:MM overlay + night mode |
| **Photo album** | JPG/GIF upload to flash, slideshow, offline `GIFTV` AP | Not yet |
| **Crypto prices** | Basic crypto display | **Ticker** (Yahoo/webhook — richer) |
| **Theme loop** | Rotate stock themes on a timer | **Carousel** mode |

## Stock device / web features

| Feature | Stock | smalltv-mod |
|---------|-------|-------------|
| OpenWeather API key + city | Yes | **Weather tab** |
| Scheduled night mode | Yes | **Display → Clock & night mode** |
| WiFi manager + retry | Yes | **WiFi tab** (up to 4 networks) |
| NTP + timezone + DST | Yes | POSIX TZ in web UI |
| Weather time-sync fallback (V9.0.48) | Uses OpenWeather `dt` when NTP blocked | Backlog |
| Image push HTTP API (`/doUpload`) | Yes | Backlog (HA integrations target stock FW) |
| Custom 80×80 weather GIFs | Yes | Backlog |
| Factory reset | Yes | Web UI reset |

## Import backlog (prioritized)

1. **Photo / slideshow mode** — high demand; needs LittleFS image storage + JPEG decode (GIF harder on ESP8266).
2. **Dedicated clock face mode** — medium effort; reuses existing NTP and `Clock.cpp`.
3. **Weather time-sync fallback** — low; mirror stock V9.0.48 when NTP is blocked.
4. **Stock-compatible push API subset** — low unless Home Assistant compatibility is a goal.

## References

- [Ultra-V9.0.50 update history](https://github.com/GeekMagicClock/smalltv-ultra/blob/main/Ultra-V9.0.50/update_history.txt)
- [User manual PDF (V9.0.30)](https://github.com/GeekMagicClock/smalltv-ultra/blob/main/Latest%20GeekMagic%20SmallTV-Ultra%20User%20Manual%20V9.0.30.pdf)
- smalltv-mod [Weather mode](/smalltv-mod/features/weather/) docs
