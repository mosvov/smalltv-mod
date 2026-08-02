// WeatherIcons.h — simple OpenWeather icon glyphs (GFX primitives, no bitmaps)
#pragma once
class Arduino_GFX;

void weatherDrawIcon(Arduino_GFX* gfx, const char* iconCode, int cx, int cy, int radius);
