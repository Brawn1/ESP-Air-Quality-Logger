// ============================================================================
//  WebNet.cpp  -  WiFi + ESPAsyncWebServer
// ============================================================================
#include "WebNet.h"
#include "Data.h"
#include "Storage.h"
#include "AppTime.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>

namespace {
  AsyncWebServer s_server(80);
  bool s_apMode = false;

  // MIME-Typ anhand der Dateiendung
  String contentType(const String& path) {
    if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".ico"))  return "image/x-icon";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    return "text/plain";
  }

  // Statusflags ins Data-Modul spiegeln
  void publishNetState() {
    if (s_apMode) {
      dataSetWifi(false, true, WiFi.softAPIP().toString().c_str(), g_cfg.apSsid.c_str());
    } else {
      dataSetWifi(WiFi.status() == WL_CONNECTED, false,
                  WiFi.localIP().toString().c_str(), g_cfg.wifiSsid.c_str());
    }
  }

  // ---- API-Handler ---------------------------------------------------------
  void handleCurrent(AsyncWebServerRequest* req) {
    StatusSnapshot s; dataSnapshot(s);
    char buf[384];
    snprintf(buf, sizeof(buf),
      "{\"time\":%u,\"co2\":%.0f,\"temp\":%.2f,\"hum\":%.1f,\"valid\":%s,"
      "\"wifi\":%s,\"ap\":%s,\"ip\":\"%s\",\"ssid\":\"%s\","
      "\"rtc\":%s,\"sd\":%s,\"synced\":%s,"
      "\"heap\":%u,\"heapmin\":%u,\"uptime\":%u}",
      s.latest.epoch, s.latest.co2, s.latest.temp, s.latest.hum,
      s.latest.valid ? "true" : "false",
      s.wifiConnected ? "true" : "false", s.apActive ? "true" : "false",
      s.ip, s.ssid,
      s.rtcOk ? "true" : "false", s.sdOk ? "true" : "false",
      s.timeSynced ? "true" : "false",
      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
      (unsigned)(millis() / 1000));
    req->send(200, "application/json", buf);
  }

  void handleHistory(AsyncWebServerRequest* req) {
    StatusSnapshot s; dataSnapshot(s);
    String out; out.reserve(64 + s.historyCount * 72);
    out += "{\"count\":"; out += s.historyCount; out += ",\"points\":[";
    for (int i = 0; i < s.historyCount; i++) {
      char p[104];
      snprintf(p, sizeof(p), "%s{\"t\":%u,\"co2\":%.0f,\"temp\":%.2f,\"hum\":%.1f,\"heap\":%u}",
               i ? "," : "", s.history[i].epoch, s.history[i].co2,
               s.history[i].temp, s.history[i].hum, s.history[i].heap);
      out += p;
    }
    out += "]}";
    req->send(200, "application/json", out);
  }

  // ---- statische Dateien von der SD ---------------------------------------
  void handleStatic(AsyncWebServerRequest* req) {
    String path = req->url();
    if (path.endsWith("/")) path += "index.html";
    String full = "/www" + path;

    if (!storageFileExists(full)) {
      req->send(404, "text/plain", "404: " + path + " nicht auf SD gefunden");
      return;
    }
    String body;
    if (!storageReadFile(full, body)) {
      req->send(500, "text/plain", "Lesefehler SD");
      return;
    }
    // Statische Dateien im Browser cachen lassen -> nach dem ersten Laden
    // werden bei Reload/Reconnect nur noch die kleinen /api-Aufrufe geholt.
    AsyncWebServerResponse* resp = req->beginResponse(200, contentType(full), body);
    resp->addHeader("Cache-Control", "max-age=86400");   // 1 Tag
    req->send(resp);
  }
}

bool networkStaConnected() { return !s_apMode && WiFi.status() == WL_CONNECTED; }

void networkConnect() {
  WiFi.persistent(false);
  WiFi.setHostname(g_cfg.hostname.c_str());

  bool haveCreds = g_cfg.wifiSsid.length() > 0;
  if (haveCreds) {
    Serial.printf("[Net] Verbinde mit '%s' ...\n", g_cfg.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_cfg.wifiSsid.c_str(), g_cfg.wifiPass.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      vTaskDelay(pdMS_TO_TICKS(250));
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    s_apMode = false;
    Serial.printf("[Net] STA verbunden, IP %s\n", WiFi.localIP().toString().c_str());
    // Erreichbar als http://<hostname>.local
    if (MDNS.begin(g_cfg.hostname.c_str())) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[Net] mDNS: http://%s.local\n", g_cfg.hostname.c_str());
    }
  } else {
    // Fallback: eigenen Access Point aufspannen
    s_apMode = true;
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(g_cfg.apSsid.c_str(),
                          g_cfg.apPass.length() >= 8 ? g_cfg.apPass.c_str() : nullptr);
    Serial.printf("[Net] AP '%s' %s, IP %s\n", g_cfg.apSsid.c_str(),
                  ok ? "aktiv" : "FEHLER", WiFi.softAPIP().toString().c_str());
  }
  publishNetState();
}

void networkStartServer() {
  s_server.on("/api/current", HTTP_GET, handleCurrent);
  s_server.on("/api/history", HTTP_GET, handleHistory);
  // Alles Uebrige: statische Datei von der SD ausliefern.
  s_server.onNotFound(handleStatic);
  s_server.begin();
  Serial.println(F("[Net] Webserver gestartet (Port 80)."));
}

void networkLoop() {
  static bool wasConnected = false;

  if (!s_apMode) {
    bool c = (WiFi.status() == WL_CONNECTED);
    if (c != wasConnected) {         // Statuswechsel melden
      wasConnected = c;
      publishNetState();
      if (c) Serial.printf("[Net] wieder verbunden, IP %s\n",
                           WiFi.localIP().toString().c_str());
      else   Serial.println(F("[Net] STA-Verbindung verloren."));
    }
    if (!c) WiFi.reconnect();
  }

  // Diagnose: Heap alle ~30 s ausgeben. Faellt der Wert stetig -> Leck/
  // Fragmentierung; bleibt er stabil, liegt es eher an den Verbindungs-Slots.
  static uint32_t lastHeapMs = 0;
  if (millis() - lastHeapMs >= 30000) {
    lastHeapMs = millis();
    Serial.printf("[Net] Heap frei %u B (min seit Boot %u B)\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
  }
}
