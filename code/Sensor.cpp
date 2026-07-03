// ============================================================================
//  Sensor.cpp  -  Sensirion I2C SCD30
// ============================================================================
#include "Sensor.h"
#include "AppTime.h"
#include "Sync.h"
#include <SensirionI2cScd30.h>

namespace {
  SensirionI2cScd30 s_scd30;
  bool s_ok = false;

  void logErr(const char* what, int16_t err) {
    char msg[64];
    errorToString(err, msg, sizeof(msg));
    Serial.printf("[Sensor] %s: %s\n", what, msg);
  }
}

bool sensorBegin() {
  // begin() speichert nur Wire-Referenz + Adresse (kein Buszugriff).
  s_scd30.begin(Wire, SCD30_I2C_ADDR_61);

  // Mess-Takt aus der Konfig (config.json: "scd30_interval_s"), auf 2..1800 s begrenzt
  uint16_t interval = g_cfg.scd30IntervalS;
  if (interval < 2)    interval = 2;
  if (interval > 1800) interval = 1800;

  I2cGuard g;   // ab hier echte I2C-Kommandos
  s_scd30.stopPeriodicMeasurement();          // evtl. laufende Messung stoppen
  s_scd30.softReset();
  delay(2000);                                 // SCD30 braucht ~2 s nach Reset

  int16_t err = s_scd30.setMeasurementInterval(interval);
  if (err) { logErr("setMeasurementInterval", err); }

  err = s_scd30.startPeriodicMeasurement(0);   // 0 = keine Druckkompensation
  if (err) { logErr("startPeriodicMeasurement", err); s_ok = false; return false; }

  Serial.printf("[Sensor] SCD30 ok, Intervall %us\n", interval);
  s_ok = true;
  return true;
}

bool sensorRead(Sample& out) {
  if (!s_ok) return false;
  {
    I2cGuard g;
    uint16_t ready = 0;
    if (s_scd30.getDataReady(ready) != 0 || ready == 0) return false;
    if (s_scd30.readMeasurementData(out.co2, out.temp, out.hum) != 0) return false;
  }
  out.epoch = timeNowEpoch();   // liest RTC (eigener I2C-Guard darin)
  out.heap  = ESP.getFreeHeap();
  out.valid = (out.co2 > 0.0f && out.co2 < 40000.0f);
  return out.valid;
}

bool sensorForceCalibration(uint16_t ppm) {
  if (!s_ok) return false;
  I2cGuard g;
  int16_t err = s_scd30.forceRecalibration(ppm);
  if (err) { logErr("forceRecalibration", err); return false; }
  Serial.printf("[Sensor] Forced Recalibration auf %u ppm gesetzt.\n", ppm);
  return true;
}
