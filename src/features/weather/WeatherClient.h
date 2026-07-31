// WeatherClient.h — fetches current weather + forecast from OpenWeatherMap
#pragma once
#include "Settings.h"
#include "WeatherData.h"

void weatherInit(const Settings& s);
void weatherService(const Settings& s);
void weatherForceRefresh();

const WeatherData& weatherData();
uint32_t           weatherLastOkMs();
bool               weatherError();
