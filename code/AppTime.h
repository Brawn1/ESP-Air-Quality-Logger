// ============================================================================
//  AppTime.h  -  DS3231-RTC + NTP-Synchronisation
// ============================================================================
#pragma once
#include "config.h"

bool     timeBegin();               // RTC initialisieren (I2C bereits gestartet)
uint32_t timeNowEpoch();            // aktuelle Unix-Zeit (aus RTC, sonst System)
void     timeFormat(uint32_t epoch, char* buf, size_t len, bool withDate);

// NTP: System per SNTP stellen und danach die RTC nachziehen.
// Rueckgabe true, wenn erfolgreich synchronisiert.
bool     timeSyncNtp(uint32_t timeoutMs = 10000);

bool     timeRtcPresent();          // true, wenn DS3231 antwortet
