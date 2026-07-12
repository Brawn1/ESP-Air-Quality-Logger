# Raumluft-Qualität-Messung (ESP32-S3-Zero)

CO₂-, Temperatur- und Feuchte-Messung mit dem **SCD30**, Anzeige auf einem
**0,96" OLED**, Status-Anzeige über eine **WS2812B-LED-Ampel**, Uhrzeit von
einer **DS3231-RTC** (mit NTP-Abgleich), Datenlogging auf **MicroSD** und einem
**WLAN-Webinterface**, dessen statische Dateien von der SD-Karte ausgeliefert
werden.

Die Firmware läuft unter **FreeRTOS** und verteilt die Arbeit auf beide Kerne
des ESP32-S3.

---

## Funktionen

- **Messung** von CO₂ / Temperatur / Luftfeuchte (SCD30, NDIR-Sensor)
- **OLED-Anzeige**: Uhrzeit, IP-Adresse, Messwerte groß + Ampel-Bewertung
- **LED-Ampel** (WS2812B): grün/orange/rot je nach CO₂, gut ablesbar quer durch den Raum
- **Weboberfläche** (responsiv, ohne externe Bibliotheken) mit Live-Werten und
  Verlaufsgraph (CO₂ / Temp / Feuchte umschaltbar)
- **Datenlogging** als Tages-CSV auf SD; Verlauf übersteht Neustart (`/history.json`)
- **Uhrzeit** aus DS3231-RTC, automatischer **NTP-Abgleich** bei WLAN-Verbindung
- **WLAN**: Verbindung ins Heimnetz, bei Fehlschlag eigener **Access-Point-Fallback**
- **Manuelle Kalibrierung** des SCD30 per BOOT-Taster (Forced Recalibration)

---

## Architektur (FreeRTOS, Dual-Core)

| Task          | Kern | Prio | Aufgabe                                                |
|---------------|:----:|:----:|--------------------------------------------------------|
| `SensorTask`  |  1   |  3   | SCD30 lesen, Live-Wert/Verlauf/LED-Ampel, Logs auslösen, BOOT-Taster |
| `DisplayTask` |  1   |  2   | OLED zeichnen (Uhr, IP, Werte, Bewertung)              |
| `LoggerTask`  |  1   |  2   | SD-Aufträge abarbeiten (CSV + `/history.json`)         |
| `NetworkTask` |  0   |  1   | WLAN (STA + AP-Fallback), NTP, Webserver-Verwaltung    |
| AsyncTCP      |  0   |  –   | Webserver-Requests (von der Bibliothek verwaltet)      |

**Geteilte Busse werden per Mutex serialisiert** (nicht thread-safe):

- `g_i2cMutex` – **I²C** (SCD30 + DS3231), Zugriffe aus Tasks beider Kerne
- `g_spiMutex` – **SD-SPI-Bus** (Logger schreibt, Webserver liest)
- Daten-Mutex im `Data`-Modul – geteilter Zustand (Live-Wert + Verlaufs-Ringpuffer)
- Das **OLED** liegt auf einem **eigenen HSPI-Bus** (nur `DisplayTask`) → kein Mutex nötig

Datenfluss: `SensorTask` → (RAM-Ringpuffer + Queue) → `LoggerTask` (SD) /
`DisplayTask` (OLED) / Web-API (RAM).

---

## Verdrahtung (bitte gegen den Aufdruck deines Boards prüfen!)

### I²C-Bus – SCD30 und DS3231-RTC
| Signal | ESP32-S3-Zero |
|--------|---------------|
| SDA    | GPIO8         |
| SCL    | GPIO9         |
| VCC    | 3V3 (SCD30 verträgt 5V am VIN, Logik bleibt 3V3) |
| GND    | GND           |

> Beide I²C-Geräte an **denselben** SDA/SCL. Der SCD30 nutzt Clock-Stretching;
> deshalb ist der I²C-Takt bewusst auf 100 kHz gesetzt. Pull-ups (4,7 kΩ) sind
> auf den Modulen meist vorhanden.

### SPI-Bus 1 (FSPI) – nur MicroSD
| Signal | ESP32-S3-Zero |
|--------|---------------|
| SCK    | GPIO12        |
| MOSI   | GPIO11        |
| MISO   | GPIO13        |
| CS     | GPIO10        |
| VCC / GND | 3V3 / GND  |

### SPI-Bus 2 (HSPI) – nur SSD1306-OLED (**eigener** Bus!)
SD und OLED **dürfen sich den SPI-Bus nicht teilen**: Die ESP32-`SD.h` nutzt den
IDF-`sdspi`-Treiber und belegt den SPI-Host exklusiv – ein zweites Gerät am
selben Bus bleibt stumm. Deshalb hat das OLED einen eigenen Bus.

| OLED-Pin (7-polige SPI-Platine) | ESP32-S3-Zero |
|---------------------------------|---------------|
| D0 (CLK/SCK)                    | GPIO7         |
| D1 (DATA/MOSI)                  | GPIO1         |
| DC                              | GPIO4         |
| RES                             | GPIO6         |
| CS                              | GPIO5         |
| VCC / GND                       | 3V3 / GND     |

### WS2812B-Statusstreifen (CO₂-Ampel)
| Streifen | ESP32-S3-Zero |
|----------|---------------|
| DIN (Pfeilrichtung beachten!) | GPIO2 (alt. GPIO1) |
| 5V       | 5V            |
| GND      | GND           |

> **GPIO3 meiden** (Strapping-Pin). Die LEDs (`NUM_STATUS_LEDS`, Standard 4) sind auf `LED_BRIGHTNESS = 30/255`
> gedimmt (Strom/Blendung); bei langen/hellen Streifen externes 5V-Netzteil mit
> gemeinsamer GND. Kurze Streifen laufen meist direkt mit dem 3,3-V-Datensignal,
> sonst Pegelwandler (3,3→5 V).

### BOOT-Taster
Der Onboard-**BOOT-Taster (GPIO0)** dient im Betrieb zum Auslösen der
Kalibrierung (siehe unten). Ruhezustand HIGH, gedrückt LOW.

### Pin-Übersicht
| Pin | Funktion | Pin | Funktion |
|-----|----------|-----|----------|
| GPIO8  | I²C SDA        | GPIO7 | OLED SCK (D0) |
| GPIO9  | I²C SCL        | GPIO1 | OLED MOSI (D1) |
| GPIO12 | SD SCK         | GPIO4 | OLED DC |
| GPIO11 | SD MOSI        | GPIO6 | OLED RES |
| GPIO13 | SD MISO        | GPIO5 | OLED CS |
| GPIO10 | SD CS          | GPIO2 | WS2812B DIN |
| GPIO0  | BOOT-Taster    |       |          |

Alle Pins zentral in [`code/config.h`](code/config.h) änderbar.

---

## Projektstruktur

```
code/                 ← Arduino-Sketch (alle Dateien gehören dazu)
  code.ino            Setup + FreeRTOS-Tasks
  config.h            Pins, Intervalle, Schwellen, Task-Konfiguration
  Sync.*              geteilter SPI-Bus + Mutexe (I²C / SPI)
  Data.*              thread-sicherer Ringpuffer + Live-Wert + Status
  Sensor.*            SCD30 (Sensirion I2C SCD30)
  AppTime.*           DS3231-RTC + NTP
  Storage.*           SD: config.json, CSV, history.json, Datei-Auslieferung
  Display.*           OLED (SSD1306 / SH1106 umschaltbar)
  Led.*               WS2812B-CO₂-Ampel
  WebNet.*            WLAN (STA/AP), NTP-Start, Async-Webserver
sd_card/              ← Inhalt auf die SD-Karten-Wurzel kopieren
  config.json         WLAN + Einstellungen
  www/                Weboberfläche (index.html, style.css, app.js)
README.md
```

---

## Benötigte Arduino-Bibliotheken

Über den **Bibliotheksverwalter** installieren:

- **Sensirion I2C SCD30** (zieht *Sensirion Core* mit)
- **Adafruit SSD1306** + **Adafruit GFX Library**
- **Adafruit SH110X** (nur nötig, wenn `OLED_USE_SH1106 = 1`)
- **RTClib** (Adafruit)
- **Adafruit NeoPixel**
- **ArduinoJson** (v7)
- **Async TCP** *(Maintainer: ESP32Async, v3.4.x)*
- **ESP Async WebServer** *(Maintainer: ESP32Async, v3.11.x)*

> ⚠️ **Richtigen Async-Fork wählen!** Es gibt mehrere gleichnamige Bibliotheken.
> Es funktionieren **nur** die von **ESP32Async** gepflegten, exakt benannt
> **„Async TCP"** und **„ESP Async WebServer"** (mit Leerzeichen). Die Varianten
> `AsyncTCP` (dvarrel) bzw. `ESPAsyncWebServer` (lacamera) sind mit ESP32-Core 3.x
> **nicht** kompatibel (Compile-Fehler in `WiFiGeneric.h`).

`WiFi`, `SD`, `SPI`, `Wire`, `ESPmDNS` sind Teil des ESP32-Arduino-Cores.

### Board / Core
- Boardverwalter-URL für ESP32 hinzufügen, **esp32 by Espressif** (Core **3.x**) installieren.
- Board: **ESP32S3 Dev Module** (oder „Waveshare ESP32-S3-Zero", falls vorhanden).
- **PSRAM**: „QSPI/OPI PSRAM" aktivieren, falls dein Modul PSRAM hat (sonst „Disabled").
- **USB CDC On Boot: Enabled** (für die serielle Ausgabe über USB-C).
- **Partition Scheme: „Huge APP (3MB No OTA/1MB SPIFFS)"** – reichlich Reserve
  (SPIFFS wird nicht gebraucht, alles liegt auf der SD).

---

## Einrichtung

1. **SD-Karte** (FAT32) vorbereiten und den **Inhalt** von [`sd_card/`](sd_card/)
   in das **Wurzelverzeichnis** kopieren:
   ```
   SD-Root/
     config.json
     www/
       index.html
       style.css
       app.js
   ```
2. In `config.json` **WLAN-Zugangsdaten** und ggf. Zeitzone/Intervalle eintragen.
   - Leeres `wifi_ssid` → Gerät startet direkt im **Access-Point-Modus**.
   - Fehlschlagende WLAN-Verbindung → automatischer AP `Raumluft-Sensor`
     (Passwort `raumluft1234`).
3. Sketch [`code/code.ino`](code/code.ino) in der Arduino IDE öffnen, Board wählen,
   **hochladen**.
4. Serielle Konsole (115200 Baud) zeigt die zugewiesene **IP-Adresse**.
   Aufruf im Browser: `http://<IP>` oder `http://raumluft.local`.

---

## Konfiguration (`config.json`)

Alle Werte werden **zur Laufzeit** von der SD geladen; die Konstanten in
`config.h` sind nur die Fallback-Defaults, falls die Datei fehlt.

| Schlüssel            | Standard            | Bedeutung |
|----------------------|---------------------|-----------|
| `wifi_ssid`          | –                   | WLAN-Name (leer = direkt AP-Modus) |
| `wifi_pass`          | –                   | WLAN-Passwort |
| `ap_ssid`            | `Raumluft-Sensor`   | Name des Fallback-Access-Points |
| `ap_pass`            | `raumluft1234`      | AP-Passwort (≥8 Zeichen, "" = offen) |
| `hostname`           | `raumluft`          | mDNS-Name → `http://raumluft.local` |
| `ntp_server`         | `pool.ntp.org`      | NTP-Server |
| `tz`                 | `CET-1CEST,M3.5.0,M10.5.0/3` | Zeitzone (POSIX-TZ) |
| `scd30_interval_s`   | `5`                 | Mess-Takt des Sensors (2…1800 s) |
| `history_interval_s` | `90`                | Graph-Punkt + `/history.json` schreiben |
| `log_interval_s`     | `300`               | CSV-Archiv-Takt (s) |
| `temp_offset_c`      | `3.0`               | Temperatur-Offset gegen SCD30-Eigenerwärmung (°C, nur positiv) |

> ⚠️ **JSON-Schlüssel sind Groß-/Kleinschreibung-sensitiv** – exakt so schreiben,
> sonst wird der Eintrag still ignoriert und der Standardwert genutzt.

---

## Bedienung

### OLED-Anzeige
```
12:34:56              WiFi   ← Uhrzeit + Netz-Kennung (WiFi/AP)
192.168.1.42     kein SD     ← IP (STA/AP); rechts "kein SD" falls Karte fehlt
612          ppm             ← CO₂ groß
T 22.8C  H 45%               ← Temperatur / Feuchte
OK                           ← Bewertung: OK / Lueften empf. / Raum lueften
```

Fehlt/defekt die SD-Karte, zeigt das Display beim Start einen Warn-Screen
(„KEINE SD-KARTE") und dauerhaft den Hinweis **`kein SD`** neben der IP. Die
Karte wird **im Betrieb überwacht** (alle ~5 s): Herausziehen wird erkannt und
angezeigt, Wiedereinstecken automatisch übernommen. (Ohne SD gibt es kein
Webinterface, da dessen Dateien auf der Karte liegen.)

### LED-Ampel (WS2812B)
Alle LEDs (`NUM_STATUS_LEDS`, Standard 4) leuchten gemeinsam in der Ampelfarbe des
aktuellen CO₂-Werts:
**grün ≤ 1000 (OK) → orange ≤ 1400 (Lüften empfohlen) → rot > 1400 (Raum lüften).**
Kein Messwert → blau.

### Weboberfläche
Live-Werte (Polling), umschaltbarer Verlaufsgraph (CO₂ / Temp / Feuchte) und
Statusleiste (WLAN, SD, RTC/NTP). Die statischen Dateien werden im Browser
gecacht – nach dem ersten Laden werden bei Reload nur noch die kleinen
`/api/*`-Aufrufe geholt (schont Verbindungen).

---

## Web-API

| Endpoint           | Antwort                                            |
|--------------------|----------------------------------------------------|
| `GET /api/current` | aktueller Messwert + Status (JSON)                 |
| `GET /api/history` | letzte 30 Verlaufspunkte (JSON, aus RAM)           |
| alles andere       | statische Datei aus `/www` auf der SD              |

Beispiel `/api/current`:
```json
{"time":1751385600,"co2":612,"temp":22.80,"hum":47.0,"valid":true,
 "wifi":true,"ap":false,"ip":"192.168.1.42","ssid":"MeinWLAN",
 "rtc":true,"sd":true,"synced":true}
```

---

## Dateien auf der SD-Karte

| Pfad              | Inhalt |
|-------------------|--------|
| `/config.json`    | Konfiguration (s. o.) |
| `/www/…`          | Weboberfläche (HTML/CSS/JS) |
| `/history.json`   | letzte 30 Verlaufspunkte – wird alle `history_interval_s` geschrieben und beim Booten geladen (Graph sofort gefüllt) |
| `/logs/JJJJ-MM-TT.csv` | Langzeit-Archiv, eine Zeile je `log_interval_s` |

CSV-Format:
```
epoch_utc,datetime_local,co2_ppm,temp_c,hum_pct
```

---

## Kalibrierung (BOOT-Taster)

Der SCD30 lässt sich per **Forced Recalibration (FRC)** über den **BOOT-Taster
(GPIO0)** auf einen Frischluft-Referenzwert setzen (`CO2_CALIBRATION_PPM`,
Standard **420 ppm** ≈ heutige Außenluft).

1. Gerät im Betrieb an **gut durchmischte Frischluft** bringen (draußen / offenes
   Fenster mit Durchzug), Sensor **frei halten, nicht bodennah**.
2. **~2 Minuten** messen lassen, damit der Wert stabil ist.
3. **BOOT-Taster ca. 3 Sekunden halten** (`CAL_HOLD_MS`).
4. Bestätigung: LED-Streifen blinkt **3× grün** (und OLED-Meldung, falls aktiv).

**Wichtig:**
- Taster nur **im Betrieb** drücken – *nicht* beim Reset/Einschalten (sonst geht
  der ESP32 in den Download-Modus).
- **Abends auf dem Land ungünstig:** windstille Abende stauen bodennah CO₂ →
  besser tagsüber bei etwas Wind.
- Eine falsche Referenz verzieht die Werte dauerhaft, lässt sich aber jederzeit
  mit besserer Frischluft neu kalibrieren.

> Alternativ gibt es die automatische Selbstkalibrierung (ASC) des SCD30. Sie ist
> **aus**; bei Dauerbetrieb in regelmäßig gelüfteten Räumen kann sie in
> `sensorBegin()` per `activateAutoCalibration(1)` eingeschaltet werden.

---

## Hinweise / Tuning / Troubleshooting

- **OLED bleibt dunkel?** SD und OLED **nicht** an denselben SPI-Bus hängen (s. o.).
  Bleibt es auf dem eigenen HSPI-Bus *immer noch* leer, ist es evtl. ein
  **SH1106**-Controller statt SSD1306. Umschalter in `config.h`:
  ```c
  #define OLED_USE_SH1106 1   // 0 = SSD1306, 1 = SH1106 (Adafruit SH110X)
  ```
- **OLED zu dunkel / ungleichmäßig?** Kontrast steht bereits auf `OLED_CONTRAST = 255`,
  Precharge (`OLED_PRECHARGE`, SSD1306) lässt sich Richtung `0xFF` anheben. Eine
  weiche Rand-Abdunklung ist meist eine Eigenschaft des Panels selbst.
- **Verlauf übersteht Neustart** über `/history.json` (Laden beim Booten). Die
  laufende `/api/history` bleibt reines RAM (schnell), das CSV ist das Archiv.
- **WLAN nur 2,4 GHz:** Der ESP32 verbindet sich nicht mit 5-GHz-Netzen. Klappt
  die STA-Verbindung nicht, läuft das Gerät im AP-Modus (`192.168.4.1`), und ohne
  Internet gibt es keinen NTP-Sync (Zeit dann rein aus der DS3231).
- **Webserver reagiert nach mehreren Verbindungen nicht mehr?** Diagnose über die
  serielle Heap-Ausgabe (alle 30 s): fällt „Heap frei" stetig → Speicher; bleibt
  er stabil → Verbindungs-Slots. Das Caching der statischen Dateien reduziert die
  Last bereits deutlich; bei Bedarf Poll-Intervall in `app.js` strecken.
- **SD-Takt:** Bei instabilen Karten in `Storage.cpp` von `20000000` auf `4000000`
  (4 MHz) reduzieren.
- **RTC erstmalig stellen:** passiert automatisch beim ersten erfolgreichen
  NTP-Sync. Ohne Internet läuft die Zeit rein aus der DS3231.

### Modul-Namen (Fallstricke)
Das Netzwerkmodul heißt bewusst `WebNet.*` (nicht `Network.*`) und die
LED-Konstante `PIN_STATUS_LED` (nicht `PIN_RGB_LED`) – beide Namen sind im
ESP32-Core/S3-Variant bereits belegt und würden sonst Compile-Fehler auslösen.

---

## Lizenz

Copyright (C) 2026 Guenter Bailey

Dieses Projekt steht unter der **GNU Affero General Public License v3.0**
(AGPL-3.0) – siehe [LICENSE](LICENSE).

> Dieses Programm ist freie Software: Sie können es unter den Bedingungen der
> AGPL-3.0 weitergeben und/oder verändern. Es wird **ohne jede Gewährleistung**
> bereitgestellt; siehe die Lizenz für Details. Sie sollten eine Kopie der Lizenz
> zusammen mit diesem Programm erhalten haben. Falls nicht, siehe
> <https://www.gnu.org/licenses/>.
