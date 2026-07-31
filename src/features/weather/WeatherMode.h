// WeatherMode.h — OpenWeather current conditions + 3-day forecast
#pragma once
#include "Mode.h"
#include "config.h"

class WeatherMode : public DisplayMode {
 public:
  const char* id() const override { return "weather"; }
  uint8_t     modeConst() const override { return MODE_WEATHER; }

  void begin(const Settings& s) override;
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override;
  void wake(const Settings& s) override { needRender_ = true; }

 private:
  void render(const Settings& s);

  uint32_t renderedOk_ = 0xFFFFFFFF;
  bool     renderedError_ = false;
  bool     needRender_ = true;
};

extern WeatherMode g_weatherMode;
