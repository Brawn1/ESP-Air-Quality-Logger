// ============================================================================
//  AppTime.cpp  -  DS3231 (RTClib) + SNTP
//
//  Zeitmodell:  Die RTC und die interne System-Uhr laufen in UTC.
//  Beim Booten wird die System-Uhr aus der DS3231 vorgeladen, damit auch
//  offline die richtige Zeit verfuegbar ist. NTP korrigiert spaeter beides.
//  Zur Anzeige wird per Zeitzone (TZ) in Lokalzeit umgerechnet.
// ============================================================================
#include "AppTime.h"
#include "Sync.h"
#include <RTClib.h>
#include <time.h>
#include <sys/time.h>

namespace {
  RTC_DS3231 s_rtc;
  bool s_rtcOk = false;

  // System-Uhr aus einem UTC-Epoch stellen
  void setSystemClock(uint32_t epochUtc) {
    struct timeval tv = { (time_t)epochUtc, 0 };
    settimeofday(&tv, nullptr);
  }
}

bool timeBegin() {
  // Zeitzone setzen (fuer localtime_r / strftime)
  setenv("TZ", g_cfg.tz.c_str(), 1);
  tzset();

  {
    I2cGuard g;
    s_rtcOk = s_rtc.begin(&Wire);
  }
  if (!s_rtcOk) {
    Serial.println(F("[Time] DS3231 nicht gefunden!"));
    return false;
  }

  // System-Uhr aus RTC (UTC) vorladen
  uint32_t e;
  {
    I2cGuard g;
    e = s_rtc.now().unixtime();
  }
  if (e > 1600000000UL) {          // plausibel (>= 2020)
    setSystemClock(e);
    Serial.printf("[Time] System aus RTC vorgeladen: %u\n", e);
  } else {
    Serial.println(F("[Time] RTC-Zeit unplausibel - warte auf NTP."));
  }
  return true;
}

bool timeRtcPresent() { return s_rtcOk; }

uint32_t timeNowEpoch() {
  // Bevorzugt die System-Uhr (kein I2C noetig); faellt sonst auf RTC zurueck.
  time_t now = time(nullptr);
  if (now > 1600000000L) return (uint32_t)now;
  if (s_rtcOk) {
    I2cGuard g;
    return s_rtc.now().unixtime();
  }
  return (uint32_t)now;
}

void timeFormat(uint32_t epoch, char* buf, size_t len, bool withDate) {
  time_t t = (time_t)epoch;
  struct tm lt;
  localtime_r(&t, &lt);          // UTC-Epoch -> Lokalzeit gemaess TZ
  if (withDate) strftime(buf, len, "%d.%m.%Y %H:%M:%S", &lt);
  else          strftime(buf, len, "%H:%M:%S", &lt);
}

bool timeSyncNtp(uint32_t timeoutMs) {
  // SNTP starten (setzt die System-Uhr im Hintergrund, Zeit in UTC).
  configTzTime(g_cfg.tz.c_str(), g_cfg.ntpServer.c_str());

  const uint32_t start = millis();
  struct tm lt;
  // getLocalTime pollt bis eine plausible Zeit gesetzt wurde.
  while (millis() - start < timeoutMs) {
    if (getLocalTime(&lt, 200)) {
      time_t nowUtc = time(nullptr);
      if (nowUtc > 1600000000L) {
        // RTC (UTC) nachziehen
        if (s_rtcOk) {
          struct tm g;
          gmtime_r(&nowUtc, &g);
          I2cGuard guard;
          s_rtc.adjust(DateTime(g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
                                g.tm_hour, g.tm_min, g.tm_sec));
        }
        Serial.printf("[Time] NTP-Sync ok: %ld (UTC)\n", (long)nowUtc);
        return true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  Serial.println(F("[Time] NTP-Sync fehlgeschlagen (Timeout)."));
  return false;
}
