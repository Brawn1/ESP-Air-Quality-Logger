// ============================================================================
//  Led.cpp  -  WS2812B-Streifen als CO2-Balken (Level-Meter)
//
//  Anzahl leuchtender LEDs steigt mit dem CO2-Wert (CO2_BAR_MIN..CO2_BAR_MAX).
//  Jede LED-Position hat eine feste Farbzone: gruen (gute Luft) -> gelb -> rot.
//  Eigener Daten-Pin, kein Bus-Mutex noetig.
// ============================================================================
#include "Led.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

namespace {
  Adafruit_NeoPixel s_px(NUM_STATUS_LEDS,
                         PIN_STATUS_LED >= 0 ? PIN_STATUS_LED : 0,
                         NEO_GRB + NEO_KHZ800);
  bool s_ok = false;

  // Farbe fuer die ppm-Schwelle, die eine LED-Position repraesentiert.
  uint32_t zoneColor(uint16_t ppmAtLed) {
    if (ppmAtLed <= CO2_GOOD_MAX)   return s_px.Color(0, 160, 0);    // gruen (OK)
    if (ppmAtLed <= CO2_MEDIUM_MAX) return s_px.Color(210, 70, 0);   // orange
    return s_px.Color(190, 0, 0);                                     // rot
  }
}

void ledBegin() {
  if (PIN_STATUS_LED < 0) return;
  s_px.begin();
  s_px.setBrightness(LED_BRIGHTNESS);
  s_px.clear();
  s_px.show();
  s_ok = true;
}

void ledSetCo2(float co2, bool valid) {
  if (!s_ok) return;
  s_px.clear();

  if (!valid) {                       // kein Messwert -> erste LED blau
    s_px.setPixelColor(0, s_px.Color(0, 0, 90));
    s_px.show();
    return;
  }

  // Fuellhoehe des Balkens aus dem CO2-Wert bestimmen
  float frac = (co2 - CO2_BAR_MIN) / (float)(CO2_BAR_MAX - CO2_BAR_MIN);
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;

  int lit = (int)ceilf(frac * NUM_STATUS_LEDS);
  if (lit < 1) lit = 1;                              // bei gueltiger Messung min. 1
  if (lit > NUM_STATUS_LEDS) lit = NUM_STATUS_LEDS;

  for (int i = 0; i < lit; i++) {
    // ppm-Wert, den diese Position im Balken markiert -> feste Farbzone
    uint16_t ppmAtLed = CO2_BAR_MIN +
        (uint32_t)(i + 1) * (CO2_BAR_MAX - CO2_BAR_MIN) / NUM_STATUS_LEDS;
    s_px.setPixelColor(i, zoneColor(ppmAtLed));
  }
  s_px.show();
}

void ledConfirm() {
  if (!s_ok) return;
  for (int k = 0; k < 3; k++) {
    for (int i = 0; i < NUM_STATUS_LEDS; i++) s_px.setPixelColor(i, s_px.Color(0, 200, 0));
    s_px.show();  delay(150);
    s_px.clear(); s_px.show();  delay(150);
  }
}
