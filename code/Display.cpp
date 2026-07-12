// ============================================================================
//  Display.cpp  -  SSD1306/SH1106, wahlweise SPI oder I2C
//    OLED_USE_SH1106 : 0 = SSD1306, 1 = SH1106
//    OLED_USE_I2C    : 0 = SPI (eigener HSPI-Bus), 1 = eigener 2. I2C-Bus (Wire1)
// ============================================================================
#include "Display.h"
#include "AppTime.h"
#include "Sync.h"
#include <Adafruit_GFX.h>
#include <Wire.h>

#if OLED_USE_SH1106
  #include <Adafruit_SH110X.h>
  typedef Adafruit_SH1106G OledDriver;
  #define OLED_WHITE SH110X_WHITE
#else
  #include <Adafruit_SSD1306.h>
  typedef Adafruit_SSD1306 OledDriver;
  #define OLED_WHITE SSD1306_WHITE
#endif

// In beiden Modi nutzt NUR der DisplayTask den Bus (I2C = eigener 2. Bus Wire1,
// SPI = eigener HSPI-Bus) -> kein Mutex noetig, Guard bleibt leer.
struct OledGuard { };

namespace {
#if OLED_USE_I2C
  // Eigener zweiter I2C-Bus (Peripherie 1) nur fuer das OLED, kein Reset-Pin (-1)
  TwoWire s_oledWire(1);
  OledDriver s_oled(OLED_WIDTH, OLED_HEIGHT, &s_oledWire, -1);
#else
  // SPI: eigener HSPI-Bus
  SPIClass s_oledSpi(HSPI);
  OledDriver s_oled(OLED_WIDTH, OLED_HEIGHT, &s_oledSpi,
                    PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);
#endif
  bool s_ok = false;

  // CO2-Bewertungstext (ASCII, da der Standard-Font keine Umlaute darstellt)
  const __FlashStringHelper* co2Text(float co2) {
    if (co2 <= CO2_GOOD_MAX)   return F("OK");
    if (co2 <= CO2_MEDIUM_MAX) return F("Lueften empf.");
    return F("Raum lueften");
  }
}

bool displayBegin() {
#if OLED_USE_I2C
  // eigenen 2. I2C-Bus an den Display-Pins starten (400 kHz, eigener Bus)
  s_oledWire.begin(PIN_OLED_I2C_SDA, PIN_OLED_I2C_SCL);
  s_oledWire.setClock(400000);
#else
  // eigenen HSPI-Bus starten (MISO = -1, nicht benoetigt)
  s_oledSpi.begin(PIN_OLED_SCK, -1, PIN_OLED_MOSI);
#endif

#if OLED_USE_SH1106
  #if OLED_USE_I2C
    s_ok = s_oled.begin(OLED_I2C_ADDR, true);
  #else
    s_ok = s_oled.begin(0, true);                  // Adresse bei SPI ignoriert
  #endif
#else  // SSD1306
  #if OLED_USE_I2C
    // periphBegin=false: Wire wurde in setup() bereits mit den Pins gestartet
    s_ok = s_oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR, true, false);
  #else
    s_ok = s_oled.begin(SSD1306_SWITCHCAPVCC, 0, true, false);
  #endif
#endif

  if (!s_ok) { Serial.println(F("[OLED] init fehlgeschlagen!")); return false; }
  Serial.println(F("[OLED] initialisiert."));

  // Kontrast/Helligkeit
#if OLED_USE_SH1106
  s_oled.setContrast(OLED_CONTRAST);
#else
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
  { OledGuard g; s_oled.display(); }
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

  // --- einmalig flushen (bei I2C unter Bus-Sperre, bei SPI ohne) ---
  { OledGuard g; s_oled.display(); }
}
