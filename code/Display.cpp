// ============================================================================
//  Display.cpp  -  SSD1306 ODER SH1106 (per OLED_USE_SH1106 in config.h)
// ============================================================================
#include "Display.h"
#include "AppTime.h"
#include "Sync.h"
#include <Adafruit_GFX.h>

#if OLED_USE_SH1106
  #include <Adafruit_SH110X.h>
  typedef Adafruit_SH1106G OledDriver;
  #define OLED_WHITE SH110X_WHITE
#else
  #include <Adafruit_SSD1306.h>
  typedef Adafruit_SSD1306 OledDriver;
  #define OLED_WHITE SSD1306_WHITE
#endif

namespace {
  // OLED an EIGENEM Hardware-SPI-Bus (HSPI) - dadurch kein Konflikt mit dem
  // SD-Treiber. Nur der DisplayTask nutzt diesen Bus -> kein Mutex noetig.
  SPIClass s_oledSpi(HSPI);
  OledDriver s_oled(OLED_WIDTH, OLED_HEIGHT, &s_oledSpi,
                    PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);
  bool s_ok = false;

  // CO2-Bewertungstext (ASCII, da der Standard-Font keine Umlaute darstellt)
  const __FlashStringHelper* co2Text(float co2) {
    if (co2 <= CO2_GOOD_MAX)   return F("OK");
    if (co2 <= CO2_MEDIUM_MAX) return F("Lueften empf.");
    return F("Raum lueften");
  }
}

bool displayBegin() {
  // eigenen HSPI-Bus starten (MISO = -1, nicht benoetigt). ESP32-SPIClass::begin
  // ist idempotent -> ein spaeteres internes begin() der Lib aendert die Pins nicht.
  s_oledSpi.begin(PIN_OLED_SCK, -1, PIN_OLED_MOSI);

#if OLED_USE_SH1106
  s_ok = s_oled.begin(0, true);            // i2caddr wird bei SPI ignoriert
  if (!s_ok) { Serial.println(F("[OLED] SH1106 init fehlgeschlagen!")); return false; }
  Serial.println(F("[OLED] SH1106 (HSPI) initialisiert."));
  s_oled.setContrast(OLED_CONTRAST);       // SH110X: kein Precharge-Kommando noetig
#else
  // reset=true, periphBegin=false (Bus haben wir gerade selbst gestartet)
  s_ok = s_oled.begin(SSD1306_SWITCHCAPVCC, 0, true, false);
  if (!s_ok) { Serial.println(F("[OLED] SSD1306 init fehlgeschlagen!")); return false; }
  Serial.println(F("[OLED] SSD1306 (HSPI) initialisiert."));
  s_oled.ssd1306_command(SSD1306_SETCONTRAST);
  s_oled.ssd1306_command(OLED_CONTRAST);
  s_oled.ssd1306_command(SSD1306_SETPRECHARGE);   // 0xD9, hellere Ausleuchtung
  s_oled.ssd1306_command(OLED_PRECHARGE);
#endif

  s_oled.clearDisplay();
  s_oled.setTextColor(OLED_WHITE);
  s_oled.display();
  return true;
}

void displayShowBoot(const char* line1, const char* line2) {
  if (!s_ok) return;
  s_oled.clearDisplay();
  s_oled.setTextSize(1);
  s_oled.setCursor(0, 0);  s_oled.println(line1);
  s_oled.setCursor(0, 12); s_oled.println(line2);
  s_oled.display();
}

void displayRender(const StatusSnapshot& snap) {
  if (!s_ok) return;

  // --- Puffer im RAM aufbauen (kein Buszugriff) ---
  s_oled.clearDisplay();
  s_oled.setTextSize(1);

  // === Kopfbereich (bei zweifarbigen OLEDs die obere Zone) ===
  // Zeile 1: Uhrzeit + Netz-Kennung (WiFi/AP)
  char timeStr[16];
  timeFormat(timeNowEpoch(), timeStr, sizeof(timeStr), false);
  s_oled.setCursor(0, 0);
  s_oled.print(timeStr);
  s_oled.setCursor(OLED_WIDTH - 24, 0);
  if (snap.wifiConnected)      s_oled.print(F("WiFi"));
  else if (snap.apActive)      s_oled.print(F("AP"));
  else                         s_oled.print(F("--"));

  // Zeile 2: IP-Adresse (STA-IP wenn verbunden, sonst eigene AP-IP)
  s_oled.setCursor(0, 8);
  s_oled.print(snap.ip);
  // Dauerhafter Warnhinweis, falls keine SD-Karte erkannt wurde
  if (!snap.sdOk) {
    s_oled.setCursor(OLED_WIDTH - 42, 8);
    s_oled.print(F("kein SD"));
  }

  // === Messwerte ===
  // CO2 gross
  s_oled.setTextSize(3);
  s_oled.setCursor(0, 18);
  if (snap.latest.valid) s_oled.printf("%4.0f", snap.latest.co2);
  else                   s_oled.print(F("----"));
  s_oled.setTextSize(1);
  s_oled.setCursor(OLED_WIDTH - 18, 34);
  s_oled.print(F("ppm"));

  // Temperatur / Feuchte
  s_oled.setCursor(0, 45);
  if (snap.latest.valid)
    s_oled.printf("T %.1fC  H %.0f%%", snap.latest.temp, snap.latest.hum);
  else
    s_oled.print(F("T --.-C  H --%"));

  // CO2-Bewertung
  s_oled.setCursor(0, 55);
  if (snap.latest.valid) s_oled.print(co2Text(snap.latest.co2));

  // --- einmalig ueber den eigenen HSPI-Bus flushen (kein Mutex noetig) ---
  s_oled.display();
}
