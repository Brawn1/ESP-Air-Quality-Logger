// ============================================================================
//  WebNet.h  -  WLAN (STA mit AP-Fallback), NTP-Sync, Async-Webserver
//  (Der Dateiname 'Network.*' darf NICHT verwendet werden - er kollidiert mit
//   der gleichnamigen Network-Bibliothek des ESP32-Cores.)
// ============================================================================
#pragma once
#include "config.h"

// Verbindet mit dem WLAN; startet bei Misserfolg einen eigenen Access Point.
// Aktualisiert die Statusflags im Data-Modul.
void networkConnect();

// Startet den Webserver (API + statische Dateien von der SD).
void networkStartServer();

// Periodische Pflege (Reconnect-Ueberwachung) - vom NetworkTask aufgerufen.
void networkLoop();

bool networkStaConnected();
