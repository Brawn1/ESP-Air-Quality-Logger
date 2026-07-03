// ============================================================================
//  Sync.cpp
// ============================================================================
#include "Sync.h"

SPIClass g_spi(FSPI);            // gemeinsamer SPI-Bus (SD + OLED)

SemaphoreHandle_t g_i2cMutex = nullptr;
SemaphoreHandle_t g_spiMutex = nullptr;

void syncBegin() {
  g_i2cMutex = xSemaphoreCreateMutex();
  g_spiMutex = xSemaphoreCreateMutex();
}
