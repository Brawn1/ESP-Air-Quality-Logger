// ============================================================================
//  Led.h  -  Onboard-WS2812-RGB-LED als CO2-Ampel (eigener Pin, kein Mutex noetig)
// ============================================================================
#pragma once
#include "config.h"

void ledBegin();
void ledSetCo2(float co2, bool valid);
void ledConfirm();      // kurzes gruenes Blinken (Bestaetigung, z.B. Kalibrierung)
