#include "UsageMode.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"
#include "UsageClient.h"
#include "Mascot.h"

UsageMode g_usageMode;

#define C_ACCENT  0xDBAA
#define C_UGREEN  0x7C6B
#define C_BARBG   0x2945
#define C_DIM     0xB574

#define C_PACE    0xFFFF
#define C_AHEAD   0x4208   // dim green tint for under-pace headroom

#define ROW_H     56
#define BAR_H     20
#define MARGIN_X  10
#define LABEL_SZ  1
#define VALUE_SZ  2

static bool            s_mascotPrimed  = false;
static const uint16_t* s_mascotPalette = nullptr;
static uint8_t         s_prevCells[MASCOT_GRID * MASCOT_GRID];

static void loadPalette(const uint16_t* palette, uint16_t* out) {
  const uint8_t* p = (const uint8_t*)palette;
  for (int k = 0; k < MASCOT_PALETTE_SIZE; k++)
    out[k] = (uint16_t)(pgm_read_byte(p + 2 * k) | (pgm_read_byte(p + 2 * k + 1) << 8));
}

static void blitMascot(Arduino_GFX* gfx, const uint8_t* cells, const uint16_t* palette,
                       int x0, int y0, int cellPx) {
  uint16_t pal[MASCOT_PALETTE_SIZE];
  loadPalette(palette, pal);
  for (int i = 0; i < MASCOT_GRID * MASCOT_GRID; i++) {
    uint8_t code = pgm_read_byte(&cells[i]);
    uint16_t color = (code < MASCOT_PALETTE_SIZE) ? pal[code] : 0;
    int gx = i % MASCOT_GRID, gy = i / MASCOT_GRID;
    gfx->fillRect(x0 + gx * cellPx, y0 + gy * cellPx, cellPx, cellPx, color);
  }
}

static uint16_t barColor(float pct) {
  if (pct >= 90) return C_RED;
  if (pct >= 75) return C_ACCENT;
  return C_UGREEN;
}

// Round "497.7K" -> "498K", "105/1000" -> "105/1k" for the small panel.
static void formatValue(const char* in, char* out, size_t n) {
  if (!in || !in[0]) { out[0] = 0; return; }

  const char* k1000 = strstr(in, "/1000");
  if (k1000) {
    snprintf(out, n, "%.*s/1k", (int)(k1000 - in), in);
    return;
  }

  float f = 0;
  char unit = 0;
  if (sscanf(in, "%f%c", &f, &unit) == 2 && (unit == 'K' || unit == 'M')) {
    snprintf(out, n, "%d%c", (int)(f + 0.5f), unit);
    return;
  }

  strlcpy(out, in, n);
}

static void fillBarSegment(Arduino_GFX* gfx, int bx, int by, int x0, int x1, int h, uint16_t col) {
  if (x1 <= x0) return;
  int w = x1 - x0;
  int r = h / 2;
  if (w >= h) gfx->fillRoundRect(bx + x0, by, w, h, r, col);
  else gfx->fillRect(bx + x0, by, w, h, col);
}

static void drawPaceBar(Arduino_GFX* gfx, int bx, int by, int bw, const ProviderMeter& m) {
  gfx->fillRoundRect(bx, by, bw, BAR_H, BAR_H / 2, C_BARBG);
  if (!m.ok) return;

  float usePct = constrain(m.pct, 0.0f, 100.0f);
  int useW = (int)(bw * usePct / 100.0f);
  bool hasPace = m.pacePct >= 0.0f;
  int paceW = hasPace ? (int)(bw * constrain(m.pacePct, 0.0f, 100.0f) / 100.0f) : 0;

  if (hasPace && paceW > 0) {
    if (useW <= paceW) {
      fillBarSegment(gfx, bx, by, 0, useW, BAR_H, C_UGREEN);
      if (useW < paceW)
        fillBarSegment(gfx, bx, by, useW, paceW, BAR_H, C_AHEAD);
    } else {
      fillBarSegment(gfx, bx, by, 0, paceW, BAR_H, C_UGREEN);
      fillBarSegment(gfx, bx, by, paceW, useW, BAR_H, C_ACCENT);
    }
    if (paceW < bw - 1) {
      gfx->drawFastVLine(bx + paceW, by + 2, BAR_H - 4, C_PACE);
      gfx->drawFastVLine(bx + paceW + 1, by + 2, BAR_H - 4, C_PACE);
    }
  } else if (useW > 0) {
    fillBarSegment(gfx, bx, by, 0, useW, BAR_H, barColor(usePct));
  }
}

static void drawProviderRow(Arduino_GFX* gfx, int y, const char* label,
                            const ProviderMeter& m) {
  const int w = TFT_WIDTH - 2 * MARGIN_X;
  char val[20];
  if (m.ok && m.line[0]) formatValue(m.line, val, sizeof(val));
  else strlcpy(val, "N/A", sizeof(val));

  gfx->setTextSize(LABEL_SZ);
  gfx->setTextColor(C_DIM);
  gfx->setCursor(MARGIN_X, y + 2);
  gfx->print(label);

  gfx->setTextSize(VALUE_SZ);
  int tw = gfxTextW(val, VALUE_SZ);
  gfx->setTextColor(m.ok ? C_WHITE : C_DIM);
  gfx->setCursor(MARGIN_X + w - tw, y);
  gfx->print(val);

  int bx = MARGIN_X, by = y + 22, bw = w;
  drawPaceBar(gfx, bx, by, bw, m);
}

static void drawUsage(const UsageData& u, const UsageSettings& cfg) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  s_mascotPrimed = false;
  gfx->fillScreen(C_BLACK);

  if (!u.valid) {
    gfxDrawCentered(u.error ? "daemon error" : "waiting...", 120, 2, C_DIM);
    return;
  }

  uint8_t rows = 0;
  if (cfg.showClaude) rows++;
  if (cfg.showCursor) rows++;
  if (cfg.showCodex)  rows++;

  int y = (TFT_HEIGHT - (int)rows * ROW_H) / 2;
  if (y < 4) y = 4;

  if (cfg.showClaude) { drawProviderRow(gfx, y, "CLAUDE", u.claude); y += ROW_H; }
  if (cfg.showCursor) { drawProviderRow(gfx, y, "CURSOR", u.cursor); y += ROW_H; }
  if (cfg.showCodex)  { drawProviderRow(gfx, y, "CODEX",  u.codex); }
}

static void drawMascot(const uint8_t* cells, const uint16_t* palette, bool restart) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx || !cells || !palette) return;
  uint16_t pal[MASCOT_PALETTE_SIZE];
  loadPalette(palette, pal);
  const int CP = TFT_WIDTH / MASCOT_GRID;
  const int x0 = (TFT_WIDTH  - MASCOT_GRID * CP) / 2;
  const int y0 = (TFT_HEIGHT - MASCOT_GRID * CP) / 2;

  bool full = restart || !s_mascotPrimed || palette != s_mascotPalette;
  if (full) gfx->fillScreen(C_BLACK);

  for (int i = 0; i < MASCOT_GRID * MASCOT_GRID; i++) {
    uint8_t code = pgm_read_byte(&cells[i]);
    if (!full && code == s_prevCells[i]) continue;
    s_prevCells[i] = code;
    uint16_t color = (code < MASCOT_PALETTE_SIZE) ? pal[code] : 0;
    int gx = i % MASCOT_GRID, gy = i / MASCOT_GRID;
    gfx->fillRect(x0 + gx * CP, y0 + gy * CP, CP, CP, color);
  }
  s_mascotPrimed  = true;
  s_mascotPalette = palette;
}

void UsageMode::begin(const Settings& s) {
  usageInit(s);
  mascotInit();
  usageSampled_ = 0;
  usageRenderedOk_ = 0xFFFFFFFF;
  showingMascot_ = false;
  needRender_ = true;
}

void UsageMode::invalidate(const Settings& s) {
  needRender_ = true;
  showingMascot_ = false;
  usageRenderedOk_ = 0xFFFFFFFF;
  if (s.usage.usageUrl.length() >= 8) usageForceRefresh();
}

void UsageMode::service(const Settings& s) {
  if (s.usage.usageUrl.length() >= 8) usageService(s);

  const UsageData& u = usageGet();

  if (u.valid && u.lastOkMs != usageSampled_) {
    usageSampled_ = u.lastOkMs;
    if (u.claude.ok) mascotSample(u.claude.pct);
  }

  uint32_t staleMs;
  if (s.usage.usageUrl.length() >= 8) {
    staleMs = (uint32_t)s.usage.pollSec * 1000UL * 2UL + USAGE_STALE_GRACE_MS;
  } else {
    staleMs = USAGE_PUSH_STALE_MS;
  }

  if (usageFresh(staleMs)) {
    if (showingMascot_) { showingMascot_ = false; needRender_ = true; }
    if (u.lastOkMs != usageRenderedOk_) { usageRenderedOk_ = u.lastOkMs; needRender_ = true; }
    if (needRender_) { drawUsage(u, s.usage); needRender_ = false; }
  } else {
    if (!showingMascot_) {
      showingMascot_ = true;
      usageRenderedOk_ = 0xFFFFFFFF;
      mascotReset();
      drawMascot(mascotCells(), mascotPalette(), true);
    } else if (mascotTick()) {
      drawMascot(mascotCells(), mascotPalette(), false);
    }
  }
}
