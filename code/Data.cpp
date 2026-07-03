// ============================================================================
//  Data.cpp
// ============================================================================
#include "Data.h"

namespace {
  SemaphoreHandle_t s_mutex = nullptr;

  // geschuetzter Zustand
  Sample  s_latest = {0, 0, 0, 0, 0, false};
  Sample  s_hist[HISTORY_LEN];
  int     s_histCount = 0;
  int     s_histHead  = 0;          // Index des naechsten Schreibplatzes
  bool    s_wifi = false, s_ap = false, s_rtc = false, s_sd = false, s_synced = false;
  char    s_ip[16]   = "0.0.0.0";
  char    s_ssid[33] = "";
  char    s_msg[24]  = "";
  uint32_t s_msgUntil = 0;         // millis()-Zeitpunkt, bis zu dem s_msg gilt

  inline void lock()   { xSemaphoreTake(s_mutex, portMAX_DELAY); }
  inline void unlock() { xSemaphoreGive(s_mutex); }
}

void dataBegin() {
  s_mutex = xSemaphoreCreateMutex();
  for (int i = 0; i < HISTORY_LEN; i++) s_hist[i].valid = false;
}

void dataSetLatest(const Sample& s) {
  lock();  s_latest = s;  unlock();
}

void dataPushHistory(const Sample& s) {
  lock();
  s_hist[s_histHead] = s;
  s_histHead = (s_histHead + 1) % HISTORY_LEN;
  if (s_histCount < HISTORY_LEN) s_histCount++;
  unlock();
}

void dataSetWifi(bool connected, bool apActive, const char* ip, const char* ssid) {
  lock();
  s_wifi = connected; s_ap = apActive;
  strncpy(s_ip,   ip,   sizeof(s_ip) - 1);   s_ip[sizeof(s_ip) - 1]   = 0;
  strncpy(s_ssid, ssid, sizeof(s_ssid) - 1); s_ssid[sizeof(s_ssid) - 1] = 0;
  unlock();
}

void dataSetRtc(bool ok, bool synced) {
  lock();  s_rtc = ok; s_synced = synced;  unlock();
}

void dataSetSd(bool ok) {
  lock();  s_sd = ok;  unlock();
}

void dataSetMessage(const char* msg, uint32_t durationMs) {
  lock();
  strncpy(s_msg, msg, sizeof(s_msg) - 1);  s_msg[sizeof(s_msg) - 1] = 0;
  s_msgUntil = millis() + durationMs;
  unlock();
}

void dataSnapshot(StatusSnapshot& out) {
  lock();
  out.latest       = s_latest;
  out.historyCount = s_histCount;
  // Verlauf in chronologischer Reihenfolge (aeltester zuerst) auslesen.
  for (int i = 0; i < s_histCount; i++) {
    int idx = (s_histHead - s_histCount + i + HISTORY_LEN * 2) % HISTORY_LEN;
    out.history[i] = s_hist[idx];
  }
  for (int i = s_histCount; i < HISTORY_LEN; i++) out.history[i].valid = false;
  out.wifiConnected = s_wifi;
  out.apActive      = s_ap;
  out.rtcOk         = s_rtc;
  out.sdOk          = s_sd;
  out.timeSynced    = s_synced;
  strncpy(out.ip,   s_ip,   sizeof(out.ip));
  strncpy(out.ssid, s_ssid, sizeof(out.ssid));
  out.messageActive = (s_msgUntil != 0 && (int32_t)(millis() - s_msgUntil) < 0);
  strncpy(out.message, s_msg, sizeof(out.message));
  unlock();
}
