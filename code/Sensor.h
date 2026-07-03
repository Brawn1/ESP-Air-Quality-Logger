// ============================================================================
//  Sensor.h  -  SCD30 (CO2 / Temperatur / Feuchte) ueber I2C
//  Der I2C-Bus wird in code.ino (Wire.begin) initialisiert und geteilt.
// ============================================================================
#pragma once
#include "config.h"

bool sensorBegin();                  // true, wenn SCD30 gefunden
bool sensorRead(Sample& out);        // true, wenn ein neuer Wert vorliegt

// Forced Recalibration: teilt dem SCD30 mit, dass der aktuelle Messwert
// 'ppm' entspricht (Sensor muss vorher ~2 min stabil in dieser Umgebung laufen).
bool sensorForceCalibration(uint16_t ppm);
