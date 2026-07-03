// ============================================================================
//  Sync.h  -  Globale, geteilte Busse + FreeRTOS-Mutexe
//
//  I2C (Wire) wird von SCD30 und RTC benutzt - teils aus Tasks auf
//  unterschiedlichen Kernen. Der SPI-Bus wird von MicroSD (Logger schreiben,
//  Webserver lesen) UND dem SSD1306-OLED (DisplayTask) gemeinsam genutzt.
//  Beide Busse muessen daher serialisiert werden.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <SPI.h>

// Gemeinsame SPI-Instanz fuer SD + OLED (in setup() mit Pins gestartet).
extern SPIClass g_spi;

extern SemaphoreHandle_t g_i2cMutex;
extern SemaphoreHandle_t g_spiMutex;

void syncBegin();   // legt die Mutexe an (vor Task-Start aufrufen)

// Bequeme Helfer
inline void i2cLock()   { xSemaphoreTake(g_i2cMutex, portMAX_DELAY); }
inline void i2cUnlock() { xSemaphoreGive(g_i2cMutex); }
inline void spiLock()   { xSemaphoreTake(g_spiMutex, portMAX_DELAY); }
inline void spiUnlock() { xSemaphoreGive(g_spiMutex); }

// Scope-Guards: sperren im Konstruktor, geben im Destruktor frei.
struct I2cGuard { I2cGuard() { i2cLock(); }  ~I2cGuard() { i2cUnlock(); } };
struct SpiGuard { SpiGuard() { spiLock(); }  ~SpiGuard() { spiUnlock(); } };
