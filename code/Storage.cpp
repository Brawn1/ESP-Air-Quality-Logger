// ============================================================================
//  Storage.cpp
// ============================================================================
#include "Storage.h"
#include "Sync.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <time.h>

namespace {
  bool s_ok = false;

  // Tages-Dateiname /logs/YYYY-MM-DD.csv aus UTC-Epoch (Lokalzeit fuer Datum)
  void dayFile(uint32_t epoch, char* buf, size_t len) {
    time_t t = (time_t)epoch;
    struct tm lt; localtime_r(&t, &lt);
    strftime(buf, len, "/logs/%Y-%m-%d.csv", &lt);
  }
}

bool storageBegin() {
  // Der gemeinsame SPI-Bus (g_spi) wird bereits in setup() gestartet.
  SpiGuard g;
  // Takt aus config.h (SD_SPI_HZ). Niedriger = robuster.
  s_ok = SD.begin(PIN_SD_CS, g_spi, SD_SPI_HZ);
  if (!s_ok) { Serial.println(F("[SD] Karte nicht gefunden/init fehlgeschlagen!")); return false; }

  uint8_t type = SD.cardType();
  if (type == CARD_NONE) { Serial.println(F("[SD] Keine Karte.")); s_ok = false; return false; }
  Serial.printf("[SD] ok, %lluMB\n", SD.cardSize() / (1024ULL * 1024ULL));

  if (!SD.exists("/logs")) SD.mkdir("/logs");
  return true;
}

bool storageOk() { return s_ok; }

bool storageCheck() {
  static uint8_t failCount = 0;
  const uint8_t FAILS_TO_REMOVE = 3;   // erst nach 3 Fehlern in Folge -> "entfernt"

  SpiGuard g;
  if (s_ok) {
    // Probe: Wurzelverzeichnis oeffnen. Ein einzelner Fehlschlag kann ein
    // transienter SPI-Glitch sein -> erst nach mehreren in Folge werten.
    File root = SD.open("/");
    if (root) {
      root.close();
      failCount = 0;
    } else if (++failCount >= FAILS_TO_REMOVE) {
      Serial.println(F("[SD] Karte entfernt/nicht mehr lesbar!"));
      s_ok = false;
      failCount = 0;
      SD.end();
    }
  } else {
    // Versuchen, eine (wieder) eingesteckte Karte zu mounten.
    if (SD.begin(PIN_SD_CS, g_spi, SD_SPI_HZ) && SD.cardType() != CARD_NONE) {
      Serial.println(F("[SD] Karte wieder erkannt."));
      if (!SD.exists("/logs")) SD.mkdir("/logs");
      s_ok = true;
    } else {
      SD.end();   // sauber schliessen, damit der naechste begin() frisch startet
    }
  }
  return s_ok;
}

void storageLoadConfig() {
  if (!s_ok) return;
  SpiGuard g;
  if (!SD.exists("/config.json")) {
    Serial.println(F("[SD] /config.json fehlt - benutze Standardwerte."));
    return;
  }
  File f = SD.open("/config.json", FILE_READ);
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { Serial.printf("[SD] config.json Fehler: %s\n", err.c_str()); return; }

  if (doc["wifi_ssid"].is<const char*>()) g_cfg.wifiSsid = (const char*)doc["wifi_ssid"];
  if (doc["wifi_pass"].is<const char*>()) g_cfg.wifiPass = (const char*)doc["wifi_pass"];
  if (doc["ap_ssid"].is<const char*>())   g_cfg.apSsid   = (const char*)doc["ap_ssid"];
  if (doc["ap_pass"].is<const char*>())    g_cfg.apPass   = (const char*)doc["ap_pass"];
  if (doc["hostname"].is<const char*>())   g_cfg.hostname = (const char*)doc["hostname"];
  if (doc["ntp_server"].is<const char*>()) g_cfg.ntpServer= (const char*)doc["ntp_server"];
  if (doc["tz"].is<const char*>())         g_cfg.tz       = (const char*)doc["tz"];
  if (doc["log_interval_s"].is<uint32_t>())     g_cfg.logIntervalS     = doc["log_interval_s"];
  if (doc["history_interval_s"].is<uint32_t>()) g_cfg.historyIntervalS = doc["history_interval_s"];
  if (doc["scd30_interval_s"].is<uint32_t>())   g_cfg.scd30IntervalS   = doc["scd30_interval_s"];
  if (doc["temp_offset_c"].is<float>())         g_cfg.tempOffsetC      = doc["temp_offset_c"];
  Serial.println(F("[SD] config.json geladen."));
}

bool storageLogSample(const Sample& s) {
  if (!s_ok || !s.valid) return false;
  char path[40]; dayFile(s.epoch, path, sizeof(path));

  SpiGuard g;
  bool isNew = !SD.exists(path);
  File f = SD.open(path, FILE_APPEND);
  if (!f) { Serial.println(F("[SD] Log-Datei konnte nicht geoeffnet werden.")); return false; }

  if (isNew) f.println(F("epoch_utc,datetime_local,co2_ppm,temp_c,hum_pct"));

  char dt[24];
  time_t t = (time_t)s.epoch; struct tm lt; localtime_r(&t, &lt);
  strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &lt);

  f.printf("%u,%s,%.0f,%.2f,%.1f\n", s.epoch, dt, s.co2, s.temp, s.hum);
  f.close();
  return true;
}

bool storageSaveHistory(const StatusSnapshot& snap) {
  if (!s_ok) return false;
  SpiGuard g;
  File f = SD.open("/history.json", FILE_WRITE);   // "w" -> Datei wird neu geschrieben
  if (!f) return false;
  f.print(F("{\"count\":"));
  f.print(snap.historyCount);
  f.print(F(",\"points\":["));
  for (int i = 0; i < snap.historyCount; i++) {
    if (i) f.print(',');
    f.printf("{\"t\":%u,\"co2\":%.0f,\"temp\":%.2f,\"hum\":%.1f,\"heap\":%u}",
             snap.history[i].epoch, snap.history[i].co2,
             snap.history[i].temp, snap.history[i].hum, snap.history[i].heap);
  }
  f.print(F("]}"));
  f.close();
  return true;
}

void storageLoadHistory() {
  if (!s_ok) return;

  Sample tmp[HISTORY_LEN];
  int n = 0;
  {
    SpiGuard g;   // nur SD-Zugriff unter der Sperre; Ringpuffer danach fuellen
    if (!SD.exists("/history.json")) return;
    File f = SD.open("/history.json", FILE_READ);
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial.printf("[SD] history.json Fehler: %s\n", err.c_str()); return; }

    for (JsonObject p : doc["points"].as<JsonArray>()) {
      if (n >= HISTORY_LEN) break;
      tmp[n].epoch = p["t"]    | 0u;
      tmp[n].co2   = p["co2"]  | 0.0f;
      tmp[n].temp  = p["temp"] | 0.0f;
      tmp[n].hum   = p["hum"]  | 0.0f;
      tmp[n].heap  = p["heap"] | 0u;
      tmp[n].valid = true;
      n++;
    }
  }
  for (int i = 0; i < n; i++) dataPushHistory(tmp[i]);   // Ringpuffer (eigener Mutex)
  Serial.printf("[SD] %d History-Punkte aus /history.json geladen.\n", n);
}

bool storageFileExists(const String& path) {
  if (!s_ok) return false;
  SpiGuard g;
  return SD.exists(path);
}

bool storageReadFile(const String& path, String& out) {
  if (!s_ok) return false;
  SpiGuard g;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) { if (f) f.close(); return false; }
  size_t n = f.size();
  out.reserve(n + 1);
  while (f.available()) out += (char)f.read();
  f.close();
  return true;
}
