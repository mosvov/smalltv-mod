#include "TickerMode.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"
#include "Net.h"
#include "StockClient.h"

TickerMode g_tickerMode;

// ---- sparkline ------------------------------------------------------------
// livePt (NAN = none) is appended as the newest point so the chart ends at the
// live price rather than the last historical close.
static void drawSparkline(Arduino_GFX* gfx, const StockData& d,
                          int top, int bottom, uint16_t color, float livePt) {
  if (d.sparkCount < 2) return;
  bool live = !isnan(livePt);
  uint8_t n = d.sparkCount + (live ? 1 : 0);
  float mn = d.spark[0], mx = d.spark[0];
  for (uint8_t i = 1; i < d.sparkCount; i++) {
    if (d.spark[i] < mn) mn = d.spark[i];
    if (d.spark[i] > mx) mx = d.spark[i];
  }
  if (live) {
    if (livePt < mn) mn = livePt;
    if (livePt > mx) mx = livePt;
  }
  float span = mx - mn;
  if (span <= 0) span = 1;

  const int padL = 6, padR = 6;
  int plotW = TFT_WIDTH - padL - padR;
  int plotH = bottom - top;

  int prevX = 0, prevY = 0;
  for (uint8_t i = 0; i < n; i++) {
    float v = (i < d.sparkCount) ? d.spark[i] : livePt;
    int x = padL + (int)((long)plotW * i / (n - 1));
    int y = bottom - (int)((v - mn) / span * plotH);
    if (i > 0) {
      gfx->drawLine(prevX, prevY, x, y, color);
      gfx->drawLine(prevX, prevY + 1, x, y + 1, color); // 2px thick
    }
    prevX = x;
    prevY = y;
  }
}

// ---- number formatting ----------------------------------------------------
static void fmtPrice(float v, char* out, size_t n) {
  float a = fabsf(v);
  if (a >= 1000)      snprintf(out, n, "%.2f", v);
  else if (a >= 1)    snprintf(out, n, "%.2f", v);
  else if (a >= 0.01) snprintf(out, n, "%.4f", v);
  else                snprintf(out, n, "%.6f", v);
}

// ---- grid layout (N tickers per screen) -----------------------------------
static void layoutGrid(uint8_t perScreen, uint8_t& cols, uint8_t& rows) {
  switch (perScreen) {
    case 2: cols = 1; rows = 2; break;
    case 3: cols = 1; rows = 3; break;
    case 4: cols = 2; rows = 2; break;
    case 5: cols = 2; rows = 3; break;
    case 6: cols = 2; rows = 3; break;
    default: cols = 1; rows = 1; break;
  }
}

static uint8_t tilesPerScreen(const Settings& s) {
  uint8_t n = s.ticker.tilesPerScreen;
  if (n < 1) n = 1;
  if (n > 6) n = 6;
  return n;
}

static uint8_t symbolPages(uint8_t symCount, uint8_t perScreen) {
  if (symCount == 0) return 0;
  return (symCount + perScreen - 1) / perScreen;
}

static void drawPageDots(Arduino_GFX* gfx, uint8_t pageIndex, uint8_t pageCount, int y) {
  if (pageCount <= 1) return;
  int total = pageCount * 10 - 4;
  int x0 = (TFT_WIDTH - total) / 2;
  for (uint8_t i = 0; i < pageCount; i++)
    gfx->fillCircle(x0 + i * 10 + 2, y + 3, 2, i == pageIndex ? C_WHITE : C_DGRAY);
}

// Compact tile for multi-ticker layouts (2..6 per screen).
static void drawCompactTile(Arduino_GFX* gfx, const StockData& d,
                            int x, int y, int w, int h,
                            const Settings& s, bool allowChart) {
  int pad = 3;
  int cy = y + 2;

  if (!d.valid) {
    const char* label = d.symbol[0] ? d.symbol : "----";
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY);
    gfx->setCursor(x + pad, cy + (h > 20 ? h / 2 - 4 : 2));
    gfx->print(label);
    if (d.error) gfx->fillCircle(x + 4, y + 4, 2, C_RED);
    return;
  }

  float chg = 0, pct = 0;
  bool onRange = false;
  bool hasChange = stockDisplayChange(d, s.ticker, chg, pct, &onRange);
  bool up = hasChange ? (chg >= 0) : true;
  uint16_t upC   = s.ticker.colorInverted ? C_RED : C_GREEN;
  uint16_t downC = s.ticker.colorInverted ? C_GREEN : C_RED;
  uint16_t trendC = !hasChange ? C_WHITE : (up ? upC : downC);

  const char* label = d.name[0] ? d.name : d.symbol;
  if (s.ticker.showName && h >= 28) {
    char nm[16];
    strlcpy(nm, label, sizeof(nm));
    uint8_t nsz = gfxFitSize(nm, w - 2 * pad, h >= 70 ? 2 : 1);
    gfx->setTextSize(nsz);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(x + pad, cy);
    gfx->print(nm);
    cy += 8 * nsz + 2;
  }

  if (s.ticker.showPrice) {
    char num[20];
    fmtPrice(d.price, num, sizeof(num));
    char line[28];
    snprintf(line, sizeof(line), "%s%s", d.currency, num);
    uint8_t psz = gfxFitSize(line, w - 2 * pad, h >= 70 ? 3 : 2);
    gfx->setTextSize(psz);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(x + pad, cy);
    gfx->print(line);
    cy += 8 * psz + 2;
  }

  if (s.ticker.showChange && hasChange && h >= 36) {
    char line[24];
    if (pct != 0 || chg != 0)
      snprintf(line, sizeof(line), "%+.2f (%+.1f%%)", chg, pct);
    else
      snprintf(line, sizeof(line), "%+.2f", chg);
    gfx->setTextSize(1);
    gfx->setTextColor(trendC);
    gfx->setCursor(x + pad, cy);
    gfx->print(line);
    cy += 10;
  }

  if (allowChart && s.ticker.showChart && d.sparkCount >= 2 && (y + h - cy) >= 18) {
    float livePt = onRange ? d.price : NAN;
    drawSparkline(gfx, d, cy, y + h - 3, trendC, livePt);
  }

  if (d.error) gfx->fillCircle(x + 4, y + 4, 2, C_RED);
}

static void drawMultiStock(uint8_t pageIndex, uint8_t pageCount,
                           uint8_t symStart, uint8_t symOnPage,
                           const Settings& s) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);

  int y0 = 4;
  if (s.ticker.showPageDots && pageCount > 1) {
    drawPageDots(gfx, pageIndex, pageCount, 6);
    y0 = 20;
  }

  uint8_t perScreen = tilesPerScreen(s);
  uint8_t cols, rows;
  layoutGrid(perScreen, cols, rows);

  const int margin = 2;
  const int availH = TFT_HEIGHT - y0 - margin;
  const int availW = TFT_WIDTH - 2 * margin;
  const int tileW = availW / cols;
  const int tileH = availH / rows;
  const bool allowChart = perScreen <= 3;

  for (uint8_t i = 0; i < symOnPage; i++) {
    uint8_t col = i % cols;
    uint8_t row = i / cols;
    int x = margin + col * tileW;
    int y = y0 + row * tileH;
    if (row > 0) gfx->drawFastHLine(margin, y, availW, C_DGRAY);
    drawCompactTile(gfx, stockAt(symStart + i), x, y, tileW, tileH, s, allowChart);
  }
}

// ---- one ticker page ------------------------------------------------------
static void drawStock(const StockData& d, uint8_t pageIndex, uint8_t pageCount,
                      const Settings& s) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);

  // No data yet for this symbol.
  if (!d.valid) {
    gfxDrawCentered(d.symbol[0] ? d.symbol : "----", 80, 3, C_WHITE);
    gfxDrawCentered(d.error ? "fetch error" : "loading...", 120, 2, C_GRAY);
    if (s.ticker.showPageDots) drawPageDots(gfx, pageIndex, pageCount, 6);
    return;
  }

  // Trend color — from the displayed change (chart-range or 1-day basis)
  float chg = 0, pct = 0;
  bool onRange = false;
  bool hasChange = stockDisplayChange(d, s.ticker, chg, pct, &onRange);
  bool up = hasChange ? (chg >= 0) : true;
  uint16_t upC   = s.ticker.colorInverted ? C_RED : C_GREEN;
  uint16_t downC = s.ticker.colorInverted ? C_GREEN : C_RED;
  uint16_t trendC = !hasChange ? C_WHITE : (up ? upC : downC);

  int y = 6;

  // Page dots (top) — a single ticker still gets its one dot
  if (s.ticker.showPageDots) {
    drawPageDots(gfx, pageIndex, pageCount, 6);
    y += 20;                       // extra breathing room below the dots
  }

  // Name / symbol
  if (s.ticker.showName) {
    const char* label = d.name[0] ? d.name : d.symbol;
    gfxDrawCentered(label, y, gfxFitSize(label, 232, 3), C_WHITE);
    y += 28;
  }

  // Price (big, auto-fit)
  if (s.ticker.showPrice) {
    char num[20];
    fmtPrice(d.price, num, sizeof(num));
    char line[28];
    snprintf(line, sizeof(line), "%s%s", d.currency, num);
    uint8_t sz = gfxFitSize(line, 236, 6);
    int ph = 8 * sz;
    int py = s.ticker.showName ? 74 : 64;
    gfxDrawCentered(line, py, sz, C_WHITE);   // price stays neutral (not trend-colored)
    y = py + ph + 8;
  }

  // Change line: [arrow] +chg (+pct%)
  if (s.ticker.showChange && hasChange) {
    char line[40];
    if (pct != 0 || chg != 0)
      snprintf(line, sizeof(line), "%+.2f (%+.2f%%)", chg, pct);
    else
      snprintf(line, sizeof(line), "%+.2f", chg);
    uint8_t sz = gfxFitSize(line, 210, 2);
    int tw = gfxTextW(line, sz);
    int ah = 8 * sz;             // arrow box height
    int aw = ah;
    int totalW = aw + 4 + tw;
    int x = (TFT_WIDTH - totalW) / 2;
    if (x < 2) x = 2;
    // arrow triangle
    int ax = x, ay = y;
    if (up)
      gfx->fillTriangle(ax, ay + ah, ax + aw, ay + ah, ax + aw / 2, ay, trendC);
    else
      gfx->fillTriangle(ax, ay, ax + aw, ay, ax + aw / 2, ay + ah, trendC);
    gfx->setTextSize(sz);
    gfx->setTextColor(trendC);
    gfx->setCursor(x + aw + 4, ay);
    gfx->print(line);
    y = ay + ah + 8;
  }

  // Position P/L vs the cost basis (symbols with qty and cost configured)
  if (s.ticker.showPortfolio && d.qty > 0 && d.cost > 0) {
    float plPct = (d.price / d.cost - 1.0f) * 100.0f;
    char pl[40];
    snprintf(pl, sizeof(pl), "P/L %+.0f (%+.1f%%)", (d.price - d.cost) * d.qty, plPct);
    uint8_t sz = gfxFitSize(pl, 220, 2);
    gfxDrawCentered(pl, y, sz, plPct >= 0 ? upC : downC);
    y += 8 * sz + 6;
  }

  // Chart (with the live price as its newest point when the displayed change
  // is actually on the range basis, so the drawn end matches the number)
  if (s.ticker.showChart && d.sparkCount >= 2) {
    int top = y < 150 ? 156 : y + 4;
    int bottom = 228;
    float livePt = onRange ? d.price : NAN;
    if (top < bottom - 10) drawSparkline(gfx, d, top, bottom, trendC, livePt);
  }

  // Range label (top-right; the very bottom row is overscanned on this panel)
  if (s.ticker.showRangeLabel && d.rangeLabel[0]) {
    int sz = 2;
    int tw = gfxTextW(d.rangeLabel, sz);
    gfx->setTextSize(sz);
    gfx->setTextColor(C_GRAY);
    gfx->setCursor(TFT_WIDTH - tw - 4, 4);
    gfx->print(d.rangeLabel);
  }

  // Updated-ago (bottom-left)
  if (s.ticker.showUpdatedAgo && d.lastOkMs) {
    uint32_t ago = (millis() - d.lastOkMs) / 1000;
    char buf[12];
    if (ago < 100) snprintf(buf, sizeof(buf), "%lus", (unsigned long)ago);
    else           snprintf(buf, sizeof(buf), "%lum", (unsigned long)(ago / 60));
    gfx->setTextSize(2);
    gfx->setTextColor(d.error ? C_RED : C_DGRAY);
    gfx->setCursor(4, 224);
    gfx->print(buf);
  }

  // Stale/error dot (top-left) when last refresh failed but we have old data.
  // (Top-right now holds the range label.)
  if (d.error) gfx->fillCircle(6, 6, 3, C_RED);
}

// ---- portfolio page --------------------------------------------------------
// Compact value: the 240 px row has ~6 characters for it at text size 2.
static void fmtVal(float v, char* out, size_t n) {
  float a = fabsf(v);
  if      (a >= 1000000) snprintf(out, n, "%.2fM", v / 1000000);
  else if (a >= 10000)   snprintf(out, n, "%.1fk", v / 1000);
  else if (a >= 100)     snprintf(out, n, "%.0f", v);
  else                   snprintf(out, n, "%.2f", v);
}

// The summary page exists once any symbol carries a position.
static bool hasPortfolioPage(const Settings& s) {
  if (!s.ticker.showPortfolio) return false;
  for (uint8_t i = 0; i < stocksCount(); i++)
    if (stockAt(i).qty > 0) return true;
  return false;
}

// One row per position (name / P/L% / value), then a total per currency.
static void drawPortfolio(uint8_t pageIndex, uint8_t pageCount, const Settings& s) {
  Arduino_GFX* gfx = gfxDev();
  if (!gfx) return;
  gfx->fillScreen(C_BLACK);

  int y = 6;
  if (s.ticker.showPageDots) {
    drawPageDots(gfx, pageIndex, pageCount, 6);
    y += 20;
  }

  gfxDrawCentered("Portfolio", y, 3, C_WHITE);
  y += 32;

  uint16_t upC   = s.ticker.colorInverted ? C_RED : C_GREEN;
  uint16_t downC = s.ticker.colorInverted ? C_GREEN : C_RED;

  // Totals bucketed by currency prefix (usually just one bucket).
  float totV[3] = {0, 0, 0}, totC[3] = {0, 0, 0};
  char  totCur[3][6];
  uint8_t curN = 0;

  for (uint8_t i = 0; i < stocksCount(); i++) {
    const StockData& d = stockAt(i);
    if (d.qty <= 0) continue;
    if (y > 186) break;                       // keep room for the totals

    char nm[10];
    strlcpy(nm, d.name[0] ? d.name : d.symbol, sizeof(nm));
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(4, y);
    gfx->print(nm);

    if (d.valid) {
      float val = d.qty * d.price;
      char vbuf[12];
      fmtVal(val, vbuf, sizeof(vbuf));
      gfx->setCursor(TFT_WIDTH - gfxTextW(vbuf, 2) - 4, y);
      gfx->print(vbuf);
      if (d.cost > 0) {
        float plPct = (d.price / d.cost - 1.0f) * 100.0f;
        char pbuf[10];
        snprintf(pbuf, sizeof(pbuf), "%+.1f%%", plPct);
        gfx->setTextColor(plPct >= 0 ? upC : downC);
        gfx->setCursor(116, y);
        gfx->print(pbuf);
      }
      uint8_t b = 0xFF;
      for (uint8_t k = 0; k < curN; k++)
        if (!strcmp(totCur[k], d.currency)) { b = k; break; }
      if (b == 0xFF && curN < 3) {
        b = curN++;
        strlcpy(totCur[b], d.currency, sizeof(totCur[b]));
      }
      if (b != 0xFF) {
        totV[b] += val;
        if (d.cost > 0) totC[b] += d.qty * d.cost;
      }
    } else {
      const char* st = d.error ? "err" : "...";
      gfx->setTextColor(d.error ? C_RED : C_GRAY);
      gfx->setCursor(TFT_WIDTH - gfxTextW(st, 2) - 4, y);
      gfx->print(st);
    }
    y += 18;
  }

  y += 2;
  gfx->drawFastHLine(4, y, TFT_WIDTH - 8, C_DGRAY);
  y += 6;
  for (uint8_t k = 0; k < curN && y <= 214; k++) {
    char vbuf[12];
    fmtVal(totV[k], vbuf, sizeof(vbuf));
    char line[20];
    snprintf(line, sizeof(line), "%s%s", totCur[k], vbuf);
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(4, y);
    gfx->print("Total");
    gfx->setCursor(TFT_WIDTH - gfxTextW(line, 2) - 4, y);
    gfx->print(line);
    if (totC[k] > 0) {
      float plPct = (totV[k] / totC[k] - 1.0f) * 100.0f;
      char pbuf[10];
      snprintf(pbuf, sizeof(pbuf), "%+.1f%%", plPct);
      gfx->setTextColor(plPct >= 0 ? upC : downC);
      gfx->setCursor(78, y);
      gfx->print(pbuf);
    }
    y += 18;
  }
}

// ---- DisplayMode ----------------------------------------------------------
void TickerMode::begin(const Settings& s) {
  stocksInit(s);
  curPage_ = 0;
  lastRotate_ = millis();
  renderedLastOk_ = 0xFFFFFFFF;
  renderedError_ = false;
  needRender_ = true;
}

void TickerMode::invalidate(const Settings& s) {
  stocksInit(s);
  stocksForceRefresh();
  curPage_ = 0;
  renderedLastOk_ = 0xFFFFFFFF;
  needRender_ = true;
}

void TickerMode::render(const Settings& s) {
  uint8_t n = stocksCount();
  if (n == 0) {
    gfxMessage("No tickers", netIP().c_str(), C_YELLOW);
    return;
  }
  // Only complain about a missing webhook URL if every ticker depends on it.
  bool allWebhook = true;
  for (uint8_t i = 0; i < n; i++)
    if (stockAt(i).source != SRC_WEBHOOK) { allWebhook = false; break; }
  if (allWebhook && s.ticker.webhookUrl.length() < 8) {
    gfxMessage("Set webhook", netIP().c_str(), C_YELLOW);
    return;
  }

  uint8_t perScreen = tilesPerScreen(s);
  uint8_t symPages = symbolPages(n, perScreen);
  uint8_t pages = symPages + (hasPortfolioPage(s) ? 1 : 0);
  if (pages == 0) return;
  if (curPage_ >= pages) curPage_ = 0;

  if (curPage_ >= symPages) {
    drawPortfolio(curPage_, pages, s);
    return;
  }

  uint8_t symStart = curPage_ * perScreen;
  uint8_t symOnPage = n - symStart;
  if (symOnPage > perScreen) symOnPage = perScreen;

  if (perScreen == 1)
    drawStock(stockAt(symStart), curPage_, pages, s);
  else
    drawMultiStock(curPage_, pages, symStart, symOnPage, s);
}

void TickerMode::service(const Settings& s) {
  stocksService(s);

  uint8_t n = stocksCount();
  uint8_t perScreen = tilesPerScreen(s);
  uint8_t symPages = symbolPages(n, perScreen);
  uint8_t pages = symPages + (hasPortfolioPage(s) ? 1 : 0);

  // Rotate only when there is more than one page (extra symbols or portfolio).
  if (pages > 1 && millis() - lastRotate_ >= (uint32_t)s.ticker.rotateSec * 1000UL) {
    curPage_ = (curPage_ + 1) % pages;
    lastRotate_ = millis();
    needRender_ = true;
  }

  // Re-render when any symbol on the current view changed.
  if (n > 0 && curPage_ < symPages) {
    uint8_t symStart = curPage_ * perScreen;
    uint8_t symOnPage = n - symStart;
    if (symOnPage > perScreen) symOnPage = perScreen;

    uint32_t h = 0;
    bool err = false;
    for (uint8_t i = 0; i < symOnPage; i++) {
      const StockData& d = stockAt(symStart + i);
      h += d.lastOkMs;
      err |= d.error;
    }
    if (h != renderedLastOk_ || err != renderedError_) {
      needRender_ = true;
      renderedLastOk_ = h;
      renderedError_ = err;
    }
  } else if (n > 0 && pages > symPages) {
    uint32_t h = 0;
    bool err = false;
    for (uint8_t i = 0; i < n; i++) {
      h += stockAt(i).lastOkMs;
      err |= stockAt(i).error;
    }
    if (h != renderedLastOk_ || err != renderedError_) {
      needRender_ = true;
      renderedLastOk_ = h;
      renderedError_ = err;
    }
  }

  if (needRender_) {
    render(s);
    needRender_ = false;
  }
}
