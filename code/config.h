// ============================================================================
//  config.h  -  Zentrale Konfiguration & gemeinsame Datentypen
//  Projekt: Raumluft-Qualitaet-Messung (ESP32-S3-Zero)
// ============================================================================
#pragma once
#include <Arduino.h>

// ----------------------------------------------------------------------------
//  PIN-BELEGUNG  (ESP32-S3-Zero / Waveshare)
//  -> Bitte gegen den Aufdruck deines Boards pruefen und ggf. anpassen.
//
//  I2C-Bus (gemeinsam fuer SCD30-Sensor und DS3231-RTC):
//     SDA = GPIO8      SCL = GPIO9
//
//  SPI-Bus 1 (FSPI) - NUR MicroSD:
//     SCK = GPIO12   MOSI = GPIO11   MISO = GPIO13   SD-CS = GPIO10
//  SPI-Bus 2 (HSPI) - NUR SSD1306-OLED (eigener Bus, damit kein Konflikt mit
//                     dem IDF-sdspi-Treiber der SD-Karte entsteht):
//     SCK = GPIO7   MOSI = GPIO1   DC = GPIO4   RST = GPIO6   CS = GPIO5
//     (OLED hat kein MISO - Display ist reines Schreibziel)
//
//  WS2812B-Statusstreifen (CO2-Balken), Daten-Pin (DIN): GPIO2
//  (GPIO1 oder GPIO2 verwenden; GPIO3 meiden = Strapping-Pin.
//   Hinweis: NICHT 'PIN_RGB_LED' verwenden - dieser Name ist im S3-Variant
//   bereits als Makro belegt.)
// ----------------------------------------------------------------------------
static const int PIN_I2C_SDA = 8;
static const int PIN_I2C_SCL = 9;

// SD-Bus (FSPI)
static const int PIN_SPI_MOSI = 11;
static const int PIN_SPI_SCK  = 12;
static const int PIN_SPI_MISO = 13;
static const int PIN_SD_CS    = 10;

// OLED-Bus (HSPI, eigener Bus)
static const int PIN_OLED_SCK  = 7;
static const int PIN_OLED_MOSI = 1;
static const int PIN_OLED_DC   = 4;
static const int PIN_OLED_RST  = 6;
static const int PIN_OLED_CS   = 5;

static const int PIN_STATUS_LED = 2;    // WS2812B-Streifen DIN; -1 = deaktiviert

static const int PIN_BOOT_BUTTON = 0;   // Onboard-BOOT-Taster (GPIO0, active low)

// ----------------------------------------------------------------------------
//  OLED-Geometrie (0,96" SSD1306)
// ----------------------------------------------------------------------------
static const int OLED_WIDTH  = 128;
static const int OLED_HEIGHT = 64;

// OLED-Controller waehlen:  0 = SSD1306,  1 = SH1106 (Adafruit SH110X)
// -> Zum Testen des anderen Display-Typs einfach umschalten und neu flashen.
#define OLED_USE_SH1106 0

// Helligkeit/Kontrast des OLED (0..255). 255 = maximal hell.
static const uint8_t OLED_CONTRAST = 255;

// Precharge-Periode (Befehl 0xD9): High-Nibble = Phase 2, Low-Nibble = Phase 1
// (je 1..15 Takte). Laenger = heller/gleichmaessiger. Adafruit-Default 0xF1.
// Zum Aufhellen v.a. das Low-Nibble (Phase 1) anheben, bis max. 0xFF.
static const uint8_t OLED_PRECHARGE = 0xF1;

// ----------------------------------------------------------------------------
//  Mess- & Logintervalle
// ----------------------------------------------------------------------------
// Drei getrennte Kadenzen (alle zur Laufzeit ueber config.json ueberschreibbar):
static const uint32_t SCD30_INTERVAL_S   = 8;    // Sensor-Messintervall (2..1800 s)
static const uint32_t HISTORY_INTERVAL_S = 90;   // Graph-Punkt + /history.json schreiben
static const uint32_t LOG_INTERVAL_S     = 300;  // CSV-Archiv (5 min)

// Anzahl der Punkte im Verlaufsgraphen (RAM-Ringpuffer + /history.json).
static const int HISTORY_LEN = 30;

// ----------------------------------------------------------------------------
//  CO2-Ampel-Schwellen (ppm)
// ----------------------------------------------------------------------------
static const uint16_t CO2_GOOD_MAX    = 1000;   // <= gruen (OK)
static const uint16_t CO2_MEDIUM_MAX  = 1400;   // <= orange (Lueften empf.), darueber rot

// ----------------------------------------------------------------------------
//  Manuelle Kalibrierung (Forced Recalibration) per BOOT-Taster
// ----------------------------------------------------------------------------
static const uint16_t CO2_CALIBRATION_PPM = 420;    // Referenz = Frischluft (akt. Atmosphaere ~420 ppm)
static const uint32_t CAL_HOLD_MS         = 3000;   // Taster so lange halten

// ----------------------------------------------------------------------------
//  Temperatur-Offset (SCD30 misst durch Eigenerwaermung zu hoch)
//  Wird per Software vom Messwert abgezogen: angezeigt = gemessen - Offset.
//  Positiv = Anzeige senken (Normalfall), negativ = anheben moeglich.
// ----------------------------------------------------------------------------
static const float TEMP_OFFSET_C = 3.0f;

// ----------------------------------------------------------------------------
//  WS2812B-Statusstreifen als CO2-Ampel (alle LEDs = Ampelfarbe)
// ----------------------------------------------------------------------------
static const int      NUM_STATUS_LEDS = 1;      // Anzahl LEDs am Streifen (bei neuem Streifen hier erhoehen)
static const uint8_t  LED_BRIGHTNESS  = 30;     // 0..255 (gedimmt, spart Strom)

// ----------------------------------------------------------------------------
//  WLAN / Netzwerk  (Standardwerte; werden von /config.json auf der SD ueberschrieben)
// ----------------------------------------------------------------------------
#define DEFAULT_WIFI_SSID     ""            // dein Heim-WLAN
#define DEFAULT_WIFI_PASS     ""
#define DEFAULT_AP_SSID       "Raumluft-Sensor"
#define DEFAULT_AP_PASS       "raumluft1234"  // min. 8 Zeichen (WPA2), "" = offen
#define DEFAULT_HOSTNAME      "raumluft"

// NTP / Zeitzone (Standard: Mitteleuropa mit Sommerzeit)
#define DEFAULT_NTP_SERVER    "pool.ntp.org"
#define DEFAULT_TZ            "CET-1CEST,M3.5.0,M10.5.0/3"

// ----------------------------------------------------------------------------
//  Gemeinsamer Datentyp fuer einen Messpunkt
// ----------------------------------------------------------------------------
struct Sample {
  uint32_t epoch;   // Unix-Zeit (Sekunden) laut RTC
  float    co2;     // ppm
  float    temp;    // Grad Celsius
  float    hum;     // % relative Feuchte
  uint32_t heap;    // freier Heap (Bytes) zum Messzeitpunkt - Diagnose
  bool     valid;
};

// ----------------------------------------------------------------------------
//  Laufzeit-Konfiguration (aus /config.json geladen)
// ----------------------------------------------------------------------------
struct AppConfig {
  String wifiSsid   = DEFAULT_WIFI_SSID;
  String wifiPass   = DEFAULT_WIFI_PASS;
  String apSsid     = DEFAULT_AP_SSID;
  String apPass     = DEFAULT_AP_PASS;
  String hostname   = DEFAULT_HOSTNAME;
  String ntpServer  = DEFAULT_NTP_SERVER;
  String tz         = DEFAULT_TZ;
  uint32_t logIntervalS     = LOG_INTERVAL_S;      // CSV-Archiv-Takt (s)
  uint32_t historyIntervalS = HISTORY_INTERVAL_S;  // Graph-/History-Takt (s)
  uint32_t scd30IntervalS   = SCD30_INTERVAL_S;    // SCD30-Mess-Takt (s, 2..1800)
  float    tempOffsetC      = TEMP_OFFSET_C;       // Temperatur-Offset (Grad C)
};

extern AppConfig g_cfg;

// ----------------------------------------------------------------------------
//  FreeRTOS-Task-Konfiguration
//  ESP32-S3 = Dual-Core:  Core 0 = Netzwerk, Core 1 = Messung/Anzeige
// ----------------------------------------------------------------------------
static const BaseType_t CORE_MEASURE = 1;   // Messkern
static const BaseType_t CORE_NETWORK = 0;   // Netzwerkkern

// Stackgroessen (Bytes) und Prioritaeten (hoeher = wichtiger, IDLE = 0)
static const uint32_t STACK_SENSOR  = 4096;
static const uint32_t STACK_DISPLAY = 4096;
static const uint32_t STACK_LOGGER  = 4096;
static const uint32_t STACK_NETWORK = 8192;

static const UBaseType_t PRIO_SENSOR  = 3;
static const UBaseType_t PRIO_DISPLAY = 2;
static const UBaseType_t PRIO_LOGGER  = 2;
static const UBaseType_t PRIO_NETWORK = 1;

// Laenge der Logger-Queue (gepufferte Samples, falls SD kurz blockiert)
static const uint32_t LOGGER_QUEUE_LEN = 8;
