// WeatherData.h — runtime weather snapshot for the weather feature
#pragma once
#include <Arduino.h>
#include "config.h"

struct ForecastDay {
  char  label[4];   // e.g. "Mon"
  float hi;
  float lo;
};

struct WeatherData {
  bool  valid;
  bool  error;
  char  errorMsg[24];     // short UI message, e.g. "Err city!"
  uint32_t lastOkMs;

  char  city[MAX_CITY_LEN];
  float temp;
  float feelsLike;
  float tempMin;
  float tempMax;
  uint8_t humidity;
  float wind;             // m/s or mph per units setting
  char  description[MAX_WEATHER_DESC];

  uint8_t     forecastCount;
  ForecastDay forecast[WEATHER_FORECAST_DAYS];

  void clear() {
    valid = false;
    error = false;
    errorMsg[0] = 0;
    lastOkMs = 0;
    city[0] = 0;
    temp = feelsLike = tempMin = tempMax = wind = 0;
    humidity = 0;
    description[0] = 0;
    forecastCount = 0;
    for (uint8_t i = 0; i < WEATHER_FORECAST_DAYS; i++) {
      forecast[i].label[0] = 0;
      forecast[i].hi = forecast[i].lo = 0;
    }
  }
};
