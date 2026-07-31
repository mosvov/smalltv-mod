#include "WeatherClient.h"
#include "Platform.h"
#include <ArduinoJson.h>

static WeatherData g_data;
static uint32_t    g_nextPollMs = 0;
static uint8_t     g_fetchPhase = 0;   // 0 = current, 1 = forecast

// Probe MFLN once so BearSSL can use the smallest safe RX buffer (ESP8266 heap).
static uint16_t g_tlsRx = 0;

static void probeTls() {
#if defined(SMALLTV_ESP8266)
  if (g_tlsRx) return;
  if (BearSSL::WiFiClientSecure::probeMaxFragmentLength(OWM_HOST, 443, 512))       g_tlsRx = 512;
  else if (BearSSL::WiFiClientSecure::probeMaxFragmentLength(OWM_HOST, 443, 1024)) g_tlsRx = 1024;
  else                                                                             g_tlsRx = 4096;
#else
  if (!g_tlsRx) g_tlsRx = 2048;
#endif
}

const WeatherData& weatherData()    { return g_data; }
uint32_t           weatherLastOkMs(){ return g_data.lastOkMs; }
bool               weatherError()   { return g_data.error; }

void weatherInit(const Settings& s) {
  (void)s;
  g_data.clear();
  g_fetchPhase = 0;
  g_nextPollMs = millis();
  probeTls();
}

void weatherForceRefresh() { g_nextPollMs = millis(); g_fetchPhase = 0; }

static void setError(const char* msg) {
  g_data.error = true;
  g_data.valid = false;
  strlcpy(g_data.errorMsg, msg, sizeof(g_data.errorMsg));
}

static void appendUrlEncoded(String& u, const char* s) {
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (c == ' ') u += '%20';
    else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      u += (char)c;
    else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", c);
      u += hex;
    }
  }
}

static String buildCurrentUrl(const Settings& s) {
  String u = F("https://");
  u += F(OWM_HOST);
  u += F("/data/2.5/weather?q=");
  appendUrlEncoded(u, s.weather.city.c_str());
  u += F("&appid=");
  u += s.weather.apiKey;
  u += s.weather.unitsMetric ? F("&units=metric") : F("&units=imperial");
  return u;
}

static String buildForecastUrl(const Settings& s) {
  String u = F("https://");
  u += F(OWM_HOST);
  u += F("/data/2.5/forecast?q=");
  appendUrlEncoded(u, s.weather.city.c_str());
  u += F("&appid=");
  u += s.weather.apiKey;
  u += s.weather.unitsMetric ? F("&units=metric") : F("&units=imperial");
  u += F("&cnt=27");
  return u;
}

static bool mapHttpError(int code, JsonObjectConst root) {
  const char* msg = root["message"].is<const char*>() ? root["message"].as<const char*>() : "";
  if (code == HTTP_CODE_UNAUTHORIZED || code == 401) {
    setError("Bad API key");
    return true;
  }
  if (code == HTTP_CODE_NOT_FOUND || code == 404) {
    setError("Err city!");
    return true;
  }
  if (strstr(msg, "Invalid API") || strstr(msg, "API key"))
    setError("Bad API key");
  else if (strstr(msg, "city") || strstr(msg, "City"))
    setError("Err city!");
  else
    setError("API error");
  return true;
}

static bool fetchJsonBody(const Settings& s, const String& url, JsonDocument& doc,
                          JsonDocument* filterDoc) {
  bool https = url.startsWith("https://");
  std::unique_ptr<NetClient> client;
  if (https) {
    probeTls();
    const uint32_t needBlock = (uint32_t)g_tlsRx + 1024;
    if (ESP.getFreeHeap() < 18000 || platformMaxFreeBlock() < needBlock) {
      setError("Low heap");
      return false;
    }
    // OpenWeather needs modern ECDHE suites — not the static-RSA-only list.
    client.reset(platformMakeSecureClient(g_tlsRx, nullptr, 512, /*cheapCiphers=*/false));
  } else {
    client.reset(new WiFiClient());
  }

  HTTPClient http;
  http.setTimeout(s.httpTimeout);
  http.setReuse(false);
  http.useHTTP10(true);
  if (!http.begin(*client, url)) {
    setError("Connect fail");
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.setUserAgent(F(OWM_USER_AGENT));

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    JsonDocument errDoc;
    (void)deserializeJson(errDoc, http.getStream());
    http.end();
    mapHttpError(code, errDoc.as<JsonObjectConst>());
    return false;
  }

  DeserializationError err = filterDoc
      ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filterDoc))
      : deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    setError("Parse error");
    return false;
  }
  return true;
}

static void titleCase(char* s) {
  if (!s || !s[0]) return;
  bool cap = true;
  for (char* p = s; *p; p++) {
    if (*p == ' ') cap = true;
    else if (cap) { *p = (char)toupper((unsigned char)*p); cap = false; }
    else *p = (char)tolower((unsigned char)*p);
  }
}

static bool parseCurrent(JsonObjectConst root) {
  if (root["cod"].is<int>() && root["cod"].as<int>() != 200) return false;
  if (root["cod"].is<const char*>()) {
    String c = root["cod"].as<String>();
    if (c != "200") return false;
  }

  JsonObjectConst main = root["main"];
  if (main.isNull()) return false;

  const char* name = root["name"] | "";
  strlcpy(g_data.city, name, sizeof(g_data.city));

  g_data.temp      = main["temp"]      | 0.0f;
  g_data.feelsLike = main["feels_like"]| 0.0f;
  g_data.tempMin   = main["temp_min"]  | g_data.temp;
  g_data.tempMax   = main["temp_max"]  | g_data.temp;
  g_data.humidity  = (uint8_t)constrain((int)(main["humidity"] | 0), 0, 100);

  JsonObjectConst wind = root["wind"];
  g_data.wind = wind.isNull() ? 0.0f : (wind["speed"] | 0.0f);

  JsonArrayConst warr = root["weather"].as<JsonArrayConst>();
  if (!warr.isNull() && warr.size() > 0) {
    const char* desc = warr[0]["description"] | "";
    strlcpy(g_data.description, desc, sizeof(g_data.description));
    titleCase(g_data.description);
  } else {
    g_data.description[0] = 0;
  }

  g_data.valid = true;
  g_data.error = false;
  g_data.errorMsg[0] = 0;
  g_data.lastOkMs = millis();
  return true;
}

static const char* weekdayLabel(int wday) {
  static const char* d[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (wday < 0 || wday > 6) return "---";
  return d[wday];
}

static void parseForecast(JsonArrayConst list) {
  g_data.forecastCount = 0;
  if (list.isNull()) return;

  // Bucket by calendar day (YYYYMMDD) and track min/max per day.
  struct DayBucket {
    int   key;
    float hi;
    float lo;
    int   wday;
    bool  used;
  };
  DayBucket buckets[8];
  uint8_t bucketCount = 0;

  for (JsonObjectConst item : list) {
    const char* dtTxt = item["dt_txt"] | "";
    if (strlen(dtTxt) < 10) continue;
    int y, mo, d;
    if (sscanf(dtTxt, "%d-%d-%d", &y, &mo, &d) != 3) continue;
    int key = y * 10000 + mo * 100 + d;

    JsonObjectConst main = item["main"];
    if (main.isNull()) continue;
    float t = main["temp"] | 0.0f;
    float tmin = main["temp_min"] | t;
    float tmax = main["temp_max"] | t;

    int idx = -1;
    for (uint8_t i = 0; i < bucketCount; i++) {
      if (buckets[i].key == key) { idx = (int)i; break; }
    }
    if (idx < 0) {
      if (bucketCount >= 8) continue;
      idx = bucketCount++;
      buckets[idx].key = key;
      buckets[idx].hi = tmax;
      buckets[idx].lo = tmin;
      buckets[idx].used = true;
      time_t ts = item["dt"] | 0;
      struct tm tm;
      if (ts > 0) {
        localtime_r(&ts, &tm);
        buckets[idx].wday = tm.tm_wday;
      } else {
        buckets[idx].wday = 0;
      }
    } else {
      if (tmax > buckets[idx].hi) buckets[idx].hi = tmax;
      if (tmin < buckets[idx].lo) buckets[idx].lo = tmin;
    }
  }

  // Sort buckets by date key (simple insertion sort, n <= 8).
  for (uint8_t i = 1; i < bucketCount; i++) {
    DayBucket tmp = buckets[i];
    int j = (int)i - 1;
    while (j >= 0 && buckets[j].key > tmp.key) {
      buckets[j + 1] = buckets[j];
      j--;
    }
    buckets[j + 1] = tmp;
  }

  uint8_t out = 0;
  for (uint8_t i = 0; i < bucketCount && out < WEATHER_FORECAST_DAYS; i++) {
    strlcpy(g_data.forecast[out].label, weekdayLabel(buckets[i].wday), 4);
    g_data.forecast[out].hi = buckets[i].hi;
    g_data.forecast[out].lo = buckets[i].lo;
    out++;
  }
  g_data.forecastCount = out;
}

static bool fetchCurrent(const Settings& s) {
  if (s.weather.apiKey.length() < 8) {
    setError("No API key");
    return true;
  }
  if (s.weather.city.length() < 2) {
    setError("Err city!");
    return true;
  }

  JsonDocument doc;
  if (!fetchJsonBody(s, buildCurrentUrl(s), doc, nullptr)) {
    if (!g_data.error) setError("Fetch failed");
    return true;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  if (root["cod"].is<int>() && root["cod"].as<int>() != 200) {
    mapHttpError(root["cod"].as<int>(), root);
    return true;
  }
  if (root["message"].is<const char*>()) {
    mapHttpError(0, root);
    return true;
  }

  if (!parseCurrent(root)) {
    setError("Parse error");
    return true;
  }
  return true;
}

static bool fetchForecast(const Settings& s) {
  if (!s.weather.showForecast) return true;

  JsonDocument filter;
  JsonObject item = filter["list"][0].to<JsonObject>();
  item["dt"] = true;
  item["dt_txt"] = true;
  JsonObject main = item["main"].to<JsonObject>();
  main["temp"] = true;
  main["temp_min"] = true;
  main["temp_max"] = true;

  JsonDocument doc;
  if (!fetchJsonBody(s, buildForecastUrl(s), doc, &filter)) {
    // Forecast failure is non-fatal if current data is valid.
    return true;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  parseForecast(root["list"].as<JsonArrayConst>());
  return true;
}

void weatherService(const Settings& s) {
  if (millis() < g_nextPollMs) return;

  if (g_fetchPhase == 0) {
    if (fetchCurrent(s)) {
      g_fetchPhase = 1;
      g_nextPollMs = millis() + 500;   // brief gap before forecast request
    } else {
      g_nextPollMs = millis() + 5000;
    }
    return;
  }

  fetchForecast(s);
  g_fetchPhase = 0;
  uint16_t poll = s.weather.pollSec > 0 ? s.weather.pollSec : DEFAULT_WEATHER_POLL_SEC;
  g_nextPollMs = millis() + (uint32_t)poll * 1000UL;
}
