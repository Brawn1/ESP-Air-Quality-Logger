// ============================================================================
//  Led.cpp  -  WS2812B-Streifen als CO2-Ampel
//
//  ALLE LEDs leuchten in der Ampelfarbe des aktuellen Werts:
//    gruen (OK) -> orange (Lueften empf.) -> rot (Raum lueften).
//  Eigener Daten-Pin, kein Bus-Mutex noetig.
// ============================================================================
#include "Led.h"
#include <Adafruit_NeoPixel.h>

namespace {
  Adafruit_NeoPixel s_px(NUM_STATUS_LEDS,
                         PIN_STATUS_LED >= 0 ? PIN_STATUS_LED : 0,
                         NEO_GRB + NEO_KHZ800);
  bool s_ok = false;

  // Ampelfarbe fuer den aktuellen CO2-Wert.
  uint32_t levelColor(float co2) {
    if (co2 <= CO2_GOOD_MAX)   return s_px.Color(0, 160, 0);    // gruen (OK)
    if (co2 <= CO2_MEDIUM_MAX) return s_px.Color(210, 70, 0);   // orange
    return s_px.Color(190, 0, 0);                                // rot
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

void ledStartupTest() {
  if (!s_ok) return;

  // 1) Lauflicht: jede LED einzeln nacheinander -> zeigt, welche LEDs reagieren
  for (int i = 0; i < NUM_STATUS_LEDS; i++) {
    s_px.clear();
    s_px.setPixelColor(i, s_px.Color(0, 150, 0));
    s_px.show();
    delay(300);
  }

  // 2) alle LEDs gemeinsam in Rot, Gruen, Blau -> testet alle LEDs + Farbkanaele
  const uint32_t cols[3] = { s_px.Color(150, 0, 0), s_px.Color(0, 150, 0), s_px.Color(0, 0, 150) };
  for (int c = 0; c < 3; c++) {
    for (int i = 0; i < NUM_STATUS_LEDS; i++) s_px.setPixelColor(i, cols[c]);
    s_px.show();
    delay(400);
  }

  s_px.clear();
  s_px.show();
}

void ledSetCo2(float co2, bool valid) {
  if (!s_ok) return;

  // kein Messwert -> blau, sonst Ampelfarbe. ALLE LEDs leuchten gleich.
  uint32_t col = valid ? levelColor(co2) : s_px.Color(0, 0, 90);
  for (int i = 0; i < NUM_STATUS_LEDS; i++) s_px.setPixelColor(i, col);
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
