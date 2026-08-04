#include "UsageMode.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"
#include "UsageClient.h"
#include "Mascot.h"

UsageMode g_usageMode;

#define C_ACCENT  0xDBAA
#define C_UGREEN  0x7C6B
#define C_PANEL   0x18E3
#define C_BARBG   0x2945
#define C_DIM     0xB574

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

static void drawProviderRow(Arduino_GFX* gfx, int y, const char* label,
                            const ProviderMeter& m) {
  const int x = 8, w = 224, h = 52;
  gfx->fillRoundRect(x, y, w, h, 6, C_PANEL);

  gfx->setTextSize(1);
  gfx->setTextColor(C_DIM);
  gfx->setCursor(x + 10, y + 8);
  gfx->print(label);

  const char* val = m.ok && m.line[0] ? m.line : "N/A";
  int tw = gfxTextW(val, 1);
  gfx->setTextColor(m.ok ? C_WHITE : C_DIM);
  gfx->setCursor(x + w - tw - 10, y + 8);
  gfx->print(val);

  int bx = x + 10, by = y + 24, bw = w - 20, bh = 10;
  gfx->fillRoundRect(bx, by, bw, bh, bh / 2, C_BARBG);
  if (m.ok) {
    int fw = (int)(bw * constrain(m.pct, 0.0f, 100.0f) / 100.0f);
    if (fw >= bh) gfx->fillRoundRect(bx, by, fw, bh, bh / 2, barColor(m.pct));
    else if (fw > 0) gfx->fillRect(bx, by, fw, bh, barColor(m.pct));
  }

  if (m.ok && m.sub[0]) {
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(x + 10, y + 38);
    gfx->print(m.sub);
  }
}

static void drawUsage(const UsageData& u, const UsageSettings& cfg) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  s_mascotPrimed = false;
  gfx->fillScreen(C_BLACK);

  gfx->setTextSize(2);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(12, 6);
  gfx->print("AI USAGE");

  if (!u.valid) {
    gfxDrawCentered(u.error ? "daemon error" : "waiting...", 120, 2, C_DIM);
    return;
  }

  int y = 28;
  if (cfg.showClaude) { drawProviderRow(gfx, y, "CLAUDE", u.claude); y += 58; }
  if (cfg.showCursor) { drawProviderRow(gfx, y, "CURSOR", u.cursor); y += 58; }
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
  usageInit(s);
  usageForceRefresh();
}

void UsageMode::service(const Settings& s) {
  if (s.usage.usageUrl.length() >= 8) usageService(s);

  const UsageData& u = usageGet();

  if (u.valid && u.lastOkMs != usageSampled_) {
    usageSampled_ = u.lastOkMs;
    if (u.claude.ok) mascotSample(u.claude.pct);
  }

  uint32_t staleMs = (uint32_t)s.usage.pollSec * 1000UL * 2UL + USAGE_STALE_GRACE_MS;

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
