// ============================================================================
//  Data.h  -  Thread-sicherer, geteilter Zustand (Live-Wert + Verlaufs-Ringpuffer)
//  Zugriff von SensorTask (Schreiber), DisplayTask & Web-Handlern (Leser).
// ============================================================================
#pragma once
#include "config.h"

// Status-Schnappschuss fuer Anzeige & Web-API.
struct StatusSnapshot {
  Sample   latest;                 // aktuellster Messwert
  Sample   history[HISTORY_LEN];   // Verlauf (aeltester ... neuester)
  int      historyCount;           // gefuellte Punkte (0..HISTORY_LEN)
  bool     wifiConnected;
  bool     apActive;
  bool     rtcOk;
  bool     sdOk;
  bool     timeSynced;
  char     ip[16];
  char     ssid[33];
  bool     messageActive;          // temporaere Einblendung (z.B. Kalibrierung)
  char     message[24];
};

// Muss VOR dem Start der Tasks aufgerufen werden (legt den Mutex an).
void dataBegin();

// --- Schreiber (SensorTask) -------------------------------------------------
void dataSetLatest(const Sample& s);       // Live-Wert aktualisieren
void dataPushHistory(const Sample& s);     // neuen Verlaufspunkt einfuegen

// --- Statusflags (Network/Storage-Task) ------------------------------------
void dataSetWifi(bool connected, bool apActive, const char* ip, const char* ssid);
void dataSetRtc(bool ok, bool synced);
void dataSetSd(bool ok);

// Temporaere Bildschirm-Meldung fuer 'durationMs' Millisekunden einblenden.
void dataSetMessage(const char* msg, uint32_t durationMs);

// --- Leser (Display / Web) --------------------------------------------------
// Erstellt eine konsistente Kopie des gesamten Zustands.
void dataSnapshot(StatusSnapshot& out);
