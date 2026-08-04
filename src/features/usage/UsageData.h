// UsageData.h — runtime AI usage snapshot from the daemon (Claude/Cursor/Codex).
#pragma once
#include <Arduino.h>

struct ProviderMeter {
  bool  ok;
  float pct;              // 0..100 for bar width
  char  line[20];         // e.g. "78/1000", "$563/$5k", "29%"
  char  sub[12];          // e.g. "5h", "req", "spend"

  void clear() {
    ok = false;
    pct = 0;
    line[0] = 0;
    sub[0] = 0;
  }
};

struct UsageData {
  ProviderMeter claude;
  ProviderMeter cursor;
  ProviderMeter codex;

  // Legacy Claude fields (v1 contract + mascot burn-rate)
  float    sessionPct;
  int      sessionResetMin;
  float    weeklyPct;
  int      weeklyResetMin;
  char     status[16];

  bool     valid;
  bool     error;
  uint32_t lastOkMs;

  void clear() {
    claude.clear();
    cursor.clear();
    codex.clear();
    sessionPct = weeklyPct = 0;
    sessionResetMin = weeklyResetMin = 0;
    status[0] = 0;
    valid = false;
    error = false;
    lastOkMs = 0;
  }
};
