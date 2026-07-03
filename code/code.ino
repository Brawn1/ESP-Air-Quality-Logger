// ============================================================================
//  Raumluft-Qualitaet-Messung  -  ESP32-S3-Zero
//
//  Sensor:   SCD30  (CO2 / Temperatur / Feuchte, I2C - Sensirion-Lib)
//  Anzeige:  SSD1306 0,96" OLED (SPI) - Uhrzeit, IP, CO2/Temp/Feuchte + Bewertung
//  Uhr:      DS3231 RTC (I2C) + NTP-Synchronisation
//  Speicher: MicroSD (SPI) - CSV-Archiv, /history.json, statische Webdateien
//  Netzwerk: WLAN (STA mit AP-Fallback) + Async-Webserver
//
//  Architektur (FreeRTOS, Dual-Core):
//    Core 1 (Messkern):   SensorTask, DisplayTask, LoggerTask
//    Core 0 (Netzkern):   NetworkTask (+ AsyncTCP-Webserver-Task)
//
//  Geteilte, nicht-threadsichere Busse werden per Mutex serialisiert:
//    g_i2cMutex -> Wire (SCD30 + RTC)
//    g_spiMutex -> SD-SPI-Bus (Logger/Webserver); OLED hat eigenen HSPI-Bus
// ============================================================================
#include <Wire.h>
#include "config.h"
#include "Sync.h"
#include "Data.h"
#include "Sensor.h"
#include "AppTime.h"
#include "Storage.h"
#include "Display.h"
#include "Led.h"
#include "WebNet.h"

AppConfig     g_cfg;                 // globale Laufzeitkonfiguration
QueueHandle_t g_logQueue = nullptr;  // SensorTask -> LoggerTask

// SD-Auftraege an den LoggerTask
enum LogCmdType { CMD_LOG_CSV, CMD_SAVE_HISTORY };
struct LogCmd { LogCmdType type; Sample sample; };

// ---------------------------------------------------------------------------
//  Tasks
// ---------------------------------------------------------------------------

// BOOT-Taster (GPIO0) pruefen und bei langem Druck die SCD30-Kalibrierung ausloesen
static void handleCalibrationButton() {
  static uint32_t btnDownMs = 0;
  static bool     triggered = false;

  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {          // gedrueckt (active low)
    if (btnDownMs == 0) {
      btnDownMs = millis();
    } else if (!triggered && (millis() - btnDownMs) >= CAL_HOLD_MS) {
      triggered = true;                               // pro Druck nur einmal
      dataSetMessage("Kalibriere...", 3000);
      bool ok = sensorForceCalibration(CO2_CALIBRATION_PPM);
      dataSetMessage(ok ? "Kalibriert!" : "Kal. Fehler", 4000);
      if (ok) ledConfirm();
    }
  } else {                                            // losgelassen -> zuruecksetzen
    btnDownMs = 0;
    triggered = false;
  }
}

// Core 1: Sensor lesen, Live-Wert + Verlauf + Ampel aktualisieren, Logs ausloesen
void sensorTask(void*) {
  uint32_t lastHistMs = 0, lastCsvMs = 0;
  Sample s;
  for (;;) {
    if (sensorRead(s)) {
      dataSetLatest(s);
      ledSetCo2(s.co2, s.valid);

      uint32_t nowMs = millis();

      // History-Punkt (Graph) + /history.json schreiben
      if (lastHistMs == 0 || (nowMs - lastHistMs) >= g_cfg.historyIntervalS * 1000UL) {
        lastHistMs = nowMs;
        dataPushHistory(s);
        LogCmd cmd = { CMD_SAVE_HISTORY, s };
        xQueueSend(g_logQueue, &cmd, 0);
      }
      // CSV-Archiv
      if (lastCsvMs == 0 || (nowMs - lastCsvMs) >= g_cfg.logIntervalS * 1000UL) {
        lastCsvMs = nowMs;
        LogCmd cmd = { CMD_LOG_CSV, s };
        xQueueSend(g_logQueue, &cmd, 0);
      }
    }
    handleCalibrationButton();
    vTaskDelay(pdMS_TO_TICKS(100));         // 100 ms -> Taster reagiert fluessig
  }
}

// Core 1: OLED regelmaessig neu zeichnen
void displayTask(void*) {
  for (;;) {
    StatusSnapshot snap;
    dataSnapshot(snap);
    displayRender(snap);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Core 1: SD-Auftraege abarbeiten (blockierende I/O isoliert vom Sensor)
void loggerTask(void*) {
  LogCmd cmd;
  for (;;) {
    if (xQueueReceive(g_logQueue, &cmd, portMAX_DELAY) == pdTRUE) {
      if (cmd.type == CMD_LOG_CSV) {
        storageLogSample(cmd.sample);
      } else {                              // CMD_SAVE_HISTORY
        StatusSnapshot snap;
        dataSnapshot(snap);                 // Datenmutex (kurz), dann...
        storageSaveHistory(snap);           // ...SD-Mutex -> keine Verschachtelung
      }
    }
  }
}

// Core 0: WLAN aufbauen, NTP synchronisieren, Webserver betreiben & ueberwachen
void networkTask(void*) {
  networkConnect();

  if (networkStaConnected()) {
    bool synced = timeSyncNtp(10000);
    dataSetRtc(timeRtcPresent(), synced);
  }
  networkStartServer();

  uint32_t lastSyncMs = millis();
  const uint32_t RESYNC_MS = 6UL * 3600UL * 1000UL;   // alle 6 h
  for (;;) {
    networkLoop();
    if (networkStaConnected() && (millis() - lastSyncMs) >= RESYNC_MS) {
      lastSyncMs = millis();
      bool synced = timeSyncNtp(5000);
      dataSetRtc(timeRtcPresent(), synced);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// ---------------------------------------------------------------------------
//  Setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n=== Raumluft-Qualitaet-Messung (ESP32-S3, FreeRTOS) ==="));

  // 1) Synchronisationsobjekte VOR jeglichem Buszugriff anlegen
  syncBegin();
  dataBegin();

  // 2) Busse starten
  //    I2C (gemeinsam fuer SCD30 + RTC)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);          // 100 kHz: vertraeglich mit SCD30-Clock-Stretching
  Wire.setTimeOut(100);           // laengeres Timeout fuer Clock-Stretching (ms)
  //    SPI (gemeinsam fuer SD + OLED) - einmal mit Pins starten
  g_spi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

  // 3) Peripherie
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);   // Taster active low (HIGH = nicht gedrueckt)
  ledBegin();
  ledSetCo2(0, false);

  if (storageBegin()) {
    storageLoadConfig();      // SD zuerst -> Konfig laden
    storageLoadHistory();     // gespeicherten Verlauf in den Ringpuffer laden
  }
  dataSetSd(storageOk());

  bool rtc = timeBegin();                     // nutzt TZ aus der Konfig
  dataSetRtc(rtc, false);

  sensorBegin();

  if (displayBegin()) displayShowBoot("Raumluft-Messung", "starte...");

  // 4) Queue + Tasks
  g_logQueue = xQueueCreate(LOGGER_QUEUE_LEN, sizeof(LogCmd));

  xTaskCreatePinnedToCore(sensorTask,  "sensor",  STACK_SENSOR,  nullptr, PRIO_SENSOR,  nullptr, CORE_MEASURE);
  xTaskCreatePinnedToCore(displayTask, "display", STACK_DISPLAY, nullptr, PRIO_DISPLAY, nullptr, CORE_MEASURE);
  xTaskCreatePinnedToCore(loggerTask,  "logger",  STACK_LOGGER,  nullptr, PRIO_LOGGER,  nullptr, CORE_MEASURE);
  xTaskCreatePinnedToCore(networkTask, "network", STACK_NETWORK, nullptr, PRIO_NETWORK, nullptr, CORE_NETWORK);

  Serial.println(F("[Setup] Tasks gestartet."));
}

// Die eigentliche Arbeit passiert in den Tasks; loop() bleibt leer.
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
