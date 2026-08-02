#include "WeatherIcons.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"

static void drawCloud(Arduino_GFX* gfx, int cx, int cy, int r, uint16_t color) {
  gfx->fillCircle(cx - r / 2, cy, r, color);
  gfx->fillCircle(cx + r / 3, cy - r / 4, r - 2, color);
  gfx->fillCircle(cx + r, cy + r / 6, r - 3, color);
  gfx->fillRect(cx - r, cy, r * 3, r, color);
}

static void drawRain(Arduino_GFX* gfx, int cx, int cy, int r) {
  drawCloud(gfx, cx, cy - r / 3, r, C_GRAY);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * (r / 2);
    gfx->drawLine(x, cy + r / 2, x - 2, cy + r + 4, C_BLUE);
    gfx->drawLine(x + 1, cy + r / 2, x - 1, cy + r + 4, C_BLUE);
  }
}

static void drawSnow(Arduino_GFX* gfx, int cx, int cy, int r) {
  drawCloud(gfx, cx, cy - r / 3, r, C_WHITE);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * (r / 2);
    int y = cy + r;
    gfx->drawLine(x, y - 3, x, y + 3, C_WHITE);
    gfx->drawLine(x - 3, y, x + 3, y, C_WHITE);
  }
}

static void drawStorm(Arduino_GFX* gfx, int cx, int cy, int r) {
  drawCloud(gfx, cx, cy - r / 3, r, C_DGRAY);
  gfx->drawLine(cx, cy + r / 2, cx - 4, cy + r + 2, C_YELLOW);
  gfx->drawLine(cx - 4, cy + r + 2, cx + 2, cy + r / 2 + 2, C_YELLOW);
}

static void drawFog(Arduino_GFX* gfx, int cx, int cy, int r) {
  for (int i = -2; i <= 2; i++)
    gfx->drawFastHLine(cx - r, cy + i * 5, r * 2, C_GRAY);
}

void weatherDrawIcon(Arduino_GFX* gfx, const char* code, int cx, int cy, int radius) {
  if (!gfx || !code || code[0] == 0 || code[1] == 0) return;
  int id = (code[0] - '0') * 10 + (code[1] - '0');
  bool night = code[2] == 'n';
  int r = radius > 4 ? radius : 12;

  switch (id) {
    case 1:
      if (night) {
        gfx->fillCircle(cx, cy, r, C_GRAY);
        gfx->fillCircle(cx + r / 2, cy - 1, r - 2, C_BLACK);
      } else {
        gfx->fillCircle(cx, cy, r, C_YELLOW);
      }
      break;
    case 2:
      if (!night) gfx->fillCircle(cx - r, cy - r / 2, r / 2 + 2, C_YELLOW);
      drawCloud(gfx, cx + r / 6, cy, r - 2, C_WHITE);
      break;
    case 3:
    case 4:
      drawCloud(gfx, cx, cy, r, id == 4 ? C_GRAY : C_WHITE);
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
