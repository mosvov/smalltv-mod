#include "UsageClient.h"
#include "Platform.h"
#include <ArduinoJson.h>
#include <math.h>

static UsageData g_usage;
static uint32_t  g_nextPollMs = 0;
static bool      g_inited = false;

// ---------------------------------------------------------------------------
void usageInit(const Settings& s) {
  (void)s;
  g_usage.clear();
  g_nextPollMs = millis();
  g_inited = true;
}

void usageForceRefresh() { g_nextPollMs = millis(); }

const UsageData& usageGet() { return g_usage; }

bool usageFresh(uint32_t withinMs) {
  return g_usage.valid && (millis() - g_usage.lastOkMs) <= withinMs;
}

static void usageFilter(JsonDocument& f) {
  f["v"] = true;
  f["ok"] = true;
  JsonObject cl = f["claude"].to<JsonObject>();
  cl["ok"] = true;
  cl["s"] = true;
  cl["sr"] = true;
  cl["w"] = true;
  cl["wr"] = true;
  cl["st"] = true;
  cl["pct"] = true;
  cl["line"] = true;
  cl["sub"] = true;
  cl["tokens"] = true;
  cl["cache_read"] = true;
  cl["mode"] = true;
  JsonObject cu = f["cursor"].to<JsonObject>();
  cu["ok"] = true;
  cu["used"] = true;
  cu["limit"] = true;
  cu["pct"] = true;
  JsonObject co = f["codex"].to<JsonObject>();
  co["ok"] = true;
  co["pct"] = true;
  co["used"] = true;
  co["limit"] = true;
  co["unit"] = true;
  co["label"] = true;
  co["reset_label"] = true;
  co["remaining_pct"] = true;
  f["s"] = true;
  f["sr"] = true;
  f["w"] = true;
  f["wr"] = true;
  f["st"] = true;
}

static void formatUsd(float v, char* out, size_t n) {
  if (v >= 1000.0f) snprintf(out, n, "$%.0fk", v / 1000.0f);
  else snprintf(out, n, "$%.0f", v);
}

static void applyProviderMeter(ProviderMeter& m, JsonObjectConst o) {
  m.clear();
  if (o.isNull() || !o["ok"].is<bool>() || !o["ok"].as<bool>()) return;
  m.ok = true;
  m.pct = constrain(o["pct"] | 0.0f, 0.0f, 100.0f);

  if (o["line"].is<const char*>()) {
    strlcpy(m.line, o["line"].as<const char*>(), sizeof(m.line));
    if (o["sub"].is<const char*>())
      strlcpy(m.sub, o["sub"].as<const char*>(), sizeof(m.sub));
    return;
  }

  if (!o["s"].isNull()) {
    m.pct = constrain(o["s"].as<float>(), 0.0f, 100.0f);
    snprintf(m.line, sizeof(m.line), "%.0f%%", m.pct);
    strlcpy(m.sub, "5h", sizeof(m.sub));
    return;
  }

  const char* unit = o["unit"] | "";
  if (strcmp(unit, "credits") == 0 && !o["used"].isNull() && !o["limit"].isNull()) {
    int used = (int)(o["used"] | 0.0f);
    int limit = (int)(o["limit"] | 0.0f);
    m.pct = constrain(o["pct"] | 0.0f, 0.0f, 100.0f);
    if (limit >= 1000) snprintf(m.line, sizeof(m.line), "%d/%dk", used, limit / 1000);
    else snprintf(m.line, sizeof(m.line), "%d/%d", used, limit);
    const char* reset = o["reset_label"] | "";
    if (reset[0]) snprintf(m.sub, sizeof(m.sub), "r %s", reset);
    else strlcpy(m.sub, "credits", sizeof(m.sub));
    return;
  }

  if (strcmp(unit, "usd") == 0 && !o["used"].isNull() && !o["limit"].isNull()) {
    float used = o["used"] | 0.0f;
    float limit = o["limit"] | 0.0f;
    m.pct = constrain(o["pct"] | 0.0f, 0.0f, 100.0f);
    char u[12], l[12];
    formatUsd(used, u, sizeof(u));
    formatUsd(limit, l, sizeof(l));
    snprintf(m.line, sizeof(m.line), "%s/%s", u, l);
    strlcpy(m.sub, "spend", sizeof(m.sub));
    return;
  }

  if (!o["used"].isNull() && !o["limit"].isNull()) {
    int used = o["used"] | 0;
    int limit = o["limit"] | 0;
    if (limit > 0) m.pct = constrain((float)used / (float)limit * 100.0f, 0.0f, 100.0f);
    snprintf(m.line, sizeof(m.line), "%d/%d", used, limit);
    strlcpy(m.sub, "req", sizeof(m.sub));
    return;
  }

  snprintf(m.line, sizeof(m.line), "%.0f%%", m.pct);
  const char* label = o["label"] | "5h";
  strlcpy(m.sub, label, sizeof(m.sub));
}

static bool applyUsageDoc(UsageData& d, JsonObjectConst root) {
  if (root["ok"].is<bool>() && root["ok"].as<bool>() == false && root["v"].isNull())
    return false;

  d.claude.clear();
  d.cursor.clear();
  d.codex.clear();

  bool any = false;

  if (root["v"].is<int>() && root["v"].as<int>() >= 2) {
    applyProviderMeter(d.claude, root["claude"].as<JsonObjectConst>());
    applyProviderMeter(d.cursor, root["cursor"].as<JsonObjectConst>());
    applyProviderMeter(d.codex, root["codex"].as<JsonObjectConst>());
    any = d.claude.ok || d.cursor.ok || d.codex.ok;
  }

  // v1 top-level Claude fields
  if (!any && !root["s"].isNull()) {
    JsonObjectConst cl = root;
    applyProviderMeter(d.claude, cl);
    any = d.claude.ok;
  }

  if (!any) return false;

  // Legacy fields for mascot burn-rate
  if (d.claude.ok) {
    d.sessionPct = d.claude.pct;
    d.weeklyPct = root["w"] | d.claude.pct;
    d.sessionResetMin = root["sr"] | 0;
    d.weeklyResetMin = root["wr"] | 0;
    strlcpy(d.status, root["st"] | "allowed", sizeof(d.status));
  } else {
    d.sessionPct = 0;
    d.weeklyPct = 0;
    d.sessionResetMin = d.weeklyResetMin = 0;
    d.status[0] = 0;
  }

  d.valid = true;
  d.error = false;
  d.lastOkMs = millis();
  return true;
}

static bool parseUsage(UsageData& d, Stream& stream) {
  JsonDocument filter;
  usageFilter(filter);
  JsonDocument doc;
  if (deserializeJson(doc, stream, DeserializationOption::Filter(filter))) return false;
  return applyUsageDoc(d, doc.as<JsonObjectConst>());
}

bool usageApply(const String& body) {
  JsonDocument filter;
  usageFilter(filter);
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  return applyUsageDoc(g_usage, doc.as<JsonObjectConst>());
}

static bool fetchUsage(const Settings& s) {
  const String& url = s.usage.usageUrl;
  if (url.length() < 8) return false;
  bool https = url.startsWith("https://");

  std::unique_ptr<NetClient> client;
  if (https) {
    if (ESP.getFreeHeap() < 20000) return false;
    client.reset(platformMakeSecureClient(2048));
  } else {
    client.reset(new WiFiClient());
  }

  HTTPClient http;
  http.setTimeout(s.httpTimeout);
  http.setReuse(false);
  if (!http.begin(*client, url)) return false;
  http.addHeader("Accept", "application/json");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  bool ok = parseUsage(g_usage, http.getStream());
  http.end();
  return ok;
}

void usageService(const Settings& s) {
  if (!g_inited) usageInit(s);
  if ((int32_t)(millis() - g_nextPollMs) < 0) return;

  if (!fetchUsage(s)) g_usage.error = true;

  g_nextPollMs = millis() + (uint32_t)s.usage.pollSec * 1000UL;
}
