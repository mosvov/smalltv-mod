#include "WeatherIcons.h"
#include <Arduino_GFX_Library.h>
#include <math.h>
#include "Gfx.h"

static void drawCloud(Arduino_GFX* gfx, int cx, int cy, int r, uint16_t color) {
  gfx->fillCircle(cx - r, cy + 1, r - 1, color);
  gfx->fillCircle(cx, cy - r / 2, r, color);
  gfx->fillCircle(cx + r, cy + 1, r - 1, color);
}

static void drawSun(Arduino_GFX* gfx, int cx, int cy, int r) {
  gfx->fillCircle(cx, cy, r, C_YELLOW);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    int x1 = cx + (int)(cosf(a) * (r + 3));
    int y1 = cy + (int)(sinf(a) * (r + 3));
    int x2 = cx + (int)(cosf(a) * (r + 7));
    int y2 = cy + (int)(sinf(a) * (r + 7));
    gfx->drawLine(x1, y1, x2, y2, C_YELLOW);
  }
}

static void drawMoon(Arduino_GFX* gfx, int cx, int cy, int r) {
  gfx->fillCircle(cx, cy, r, C_GRAY);
  gfx->fillCircle(cx + r / 2, cy - 1, r - 2, C_BLACK);
}

static void drawRain(Arduino_GFX* gfx, int cx, int cy, int r) {
  drawCloud(gfx, cx, cy - 2, r, C_GRAY);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * (r / 2 + 2);
    gfx->drawLine(x, cy + r / 2, x - 2, cy + r + 5, C_BLUE);
  }
}

static void drawSnow(Arduino_GFX* gfx, int cx, int cy, int r) {
  drawCloud(gfx, cx, cy - 2, r, C_WHITE);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * (r / 2 + 2);
    int y = cy + r + 2;
    gfx->drawPixel(x, y - 2, C_WHITE);
    gfx->drawPixel(x, y + 2, C_WHITE);
    gfx->drawPixel(x - 2, y, C_WHITE);
    gfx->drawPixel(x + 2, y, C_WHITE);
  }
}

static void drawStorm(Arduino_GFX* gfx, int cx, int cy, int r) {
  drawCloud(gfx, cx, cy - 2, r, C_DGRAY);
  gfx->drawLine(cx + 2, cy + r / 2, cx - 3, cy + r + 4, C_YELLOW);
  gfx->drawLine(cx - 3, cy + r + 4, cx + 4, cy + r / 2 + 2, C_YELLOW);
}

static void drawFog(Arduino_GFX* gfx, int cx, int cy, int r) {
  for (int i = -2; i <= 2; i++)
    gfx->drawFastHLine(cx - r, cy + i * 4, r * 2, C_GRAY);
}

void weatherDrawIcon(Arduino_GFX* gfx, const char* code, int cx, int cy, int radius) {
  if (!gfx || !code || code[0] == 0 || code[1] == 0) return;
  int id = (code[0] - '0') * 10 + (code[1] - '0');
  bool night = code[2] == 'n';
  int r = radius > 4 ? radius : 12;

  switch (id) {
    case 1:
      if (night) drawMoon(gfx, cx, cy, r);
      else drawSun(gfx, cx, cy, r - 2);
      break;
    case 2:
      if (!night) drawSun(gfx, cx - r - 4, cy, r - 4);
      drawCloud(gfx, cx + 4, cy, r - 2, C_WHITE);
      break;
    case 3:
      drawCloud(gfx, cx, cy, r, C_WHITE);
      break;
    case 4:
      drawCloud(gfx, cx, cy, r, C_GRAY);
      break;
    case 9:
    case 10:
      drawRain(gfx, cx, cy, r);
      break;
    case 11:
      drawStorm(gfx, cx, cy, r);
      break;
    case 13:
      drawSnow(gfx, cx, cy, r);
      break;
    case 50:
      drawFog(gfx, cx, cy, r / 2);
      break;
    default:
      drawCloud(gfx, cx, cy, r - 2, C_GRAY);
      break;
  }
}
