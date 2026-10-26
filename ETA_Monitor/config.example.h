#pragma once

// ============================================================
//  ETA Monitor - Konfigurationsdatei
//  VORLAGE: Diese Datei als config.h kopieren und anpassen.
//  config.h ist in .gitignore und wird NICHT ins Repository
//  übertragen (enthält Zugangsdaten).
// ============================================================

// ------ WiFi -----------------------------------------------
#define WIFI_SSID     "MeinNetzwerk"
#define WIFI_PASS     "MeinWLAN-Passwort"

// Fallback: wenn kein WLAN erreichbar → Access Point
#define AP_SSID       "ETA-Monitor"
#define AP_PASS       "BITTE-AENDERN-min-8-Zeichen"

// ------ WLAN-Watchdog --------------------------------------
// Startet der ESP nach einem Stromausfall schneller als der Router,
// ist beim Verbindungsversuch noch kein WLAN da. Ohne Watchdog blieb
// das Geraet danach bis zu einem manuellen Power-Cycle im AP-Modus.
// Der Fallback-AP bleibt erhalten - er wird nur keine Sackgasse mehr.
#define WIFI_CONNECT_TRIES  60          // 60 x 500 ms = 30 s beim Start
#define WIFI_CHECK_MS       30000UL     // Takt des Watchdogs im loop()
#define WIFI_AP_RETRY_MS    120000UL    // im AP-Modus alle 2 min erneut suchen
#define WIFI_STA_LOST_MS    600000UL    // 10 min ohne Verbindung -> Neustart

// ------ ETA RS232 ------------------------------------------
// Pins des UART2 am ESP32 (ggf. ans ECM50-A Pinlayout anpassen)
// Falls das ECM50-A einen eigenen RS232-Port hat, hier die
// zugehörigen GPIO-Nummern eintragen.
#define ETA_RX_PIN    18        // GPIO18 → A/RX-Klemme ECM50-A06 → DB9 Pin 2 (Ofen TX)
#define ETA_TX_PIN    17        // GPIO17 → B/TX-Klemme ECM50-A06 → DB9 Pin 3 (Ofen RX)
#define ETA_BAUD      19200     // typisch 19200, Baud laut ETA-Doku

// ------ ETA Knoten (Node-Nummern) --------------------------
// Diese Werte NICHT raten! Sie lassen sich mit der eingebauten
// Scan-Funktion direkt aus dem Kessel auslesen:
//     POST http://<ip>/scan?start=1&level=1
//     GET  http://<ip>/scan
// Die Antwort enthält für jeden Menüeintrag Knoten + Index + Teiler.
//
// Bei dieser Anlage (ETA Twin):
//   Knoten  8 → Kesselregelung / Hydraulik (Puffer, Boiler, Rücklauf)
//   Knoten 16 → Pelletsteil "PELL.KES" (Verbrennung, Zähler, Bedienteil)
//   Knoten 33 → Heizkreis-/Solarmodul (MK1..MK3, Kollektor, Aussen)
#define ETA_NODE_KESSEL   8
#define ETA_NODE_PELLET   16
#define ETA_NODE_HK       33

// Dauer eines kompletten Abfragezyklus über alle Blöcke (Sekunden, 10–255)
#define ETA_INTERVAL_SEC  60

// Intervall, das im MC-Befehl an den Kessel geschickt wird (Sekunden).
// Der Kessel sendet den Block frühestens nach dieser Zeit.
#define ETA_BLOCK_SEC     5

// ------ Web-Dashboard --------------------------------------
#define HTTP_PORT     80

// ------ OTA / Web-Login ------------------------------------
// Passwort fuer OTA-Updates (ArduinoOTA + Web-Upload /update) UND fuer das
// Web-Dashboard (Benutzer "admin"). Wer es kennt, kann den Kessel bedienen:
// unbedingt eigenes, langes Passwort setzen. Die Seite laeuft nur ueber HTTP
// (ohne Verschluesselung) - nur im vertrauenswuerdigen Heimnetz betreiben,
// niemals ins Internet freigeben.
#define OTA_PASS      "BITTE-AENDERN-langes-Passwort"
// mDNS-Hostname (erscheint als Netzwerk-Port in der Arduino IDE)
#define OTA_HOSTNAME  "eta-monitor"
// Firmware-Version (im Dashboard + auf der Update-Seite)
#define FW_VERSION    "2.5.0"