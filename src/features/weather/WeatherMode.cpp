#include "WeatherMode.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"
#include "Net.h"
#include "Clock.h"
#include "WeatherClient.h"

WeatherMode g_weatherMode;

static int drawStatusBar(const Settings& s) {
  if (!s.weather.showClock && !s.weather.showWifi) return 0;
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return 0;

  if (s.weather.showClock) {
    String hm = clockTimeHm();
    gfx->setTextSize(1);
    gfx->setTextColor(hm.length() ? C_GRAY : C_DGRAY);
    gfx->setCursor(4, 2);
    gfx->print(hm.length() ? hm.c_str() : "--:--");
  }

  if (s.weather.showWifi) {
    uint16_t c = C_RED;
    if (netConnected()) {
      int rssi = netRSSI();
      c = rssi >= -60 ? C_GREEN : (rssi >= -75 ? C_YELLOW : C_RED);
    }
    gfx->fillCircle(TFT_WIDTH - 6, 5, 3, c);
  }
  return 14;
}

void WeatherMode::begin(const Settings& s) {
  weatherInit(s);
  weatherForceRefresh();
  needRender_ = true;
  renderedOk_ = 0xFFFFFFFF;
  renderedError_ = false;
}

void WeatherMode::invalidate(const Settings& s) {
  weatherInit(s);
  weatherForceRefresh();
  needRender_ = true;
  renderedOk_ = 0xFFFFFFFF;
  renderedError_ = false;
}

void WeatherMode::render(const Settings& s) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);

  const WeatherData& d = weatherData();
  int y0 = drawStatusBar(s) + 4;

  if (d.error || !d.valid) {
    const char* msg = d.errorMsg[0] ? d.errorMsg
                    : (s.weather.apiKey.length() < 8 ? "Set API key"
                       : s.weather.city.length() < 2 ? "Set city" : "Loading...");
    gfxDrawCentered(msg, y0 + 90, 2, d.error ? C_RED : C_GRAY);
    if (d.error) gfx->fillCircle(8, y0 + 8, 3, C_RED);
    return;
  }

  bool metric = s.weather.unitsMetric;
  char buf[32];

  gfxDrawCentered(d.city[0] ? d.city : s.weather.city.c_str(), y0 + 4, 2, C_WHITE);

  snprintf(buf, sizeof(buf), "%.0f%s", d.temp, metric ? "°C" : "°F");
  gfxDrawCentered(buf, y0 + 28, 4, C_WHITE);

  if (d.description[0])
    gfxDrawCentered(d.description, y0 + 72, 1, C_GRAY);

  snprintf(buf, sizeof(buf), "H:%.0f L:%.0f  %u%%",
           d.tempMax, d.tempMin, (unsigned)d.humidity);
  gfxDrawCentered(buf, y0 + 88, 1, C_DGRAY);

  if (s.weather.showForecast && d.forecastCount > 0) {
    int fy = y0 + 108;
    gfx->drawFastHLine(12, fy, TFT_WIDTH - 24, C_DGRAY);
    fy += 8;

    int colW = (TFT_WIDTH - 16) / (int)d.forecastCount;
    for (uint8_t i = 0; i < d.forecastCount; i++) {
      int cx = 8 + (int)i * colW + colW / 2;
      gfx->setTextSize(1);
      gfx->setTextColor(C_GRAY);
      int tw = gfxTextW(d.forecast[i].label, 1);
      gfx->setCursor(cx - tw / 2, fy);
      gfx->print(d.forecast[i].label);

      snprintf(buf, sizeof(buf), "%.0f", d.forecast[i].hi);
      tw = gfxTextW(buf, 1);
      gfx->setTextColor(C_WHITE);
      gfx->setCursor(cx - tw / 2, fy + 12);
      gfx->print(buf);

      snprintf(buf, sizeof(buf), "%.0f", d.forecast[i].lo);
      tw = gfxTextW(buf, 1);
      gfx->setTextColor(C_DGRAY);
      gfx->setCursor(cx - tw / 2, fy + 22);
      gfx->print(buf);
    }
  }
}

void WeatherMode::service(const Settings& s) {
  // Data fetch runs from main loop so weather updates in any display mode.

  const WeatherData& d = weatherData();
  uint32_t ok = d.lastOkMs;
  bool err = d.error;
  if (ok != renderedOk_ || err != renderedError_) {
    renderedOk_ = ok;
    renderedError_ = err;
    needRender_ = true;
  }

  if (needRender_) {
    render(s);
    needRender_ = false;
  }
}
