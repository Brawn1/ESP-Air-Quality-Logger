// ============================================================================
//  Storage.h  -  MicroSD (SPI): Konfiguration, CSV-Logging, Datei-Auslieferung
//  Alle SD-Zugriffe laufen serialisiert ueber g_spiMutex (siehe Sync.h).
// ============================================================================
#pragma once
#include "config.h"
#include "Data.h"

bool storageBegin();                       // SD-Karte ueber SPI initialisieren
bool storageOk();

void storageLoadConfig();                  // /config.json -> g_cfg (Defaults bleiben, falls fehlt)

bool storageLogSample(const Sample& s);    // Messpunkt an Tages-CSV anhaengen

// Verlaufspuffer als /history.json sichern bzw. beim Booten in den Ringpuffer laden.
bool storageSaveHistory(const StatusSnapshot& snap);
void storageLoadHistory();

// Datei von der SD in einen String lesen (fuer den Webserver).
bool storageReadFile(const String& path, String& out);
bool storageFileExists(const String& path);
